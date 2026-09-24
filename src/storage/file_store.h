// src/storage/file_store.h
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

struct FileRow {
    std::uint64_t dev            = 0;
    std::uint64_t inode          = 0;

    std::string   path;
    std::string   basename;
    std::string   dirname;
    std::string   extension;

    std::uint64_t size           = 0;
    std::int64_t  mtime_ns       = 0;
    std::int64_t  ctime_ns       = 0;
    std::int64_t  atime_ns       = 0;

    std::uint32_t mode           = 0;
    std::uint32_t uid            = 0;
    std::uint32_t gid            = 0;
    std::uint32_t nlink          = 0;

    std::string   file_type      = "file";
    std::string   category       = "other";

    bool          is_directory   = false;
    bool          is_symlink     = false;
    bool          is_hidden      = false;
    bool          is_executable  = false;

    bool          is_sensitive   = false;
    std::string   sensitivity;
    std::string   scan_status    = "pending";
};

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