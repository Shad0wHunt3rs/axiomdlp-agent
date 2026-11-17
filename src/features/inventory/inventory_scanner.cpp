// src/features/inventory/inventory_scanner.cpp

#include "features/inventory/inventory_scanner.h"
#include "storage/file_repository.h"
#include "util/file_hash.h"
#include "util/glob_match.h"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#ifndef _WIN32
#  include <sys/stat.h>
#endif

namespace axiom::core {

namespace {

constexpr std::size_t kBatchSize         = 5000;
constexpr std::size_t kStopCheckInterval = 256;

std::string to_lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

std::string extract_extension(const std::filesystem::path& p) {
    const auto ext = p.extension().string();
    if (ext.empty() || ext[0] != '.') return {};
    return to_lower(ext.substr(1));
}

std::string extract_basename(const std::filesystem::path& p) {
    return p.filename().string();
}

std::string extract_dirname(const std::filesystem::path& p) {
    return p.parent_path().string();
}

std::string categorize(const std::string& ext, bool is_executable) {
    static const std::unordered_map<std::string, std::string> table = {
        {"jpg","image"},{"jpeg","image"},{"png","image"},{"gif","image"},
        {"bmp","image"},{"svg","image"},{"webp","image"},{"ico","image"},
        {"tiff","image"},{"tif","image"},{"heic","image"},{"heif","image"},
        {"mp4","video"},{"mkv","video"},{"avi","video"},{"mov","video"},
        {"wmv","video"},{"flv","video"},{"webm","video"},{"m4v","video"},
        {"mpg","video"},{"mpeg","video"},
        {"mp3","audio"},{"wav","audio"},{"flac","audio"},{"ogg","audio"},
        {"m4a","audio"},{"aac","audio"},{"wma","audio"},{"opus","audio"},
        {"pdf","document"},{"doc","document"},{"docx","document"},
        {"xls","document"},{"xlsx","document"},{"ppt","document"},
        {"pptx","document"},{"odt","document"},{"ods","document"},
        {"odp","document"},{"txt","document"},{"md","document"},
        {"rtf","document"},{"tex","document"},
        {"zip","archive"},{"tar","archive"},{"gz","archive"},
        {"bz2","archive"},{"xz","archive"},{"7z","archive"},
        {"rar","archive"},{"zst","archive"},{"tgz","archive"},
        {"c","code"},{"cpp","code"},{"cc","code"},{"cxx","code"},
        {"h","code"},{"hpp","code"},{"hxx","code"},{"py","code"},
        {"js","code"},{"ts","code"},{"jsx","code"},{"tsx","code"},
        {"java","code"},{"go","code"},{"rs","code"},{"rb","code"},
        {"php","code"},{"sh","code"},{"bash","code"},{"zsh","code"},
        {"ps1","code"},{"html","code"},{"css","code"},{"scss","code"},
        {"json","code"},{"xml","code"},{"yaml","code"},{"yml","code"},
        {"toml","code"},{"lua","code"},{"pl","code"},{"swift","code"},
        {"kt","code"},{"scala","code"},{"cs","code"},{"sql","code"},
        {"csv","data"},{"tsv","data"},{"db","data"},
        {"sqlite","data"},{"sqlite3","data"},{"parquet","data"},
        {"arrow","data"},{"feather","data"},
        {"conf","config"},{"cfg","config"},{"ini","config"},
        {"env","config"},{"properties","config"},
        {"exe","executable"},{"bin","executable"},{"so","executable"},
        {"dll","executable"},{"dylib","executable"},{"app","executable"},
        {"deb","package"},{"rpm","package"},{"msi","package"},
        {"apk","package"},{"pkg","package"},{"dmg","package"},
        {"iso","other"},
    };

    auto it = table.find(ext);
    if (it != table.end()) return it->second;
    if (is_executable) return "executable";
    return "other";
}

std::uint64_t path_identity(const std::string& path) {
    std::hash<std::string> hasher;
    return static_cast<std::uint64_t>(hasher(path));
}

bool is_scannable_category(const std::string& category,
                           const std::vector<std::string>& scannable) {
    return std::find(scannable.begin(), scannable.end(), category) != scannable.end();
}

bool is_text_extension(const std::string& ext,
                       const std::vector<std::string>& text_extensions) {
    return std::find(text_extensions.begin(),
                     text_extensions.end(), ext) != text_extensions.end();
}

} // namespace

// ----------------------------------------------------------------
// InventoryScanner
// ----------------------------------------------------------------

bool InventoryScanner::is_excluded(const std::string& path) const {
    for (const auto& pattern : ctx_.config.modules.inventory.exclude_paths) {
        if (util::path_matches_pattern(pattern, path)) return true;
    }
    return false;
}

void InventoryScanner::scan_one_root(const std::string& root,
                                     std::uint64_t walk_id,
                                     const std::atomic<bool>& stop_flag,
                                     InventoryResult& result) {
    namespace fs = std::filesystem;
    std::error_code ec;

    if (!fs::exists(root, ec) || !fs::is_directory(root, ec)) {
        SPDLOG_WARN("InventoryScanner: skipping '{}' (not a directory)", root);
        return;
    }

    const auto& inv_cfg  = ctx_.config.modules.inventory;
    const auto& scan_cfg = ctx_.config.modules.content_scanner;

    auto options = fs::directory_options::skip_permission_denied;
    std::size_t since_commit   = 0;
    std::size_t since_stop_chk = 0;

    for (auto it = fs::recursive_directory_iterator(root, options, ec);
         it != fs::recursive_directory_iterator();
         it.increment(ec)) {

        if (ec) { ec.clear(); continue; }

        if (++since_stop_chk >= kStopCheckInterval) {
            since_stop_chk = 0;
            if (stop_flag.load(std::memory_order_relaxed)) {
                SPDLOG_INFO("InventoryScanner: stop requested, aborting scan early");
                result.was_cancelled = true;
                return;
            }
            // If a batch-level transaction op failed, bail out of the
            // walk. scan_all will log the error and skip prune_stale.
            if (result.commit_failed) {
                return;
            }
        }

        const auto& entry   = *it;
        const auto path_str = entry.path().string();

        if (is_excluded(path_str)) {
            if (entry.is_directory(ec)) it.disable_recursion_pending();
            continue;
        }

        if (entry.is_symlink(ec)) continue;

        FileRow row;

        row.path      = path_str;
        row.basename  = extract_basename(entry.path());
        row.dirname   = extract_dirname(entry.path());
        row.extension = extract_extension(entry.path());

        row.dev   = 0;
        row.inode = path_identity(path_str);

        row.is_directory = entry.is_directory(ec);
        row.file_type    = row.is_directory ? "dir" : "file";

        row.is_hidden = (!row.basename.empty() &&
                         row.basename[0] == '.' &&
                         row.basename != "." &&
                         row.basename != "..");

#ifndef _WIN32
        struct ::stat st{};
        if (::stat(path_str.c_str(), &st) == 0) {
            row.size     = static_cast<std::uint64_t>(st.st_size);
            row.mode     = static_cast<std::uint32_t>(st.st_mode);
            row.uid      = static_cast<std::uint32_t>(st.st_uid);
            row.gid      = static_cast<std::uint32_t>(st.st_gid);
            row.nlink    = static_cast<std::uint32_t>(st.st_nlink);

            row.mtime_ns = static_cast<std::int64_t>(st.st_mtim.tv_sec) * 1000000000LL
                         + static_cast<std::int64_t>(st.st_mtim.tv_nsec);
            row.ctime_ns = static_cast<std::int64_t>(st.st_ctim.tv_sec) * 1000000000LL
                         + static_cast<std::int64_t>(st.st_ctim.tv_nsec);
            row.atime_ns = static_cast<std::int64_t>(st.st_atim.tv_sec) * 1000000000LL
                         + static_cast<std::int64_t>(st.st_atim.tv_nsec);

            if (S_ISDIR(st.st_mode))       row.file_type = "dir";
            else if (S_ISREG(st.st_mode))  row.file_type = "file";
            else if (S_ISLNK(st.st_mode))  row.file_type = "symlink";
            else if (S_ISCHR(st.st_mode))  row.file_type = "char_device";
            else if (S_ISBLK(st.st_mode))  row.file_type = "block_device";
            else if (S_ISFIFO(st.st_mode)) row.file_type = "fifo";
            else if (S_ISSOCK(st.st_mode)) row.file_type = "socket";
            else                           row.file_type = "other";

            row.is_executable = (st.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH)) != 0;
        } else {
            SPDLOG_DEBUG("InventoryScanner: stat failed for '{}'", path_str);
            if (!row.is_directory) {
                const auto sz = entry.file_size(ec);
                row.size = ec ? 0 : static_cast<std::uint64_t>(sz);
                ec.clear();
            }
            const auto ftime = entry.last_write_time(ec);
            if (!ec) {
                row.mtime_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                    ftime.time_since_epoch()).count();
            }
            ec.clear();
            row.is_executable = false;
        }
#else
        if (!row.is_directory) {
            const auto sz = entry.file_size(ec);
            row.size = ec ? 0 : static_cast<std::uint64_t>(sz);
            ec.clear();
        }
        const auto ftime = entry.last_write_time(ec);
        if (!ec) {
            row.mtime_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                ftime.time_since_epoch()).count();
        }
        ec.clear();

        static const std::vector<std::string> exec_exts = {
            "exe", "bat", "cmd", "com", "ps1", "msi",
        };
        row.is_executable =
            std::find(exec_exts.begin(), exec_exts.end(), row.extension)
                != exec_exts.end();
