// src/platform/windows/paths_windows.cpp

#include "platform/paths.h"

#include <windows.h>
#include <shlobj.h>
#include <objbase.h>   // CoTaskMemFree

#include <filesystem>
#include <string>

namespace axiom::core::paths {

std::string executable_path() {
    char buf[MAX_PATH] = {0};
    DWORD n = GetModuleFileNameA(nullptr, buf, MAX_PATH);
    if (n == 0 || n == MAX_PATH) {
        return {};
    }
    return std::string(buf, n);
}

std::string executable_dir() {
    const std::string p = executable_path();
    if (p.empty()) {
        return {};
    }
    return std::filesystem::path(p).parent_path().string();
}

namespace {

// Convert a wide (UTF-16) string to UTF-8.
// Returns an empty string on failure or if input is null.
std::string wide_to_utf8(const wchar_t* wstr) {
    if (wstr == nullptr) return {};

    // -1 means "the string is null-terminated; find the length yourself"
    const int len = WideCharToMultiByte(
        CP_UTF8, 0, wstr, -1, nullptr, 0, nullptr, nullptr);
    if (len <= 0) return {};

    // len includes the null terminator. Reserve len-1 chars.
    std::string result(static_cast<size_t>(len - 1), '\0');

    WideCharToMultiByte(
        CP_UTF8, 0, wstr, -1, result.data(), len, nullptr, nullptr);

    return result;
}

// Returns C:\ProgramData.
// Uses the modern SHGetKnownFolderPath API (non-deprecated).
std::string program_data() {
    PWSTR path = nullptr;
    const HRESULT hr = SHGetKnownFolderPath(
        FOLDERID_ProgramData, 0, nullptr, &path);

    if (FAILED(hr) || path == nullptr) {
        return "C:\\ProgramData";
    }

    std::string result = wide_to_utf8(path);
    CoTaskMemFree(path);

    if (result.empty()) {
        return "C:\\ProgramData";
    }
    return result;
}

} // namespace

std::string default_config_path() {
    return program_data() + "\\AxiomDLP\\agent.json";
}

std::string default_log_dir() {
    return program_data() + "\\AxiomDLP\\logs";
}

std::string default_data_dir() {
    return program_data() + "\\AxiomDLP";
}

} // namespace axiom::core::paths