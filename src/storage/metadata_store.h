// src/storage/metadata_store.h
//
// Domain-specific wrapper for the files table. Knows about FileRow
// and the queries the rest of the agent needs.

#pragma once

#include "storage/database.h"

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
    std::string   extension;     // lowercase, no leading dot

    // Timestamps and size
    std::uint64_t size           = 0;
    std::int64_t  mtime_ns       = 0;   // modification time
    std::int64_t  ctime_ns       = 0;   // inode change time
    std::int64_t  atime_ns       = 0;   // access time

    // Permissions and identity
    std::uint32_t mode           = 0;   // full POSIX mode (Linux)
    std::uint32_t uid            = 0;
    std::uint32_t gid            = 0;
    std::uint32_t nlink          = 0;

    // Classification
    std::string   file_type      = "file";   // file/dir/symlink/socket/...
    std::string   category       = "other";  // image/video/document/code/...

    // Flags
    bool          is_directory   = false;
    bool          is_symlink     = false;
    bool          is_hidden      = false;
    bool          is_executable  = false;

    // Populated later by the Scanner module. Always 0 for now.
    bool          is_sensitive   = false;
    std::string   sensitivity;                     // empty for now
    std::string   scan_status    = "pending";
};

// Query parameters for MetadataStore::query().
struct FileQuery {
    std::string path_prefix;
    std::string category;
    std::string extension;
    std::string dirname;

    bool        only_files       = true;   // skip directories
    bool        only_sensitive   = false;  // ignored until the Scanner populates it

    std::size_t limit            = 1000;
    std::size_t offset           = 0;

    // Sort options:
    //   "path", "size", "mtime", "ctime", "category", "extension"
    std::string order_by         = "path";
    bool        descending       = false;
};

class MetadataStore {
public:
    explicit MetadataStore(Database& db) : db_(db) {}

    // Create or verify the schema. Called once after migrations.
    bool initialize_schema();

    // Insert or update one file. Keyed by (dev, inode).
    bool upsert_file(const FileRow& row, std::uint64_t walk_id);

    // Remove files whose walk_id is older than `current_walk_id`.
    bool prune_stale(std::uint64_t current_walk_id);

    // Walk bookkeeping
    std::uint64_t begin_walk(std::int64_t started_at);
    bool finish_walk(std::uint64_t walk_id,
                     std::int64_t started_at,
                     std::int64_t finished_at,
                     std::size_t files_seen,
                     std::size_t files_new,
                     std::size_t files_changed,
                     std::size_t files_deleted,
                     std::int64_t duration_ms);

    // Queries
    std::vector<FileRow> query(const FileQuery& q) const;

    // Aggregations for the visualization layer.
    struct Stats {
        std::size_t   total_files       = 0;
        std::size_t   total_directories = 0;
        std::uint64_t total_bytes       = 0;
        std::size_t   sensitive_files   = 0;
    };
    Stats stats() const;

    // Count per category. Used for the pie chart on the dashboard.
    struct CategoryCount {
        std::string category;
        std::size_t count = 0;
        std::uint64_t bytes = 0;
    };
    std::vector<CategoryCount> category_breakdown() const;

    // Count per top-level directory prefix (up to 3 segments deep).
    struct DirCount {
        std::string dirname;
        std::size_t count = 0;
        std::uint64_t bytes = 0;
    };
    std::vector<DirCount> top_directories(std::size_t limit = 20) const;

    // Transaction control (forwarded to Database).
    bool begin_transaction();
    bool commit();
    bool rollback();

private:
    Database& db_;
};

} // namespace axiom::core