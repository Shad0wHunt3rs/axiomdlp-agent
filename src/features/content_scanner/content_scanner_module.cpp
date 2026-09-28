// src/features/content_scanner/content_scanner_module.cpp

#include "features/content_scanner/content_scanner_module.h"

#include "net/http_client.h"
#include "storage/file_repository.h"
#include "util/base64.h"
#include "util/file_hash.h"

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <string>
#include <thread>
#include <vector>

namespace axiom::core {

using json = nlohmann::json;
using namespace std::chrono_literals;

namespace {

// Sleep in small ticks so stop_requested_ is honored promptly.
void interruptible_sleep(std::atomic<bool>& stop_flag,
                         std::chrono::seconds total) {
    constexpr auto tick = 100ms;
    auto elapsed = 0ms;
    const auto target =
        std::chrono::duration_cast<std::chrono::milliseconds>(total);

    while (elapsed < target) {
        if (stop_flag.load(std::memory_order_relaxed)) return;
        std::this_thread::sleep_for(tick);
        elapsed += tick;
    }
}

bool file_exists(const std::string& path) {
    std::error_code ec;
    return std::filesystem::exists(path, ec);
}

bool ext_in_list(const std::string& ext,
                 const std::vector<std::string>& list) {
    return std::find(list.begin(), list.end(), ext) != list.end();
}

struct ReadResult {
    std::vector<char> data;
    std::string       error;
    bool ok() const { return error.empty(); }
};

// Read up to `max_bytes` from the file, in one shot.
ReadResult read_file_bounded(const std::string& path, std::size_t max_bytes) {
    ReadResult r;

    std::FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) {
        r.error = "cannot open file";
        return r;
    }

    struct Closer {
        std::FILE* p;
        ~Closer() { if (p) std::fclose(p); }
    } closer{f};

    if (std::fseek(f, 0, SEEK_END) != 0) {
        r.error = "seek failed";
        return r;
    }
    long file_size = std::ftell(f);
    if (file_size < 0) {
        r.error = "cannot determine file size";
        return r;
    }
    std::rewind(f);

    std::size_t to_read = std::min<std::size_t>(
        static_cast<std::size_t>(file_size), max_bytes);

    r.data.resize(to_read);
    std::size_t got = std::fread(r.data.data(), 1, to_read, f);
    if (got < to_read) {
        if (std::ferror(f)) {
            r.error = "read error";
            return r;
        }
        r.data.resize(got);
    }

    return r;
}

// Compute non-overlapping-start ranges for chunks that overlap by
// `overlap` bytes. The final chunk may be shorter than chunk_size.
//
// Example: size=10, chunk_size=4, overlap=1
//   Chunks: [0,4), [3,7), [6,10)
//   Each consecutive pair overlaps by 1 byte.
std::vector<std::pair<std::size_t, std::size_t>>
compute_chunk_ranges(std::size_t size,
                     std::size_t chunk_size,
                     std::size_t overlap) {
    std::vector<std::pair<std::size_t, std::size_t>> ranges;
    if (size == 0) return ranges;

    std::size_t stride = (chunk_size > overlap) ? (chunk_size - overlap)
                                                : chunk_size;

    std::size_t start = 0;
    while (start < size) {
        std::size_t end = std::min(start + chunk_size, size);
        ranges.push_back({start, end});
        if (end >= size) break;
        start += stride;
    }
    return ranges;
}

} // namespace

// ----------------------------------------------------------------
// Construction
// ----------------------------------------------------------------

ContentScannerModule::ContentScannerModule()  = default;
ContentScannerModule::~ContentScannerModule() = default;

// ----------------------------------------------------------------
// Lifecycle
// ----------------------------------------------------------------