#endif

        row.category = categorize(row.extension, row.is_executable);

        // ------------------------------------------------------------
        // Decide whether to hash and queue this file for scanning.
        // ------------------------------------------------------------
        const bool is_regular    = !row.is_directory && !row.is_symlink;
        const bool category_ok   = is_scannable_category(row.category,
                                                         inv_cfg.scannable_categories);
        const bool extension_ok  = is_text_extension(row.extension,
                                                     scan_cfg.text_extensions);
        const bool size_ok       = row.size <= inv_cfg.max_file_size_bytes;

        if (!is_regular) {
            row.scan_status = "not_applicable";
        } else if (!category_ok) {
            row.scan_status = "not_applicable";
        } else if (!extension_ok) {
            row.scan_status = "not_applicable";
        } else if (!size_ok) {
            row.scan_status = "skipped_too_large";
            SPDLOG_WARN("InventoryScanner: skipping '{}' "
                        "({} bytes > {} byte limit)",
                        path_str, row.size, inv_cfg.max_file_size_bytes);
        } else if (inv_cfg.compute_hashes) {
            const auto hash_result = util::compute_sha256(path_str);
            if (hash_result.ok()) {
                row.content_hash = hash_result.hex_digest;
                row.scan_status  = "pending";
            } else {
                row.scan_status = "scan_error";
                row.scan_error  = "hash: " + hash_result.error;
                SPDLOG_WARN("InventoryScanner: hash failed for '{}': {}",
                            path_str, hash_result.error);
            }
        } else {
            row.scan_status = "pending";
        }

        if (ctx_.file_store.upsert_file(row, walk_id)) {
            ++result.files_seen;
            ++since_commit;

            if (since_commit >= kBatchSize) {
                // B3 fix — check both the commit and the re-arm. Before
                // this change, a failed commit silently switched SQLite
                // to autocommit mode and the next batch of rows landed
                // without the batch transaction, leaving a partially
                // committed table with no error in the log.
                if (!ctx_.file_store.commit()) {
                    SPDLOG_ERROR("InventoryScanner: mid-scan commit failed "
                                 "for '{}'; aborting scan", root);
                    result.commit_failed = true;
                    return;
                }
                if (!ctx_.file_store.begin_transaction()) {
                    SPDLOG_ERROR("InventoryScanner: mid-scan begin_transaction "
                                 "failed for '{}'; aborting scan", root);
                    result.commit_failed = true;
                    return;
                }
                since_commit = 0;
            }
        } else {
            // B3 fix — count the failure. Log at WARN so a pattern of
            // failures (disk full, lock contention, corrupt row) shows
            // up in the log without spamming one line per file.
            ++result.files_failed;
            if (result.files_failed == 1) {
                SPDLOG_WARN("InventoryScanner: first upsert failure "
                            "(path='{}'); subsequent failures counted silently",
                            path_str);
            }
        }
    }
}

