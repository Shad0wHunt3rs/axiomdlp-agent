// src/storage/file_repository.h
//
// Domain-specific wrapper for the files table. Knows about FileRow
// and the queries the rest of the agent needs.

#pragma once

#include "storage/sqlite_database.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace axiom::core {

// One row in the files table.
//
// B6 fix: id is now populated by every read path. update_scan_result_by_id
// and the mark_* / record_scan_error_by_id helpers match on id (the
// unique SQLite ROWID) instead of path. The previous implementation
// matched on path, which is not unique — deleting a file and recreating
// it at the same path creates two rows (different inodes), and path-
// based updates hit both. The id is also necessary to distinguish the
// row the scanner just read from any other row that happens to share
// the same path.
struct FileRow {
    // Identity
    std::int64_t  id             = 0;    // SQLite ROWID; 0 = not-yet-inserted
    std::uint64_t dev            = 0;
    std::uint64_t inode          = 0;

    // Path decomposition
    std::string   path;
    std::string   basename;
    std::string   dirname;
    std::string   extension;

    // Timestamps and size
    std::uint64_t size           = 0;
    std::int64_t  mtime_ns       = 0;
    std::int64_t  ctime_ns       = 0;
    std::int64_t  atime_ns       = 0;

    // Permissions and identity
    std::uint32_t mode           = 0;
    std::uint32_t uid            = 0;
    std::uint32_t gid            = 0;
    std::uint32_t nlink          = 0;

    // Classification
    std::string   file_type      = "file";
    std::string   category       = "other";

    // Flags
    bool          is_directory   = false;
    bool          is_symlink     = false;
    bool          is_hidden      = false;
    bool          is_executable  = false;

    // Content scanning result
    bool          is_sensitive   = false;
    std::string   sensitivity;
    std::string   scan_status    = "pending";

    // Content scanning metadata
    std::string   content_hash;
    std::int64_t  last_scanned_at = 0;
    int           scan_attempts   = 0;
    std::string   scan_error;
};

// Query parameters for FileStore::query().
struct FileQuery {
    std::string path_prefix;
    std::string category;
    std::string extension;
    std::string dirname;

    bool        only_files       = true;
    bool        only_sensitive   = false;

    std::size_t limit            = 1000;
    std::size_t offset           = 0;

    std::string order_by         = "path";
    bool        descending       = false;
};

// A prior scan verdict for a given content hash.
struct ScanVerdict {
    bool        sensitive   = false;
    std::string sensitivity;
};

class FileStore {
public:
    explicit FileStore(Database& db) : db_(db) {}

    // Create or verify the schema. Called once after migrations.
    bool initialize_schema();

    // Insert or update one file. Keyed by (dev, inode).
    //
    // B7 fix: when content_hash changes, every scan-state field is
    // reset to the value from the fresh row (which defaults to "not
    // scanned"). Before this fix, only scan_status was reset; a file
    // that was is_sensitive=1 (e.g. "contains EMAIL") and whose
    // content was overwritten with safe text would keep is_sensitive=1
    // and sensitivity="EMAIL" until the next scan cycle finished.
    // Enforcement code reading is_sensitive during that window saw
    // stale data.
    bool upsert_file(const FileRow& row, std::uint64_t walk_id);

    // Remove files whose walk_id is older than `current_walk_id`.
    //
    // Returns the number of rows deleted on success, or std::nullopt on
    // SQL error. Called by InventoryScanner::scan_all after a
    // successful non-cancelled scan.
    std::optional<std::size_t> prune_stale(std::uint64_t current_walk_id);

    // Walk bookkeeping.
    std::uint64_t begin_walk(std::int64_t started_at);
    bool finish_walk(std::uint64_t walk_id,
                     std::int64_t started_at,
                     std::int64_t finished_at,
                     std::size_t files_seen,
                     std::size_t files_new,
                     std::size_t files_changed,
                     std::size_t files_deleted,
                     std::int64_t duration_ms);

    // General row query.
    std::vector<FileRow> query(const FileQuery& q) const;

    // ----------------------------------------------------------------
    // Scanner support
    // ----------------------------------------------------------------

    // Return up to `limit` files that are candidates for scanning.
    // Excludes files whose scan_attempts has reached max_attempts.
    std::vector<FileRow> fetch_pending_for_scan(std::size_t limit,
                                                int max_attempts) const;

    // Look up a previously recorded verdict for the given content hash.
    // Used to skip HTTP calls when a copy or move of an already-scanned
    // file appears.
    std::optional<ScanVerdict> find_scan_verdict_by_hash(
        const std::string& content_hash) const;

    // B6 fix — all scan-state writers below match on the unique ROWID.
    // The previous * _by_path variants matched on path, which is not
    // unique (delete+recreate at the same path leaves two rows).

    // Record a successful scan result for the row with the given id.
    bool update_scan_result_by_id(std::int64_t id,
                                  const std::string& content_hash,
                                  bool sensitive,
                                  const std::string& sensitivity);

    // Record a failed scan attempt (bumps scan_attempts).
    bool record_scan_error_by_id(std::int64_t id,
                                 const std::string& error);

    // Mark a file that no longer exists on disk.
    bool mark_file_missing(std::int64_t id);

    // Mark a file whose extension is outside the scanner's scope.
    bool mark_not_applicable(std::int64_t id);

    // Mark a file that exceeds the scanner's max_file_bytes limit.
    bool mark_skipped_too_large(std::int64_t id);

    // ----------------------------------------------------------------
    // Transport support
    // ----------------------------------------------------------------
    struct SensitiveRow {
        std::uint64_t id           = 0;
        std::string   path;
        std::uint64_t size         = 0;
        std::string   sensitivity;
        std::int64_t  last_seen    = 0;
    };

    std::uint64_t max_file_id() const;
    std::vector<SensitiveRow> query_sensitive_after(std::uint64_t after_id,
                                                    std::size_t limit) const;
    bool mark_random_files_sensitive(std::size_t count);

    // ----------------------------------------------------------------
    // Aggregations
    // ----------------------------------------------------------------
    struct Stats {
        std::size_t   total_files       = 0;
        std::size_t   total_directories = 0;
        std::uint64_t total_bytes       = 0;
        std::size_t   sensitive_files   = 0;
    };
    Stats stats() const;

    struct CategoryCount {
        std::string category;
        std::size_t count = 0;
        std::uint64_t bytes = 0;
    };
    std::vector<CategoryCount> category_breakdown() const;

    struct DirCount {
        std::string dirname;
        std::size_t count = 0;
        std::uint64_t bytes = 0;
    };
    std::vector<DirCount> top_directories(std::size_t limit = 20) const;

    // Transaction control.
    bool begin_transaction();
    bool commit();
    bool rollback();

private:
    Database& db_;
};

} // namespace axiom::core