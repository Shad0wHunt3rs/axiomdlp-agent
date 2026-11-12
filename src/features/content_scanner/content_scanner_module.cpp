// src/features/content_scanner/content_scanner_module.cpp
//
// Phase 9: PII scan batches now travel over MQTT.
//
// Wire shape (docs/mqtt_protocol.md §9.3):
//   Request  → axiomdlp/agents/{id}/scan/batch  (QoS 1)
//     {
//       "v": 1,
//       "correlation_id": "<ulid>",
//       "agent_id": <int>,
//       "chunks": [
//         { "file_hash": "...", "path": "...",
//           "chunk_index": N, "chunk_count": M,
//           "content_b64": "<base64>" },
//         ...
//       ]
//     }
//   Response → axiomdlp/replies/{instance_uuid}
//     {
//       "v": 1, "correlation_id": "<ulid>",
//       "ok": true,
//       "results": [
//         { "file_hash": "...", "chunk_index": N,
//           "sensitive": bool, "entity_type": "...",
//           "risk_score": int, "action": "block|alert|allow",
//           "error": "..." }
//       ]
//     }
//   Failure:
//     { "v": 1, "ok": false, "error": "not_found" | "agent_revoked" |
//                                       "invalid_request" | "internal" }
//
// Per-chunk errors are reported as `results[].error` with the batch `ok`
// still true. Files with any chunk error stay 'pending' so they retry
// next cycle; we do NOT commit a partial verdict.

#include "features/content_scanner/content_scanner_module.h"
#include "storage/file_repository.h"
#include "storage/state_repository.h"
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

constexpr const char* kStateKeyAgentId = "agent_id";

void interruptible_sleep(std::atomic<bool>& stop_flag,
                         std::chrono::seconds total) {
    constexpr auto tick = 100ms;
    auto elapsed = 0ms;
    const auto target = std::chrono::duration_cast<std::chrono::milliseconds>(total);
    while (elapsed < target) {
        if (stop_flag.load(std::memory_order_relaxed)) return;
        std::this_thread::sleep_for(tick);
        elapsed += tick;
    }
}

bool file_exists(const std::string& path) {
    std::error_code ec;
    return std::filesystem::exists(std::filesystem::path(path), ec);
}

bool ext_in_list(const std::string& ext, const std::vector<std::string>& list) {
    return std::find(list.begin(), list.end(), ext) != list.end();
}

struct ReadResult {
    std::vector<char> data;
    std::string       error;
    bool ok() const { return error.empty(); }
};

ReadResult read_file_bounded(const std::string& path, std::size_t max_bytes) {
    ReadResult r;
    std::FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) { r.error = "cannot open file"; return r; }
    struct Closer { std::FILE* p; ~Closer() { if (p) std::fclose(p); } } closer{f};

    if (std::fseek(f, 0, SEEK_END) != 0) { r.error = "seek failed"; return r; }
    long file_size = std::ftell(f);
    if (file_size < 0) { r.error = "cannot determine file size"; return r; }
    std::rewind(f);

    std::size_t to_read = std::min<std::size_t>(static_cast<std::size_t>(file_size), max_bytes);
    r.data.resize(to_read);
    std::size_t got = std::fread(r.data.data(), 1, to_read, f);
    if (got < to_read) {
        if (std::ferror(f)) { r.error = "read error"; return r; }
        r.data.resize(got);
    }
    return r;
}

