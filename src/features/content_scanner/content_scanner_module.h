// src/features/content_scanner/content_scanner_module.h
//
// Phase 9: content scanner now talks to the Core over MQTT via
// IRequestClient::request("axiomdlp/agents/{id}/scan/batch", ...).
// See docs/mqtt_protocol.md §9.3 for the wire schema.
//
// B2 fix: per-chunk verdicts are cached across cycles, keyed by the
// file's content hash.
//
// B6 fix: FileProgress now carries the FileRow id so commit_completed_files
// can update by id (SQLite ROWID) instead of by path. Path is not unique
// when a file is deleted and recreated at the same path.

#pragma once

#include "modules/module_base.h"
#include "net/i_request_client.h"

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <thread>
#include <vector>

#include <nlohmann/json.hpp>

namespace axiom::core {

struct FileRow;

class ContentScannerModule final : public ModuleBase {
public:
    ContentScannerModule();
    ~ContentScannerModule() override;

    std::string name() const override { return "content_scanner"; }

    bool initialize(ModuleContext& context) override;
    bool start() override;
    void stop() override;
    void shutdown() override;
    HealthStatus health() const override;

private:
    struct Chunk {
        std::string       file_hash;
        std::string       path;
        std::uint32_t     chunk_index;
        std::uint32_t     chunk_count;
        std::vector<char> content;
    };

    struct FileProgress {
        // B6 fix: SQLite ROWID of the files row this file came from.
        // Used to route the final commit to the exact row, even if
        // another row shares the same path.
        std::int64_t  id              = 0;
        std::string   path;
        std::uint32_t chunks_expected = 0;
        std::uint32_t chunks_returned = 0;
        bool          sensitive       = false;
        std::string   sensitivity;
    };

    // B2 fix — one chunk's verdict as remembered from a previous cycle.
    struct CachedChunkVerdict {
        bool        sensitive   = false;
        std::string entity_type;
    };

    struct CachedChunkResults {
        std::map<std::uint32_t, CachedChunkVerdict> chunks;
        std::chrono::steady_clock::time_point       last_activity;
        std::uint32_t                               total_chunks_expected = 0;
    };

    void sync_loop();
    void process_one_cycle();
    void sweep_chunk_cache();

    bool prepare_file(const FileRow& row,
                      std::vector<Chunk>& out_chunks,
                      std::map<std::string, FileProgress>& progress);

    void send_chunks_in_batches(std::vector<Chunk>& chunks,
                                std::map<std::string, FileProgress>& progress);

    void merge_batch_response(const nlohmann::json& parsed,
                              std::map<std::string, FileProgress>& progress);

    void commit_completed_files(std::map<std::string, FileProgress>& progress);

    bool        enabled_             = true;
    IRequestClient* req_             = nullptr;
    int         agent_id_            = 0;

    int         batch_size_          = 50;
    std::size_t batch_max_bytes_     = 50 * 1024 * 1024;
    int         scan_interval_sec_   = 30;
    std::size_t chunk_size_          = 1024 * 1024;
    std::size_t chunk_overlap_       = 256;
    std::size_t max_file_bytes_      = 100 * 1024 * 1024;
    int         max_retries_         = 3;
    int         request_timeout_sec_ = 300;

    std::vector<std::string> text_extensions_;

    std::thread       sync_thread_;
    std::atomic<bool> stop_requested_{false};
    std::atomic<bool> running_{false};

    std::map<std::string, CachedChunkResults> chunk_cache_;

    std::size_t total_scanned_   = 0;
    std::size_t total_sensitive_ = 0;
    std::size_t total_skipped_   = 0;
};

} // namespace axiom::core