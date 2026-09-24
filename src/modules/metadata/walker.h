// src/modules/metadata/walker.h

#pragma once

#include "core/module_context.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace axiom::core {

struct WalkResult {
    std::uint64_t walk_id       = 0;
    std::size_t   files_seen    = 0;
    std::size_t   files_changed = 0;
    std::size_t   files_deleted = 0;
    std::int64_t  duration_ms   = 0;
};

class Walker {
public:
    explicit Walker(ModuleContext& ctx) : ctx_(ctx) {}

    WalkResult walk_all();

private:
    ModuleContext& ctx_;

    bool is_excluded(const std::string& path) const;
    void walk_one_root(const std::string& root, std::uint64_t walk_id, WalkResult& result);
};

} // namespace axiom::core