std::vector<std::pair<std::size_t, std::size_t>>
compute_chunk_ranges(std::size_t size, std::size_t chunk_size, std::size_t overlap) {
    std::vector<std::pair<std::size_t, std::size_t>> ranges;
    if (size == 0) return ranges;
    std::size_t stride = (chunk_size > overlap) ? (chunk_size - overlap) : chunk_size;
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

ContentScannerModule::ContentScannerModule()  = default;
ContentScannerModule::~ContentScannerModule() = default;

bool ContentScannerModule::initialize(ModuleContext& context) {
    ModuleBase::initialize(context);

    const auto& scan_cfg = ctx_->config.modules.content_scanner;

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

    if (!enabled_) return true;

    if (ctx_->request_client == nullptr) {
        SPDLOG_ERROR("content_scanner: no request client (MQTT disabled at build time); "
                     "module disabled");
        enabled_ = false;
        return true;
    }
    req_ = ctx_->request_client;

    SPDLOG_INFO("content_scanner module: initialized (batch_size={}, interval={}s)",
                batch_size_, scan_interval_sec_);
    return true;
}

bool ContentScannerModule::start() {
    if (!enabled_) return true;
    stop_requested_.store(false, std::memory_order_relaxed);
    sync_thread_ = std::thread([this] { sync_loop(); });
    running_.store(true, std::memory_order_relaxed);
    SPDLOG_INFO("content_scanner module: started");
    return true;
}

void ContentScannerModule::stop() {
    if (!running_.load(std::memory_order_relaxed)) return;

    stop_requested_.store(true, std::memory_order_relaxed);
    if (sync_thread_.joinable()) sync_thread_.join();

    running_.store(false, std::memory_order_relaxed);

    SPDLOG_INFO("content_scanner module: stopped (scanned={}, sensitive={}, skipped={})",
                total_scanned_, total_sensitive_, total_skipped_);
}

void ContentScannerModule::shutdown() {
    SPDLOG_INFO("content_scanner module: shutting down");
}

HealthStatus ContentScannerModule::health() const {
    if (ctx_ == nullptr) return HealthStatus::unknown("not initialized");
    if (!enabled_)       return HealthStatus::ok("disabled");
    if (!running_.load(std::memory_order_relaxed)) return HealthStatus::failed("scanner thread not running");
    return HealthStatus::ok("scanned=" + std::to_string(total_scanned_));
}

void ContentScannerModule::sync_loop() {
    // Wait until agent_sync has persisted agent_id. Content scanner has
    // no way to make useful requests without it.
    while (!stop_requested_.load(std::memory_order_relaxed)) {
        if (ctx_->shutdown.is_requested()) return;
        if (auto saved = ctx_->state_store.get(kStateKeyAgentId)) {
            try {
                const int id = std::stoi(*saved);
                if (id > 0) {
                    agent_id_ = id;
                    SPDLOG_DEBUG("content_scanner: using agent_id={}", agent_id_);
                    break;
                }
            } catch (const std::exception&) { /* ignore */ }
        }
        std::this_thread::sleep_for(1s);
    }

    if (stop_requested_.load(std::memory_order_relaxed) || ctx_->shutdown.is_requested()) return;

    // Small grace period so agent_sync's initial register/heartbeat has
    // settled before we hammer the scan/batch topic.
    interruptible_sleep(stop_requested_, std::chrono::seconds(5));

    while (!stop_requested_.load(std::memory_order_relaxed)) {
        if (ctx_->shutdown.is_requested()) return;
        process_one_cycle();
        if (stop_requested_.load(std::memory_order_relaxed)) break;
        interruptible_sleep(stop_requested_, std::chrono::seconds(scan_interval_sec_));
    }
}

void ContentScannerModule::process_one_cycle() {
    // Re-check agent_id: it can go stale if the server says not_found
    // or if a prior cycle's re-enroll cleared it.
    if (agent_id_ <= 0) {
        if (auto saved = ctx_->state_store.get(kStateKeyAgentId)) {
            try { agent_id_ = std::stoi(*saved); } catch (const std::exception&) { /* ignore */ }
        }
    }
    if (agent_id_ <= 0) {
        SPDLOG_DEBUG("content_scanner: no agent_id yet — deferring cycle");
        return;
    }
    if (!req_->is_connected()) {
        SPDLOG_DEBUG("content_scanner: transport not connected — deferring cycle");
        return;
    }

    auto pending = ctx_->file_store.fetch_pending_for_scan(
        static_cast<std::size_t>(batch_size_), max_retries_);
    if (pending.empty()) return;

    SPDLOG_INFO("content_scanner: {} pending files", pending.size());

    std::vector<Chunk> chunks;
    std::map<std::string, FileProgress> progress;

    for (auto& row : pending) {
        if (stop_requested_.load(std::memory_order_relaxed)) break;
        prepare_file(row, chunks, progress);
        if (chunks.size() >= static_cast<std::size_t>(batch_size_)) {
            send_chunks_in_batches(chunks, progress);
            chunks.clear();
        }
    }

    if (!chunks.empty()) send_chunks_in_batches(chunks, progress);
    commit_completed_files(progress);
}

bool ContentScannerModule::prepare_file(
        const FileRow& row,
        std::vector<Chunk>& out_chunks,
        std::map<std::string, FileProgress>& progress) {

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

    std::string new_hash = util::sha256_hex(read.data.data(), read.data.size());

    if (auto cached = ctx_->file_store.find_scan_verdict_by_hash(new_hash)) {
        ctx_->file_store.update_scan_result_by_path(
            row.path, new_hash, cached->sensitive, cached->sensitivity);
        ++total_scanned_;
        if (cached->sensitive) ++total_sensitive_;
        return false;
    }

    auto ranges = compute_chunk_ranges(read.data.size(), chunk_size_, chunk_overlap_);
    if (ranges.empty()) {
        ctx_->file_store.update_scan_result_by_path(row.path, new_hash, false, "");
        ++total_scanned_;
        return false;
    }

    FileProgress fp;
    fp.path            = row.path;
    fp.chunks_expected = static_cast<std::uint32_t>(ranges.size());
    progress[new_hash] = fp;

    for (std::size_t i = 0; i < ranges.size(); ++i) {
        const auto [start, end] = ranges[i];
        Chunk c;
        c.file_hash   = new_hash;
        c.path        = row.path;
        c.chunk_index = static_cast<std::uint32_t>(i);
        c.chunk_count = static_cast<std::uint32_t>(ranges.size());
        c.content.assign(read.data.begin() + start, read.data.begin() + end);
        out_chunks.push_back(std::move(c));
    }
    return true;
}

void ContentScannerModule::send_chunks_in_batches(
        std::vector<Chunk>& chunks,
        std::map<std::string, FileProgress>& progress) {

    std::size_t i = 0;
    while (i < chunks.size()) {
        if (stop_requested_.load(std::memory_order_relaxed)) return;
        if (ctx_->shutdown.is_requested()) return;

        // Re-check agent_id inside the loop — a prior batch may have
        // received not_found and cleared it.
        if (agent_id_ <= 0) {
            if (auto saved = ctx_->state_store.get(kStateKeyAgentId)) {
                try { agent_id_ = std::stoi(*saved); } catch (const std::exception&) { /* ignore */ }
            }
        }
        if (agent_id_ <= 0) {
            SPDLOG_DEBUG("content_scanner: agent_id lost mid-cycle — aborting send loop");
            return;
        }

        std::size_t batch_bytes = 0;
        std::size_t j = i;
        while (j < chunks.size() && (j - i) < static_cast<std::size_t>(batch_size_)) {
            const std::size_t estimate = chunks[j].content.size() + 256;
            if (j > i && batch_bytes + estimate > batch_max_bytes_) break;
            batch_bytes += estimate;
            ++j;
        }
        if (j == i) break;

        // Build the chunk array — content_b64, matching messages.go ScanChunk.
        json batch_chunks = json::array();
        for (std::size_t k = i; k < j; ++k) {
            batch_chunks.push_back({
                {"file_hash",   chunks[k].file_hash},
                {"path",        chunks[k].path},
                {"chunk_index", chunks[k].chunk_index},
                {"chunk_count", chunks[k].chunk_count},
                {"content_b64", util::base64_encode(chunks[k].content.data(),
                                                    chunks[k].content.size())}
            });
        }

        json payload;
        payload["agent_id"] = agent_id_;
        payload["chunks"]   = std::move(batch_chunks);

        const std::string topic =
            "axiomdlp/agents/" + std::to_string(agent_id_) + "/scan/batch";

        SPDLOG_INFO("content_scanner: request '{}' ({} chunks, ~{} KB)",
                    topic, j - i, batch_bytes / 1024);

        const auto resp = req_->request(topic, payload,
                                        /*qos=*/1, request_timeout_sec_);

        if (!resp) {
            SPDLOG_DEBUG("content_scanner: scan batch timed out or transport down");
            // Do not advance i — leave chunks in the buffer so next
            // cycle can retry them. Return now; caller commits nothing.
            return;
        }

        if (!resp->value("ok", false)) {
            const std::string err = resp->value("error", "");
            SPDLOG_WARN("content_scanner: scan batch rejected: error='{}'", err);

            if (err == "agent_revoked") {
                ctx_->shutdown.request(ShutdownReason::AgentRevoked);
                return;
            }
            if (err == "not_found") {
                SPDLOG_WARN("content_scanner: server does not know agent_id={}; "
                            "clearing state, waiting for re-enroll", agent_id_);
                ctx_->state_store.remove(kStateKeyAgentId);
                agent_id_ = 0;
                return;
            }
            // invalid_request / internal — log already done; leave chunks
            // buffered so next cycle retries. Do not advance i.
            return;
        }

        merge_batch_response(*resp, progress);

        // Wipe chunk content from memory (security + memory hygiene).
        for (std::size_t k = i; k < j; ++k) {
            std::fill(chunks[k].content.begin(), chunks[k].content.end(), 0);
            chunks[k].content.clear();
            chunks[k].content.shrink_to_fit();
        }
        i = j;
    }
}

void ContentScannerModule::merge_batch_response(
        const nlohmann::json& parsed,
        std::map<std::string, FileProgress>& progress) {

    if (!parsed.contains("results") || !parsed["results"].is_array()) return;

    for (const auto& r : parsed["results"]) {
        if (!r.is_object()) continue;
        const std::string file_hash = r.value("file_hash", "");
        if (file_hash.empty()) continue;

        auto it = progress.find(file_hash);
        if (it == progress.end()) continue;

        // Per-chunk error means we could not score this chunk (PII service
        // down, etc). Skip it — the file stays incomplete and retries.
        if (r.contains("error") && !r["error"].get<std::string>().empty()) {
            SPDLOG_DEBUG("content_scanner: chunk error ({}): {}",
                         file_hash.substr(0, 12),
                         r["error"].get<std::string>());
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
}

void ContentScannerModule::commit_completed_files(
        std::map<std::string, FileProgress>& progress) {

    std::size_t complete = 0;
    std::size_t incomplete = 0;

    for (auto& [hash, fp] : progress) {
        if (fp.chunks_returned < fp.chunks_expected) { ++incomplete; continue; }
        ctx_->file_store.update_scan_result_by_path(
            fp.path, hash, fp.sensitive, fp.sensitivity);
        ++total_scanned_;
        if (fp.sensitive) ++total_sensitive_;
        ++complete;
    }

    if (complete > 0 || incomplete > 0) {
        SPDLOG_INFO("content_scanner: cycle done (committed={}, incomplete={})",
                    complete, incomplete);
    }
}

} // namespace axiom::core