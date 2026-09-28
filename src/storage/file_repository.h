// src/storage/file_repository.h
//
// Domain-specific wrapper for the files table.

#pragma once

#include "storage/sqlite_database.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace axiom::core {

// One row in the files table.
struct FileRow {
    // Identity
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

    // Content scanning results
    bool          is_sensitive   = false;
    std::string   sensitivity;
    std::string   scan_status    = "pending";

    // Content scanning metadata (populated during inventory)
    std::string   content_hash;        // SHA-256 hex, empty if not hashed
    std::int64_t  last_scanned_at = 0; // unix seconds, 0 = never scanned
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

class FileStore {
public:
    explicit FileStore(Database& db) : db_(db) {}

    bool initialize_schema();

    bool upsert_file(const FileRow& row, std::uint64_t walk_id);
    bool prune_stale(std::uint64_t current_walk_id);

    std::uint64_t begin_walk(std::int64_t started_at);
    bool finish_walk(std::uint64_t walk_id,
                     std::int64_t started_at,
                     std::int64_t finished_at,
                     std::size_t files_seen,
                     std::size_t files_new,
                     std::size_t files_changed,
                     std::size_t files_deleted,
                     std::int64_t duration_ms);

    std::vector<FileRow> query(const FileQuery& q) const;

    // Transport support — compact rows for the shipping loop.
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
    // Scanner support
    // ----------------------------------------------------------------

    // Return up to `limit` files that are candidates for content scanning
    // (scan_status = 'pending' and content_hash is not empty).
    std::vector<FileRow> fetch_pending_for_scan(std::size_t limit) const;

    // Record the outcome of a scan for the file with this content_hash.
    bool update_scan_result(const std::string& content_hash,
                            bool sensitive,
                            const std::string& sensitivity);

    // Mark a scan as failed. Increments scan_attempts and stores the error.
    bool record_scan_error(const std::string& content_hash,
                           const std::string& error);

    // Aggregations.
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

    bool begin_transaction();
    bool commit();
    bool rollback();

private:
    Database& db_;
};

} // namespace axiom::core