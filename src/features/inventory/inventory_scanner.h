// src/features/inventory/inventory_scanner.h
//
// InventoryScanner walks the configured watch paths, gathers file
// metadata, and writes each record to the shared FileStore. Streams
// records one at a time so memory usage stays constant regardless of
// file count.

#pragma once

#include "modules/module_context.h"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <string>

namespace axiom::core {

struct InventoryResult {
    std::uint64_t walk_id       = 0;
    std::size_t   files_seen    = 0;

    // B3 fix — count upserts that failed to land. Before this change,
    // upsert_file() return values were silently discarded. A files_seen
    // of 500 with files_failed of 500 told the operator nothing; they
    // saw "500 files seen" and assumed the DB had 500 rows when it had
    // zero. Now the finished log line shows the failure count when it
    // is non-zero, and the walk row's files_seen is the accurate
    // landed count.
    std::size_t   files_failed  = 0;

    std::size_t   files_changed = 0;
    std::size_t   files_deleted = 0;
    std::int64_t  duration_ms   = 0;
    bool          was_cancelled = false;

    // B3 fix — set when a transaction operation (begin, commit, or
    // mid-batch re-arm) failed. When true, scan_all skips prune_stale
    // (the DB state is uncertain, pruning could delete the wrong rows)
    // and logs at ERROR. finish_walk is still called so operators see
    // the failed pass in the walks table.
    bool          commit_failed = false;
};

class InventoryScanner {
public:
    explicit InventoryScanner(ModuleContext& ctx) : ctx_(ctx) {}

    // Run one full inventory pass. If `stop_flag` becomes true at any
    // point, the scan aborts gracefully and returns with
    // was_cancelled = true. Partial progress is committed.
    InventoryResult scan_all(const std::atomic<bool>& stop_flag);

private:
    ModuleContext& ctx_;

    bool is_excluded(const std::string& path) const;
    void scan_one_root(const std::string& root,
                       std::uint64_t walk_id,
                       const std::atomic<bool>& stop_flag,
                       InventoryResult& result);
};

} // namespace axiom::core