bool ContentScannerModule::initialize(ModuleContext& context) {
    ModuleBase::initialize(context);

    const auto& scan_cfg = ctx_->config.modules.content_scanner;
    const auto& sync_cfg = ctx_->config.modules.agent_sync;

    enabled_             = scan_cfg.enabled;
    batch_size_          = scan_cfg.batch_size;
    batch_max_bytes_     = scan_cfg.batch_max_bytes;
    scan_interval_sec_   = scan_cfg.scan_interval_seconds;
    chunk_size_          = scan_cfg.chunk_size_bytes;
    chunk_overlap_       = scan_cfg.chunk_overlap_bytes;
    max_file_bytes_      = scan_cfg.max_file_bytes;
    max_retries_         = scan_cfg.max_retries;
    request_timeout_sec_ = scan_cfg.request_timeout_seconds;
    text_extensions_     = scan_cfg.text_extensions;

    http_base_url_ = sync_cfg.http_base_url;
    api_token_     = sync_cfg.api_token;
    verify_tls_    = sync_cfg.verify_tls;

    if (!enabled_) {
        SPDLOG_INFO("content_scanner module: disabled in config");
        return true;
    }

    if (http_base_url_.empty() || api_token_.empty()) {
        SPDLOG_ERROR("content_scanner module: agent_sync must be enabled "
                     "with a valid http_base_url and api_token");
        return false;
    }

    if (text_extensions_.empty()) {
        SPDLOG_ERROR("content_scanner module: text_extensions is empty");
        return false;
    }

    SPDLOG_INFO("content_scanner module: initialized "
                "(batch_size={}, interval={}s, chunk={}KB, "
                "overlap={}B, max_file={}MB, ext_count={})",
                batch_size_, scan_interval_sec_,
                chunk_size_ / 1024, chunk_overlap_,
                max_file_bytes_ / (1024 * 1024), text_extensions_.size());
    return true;
}

bool ContentScannerModule::start() {
    if (!enabled_) return true;

    http_ = std::make_unique<HttpClient>();

    stop_requested_.store(false, std::memory_order_relaxed);
    sync_thread_ = std::thread([this] { sync_loop(); });
    running_.store(true, std::memory_order_relaxed);

    SPDLOG_INFO("content_scanner module: started");
    return true;
}

void ContentScannerModule::stop() {
    if (!running_.load(std::memory_order_relaxed)) return;

    stop_requested_.store(true, std::memory_order_relaxed);

    if (sync_thread_.joinable()) {
        sync_thread_.join();
    }

    http_.reset();
    running_.store(false, std::memory_order_relaxed);

    SPDLOG_INFO("content_scanner module: stopped "
                "(scanned={}, sensitive={}, skipped={})",
                total_scanned_, total_sensitive_, total_skipped_);
}

void ContentScannerModule::shutdown() {
    SPDLOG_INFO("content_scanner module: shutting down");
}

HealthStatus ContentScannerModule::health() const {
    if (ctx_ == nullptr) return HealthStatus::unknown("not initialized");
    if (!enabled_)       return HealthStatus::ok("disabled");
    if (!running_.load(std::memory_order_relaxed)) {
        return HealthStatus::failed("scanner thread not running");
    }
    return HealthStatus::ok(
        "scanned=" + std::to_string(total_scanned_) +
        " sensitive=" + std::to_string(total_sensitive_));
}

// ----------------------------------------------------------------
// Sync loop
// ----------------------------------------------------------------

void ContentScannerModule::sync_loop() {
    // Short delay so inventory has time to populate some rows.
    interruptible_sleep(stop_requested_, std::chrono::seconds(5));

    while (!stop_requested_.load(std::memory_order_relaxed)) {
        process_one_cycle();

        if (stop_requested_.load(std::memory_order_relaxed)) break;

        interruptible_sleep(stop_requested_,
                            std::chrono::seconds(scan_interval_sec_));
    }
}

void ContentScannerModule::process_one_cycle() {
    auto pending = ctx_->file_store.fetch_pending_for_scan(
        static_cast<std::size_t>(batch_size_), max_retries_);

    if (pending.empty()) {
        SPDLOG_DEBUG("content_scanner: no pending files");
        return;
    }

    SPDLOG_INFO("content_scanner: {} pending files", pending.size());

    std::vector<Chunk> chunks;
    std::map<std::string, FileProgress> progress;

    for (auto& row : pending) {
        if (stop_requested_.load(std::memory_order_relaxed)) break;

        prepare_file(row, chunks, progress);

        // Send the current batch if it's already full.
        if (chunks.size() >= static_cast<std::size_t>(batch_size_)) {
            send_chunks_in_batches(chunks, progress);
            chunks.clear();
        }
    }

    // Send whatever remains.
    if (!chunks.empty()) {
        send_chunks_in_batches(chunks, progress);
    }

    // Write verdicts to the DB for files whose chunks all came back.
    commit_completed_files(progress);
}

