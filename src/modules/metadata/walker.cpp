// src/modules/metadata/walker.cpp

#include "modules/metadata/walker.h"
#include "storage/metadata_store.h"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <filesystem>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

#ifndef _WIN32
#  include <sys/stat.h>
#endif

namespace axiom::core {

namespace {

constexpr std::size_t kBatchSize = 5000;

// ----------------------------------------------------------------
// Glob / exclusion helpers (unchanged from the previous walker)
// ----------------------------------------------------------------

bool glob_match(const std::string& pattern, const std::string& text) {
    std::size_t pi = 0, ti = 0;
    std::size_t star_p = std::string::npos, star_t = 0;

    while (ti < text.size()) {
        if (pi < pattern.size() &&
            (pattern[pi] == '?' || pattern[pi] == text[ti])) {
            ++pi; ++ti;
        } else if (pi < pattern.size() && pattern[pi] == '*') {
            star_p = pi++;
            star_t = ti;
        } else if (star_p != std::string::npos) {
            pi = star_p + 1;
            ti = ++star_t;
        } else {
            return false;
        }
    }
    while (pi < pattern.size() && pattern[pi] == '*') ++pi;
    return pi == pattern.size();
}

bool path_matches_pattern(const std::string& pattern, const std::string& path) {
    if (pattern.empty()) return false;
    if (glob_match(pattern, path)) return true;

    if (pattern.back() != '*' && pattern.back() != '?') {
        const std::string prefix = pattern + "/";
        if (path.size() >= prefix.size() &&
            path.compare(0, prefix.size(), prefix) == 0) {
            return true;
        }
    }
    return false;
}

// ----------------------------------------------------------------
// Path decomposition helpers
// ----------------------------------------------------------------

std::string to_lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

// Lowercase extension without the leading dot. Empty for no extension.
std::string extract_extension(const std::filesystem::path& p) {
    const auto ext = p.extension().string();
    if (ext.empty() || ext[0] != '.') return {};
    return to_lower(ext.substr(1));
}

// Filename only.
std::string extract_basename(const std::filesystem::path& p) {
    return p.filename().string();
}

// Parent directory as an absolute path string.
std::string extract_dirname(const std::filesystem::path& p) {
    return p.parent_path().string();
}

// ----------------------------------------------------------------
// Category classification by extension. Cheap, no file read.
// ----------------------------------------------------------------
std::string categorize(const std::string& ext, bool is_executable) {
    static const std::unordered_map<std::string, std::string> table = {
        // images
        {"jpg","image"},{"jpeg","image"},{"png","image"},{"gif","image"},
        {"bmp","image"},{"svg","image"},{"webp","image"},{"ico","image"},
        {"tiff","image"},{"tif","image"},{"heic","image"},{"heif","image"},
        // video
        {"mp4","video"},{"mkv","video"},{"avi","video"},{"mov","video"},
        {"wmv","video"},{"flv","video"},{"webm","video"},{"m4v","video"},
        {"mpg","video"},{"mpeg","video"},
        // audio
        {"mp3","audio"},{"wav","audio"},{"flac","audio"},{"ogg","audio"},
        {"m4a","audio"},{"aac","audio"},{"wma","audio"},{"opus","audio"},
        // documents
        {"pdf","document"},{"doc","document"},{"docx","document"},
        {"xls","document"},{"xlsx","document"},{"ppt","document"},
        {"pptx","document"},{"odt","document"},{"ods","document"},
        {"odp","document"},{"txt","document"},{"md","document"},
        {"rtf","document"},{"tex","document"},
        // archives
        {"zip","archive"},{"tar","archive"},{"gz","archive"},
        {"bz2","archive"},{"xz","archive"},{"7z","archive"},
        {"rar","archive"},{"zst","archive"},{"tgz","archive"},
        // code
        {"c","code"},{"cpp","code"},{"cc","code"},{"cxx","code"},
        {"h","code"},{"hpp","code"},{"hxx","code"},{"py","code"},
        {"js","code"},{"ts","code"},{"jsx","code"},{"tsx","code"},
        {"java","code"},{"go","code"},{"rs","code"},{"rb","code"},
        {"php","code"},{"sh","code"},{"bash","code"},{"zsh","code"},
        {"ps1","code"},{"html","code"},{"css","code"},{"scss","code"},
        {"json","code"},{"xml","code"},{"yaml","code"},{"yml","code"},
        {"toml","code"},{"lua","code"},{"pl","code"},{"swift","code"},
        {"kt","code"},{"scala","code"},{"cs","code"},{"sql","code"},
        // data
        {"csv","data"},{"tsv","data"},{"db","data"},
        {"sqlite","data"},{"sqlite3","data"},{"parquet","data"},
        {"arrow","data"},{"feather","data"},
        // config
        {"conf","config"},{"cfg","config"},{"ini","config"},
        {"env","config"},{"properties","config"},
        // executables / libraries
        {"exe","executable"},{"bin","executable"},{"so","executable"},
        {"dll","executable"},{"dylib","executable"},{"app","executable"},
        {"deb","package"},{"rpm","package"},{"msi","package"},
        {"apk","package"},{"pkg","package"},{"dmg","package"},
    };

    auto it = table.find(ext);
    if (it != table.end()) return it->second;

    if (is_executable) return "executable";
    return "other";
}

// ----------------------------------------------------------------
// Surrogate (dev, inode) identity from the path hash.
// ----------------------------------------------------------------
std::uint64_t path_identity(const std::string& path) {
    std::hash<std::string> hasher;
    return static_cast<std::uint64_t>(hasher(path));
}

} // namespace

// ----------------------------------------------------------------
// Walker
// ----------------------------------------------------------------

bool Walker::is_excluded(const std::string& path) const {
    for (const auto& pattern : ctx_.config.modules.metadata.exclude_paths) {
        if (path_matches_pattern(pattern, path)) return true;
    }
    return false;
}

void Walker::walk_one_root(const std::string& root,
                           std::uint64_t walk_id,
                           WalkResult& result) {
    namespace fs = std::filesystem;
    std::error_code ec;

    if (!fs::exists(root, ec) || !fs::is_directory(root, ec)) {
        SPDLOG_WARN("Walker: skipping '{}' (not a directory)", root);
        return;
    }

    auto options = fs::directory_options::skip_permission_denied;
    std::size_t since_commit = 0;

    for (auto it = fs::recursive_directory_iterator(root, options, ec);
         it != fs::recursive_directory_iterator();
         it.increment(ec)) {

        if (ec) { ec.clear(); continue; }

        const auto& entry   = *it;
        const auto path_str = entry.path().string();

        if (is_excluded(path_str)) {
            if (entry.is_directory(ec)) it.disable_recursion_pending();
            continue;
        }

        if (entry.is_symlink(ec)) continue;

        // ------------------------------------------------------------
        // Build the row
        // ------------------------------------------------------------
        FileRow row;

        row.path      = path_str;
        row.basename  = extract_basename(entry.path());
        row.dirname   = extract_dirname(entry.path());
        row.extension = extract_extension(entry.path());

        // Surrogate identity.
        row.dev   = 0;
        row.inode = path_identity(path_str);

        // Directory flag.
        row.is_directory = entry.is_directory(ec);

        // File type string. Filled in more precisely below on Linux.
        row.file_type = row.is_directory ? "dir" : "file";

        // Hidden if the basename starts with '.' (and isn't "." or "..").
        row.is_hidden = (!row.basename.empty() &&
                         row.basename[0] == '.' &&
                         row.basename != "." &&
                         row.basename != "..");

#ifndef _WIN32
        // ------------------------------------------------------------
        // Linux: full POSIX stat for the richest metadata.
        // ------------------------------------------------------------
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
            // stat failed: fall back to std::filesystem for at least the
            // size and mtime. mode/uid/gid stay 0.
            SPDLOG_DEBUG("Walker: stat failed for '{}'", path_str);
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
        // ------------------------------------------------------------
        // Windows: std::filesystem only. POSIX stat fields stay 0.
        // ------------------------------------------------------------
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

        // Windows: executables are identified by extension.
        static const std::vector<std::string> exec_exts = {
            "exe", "bat", "cmd", "com", "ps1", "msi",
        };
        row.is_executable =
            std::find(exec_exts.begin(), exec_exts.end(), row.extension)
                != exec_exts.end();
#endif

        // Category from extension + executability flag.
        row.category = categorize(row.extension, row.is_executable);

        // Sensitivity is not populated in this milestone. The Scanner
        // module will fill these fields when it lands.
        row.is_sensitive = false;
        row.sensitivity  = "";
        row.scan_status  = "pending";

        // ------------------------------------------------------------
        // Insert
        // ------------------------------------------------------------
        if (ctx_.metadata_store.upsert_file(row, walk_id)) {
            ++result.files_seen;
            ++since_commit;

            if (since_commit >= kBatchSize) {
                ctx_.metadata_store.commit();
                ctx_.metadata_store.begin_transaction();
                since_commit = 0;
            }
        }
    }
}

WalkResult Walker::walk_all() {
    WalkResult r;

    const auto start = std::chrono::steady_clock::now();

    if (!ctx_.metadata_store.initialize_schema()) {
        SPDLOG_ERROR("Walker: metadata store not initialized");
        return r;
    }

    const auto& roots = ctx_.config.modules.metadata.watch_paths;
    if (roots.empty()) {
        SPDLOG_WARN("Walker: no watch paths configured");
        return r;
    }

    r.walk_id = 1;

    SPDLOG_INFO("Walker: starting walk over {} root(s)", roots.size());

    ctx_.metadata_store.begin_transaction();

    for (const auto& root : roots) {
        walk_one_root(root, r.walk_id, r);
    }

    ctx_.metadata_store.commit();

    r.duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start).count();

    SPDLOG_INFO("Walker: finished ({} files, {} ms)",
                r.files_seen, r.duration_ms);

    return r;
}

} // namespace axiom::core