// scan_all runs one full inventory pass.
//
// B3 fix — transaction error handling:
//
//   Before this change, begin_transaction / commit / begin_transaction
//   return values were discarded. A failed commit silently switched
//   SQLite to autocommit mode: subsequent inserts bypassed the batch
//   transaction, the scan "completed successfully" with a partially
//   committed table, and prune_stale then deleted the "stale" rows
//   that had actually just failed to insert — corrupting the DB
//   without a single log line.
//
//   After: every transaction op is checked. A failure sets
//   commit_failed, rolls back what we can, skips prune_stale (the DB
//   state is unknown), logs at ERROR, and still writes a finish_walk
//   row so operators see the failed pass.
InventoryResult InventoryScanner::scan_all(const std::atomic<bool>& stop_flag) {
    InventoryResult r;

    const auto start       = std::chrono::steady_clock::now();
    const auto start_epoch = std::chrono::system_clock::now();
    const std::int64_t started_at_epoch =
        std::chrono::duration_cast<std::chrono::seconds>(
            start_epoch.time_since_epoch()).count();

    if (!ctx_.file_store.initialize_schema()) {
        SPDLOG_ERROR("InventoryScanner: file store not initialized");
        return r;
    }

    const auto& roots = ctx_.config.modules.inventory.watch_paths;
    if (roots.empty()) {
        SPDLOG_WARN("InventoryScanner: no watch paths configured");
        return r;
    }

    // Begin a new walk. If begin_walk fails (walks table missing, disk
    // full during insert), fall back to walk_id=1 with pruning disabled
    // — the scan still runs and files are still upserted; stale rows
    // from deleted files survive this pass and will be caught by the
    // next successful scan.
    std::uint64_t walk_id = ctx_.file_store.begin_walk(started_at_epoch);
    bool prune_enabled    = true;
    if (walk_id == 0) {
        SPDLOG_WARN("InventoryScanner: begin_walk failed — stale-row pruning "
                    "disabled for this pass");
        walk_id       = 1;
        prune_enabled = false;
    }
    r.walk_id = walk_id;

    SPDLOG_INFO("InventoryScanner: starting scan over {} root(s) (walk_id={})",
                roots.size(), walk_id);

    // B3 fix — check the initial begin_transaction. If this fails,
    // every upsert below would go into autocommit mode instead of a
    // single transactional batch.
    if (!ctx_.file_store.begin_transaction()) {
        SPDLOG_ERROR("InventoryScanner: initial begin_transaction failed; "
                     "scan aborted (walk_id={})", walk_id);
        r.commit_failed = true;

        const std::int64_t finished_at_epoch =
            std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
        r.duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start).count();
        if (prune_enabled) {
            ctx_.file_store.finish_walk(walk_id,
                                        started_at_epoch,
                                        finished_at_epoch,
                                        0, 0, 0, 0, r.duration_ms);
        }
        return r;
    }

    for (const auto& root : roots) {
        if (stop_flag.load(std::memory_order_relaxed)) {
            SPDLOG_INFO("InventoryScanner: stop requested before '{}', aborting",
                        root);
            r.was_cancelled = true;
            break;
        }
        scan_one_root(root, walk_id, stop_flag, r);
        if (r.was_cancelled || r.commit_failed) break;
    }

    r.duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start).count();

    // If a mid-scan transaction op failed, the DB is in an unknown
    // state: some rows landed, some didn't. Roll back whatever we can
    // (best effort), skip pruning (deleting "stale" rows would delete
    // the rows that failed to insert), log at ERROR, and record the
    // failed pass in the walks table so operators can see it.
    if (r.commit_failed) {
        if (!ctx_.file_store.rollback()) {
            SPDLOG_ERROR("InventoryScanner: rollback failed after commit error "
                         "(walk_id={})", walk_id);
        }

        const std::int64_t finished_at_epoch =
            std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
        if (prune_enabled) {
            ctx_.file_store.finish_walk(walk_id,
                                        started_at_epoch,
                                        finished_at_epoch,
                                        r.files_seen, 0, 0, 0, r.duration_ms);
        }

        SPDLOG_ERROR("InventoryScanner: scan FAILED (walk_id={}, seen={}, "
                     "failed={}, ms={}) — partial commit, prune skipped",
                     walk_id, r.files_seen, r.files_failed, r.duration_ms);
        return r;
    }

    // B3 fix — check the final commit.
    if (!ctx_.file_store.commit()) {
        SPDLOG_ERROR("InventoryScanner: final commit failed; scan incomplete "
                     "(walk_id={})", walk_id);
        ctx_.file_store.rollback();   // best effort
        r.commit_failed = true;

        const std::int64_t finished_at_epoch =
            std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
        if (prune_enabled) {
            ctx_.file_store.finish_walk(walk_id,
                                        started_at_epoch,
                                        finished_at_epoch,
                                        r.files_seen, 0, 0, 0, r.duration_ms);
        }
        return r;
    }

    // Prune rows left over from earlier walks, but only when the scan
    // completed. Then record the walk in the walks table so operators
    // can see scan history.
    std::optional<std::size_t> pruned;
    if (prune_enabled) {
        if (!r.was_cancelled) {
            pruned = ctx_.file_store.prune_stale(walk_id);
            if (!pruned) {
                SPDLOG_WARN("InventoryScanner: prune_stale failed for walk_id={}",
                            walk_id);
            }
        }

        const std::int64_t finished_at_epoch =
            std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();

        const std::size_t files_deleted = pruned.value_or(0);
        if (!ctx_.file_store.finish_walk(walk_id,
                                         started_at_epoch,
                                         finished_at_epoch,
                                         r.files_seen,
                                         0,               // files_new (not tracked yet)
                                         0,               // files_changed (not tracked yet)
                                         files_deleted,
                                         r.duration_ms)) {
            SPDLOG_WARN("InventoryScanner: finish_walk failed for walk_id={}",
                        walk_id);
        }
    }

    const std::size_t deleted = pruned.value_or(0);

    if (r.was_cancelled) {
        if (r.files_failed > 0) {
            SPDLOG_INFO("InventoryScanner: cancelled after {} files "
                        "({} failed, {} ms, walk_id={})",
                        r.files_seen, r.files_failed, r.duration_ms, walk_id);
        } else {
            SPDLOG_INFO("InventoryScanner: cancelled after {} files "
                        "({} ms, walk_id={})",
                        r.files_seen, r.duration_ms, walk_id);
        }
    } else if (r.files_failed > 0 || deleted > 0) {
        SPDLOG_INFO("InventoryScanner: finished ({} files, {} failed, "
                    "{} pruned, {} ms, walk_id={})",
                    r.files_seen, r.files_failed, deleted,
                    r.duration_ms, walk_id);
    } else {
        SPDLOG_INFO("InventoryScanner: finished ({} files, {} ms, walk_id={})",
                    r.files_seen, r.duration_ms, walk_id);
    }

    return r;
}

} // namespace axiom::core