// ----------------------------------------------------------------
// Per-file preparation
// ----------------------------------------------------------------

bool ContentScannerModule::prepare_file(
        const FileRow& row,
        std::vector<Chunk>& out_chunks,
        std::map<std::string, FileProgress>& progress) {

    // Extension filter.
    if (!ext_in_list(row.extension, text_extensions_)) {
        ctx_->file_store.mark_not_applicable(row.path);
        ++total_skipped_;
        return false;
    }

    if (row.size > max_file_bytes_) {
        ctx_->file_store.mark_skipped_too_large(row.path);
        ++total_skipped_;
        return false;
    }

    if (!file_exists(row.path)) {
        ctx_->file_store.mark_file_missing(row.path);
        ++total_skipped_;
        return false;
    }

    auto read = read_file_bounded(row.path, max_file_bytes_);
    if (!read.ok()) {
        ctx_->file_store.record_scan_error_by_path(row.path, read.error);
        return false;
    }

    // Hash the content we actually read.
    std::string new_hash = util::sha256_hex(read.data.data(), read.data.size());

    // Dedup: if a prior verdict exists for this exact content, apply it
    // and skip the HTTP call.
    if (auto cached = ctx_->file_store.find_scan_verdict_by_hash(new_hash)) {
        ctx_->file_store.update_scan_result_by_path(
            row.path, new_hash, cached->sensitive, cached->sensitivity);
        ++total_scanned_;
        if (cached->sensitive) ++total_sensitive_;
        SPDLOG_DEBUG("content_scanner: reused verdict for {} (sensitive={})",
                     row.path, cached->sensitive);
        return false;
    }

    auto ranges = compute_chunk_ranges(read.data.size(), chunk_size_, chunk_overlap_);

    if (ranges.empty()) {
        // Empty file: nothing to scan, but it's still "done".
        ctx_->file_store.update_scan_result_by_path(row.path, new_hash, false, "");
        ++total_scanned_;
        return false;
    }

    FileProgress fp;
    fp.path             = row.path;
    fp.chunks_expected  = static_cast<std::uint32_t>(ranges.size());
    progress[new_hash]  = fp;

    for (std::size_t i = 0; i < ranges.size(); ++i) {
        const auto [start, end] = ranges[i];

        Chunk c;
        c.file_hash   = new_hash;
        c.path        = row.path;
        c.chunk_index = static_cast<std::uint32_t>(i);
        c.chunk_count = static_cast<std::uint32_t>(ranges.size());
        c.content.assign(read.data.begin() + static_cast<std::ptrdiff_t>(start),
                         read.data.begin() + static_cast<std::ptrdiff_t>(end));
        out_chunks.push_back(std::move(c));
    }

    return true;
}

// ----------------------------------------------------------------
// Batch sending
// ----------------------------------------------------------------

void ContentScannerModule::send_chunks_in_batches(
        std::vector<Chunk>& chunks,
        std::map<std::string, FileProgress>& progress) {

    std::size_t i = 0;
    while (i < chunks.size()) {
        if (stop_requested_.load(std::memory_order_relaxed)) return;

        // Build one batch: take chunks from index i until either
        // batch_size is reached or batch_max_bytes would be exceeded.
        json batch_chunks = json::array();
        std::size_t batch_bytes = 0;
        std::size_t j = i;

        while (j < chunks.size() &&
               batch_chunks.size() < static_cast<std::size_t>(batch_size_)) {

            const auto& c = chunks[j];

            // Estimate JSON payload contribution of this chunk.
            // Base64 length = 4 * ceil(len / 3) ≈ 1.37 * len.
            const std::size_t encoded_len = 4 * ((c.content.size() + 2) / 3);
            const std::size_t estimate    = encoded_len + 512;   // metadata overhead

            if (!batch_chunks.empty() &&
                batch_bytes + estimate > batch_max_bytes_) {
                break;   // this chunk goes in the next batch
            }

            batch_chunks.push_back({
                {"file_hash",   c.file_hash},
                {"path",        c.path},
                {"chunk_index", c.chunk_index},
                {"chunk_count", c.chunk_count},
                {"content",     util::base64_encode(c.content.data(),
                                                    c.content.size())}
            });
            batch_bytes += estimate;
            ++j;
        }

        if (batch_chunks.empty()) break;

        json payload;
        payload["chunks"] = std::move(batch_chunks);

        const std::string url = http_base_url_ + "/api/v1/agent/scan/batch";

        SPDLOG_INFO("content_scanner: POST {} ({} chunks, ~{} KB)",
                    url, payload["chunks"].size(), batch_bytes / 1024);

        const auto resp = http_->post_json(url, payload.dump(), api_token_,
                                           request_timeout_sec_, verify_tls_);

        // Clear the raw content of chunks that were just sent.
        for (std::size_t k = i; k < j; ++k) {
            std::fill(chunks[k].content.begin(), chunks[k].content.end(), 0);
            chunks[k].content.clear();
            chunks[k].content.shrink_to_fit();
        }

        if (!resp.error.empty()) {
            SPDLOG_WARN("content_scanner: transport error: {}", resp.error);
            return;   // chunks stay pending; retried next cycle
        }

        if (resp.status == 401 || resp.status == 403) {
            SPDLOG_ERROR("content_scanner: auth rejected ({}) — stopping",
                         resp.status);
            running_.store(false, std::memory_order_relaxed);
            stop_requested_.store(true, std::memory_order_relaxed);
            return;
        }

        if (!resp.is_2xx()) {
            SPDLOG_WARN("content_scanner: HTTP {}: {}",
                        resp.status, resp.body);
            return;
        }

        merge_batch_response(resp.body, progress);

        i = j;
    }
}

// ----------------------------------------------------------------
// Response merging
// ----------------------------------------------------------------

void ContentScannerModule::merge_batch_response(
        const std::string& response_body,
        std::map<std::string, FileProgress>& progress) {

    try {
        const auto parsed = json::parse(response_body);

        if (!parsed.contains("results") || !parsed["results"].is_array()) {
            SPDLOG_WARN("content_scanner: response missing 'results'");
            return;
        }

        for (const auto& r : parsed["results"]) {
            const std::string file_hash = r.value("file_hash", "");
            if (file_hash.empty()) continue;

            auto it = progress.find(file_hash);
            if (it == progress.end()) continue;

            if (r.contains("error")) {
                // Chunk failed; don't count it. The file will be retried.
                continue;
            }

            it->second.chunks_returned += 1;

            if (r.value("sensitive", false)) {
                it->second.sensitive = true;
                if (it->second.sensitivity.empty()) {
                    it->second.sensitivity = r.value("entity_type", "");
                }
            }
        }
    } catch (const std::exception& e) {
        SPDLOG_ERROR("content_scanner: failed to parse response: {}", e.what());
    }
}

// ----------------------------------------------------------------
// Final commit
// ----------------------------------------------------------------

void ContentScannerModule::commit_completed_files(
        std::map<std::string, FileProgress>& progress) {

    std::size_t complete = 0;
    std::size_t sensitive_in_cycle = 0;
    std::size_t incomplete = 0;

    for (auto& [hash, fp] : progress) {
        if (fp.chunks_returned < fp.chunks_expected) {
            ++incomplete;
            continue;   // retried next cycle
        }

        ctx_->file_store.update_scan_result_by_path(
            fp.path, hash, fp.sensitive, fp.sensitivity);

        ++total_scanned_;
        if (fp.sensitive) {
            ++total_sensitive_;
            ++sensitive_in_cycle;
        }
        ++complete;
    }

    if (complete > 0 || incomplete > 0) {
        SPDLOG_INFO("content_scanner: cycle done "
                    "(files committed={}, sensitive={}, incomplete={})",
                    complete, sensitive_in_cycle, incomplete);
    }
}

} // namespace axiom::core