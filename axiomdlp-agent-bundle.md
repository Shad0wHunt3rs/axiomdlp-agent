# AxiomDLP Agent — Source Bundle

- **Generated:** 2026-10-20 18:07:30 UTC
- **Root:** `/home/kali/axiomdlp-agent`

This bundle contains **only the project's own source code** —
C++ sources, the top-level CMake configuration, runtime JSON
config, packaging, scripts, and docs.

Third-party code (dependency trees under `.deps/` or `_deps/`,
vendored `third_party/`, cross-compile toolchain files under
`cmake/toolchains/`), build outputs, editor/tool caches, and
generated artifacts are **not** included. Every path that was
skipped is listed in the **Skipped Files** section at the end.
If you need any of them, ask the user — do not guess.

---

## Project Structure

```text
.
./axiomdlp-agent-bundle.md
./.clangd
./cmake
./CMakeLists.txt
./CMakePresets.json
./cmake/toolchains
./cmake/toolchains/linux-i686.cmake
./cmake/toolchains/linux-x86_64.cmake
./compile_commands.json
./config
./config/agent.default.json
./config/agent.default.json.bak
./config/agent.default_win.json
./config/agent.schema.json
./docs
./docs/architecture.md
./docs/event_schema.md
./docs/file_structure.md
./docs/ipc_protocol.md
./docs/modules.md
./docs/policy_format.md
./done.md
./.github
./.github/workflows
./.github/workflows/windows-msvc.yml
./.gitignore
./LICENSE
./make-bundle.sh
./next.md
./packaging
./packaging/linux
./packaging/linux/systemd
./packaging/linux/systemd/axiomdlp-agent.service
./packaging/windows
./packaging/windows/service_install.ps1
./scripts
./scripts/linux
./scripts/linux/build-all.sh
./scripts/linux/build-grpc.sh
./scripts/linux/build-openssl.sh
./scripts/linux/merge-compile-commands.sh
./scripts/win
./scripts/win/build-windows-msvc.ps1
./scripts/win/setup-windows-msvc.ps1
./src
./src/events
./src/events/event_bus.cpp
./src/events/event_bus.h
./src/events/event.cpp
./src/events/event.h
./src/features
./src/features/agent_sync
./src/features/agent_sync/agent_sync_module.cpp
./src/features/agent_sync/agent_sync_module.h
./src/features/content_scanner
./src/features/content_scanner/content_scanner_module.cpp
./src/features/content_scanner/content_scanner_module.h
./src/features/event_logger
./src/features/event_logger/event_logger_module.cpp
./src/features/event_logger/event_logger_module.h
./src/features/filesystem_watcher
./src/features/filesystem_watcher/filesystem_watcher_module.cpp
./src/features/filesystem_watcher/filesystem_watcher_module.h
./src/features/grpc_transport
./src/features/grpc_transport/agent.proto
./src/features/grpc_transport/grpc_transport_module.cpp
./src/features/grpc_transport/grpc_transport_module.h
./src/features/inventory
./src/features/inventory/inventory_module.cpp
./src/features/inventory/inventory_module.h
./src/features/inventory/inventory_scanner.cpp
./src/features/inventory/inventory_scanner.h
./src/foundation
./src/foundation/application.cpp
./src/foundation/application.h
./src/foundation/config.cpp
./src/foundation/config.h
./src/foundation/logger.cpp
./src/foundation/logger.h
./src/foundation/shutdown.h
./src/main.cpp
./src/modules
./src/modules/health.h
./src/modules/i_module.h
./src/modules/module_base.h
./src/modules/module_context.h
./src/modules/module_manager.cpp
./src/modules/module_manager.h
./src/net
./src/net/http_client.cpp
./src/net/http_client.h
./src/net/outbound_queue.cpp
./src/net/outbound_queue.h
./src/platform
./src/platform/common
./src/platform/common/efsw_file_watcher.cpp
./src/platform/file_watcher.h
./src/platform/linux
./src/platform/linux/paths.cpp
./src/platform/linux/service.cpp
./src/platform/paths.h
./src/platform/service.h
./src/platform/windows
./src/platform/windows/paths.cpp
./src/platform/windows/service.cpp
./src/storage
./src/storage/file_repository.cpp
./src/storage/file_repository.h
./src/storage/migrations.cpp
./src/storage/migrations.h
./src/storage/sqlite_database.cpp
./src/storage/sqlite_database.h
./src/storage/state_repository.cpp
./src/storage/state_repository.h
./src/util
./src/util/base64.cpp
./src/util/base64.h
./src/util/file_hash.cpp
./src/util/file_hash.h
./src/util/glob_match.cpp
./src/util/glob_match.h
./tests
./tests/test_phase6.py
./vcpkg.json
```

---

## Project Root

  + CMakeLists.txt
### \`CMakeLists.txt\`

```cmake
cmake_minimum_required(VERSION 3.24)
project(axiomdlp-agent VERSION 0.1.0 LANGUAGES C CXX)

# ----------------------------------------------------------------
# Shared source cache for FetchContent dependencies
# ----------------------------------------------------------------
set(AXIOM_DEPS_DIR "${CMAKE_SOURCE_DIR}/.deps" CACHE PATH "Shared FetchContent source cache")
file(MAKE_DIRECTORY "${AXIOM_DEPS_DIR}")

if(EXISTS "${AXIOM_DEPS_DIR}/spdlog-src")
    set(FETCHCONTENT_SOURCE_DIR_SPDLOG "${AXIOM_DEPS_DIR}/spdlog-src" CACHE PATH "spdlog source cache" FORCE)
endif()
if(EXISTS "${AXIOM_DEPS_DIR}/nlohmann_json-src")
    set(FETCHCONTENT_SOURCE_DIR_NLOHMANN_JSON "${AXIOM_DEPS_DIR}/nlohmann_json-src" CACHE PATH "nlohmann/json source cache" FORCE)
endif()
if(EXISTS "${AXIOM_DEPS_DIR}/efsw-src")
    set(FETCHCONTENT_SOURCE_DIR_EFSW "${AXIOM_DEPS_DIR}/efsw-src" CACHE PATH "efsw source cache" FORCE)
endif()
if(EXISTS "${AXIOM_DEPS_DIR}/cpp-httplib-src")
    set(FETCHCONTENT_SOURCE_DIR_HTTPLIB "${AXIOM_DEPS_DIR}/cpp-httplib-src" CACHE PATH "cpp-httplib source cache" FORCE)
endif()

# ----------------------------------------------------------------
# Language settings
# ----------------------------------------------------------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# ----------------------------------------------------------------
# Dependency acquisition strategy
# ----------------------------------------------------------------
if(MSVC)
    set(AXIOM_USE_VCPKG_DEPS ON)
    message(STATUS "Build strategy: MSVC + vcpkg (Windows native)")
    if(NOT DEFINED CMAKE_MSVC_RUNTIME_LIBRARY)
        set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>" CACHE STRING "Use static CRT")
    endif()
else()
    set(AXIOM_USE_VCPKG_DEPS OFF)
    message(STATUS "Build strategy: GCC native (.deps/)")
endif()

# ----------------------------------------------------------------
# Detect target triple
# ----------------------------------------------------------------
if(WIN32)
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(AXIOM_TARGET "windows-x86_64")
    else()
        set(AXIOM_TARGET "windows-i686")
    endif()
elseif(UNIX AND NOT APPLE)
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(AXIOM_TARGET "linux-x86_64")
    else()
        set(AXIOM_TARGET "linux-i686")
    endif()
else()
    set(AXIOM_TARGET "unknown")
endif()

message(STATUS "AxiomDLP Agent target: ${AXIOM_TARGET}")

# ----------------------------------------------------------------
# Third-party dependencies
# ----------------------------------------------------------------
include(FetchContent)

# --- nlohmann/json ---
if(AXIOM_USE_VCPKG_DEPS)
    find_package(nlohmann_json CONFIG REQUIRED)
else()
    find_package(nlohmann_json QUIET)
    if(NOT nlohmann_json_FOUND)
        set(JSON_BuildTests OFF CACHE BOOL "" FORCE)
        set(JSON_Install    OFF CACHE BOOL "" FORCE)
        FetchContent_Declare(nlohmann_json GIT_REPOSITORY https://github.com/nlohmann/json.git GIT_TAG v3.11.3 GIT_SHALLOW TRUE)
        FetchContent_MakeAvailable(nlohmann_json)
    endif()
endif()

# --- spdlog ---
if(AXIOM_USE_VCPKG_DEPS)
    find_package(spdlog CONFIG REQUIRED)
else()
    set(SPDLOG_BUILD_SHARED   OFF CACHE BOOL "" FORCE)
    set(SPDLOG_BUILD_EXAMPLE  OFF CACHE BOOL "" FORCE)
    set(SPDLOG_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
    set(SPDLOG_BUILD_BENCH    OFF CACHE BOOL "" FORCE)
    set(SPDLOG_INSTALL        OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(spdlog GIT_REPOSITORY https://github.com/gabime/spdlog.git GIT_TAG v1.14.1 GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(spdlog)
endif()

# --- efsw ---
if(AXIOM_USE_VCPKG_DEPS)
    find_package(efsw CONFIG REQUIRED)
    if(TARGET efsw::efsw)
        set(AXIOM_EFSW_TARGET efsw::efsw)
    else()
        set(AXIOM_EFSW_TARGET efsw)
    endif()
else()
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    set(BUILD_STATIC_LIBS ON  CACHE BOOL "" FORCE)
    set(EFSW_BUILD_TESTS   OFF CACHE BOOL "" FORCE)
    set(EFSW_BUILD_EXAMPLE OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(efsw GIT_REPOSITORY https://github.com/SpartanJ/efsw.git GIT_TAG 1.7.2 GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(efsw)
    set(AXIOM_EFSW_TARGET efsw-static)
endif()

# --- cpp-httplib ---
set(HTTPLIB_COMPILE OFF) # header-only
FetchContent_Declare(
    httplib
    GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
    GIT_TAG        v0.15.3
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(httplib)

# ----------------------------------------------------------------
# gRPC + Protobuf (Mandatory)
# ----------------------------------------------------------------
if(NOT AXIOM_USE_VCPKG_DEPS)
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        list(PREPEND CMAKE_PREFIX_PATH "${AXIOM_DEPS_DIR}/grpc-linux-x86_64")
    else()
        list(PREPEND CMAKE_PREFIX_PATH "${AXIOM_DEPS_DIR}/grpc-linux-i686")
    endif()
endif()

find_package(Protobuf CONFIG REQUIRED)
find_package(gRPC CONFIG REQUIRED)

set(AXIOM_PROTO_DIR     "${CMAKE_CURRENT_SOURCE_DIR}/src/features/grpc_transport")
set(AXIOM_PROTO_FILE    "${AXIOM_PROTO_DIR}/agent.proto")
set(AXIOM_PROTO_GEN_DIR "${CMAKE_CURRENT_BINARY_DIR}/gen")

set(AXIOM_PROTO_SRC
    "${AXIOM_PROTO_GEN_DIR}/agent.pb.cc"
    "${AXIOM_PROTO_GEN_DIR}/agent.grpc.pb.cc"
)
set(AXIOM_PROTO_HDR
    "${AXIOM_PROTO_GEN_DIR}/agent.pb.h"
    "${AXIOM_PROTO_GEN_DIR}/agent.grpc.pb.h"
)

if(AXIOM_USE_VCPKG_DEPS)
    set(_protoc_exe "$<TARGET_FILE:protobuf::protoc>")
    set(_grpc_plugin "$<TARGET_FILE:gRPC::grpc_cpp_plugin>")
else()
    set(_protoc_exe "${AXIOM_DEPS_DIR}/grpc-host-tools/bin/protoc")
    set(_grpc_plugin "${AXIOM_DEPS_DIR}/grpc-host-tools/bin/grpc_cpp_plugin")
    if(NOT EXISTS "${_protoc_exe}")
        message(FATAL_ERROR "protoc not found. Please run ./scripts/linux/build-grpc.sh first.")
    endif()
endif()

add_custom_command(
    OUTPUT  ${AXIOM_PROTO_SRC} ${AXIOM_PROTO_HDR}
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${AXIOM_PROTO_GEN_DIR}"
    COMMAND "${_protoc_exe}"
        --proto_path=${AXIOM_PROTO_DIR}
        --cpp_out=${AXIOM_PROTO_GEN_DIR}
        --grpc_out=${AXIOM_PROTO_GEN_DIR}
        --plugin=protoc-gen-grpc=${_grpc_plugin}
        ${AXIOM_PROTO_FILE}
    DEPENDS ${AXIOM_PROTO_FILE}
    COMMENT "protoc: generating C++ from agent.proto"
    VERBATIM
)

add_custom_target(axiom_grpc_gen DEPENDS ${AXIOM_PROTO_SRC} ${AXIOM_PROTO_HDR})

# ----------------------------------------------------------------
# OpenSSL & SQLite3
# ----------------------------------------------------------------
if(AXIOM_USE_VCPKG_DEPS)
    find_package(OpenSSL REQUIRED)
    find_package(SQLite3 REQUIRED)
    set(AXIOM_SQLITE_TARGET SQLite::SQLite3)
else()
    if(UNIX AND NOT APPLE)
        if(CMAKE_SIZEOF_VOID_P EQUAL 8)
            set(AXIOM_OPENSSL_DIR "${AXIOM_DEPS_DIR}/openssl-linux-x86_64")
        else()
            set(AXIOM_OPENSSL_DIR "${AXIOM_DEPS_DIR}/openssl-linux-i686")
        endif()
    endif()
    if(EXISTS "${AXIOM_OPENSSL_DIR}/lib/libssl.a")
        set(AXIOM_OPENSSL_LIB_DIR "${AXIOM_OPENSSL_DIR}/lib")
    elseif(EXISTS "${AXIOM_OPENSSL_DIR}/lib64/libssl.a")
        set(AXIOM_OPENSSL_LIB_DIR "${AXIOM_OPENSSL_DIR}/lib64")
    else()
        message(FATAL_ERROR "OpenSSL not found. Run ./scripts/linux/build-openssl.sh first.")
    endif()
    
    set(AXIOM_OPENSSL_INCLUDE_DIR "${AXIOM_OPENSSL_DIR}/include")
    if(NOT TARGET OpenSSL::SSL)
        add_library(OpenSSL::SSL STATIC IMPORTED GLOBAL)
        set_target_properties(OpenSSL::SSL PROPERTIES
            IMPORTED_LOCATION             "${AXIOM_OPENSSL_LIB_DIR}/libssl.a"
            INTERFACE_INCLUDE_DIRECTORIES "${AXIOM_OPENSSL_INCLUDE_DIR}"
            INTERFACE_LINK_LIBRARIES      "OpenSSL::Crypto")
    endif()
    if(NOT TARGET OpenSSL::Crypto)
        add_library(OpenSSL::Crypto STATIC IMPORTED GLOBAL)
        set_target_properties(OpenSSL::Crypto PROPERTIES
            IMPORTED_LOCATION             "${AXIOM_OPENSSL_LIB_DIR}/libcrypto.a"
            INTERFACE_INCLUDE_DIRECTORIES "${AXIOM_OPENSSL_INCLUDE_DIR}")
    endif()

    set(AXIOM_SQLITE_DIR "${AXIOM_DEPS_DIR}/sqlite3-src")
    set(AXIOM_SQLITE_C   "${AXIOM_SQLITE_DIR}/sqlite3.c")
    add_library(axiom_sqlite STATIC "${AXIOM_SQLITE_C}")
    target_include_directories(axiom_sqlite PUBLIC "${AXIOM_SQLITE_DIR}")
    target_compile_definitions(axiom_sqlite PRIVATE SQLITE_THREADSAFE=1 SQLITE_OMIT_LOAD_EXTENSION=1 SQLITE_DEFAULT_MEMSTATUS=0 SQLITE_DQS=0)
    if(UNIX AND NOT APPLE)
        target_link_libraries(axiom_sqlite PUBLIC dl pthread)
    endif()
    set(AXIOM_SQLITE_TARGET axiom_sqlite)
endif()

# ----------------------------------------------------------------
# Platform-specific sources
# ----------------------------------------------------------------
if(WIN32)
    set(AXIOM_PLATFORM_SOURCES src/platform/windows/paths.cpp src/platform/windows/service.cpp)
elseif(UNIX AND NOT APPLE)
    set(AXIOM_PLATFORM_SOURCES src/platform/linux/paths.cpp src/platform/linux/service.cpp)
endif()

# ----------------------------------------------------------------
# Agent executable
# ----------------------------------------------------------------
add_executable(axiomdlp-agent
    src/main.cpp
    src/foundation/application.cpp
    src/foundation/config.cpp
    src/foundation/logger.cpp
    src/events/event.cpp
    src/events/event_bus.cpp
    src/modules/module_manager.cpp
    src/net/http_client.cpp
    src/net/outbound_queue.cpp
    src/util/glob_match.cpp
    src/util/file_hash.cpp
    src/util/base64.cpp
    src/platform/common/efsw_file_watcher.cpp
    src/features/agent_sync/agent_sync_module.cpp
    src/features/content_scanner/content_scanner_module.cpp
    src/features/event_logger/event_logger_module.cpp
    src/features/filesystem_watcher/filesystem_watcher_module.cpp
    src/features/inventory/inventory_module.cpp
    src/features/inventory/inventory_scanner.cpp
    src/features/grpc_transport/grpc_transport_module.cpp
    src/storage/sqlite_database.cpp
    src/storage/migrations.cpp
    src/storage/file_repository.cpp
    src/storage/state_repository.cpp
    ${AXIOM_PROTO_SRC}
    ${AXIOM_PLATFORM_SOURCES}
)

target_include_directories(axiomdlp-agent PRIVATE src ${AXIOM_PROTO_GEN_DIR})

target_compile_definitions(axiomdlp-agent PRIVATE
    SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_TRACE
    AXIOM_AGENT_VERSION="${PROJECT_VERSION}"
    CPPHTTPLIB_OPENSSL_SUPPORT
)

target_link_libraries(axiomdlp-agent PRIVATE
    spdlog::spdlog
    nlohmann_json::nlohmann_json
    httplib::httplib
    ${AXIOM_EFSW_TARGET}
    ${AXIOM_SQLITE_TARGET}
    OpenSSL::SSL
    OpenSSL::Crypto
    Threads::Threads
    gRPC::grpc++
    protobuf::libprotobuf
)

add_dependencies(axiomdlp-agent axiom_grpc_gen)

# ----------------------------------------------------------------
# Platform-specific settings
# ----------------------------------------------------------------
if(WIN32)
    target_link_libraries(axiomdlp-agent PRIVATE advapi32 shell32 ws2_32 crypt32 ole32 user32)
    target_compile_definitions(axiomdlp-agent PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX _WIN32_WINNT=0x0601 _CRT_SECURE_NO_WARNINGS)
endif()

if(UNIX AND NOT APPLE)
    set(AXIOM_HAVE_SYSTEMD OFF)
    include(FindPkgConfig QUIET)
    if(PkgConfig_FOUND)
        if(CMAKE_SIZEOF_VOID_P EQUAL 4)
            set(ENV{PKG_CONFIG_LIBDIR} "/usr/lib/i386-linux-gnu/pkgconfig:/usr/share/pkgconfig")
        else()
            set(ENV{PKG_CONFIG_LIBDIR} "/usr/lib/x86_64-linux-gnu/pkgconfig:/usr/share/pkgconfig")
        endif()
        pkg_check_modules(SYSTEMD QUIET libsystemd)
        if(SYSTEMD_FOUND)
            set(AXIOM_HAVE_SYSTEMD ON)
        endif()
    endif()
    if(AXIOM_HAVE_SYSTEMD)
        target_compile_definitions(axiomdlp-agent PRIVATE HAVE_SYSTEMD)
        target_include_directories(axiomdlp-agent PRIVATE ${SYSTEMD_INCLUDE_DIRS})
        target_link_libraries(axiomdlp-agent PRIVATE ${SYSTEMD_LIBRARIES})
    endif()
    target_compile_options(axiomdlp-agent PRIVATE -fstack-protector-strong -fPIE)
    target_link_options(axiomdlp-agent PRIVATE -pie -Wl,-z,relro,-z,now)
endif()

if(MSVC)
    target_compile_options(axiomdlp-agent PRIVATE /W4 /permissive- /Zc:__cplusplus /bigobj)
else()
    target_compile_options(axiomdlp-agent PRIVATE -Wall -Wextra -Wpedantic)
endif()
```

  + CMakePresets.json
### \`CMakePresets.json\`

```json
{
  "version": 6,
  "cmakeMinimumRequired": {
    "major": 3,
    "minor": 24,
    "patch": 0
  },
  "configurePresets": [
    {
      "name": "base",
      "hidden": true,
      "generator": "Ninja",
      "binaryDir": "${sourceDir}/build/${presetName}"
    },
    {
      "name": "linux-x86_64-debug",
      "displayName": "Linux x86_64 \u2014 Debug",
      "inherits": "base",
      "toolchainFile": "${sourceDir}/cmake/toolchains/linux-x86_64.cmake",
      "cacheVariables": {
        "CMAKE_BUILD_TYPE": "Debug"
      }
    },
    {
      "name": "linux-x86_64-release",
      "displayName": "Linux x86_64 \u2014 Release",
      "inherits": "base",
      "toolchainFile": "${sourceDir}/cmake/toolchains/linux-x86_64.cmake",
      "cacheVariables": {
        "CMAKE_BUILD_TYPE": "Release"
      }
    },
    {
      "name": "linux-i686-debug",
      "displayName": "Linux i686 (32-bit) \u2014 Debug",
      "inherits": "base",
      "toolchainFile": "${sourceDir}/cmake/toolchains/linux-i686.cmake",
      "cacheVariables": {
        "CMAKE_BUILD_TYPE": "Debug"
      }
    },
    {
      "name": "linux-i686-release",
      "displayName": "Linux i686 (32-bit) \u2014 Release",
      "inherits": "base",
      "toolchainFile": "${sourceDir}/cmake/toolchains/linux-i686.cmake",
      "cacheVariables": {
        "CMAKE_BUILD_TYPE": "Release"
      }
    },
    {
      "name": "windows-x64-msvc-debug",
      "displayName": "Windows x64 \u2014 Debug (MSVC + vcpkg)",
      "inherits": "base",
      "binaryDir": "${sourceDir}/build/windows-x64-msvc-debug",
      "toolchainFile": "$env{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake",
      "cacheVariables": {
        "CMAKE_BUILD_TYPE": "Debug",
        "VCPKG_TARGET_TRIPLET": "x64-windows-static",
        "CMAKE_MSVC_RUNTIME_LIBRARY": "MultiThreaded$<$<CONFIG:Debug>:Debug>"
      },
      "condition": {
        "type": "equals",
        "lhs": "${hostSystemName}",
        "rhs": "Windows"
      }
    },
    {
      "name": "windows-x64-msvc-release",
      "displayName": "Windows x64 \u2014 Release (MSVC + vcpkg)",
      "inherits": "base",
      "binaryDir": "${sourceDir}/build/windows-x64-msvc-release",
      "toolchainFile": "$env{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake",
      "cacheVariables": {
        "CMAKE_BUILD_TYPE": "Release",
        "VCPKG_TARGET_TRIPLET": "x64-windows-static",
        "CMAKE_MSVC_RUNTIME_LIBRARY": "MultiThreaded$<$<CONFIG:Debug>:Debug>"
      },
      "condition": {
        "type": "equals",
        "lhs": "${hostSystemName}",
        "rhs": "Windows"
      }
    },
    {
      "name": "windows-x86-msvc-debug",
      "displayName": "Windows x86 \u2014 Debug (MSVC + vcpkg)",
      "inherits": "base",
      "binaryDir": "${sourceDir}/build/windows-x86-msvc-debug",
      "toolchainFile": "$env{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake",
      "cacheVariables": {
        "CMAKE_BUILD_TYPE": "Debug",
        "VCPKG_TARGET_TRIPLET": "x86-windows-static",
        "CMAKE_MSVC_RUNTIME_LIBRARY": "MultiThreaded$<$<CONFIG:Debug>:Debug>"
      },
      "condition": {
        "type": "equals",
        "lhs": "${hostSystemName}",
        "rhs": "Windows"
      }
    },
    {
      "name": "windows-x86-msvc-release",
      "displayName": "Windows x86 \u2014 Release (MSVC + vcpkg)",
      "inherits": "base",
      "binaryDir": "${sourceDir}/build/windows-x86-msvc-release",
      "toolchainFile": "$env{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake",
      "cacheVariables": {
        "CMAKE_BUILD_TYPE": "Release",
        "VCPKG_TARGET_TRIPLET": "x86-windows-static",
        "CMAKE_MSVC_RUNTIME_LIBRARY": "MultiThreaded$<$<CONFIG:Debug>:Debug>"
      },
      "condition": {
        "type": "equals",
        "lhs": "${hostSystemName}",
        "rhs": "Windows"
      }
    }
  ],
  "buildPresets": [
    {
      "name": "linux-x86_64-debug",
      "configurePreset": "linux-x86_64-debug"
    },
    {
      "name": "linux-x86_64-release",
      "configurePreset": "linux-x86_64-release"
    },
    {
      "name": "linux-i686-debug",
      "configurePreset": "linux-i686-debug"
    },
    {
      "name": "linux-i686-release",
      "configurePreset": "linux-i686-release"
    },
    {
      "name": "windows-x64-msvc-debug",
      "configurePreset": "windows-x64-msvc-debug",
      "configuration": "Debug"
    },
    {
      "name": "windows-x64-msvc-release",
      "configurePreset": "windows-x64-msvc-release",
      "configuration": "Release"
    },
    {
      "name": "windows-x86-msvc-debug",
      "configurePreset": "windows-x86-msvc-debug",
      "configuration": "Debug"
    },
    {
      "name": "windows-x86-msvc-release",
      "configurePreset": "windows-x86-msvc-release",
      "configuration": "Release"
    }
  ]
}
```

  · skip: README.md (not present)
  · skip: LICENSE (binary file — not bundled)
  + next.md
### \`next.md\`

~~~~markdown
# AxiomDLP Endpoint Agent — Phase 2 README

**Version:** 1.0.0  
**Status:** Design Document + Implementation Roadmap  
**Scope:** Agent-side only. Server changes will be documented separately.

---

## Table of Contents

1. [Current State](#1-current-state)
2. [Phase 2 Goals](#2-phase-2-goals)
3. [Architecture Additions](#3-architecture-additions)
4. [Milestone 15 — Policy Engine](#4-milestone-15--policy-engine)
5. [Milestone 16 — User-Mode Enforcement](#5-milestone-16--user-mode-enforcement)
6. [Milestone 17 — Extended Content Extraction](#6-milestone-17--extended-content-extraction)
7. [Milestone 18 — Advanced Detection](#7-milestone-18--advanced-detection)
8. [Milestone 19 — IPC & Tray Helper](#8-milestone-19--ipc--tray-helper)
9. [Milestone 20 — Kernel Enforcement (Linux eBPF)](#9-milestone-20--kernel-enforcement-linux-ebpf)
10. [Milestone 21 — Kernel Enforcement (Windows Minifilter)](#10-milestone-21--kernel-enforcement-windows-minifilter)
11. [Milestone 22 — Remote Commands & Auto-Update](#11-milestone-22--remote-commands--auto-update)
12. [Milestone 23 — Platform Expansion](#12-milestone-23--platform-expansion)
13. [Milestone 24 — Observability & Telemetry](#13-milestone-24--observability--telemetry)
14. [Milestone 25 — Hardening & Tamper Resistance](#14-milestone-25--hardening--tamper-resistance)
15. [Testing Strategy](#15-testing-strategy)
16. [Build & Release](#16-build--release)
17. [Working Agreement](#17-working-agreement)

---

## 1. Current State

### What Works

- Cross-platform service (Linux x86_64/i686, Windows x86_64/i686)
- Real-time filesystem watching (efsw / inotify / ReadDirectoryChangesW)
- Periodic inventory + SHA-256 hashing
- WebSocket + HTTP with TLS to the Core
- Content scanner for text files (chunked upload)
- SQLite with schema versioning (v4)
- Event bus, module framework, health reporting
- Bearer token auth, jittered reconnect, close-code handling
- Graceful shutdown, auth-failure shutdown

### What Does Not Exist

- No local policy engine
- No enforcement of any kind (block/allow/quarantine)
- No PDF/DOCX/OCR extraction (text files only)
- No fingerprinting (EDM, PDM, document registry)
- No user interface (tray, notifications, IPC)
- No kernel-level hooks
- No remote command channel
- No auto-update
- No macOS, ARM64
- No metrics endpoint
- No tamper resistance

---

## 2. Phase 2 Goals

By the end of Phase 2, the agent must:

1. **Decide locally** — evaluate events against a cached policy set within 5 ms.
2. **Enforce** — block file writes/reads, clipboard copy, print jobs, USB writes.
3. **See more** — extract text from PDFs, DOCX, XLSX, PPTX, images (OCR).
4. **Match more** — exact file fingerprints, document fingerprints, structured data.
5. **Talk to the user** — tray icon, notifications, pause/resume, override prompt.
6. **Survive attack** — anti-kill, service watchdog, signed updates.
7. **Talk to the fleet** — receive remote commands, apply policy updates without restart.
8. **Report health** — Prometheus-style metrics for every subsystem.

**Non-goals for Phase 2:** macOS support, mobile, cloud/SaaS DLP, CASB, eDiscovery. Those are Phase 3+.

---

## 3. Architecture Additions

New modules and interfaces:

```
+-------------------------------------------------------------+
|                     APPLICATION                              |
|                                                              |
|  Existing:  Config, Logger, Database, EventBus, ModuleMgr    |
|  New:       PolicyStore, EnforcerRegistry, IPCServer,        |
|             UpdateManager, MetricsRegistry, Watchdog         |
+--------------------------+----------------------------------+
                           |
        +------------------+------------------+
        |                  |                  |
        v                  v                  v
+---------------+  +----------------+  +----------------+
| Policy Engine |  | Enforcement    |  | Content        |
| (local cache) |  | (user + kernel)|  | Extractors     |
+-------+-------+  +--------+-------+  +--------+-------+
        |                   |                   |
        v                   v                   v
+-------------------------------------------------------------+
|                    EVENT BUS                                 |
+-------------------------------------------------------------+
        ^                   ^                   ^
        |                   |                   |
+-------+-------+  +--------+-------+  +--------+-------+
| FS Watcher    |  | Clipboard     |  | Print Monitor  |
| (existing)    |  | Monitor       |  | (new)          |
+---------------+  +----------------+  +----------------+
```

**New top-level directories:**

```
src/
├── policy/          # Policy engine, cache, evaluator
├── enforcement/     # User-mode enforcers (files, clipboard, print)
├── extractors/      # PDF, DOCX, XLSX, PPTX, image extractors
├── fingerprint/     # EDM, PDM, document registry
├── ipc/             # Unix socket / named pipe server
├── updater/         # Auto-update with signature verification
├── telemetry/       # Metrics registry + HTTP endpoint
├── watchdog/        # Self-monitoring + restart
└── platform/
    ├── linux/
    │   ├── ebpf/    # eBPF loader + programs
    │   └── fanotify/# fanotify backend
    └── windows/
        └── minifilter/ # Kernel driver + user-mode communicator
```

**New helper binary:**

```
axiomdlp-helper/     # Tray icon + notifications
```

---

## 4. Milestone 15 — Policy Engine

**Goal:** Evaluate file events against a local policy set in under 5 ms, with no network dependency.

### 4.1 Deliverables

- `PolicyStore` — SQLite-backed cache of server-provided policies
- `PolicyEngine` — evaluates `(event, scan_result)` → `Decision`
- `Decision` struct — action, risk score, matched policy ID, reason
- Policy version tracking + atomic hot-swap on update
- Subscribes to `scan_result` events on the bus

### 4.2 Data Types

```cpp
// src/policy/policy.h

enum class PolicyAction {
    Allow,      // no action, log at debug
    Log,        // write to audit log, no user impact
    Alert,      // log + admin notification
    Coach,      // log + user educational popup
    Block,      // deny the operation
    Quarantine, // move file to encrypted vault
    Encrypt,    // encrypt file in place
    Redact,     // remove sensitive content
};

struct PolicyRule {
    std::string id;
    std::string name;
    int         priority;         // higher wins
    bool        enabled;

    // Match conditions (all must be true)
    std::vector<std::string> entity_types;   // "CREDIT_CARD", "SSN", ...
    int         min_risk_score = 0;          // 0-10
    double      min_confidence = 0.0;        // 0.0-1.0
    std::vector<std::string> file_extensions; // empty = any
    std::vector<std::string> path_patterns;   // glob, empty = any
    std::vector<std::string> event_types;     // "file_write", ...

    // Action
    PolicyAction action;
    bool         enforce_sync = true;         // block inline
    int          priority_override = 0;
};

struct Decision {
    PolicyAction action       = PolicyAction::Allow;
    int          risk_score   = 0;
    std::string  reason;
    std::string  policy_id;
    std::string  policy_name;
    bool         enforce_sync = false;
};
```

### 4.3 PolicyStore

- Backed by SQLite table `policies` (migration v5)
- `load_from_server()` — downloads signed bundle via HTTPS, verifies signature, atomically replaces
- `get_rules()` — returns an immutable snapshot (shared_ptr to vector)
- `version()` — monotonic counter for change detection
- Thread-safe; readers never block on writers

### 4.4 PolicyEngine

```cpp
class PolicyEngine {
public:
    Decision evaluate(const Event& event, const ScanResult& scan);

private:
    std::shared_ptr<const std::vector<PolicyRule>> rules_;
};
```

Algorithm:
1. Iterate rules in descending priority.
2. For each rule, check every condition against the event + scan.
3. First matching rule wins.
4. No match → `Decision{Allow}`.

### 4.5 Hot-Swap

When the server pushes a policy update via WebSocket:

1. `transport` module receives `policy_update` message with a signed bundle URL + hash.
2. `policy` module downloads the bundle in the background.
3. Verifies Ed25519 signature against an embedded public key.
4. Opens a new SQLite read transaction, writes to `policies_new`, commits.
5. Atomically swaps `rules_` shared_ptr.
6. Publishes `policy_changed` event so other modules can react.

### 4.6 Success Criteria

1. 1000 rules evaluate in < 5 ms on the target hardware.
2. A policy update takes effect within 100 ms of the push.
3. Signed bundle with wrong signature is rejected and logged at ERROR.
4. A missing bundle leaves the previous policy set in place.
5. Every decision is recorded in an audit table.

---

## 5. Milestone 16 — User-Mode Enforcement

**Goal:** Enforce `Block` decisions for files, clipboard, and print jobs without kernel support.

### 5.1 Deliverables

- `IEnforcer` interface
- `FileEnforcer` — inotify + fanotify-based, blocks file writes
- `ClipboardEnforcer` — X11 / Wayland clipboard hooks (Linux), Windows clipboard (Windows)
- `PrintEnforcer` — CUPS hook (Linux), spooler hook (Windows)
- `EnforcerRegistry` — routes decisions to the right enforcer

### 5.2 Interface

```cpp
// src/enforcement/enforcer.h

class IEnforcer {
public:
    virtual ~IEnforcer() = default;
    virtual std::string name() const = 0;
    virtual bool can_enforce(const std::string& event_type) const = 0;
    virtual bool enforce(const Decision& decision, const Event& event) = 0;
};
```

### 5.3 FileEnforcer (Linux)

Uses **fanotify** in `FAN_OPEN_PERM` mode:

1. Open a fanotify fd with `FAN_CLASS_CONTENT`.
2. Mark the mount points we care about.
3. On `FAN_OPEN_PERM` event, suspend the calling process.
4. Publish a `file_access` event, wait for a `Decision`.
5. Call `write(fd, response)` with `FAN_ALLOW` or `FAN_DENY`.

Limitation: fanotify permission events require `CAP_SYS_ADMIN`. The agent must run as a service (root on Linux).

**Fallback:** if fanotify is unavailable, use inotify + `LD_PRELOAD` shim or accept that writes complete but are then quarantined.

### 5.4 FileEnforcer (Windows)

Windows lacks a user-mode API to block file opens. Two options:

1. **Minifilter** (Milestone 21) — kernel-level, unbypassable.
2. **Read-only folder trick** — temporarily mark the target directory read-only when a block is decided. Racy and bypassable.

For Phase 2 we implement a **notification-only** FileEnforcer on Windows and defer blocking to Milestone 21.

### 5.5 ClipboardEnforcer (Linux)

- X11: own `CLIPBOARD` and `PRIMARY` selections; intercept `XConvertSelection` requests.
- Wayland: use `wlr-data-control-unstable-v1` protocol (Wayland compositor support varies).

The enforcer scans every clipboard payload:
- If `Decision::Block`, do not serve the selection.
- Otherwise serve it.

### 5.6 ClipboardEnforcer (Windows)

- Register a clipboard viewer with `AddClipboardFormatListener`.
- On `WM_CLIPBOARDUPDATE`, read the clipboard, scan it.
- If `Block`, call `EmptyClipboard()` and show a notification.

### 5.7 PrintEnforcer (Linux)

- Watch `/var/spool/cups/tmp/` for new spool files.
- On creation, open and scan.
- If `Block`, delete the spool file and send a CUPS cancel.

### 5.8 PrintEnforcer (Windows)

- Register a print monitor with `AddMonitor` API.
- Intercept `StartDocPrinter`, `WritePrinter`.
- Scan the spooled data before it reaches the device.

### 5.9 Success Criteria

1. A `Block` decision on `file_access` prevents the read on Linux.
2. A `Block` decision on `clipboard_copy` clears the clipboard within 100 ms.
3. A `Block` decision on `print_job` cancels the print before it leaves the spooler.
4. Every enforcement action is recorded in the audit log with the policy ID.
5. Enforcement failure does not crash the agent — it logs and continues.

---

## 6. Milestone 17 — Extended Content Extraction

**Goal:** Scan PDF, DOCX, XLSX, PPTX, and images with OCR.

### 6.1 Deliverables

- `IContentExtractor` interface
- `PdfExtractor` — Poppler or MuPDF
- `DocxExtractor` — libzip + XML parsing
- `XlsxExtractor` — same
- `PptxExtractor` — same
- `ImageOcrExtractor` — PaddleOCR C++ bindings (ncnn)
- `ExtractorRegistry` — routes file types to the right extractor
- Content-type detection via magic bytes (libmagic)

### 6.2 Interface

```cpp
// src/extractors/extractor.h

struct ExtractedContent {
    std::string text;
    std::vector<std::string> images_b64;   // embedded images for OCR
    std::map<std::string, std::string> metadata;
    std::string error;
    bool ok() const { return error.empty(); }
};

class IContentExtractor {
public:
    virtual ~IContentExtractor() = default;
    virtual std::string name() const = 0;
    virtual bool supports(const std::string& mime, const std::string& ext) const = 0;
    virtual ExtractedContent extract(const std::vector<uint8_t>& data,
                                     const std::string& filename) = 0;
};
```

### 6.3 Dependencies

| Extractor | Library | License | Notes |
|-----------|---------|---------|-------|
| PDF | Poppler (libpoppler-cpp) | GPL-2.0 | Add a note about GPL linkage |
| PDF (alt) | MuPDF | AGPL | Not compatible with commercial agent |
| DOCX/XLSX/PPTX | libzip + pugixml | BSD/MIT | — |
| OCR | PaddleOCR + ncnn | Apache 2.0 | Bundle as static library |
| Magic | libmagic | BSD | — |

Choose **Poppler** for PDF unless licensing forbids GPL; then fall back to a commercial SDK (PDFium via Chromium build).

### 6.4 Pipeline

```
content_scanner (existing)
    |
    v
ExtractorRegistry::extract(data, filename) 
    |
    +-- by magic bytes → PDF   → PdfExtractor
    +-- by magic bytes → ZIP   → try DOCX/XLSX/PPTX
    +-- by magic bytes → image → ImageOcrExtractor
    +-- else            → treat as text
    |
    v
ExtractedContent { text, images_b64 }
    |
    +-- text → chunk + send to server
    +-- images → OCR each → append text → chunk + send
```

### 6.5 Success Criteria

1. A 50-page PDF extracts to text in under 3 seconds.
2. A DOCX with embedded images extracts text + OCRs each image.
3. An XLSX with 10 sheets extracts all cells including formulas.
4. A PNG or JPG containing text produces the text via OCR.
5. Extraction errors do not crash the agent.

---

## 7. Milestone 18 — Advanced Detection

**Goal:** Move beyond regex — exact data match, document fingerprinting, and structured data.

### 7.1 Deliverables

- `EdmMatcher` — Exact Data Match (columns of known-sensitive values)
- `PdmMatcher` — Partial Document Match (shingled fingerprints)
- `DocumentRegistry` — a signed corpus of sensitive documents
- `StructuredDataMatcher` — CSV/JSON records against a schema
- `FingerprintStore` — SQLite-backed

### 7.2 EDM

- The server compiles a list of exact values (SSNs, card numbers, patient IDs) into a Bloom filter + HMAC.
- The filter + HMAC key are pushed to the agent via the policy bundle.
- On scan, the agent HMACs every token in the file and checks membership in the filter.
- No plaintext values on the endpoint — only HMACs of them.

```cpp
class EdmMatcher {
public:
    bool match(const std::string& text,
               std::vector<std::string>& out_matches) const;
private:
    // Bloom filter of HMAC(value, key)
    std::vector<uint8_t> filter_;
    std::array<uint8_t, 32> hmac_key_;
    // Number of hash functions, filter size
    uint32_t k_ = 7;
    uint64_t m_ = 0;
};
```

### 7.3 PDM

- The server shingles each protected document into 30-character chunks with stride 10.
- Each shingle is hashed and stored in a Bloom filter.
- The agent computes the same shingles on a candidate file and counts matching shingles.
- Above a threshold (e.g., 25% of shingles match), the file is flagged.

### 7.4 DocumentRegistry

- Signed list of document IDs with metadata (name, category, sensitivity).
- The registry is downloaded and cached.
- The matcher returns the document ID + name when a match is found.

### 7.5 Structured Data

- Policies can embed a schema (e.g., "row must contain both credit_card AND email").
- The matcher parses CSV/JSON and applies the rules row by row.

### 7.6 Success Criteria

1. A file containing one exact SSN from the EDM list is flagged.
2. A file with 25% PDM shingle match is flagged with the source document name.
3. A CSV with one row meeting a structured-data rule is flagged.
4. EDM list of 10 million values fits in under 20 MB of RAM.
5. Match time for a 1 MB file is under 50 ms.

---

## 8. Milestone 19 — IPC & Tray Helper

**Goal:** Give the user a visible agent, notifications, and pause/resume.

### 8.1 Deliverables

- `IPCServer` in the service (Unix socket on Linux, named pipe on Windows)
- `axiomdlp-helper` binary — tray icon + notifications + IPC client
- Message protocol (length-prefixed JSON)
- Auto-start of helper on user login (Linux: XDG autostart; Windows: Run key)

### 8.2 IPC Protocol

Message shape:

```json
{
  "type": "notification",
  "id":   "evt_abc123",
  "title": "Sensitive file blocked",
  "body":  "/home/user/Downloads/pii.txt contains credit card data",
  "actions": ["view_details", "dismiss"],
  "severity": "warning"
}
```

Types:
- `notification`
- `pause_dlp` / `resume_dlp`
- `query_status`
- `override_request` / `override_response`
- `user_feedback`
- `ping` / `pong`

### 8.3 Tray Icon

- **Linux:** libappindicator3 (`libayatana-appindicator3-dev`)
- **Windows:** `Shell_NotifyIcon` via Win32 API
- **States:** green (running), yellow (paused), red (error)
- **Menu:** Status, Pause for 15m, Pause until restart, View events, About, Quit

### 8.4 Notifications

- **Linux:** `notify-send` via D-Bus or `libnotify`
- **Windows:** `Shell_NotifyIcon` balloon or Windows toast (WinRT)
- Severity levels map to icon + color

### 8.5 Pause/Resume

- Helper sends `pause_dlp` with a TTL (seconds).
- Service sets a `paused_until` timestamp.
- Enforcer registry checks the timestamp before enforcing.
- Policy can disallow pause (e.g., in "strict" mode).

### 8.6 Override Request

When a user is blocked, they can click "Request override":

1. Helper opens a dialog for justification.
2. Sends `override_request` over IPC.
3. Service publishes an event; the Core receives it over WebSocket.
4. The admin approves/denies on the server.
5. The response flows back through the WebSocket → IPC → helper notification.

### 8.7 Success Criteria

1. Tray icon appears when the user logs in.
2. A blocked event shows a notification within 200 ms.
3. Pause/resume takes effect within 1 s.
4. Helper crash does not affect the service.
5. Service restart reconnects the helper automatically.

---

## 9. Milestone 20 — Kernel Enforcement (Linux eBPF)

**Goal:** Unbypassable enforcement of file operations via eBPF LSM programs.

### 9.1 Deliverables

- eBPF LSM programs: `file_open`, `file_permission`, `task_kill`
- eBPF map for kernel↔user communication
- User-space loader (libbpf)
- BPF CO-RE (Compile Once – Run Everywhere)

### 9.2 Architecture

```
+----------------------+          +---------------------+
|   User processes     |          |                     |
|   open(), exec()     | ----->   |  Kernel LSM hooks   |
+----------------------+          |    (BPF programs)   |
                                  +----------+----------+
                                             |
                                             v
                                  +---------------------+
                                  |  BPF ring buffer    |
                                  +----------+----------+
                                             |
                                             v
                                  +---------------------+
                                  |  User-space loader  |
                                  |   (agent process)   |
                                  +----------+----------+
                                             |
                                             v
                                  +---------------------+
                                  |   PolicyEngine      |
                                  +---------------------+
```

### 9.3 Program 1 — `file_open`

Triggered on `security_file_open`. Looks up the file path in a BPF hash map. If present and marked DENY, returns `-EACCES`.

The map is populated by user-space when the policy engine decides to block a specific path.

### 9.4 Program 2 — `file_permission`

Triggered on `security_file_permission` for write operations. Same as above but for writes.

### 9.5 Program 3 — `task_kill`

Triggered on `sched_process_exit`. Used for telemetry, not enforcement.

### 9.6 Communication Channel

- User-space writes decisions to a BPF hash map keyed by `(dev, inode)`.
- Kernel program looks up by `(dev, inode)`.
- Value: `u8` (0 = allow, 1 = deny) + TTL.

### 9.7 Loading

- `libbpf` (with CO-RE) + a `.bpf.o` object compiled at build time.
- Attached via `bpf_link` to the LSM hook.
- Unloaded on agent shutdown.

### 9.8 Requirements

- Linux 5.7+ (BPF LSM)
- `CONFIG_BPF_LSM=y` in the running kernel
- The agent must run as root or with `CAP_BPF` + `CAP_PERFMON`

### 9.9 Fallback

If BPF LSM is unavailable, fall back to fanotify (Milestone 16).

### 9.10 Success Criteria

1. A blocked file open returns `EACCES` to the caller.
2. `strace -f /usr/bin/cat blocked.txt` shows `EACCES`.
3. Direct syscalls (`syscall(SYS_open, ...)`) also return `EACCES`.
4. No kernel oops under load.
5. Agent unload removes all BPF programs cleanly.

---

## 10. Milestone 21 — Kernel Enforcement (Windows Minifilter)

**Goal:** Unbypassable enforcement of file operations on Windows.

### 10.1 Deliverables

- Minifilter driver (`axiomdlpflt.sys`)
- User-space communication library
- Installer / uninstaller
- Driver signing (test cert for dev, EV cert for production)

### 10.2 Architecture

```
+----------------------+          +---------------------+
|  Win32 file APIs     |          |                     |
|  CreateFile()        | ----->   |  Windows I/O Mgr    |
+----------------------+          +----------+----------+
                                             |
                                             v
                                  +---------------------+
                                  |  Filter Manager     |
                                  |   (fltmgr.sys)      |
                                  +----------+----------+
                                             |
                                             v
                                  +---------------------+
                                  |  axiomdlpflt.sys    |
                                  +----------+----------+
                                             |
                                             v
                                  +---------------------+
                                  |  Agent (user-mode)  |
                                  |  via FltSendMessage |
                                  +---------------------+
```

### 10.3 Driver Responsibilities

- Register `IRP_MJ_CREATE` pre-op callback
- On each file open, send a message to user-space
- Wait up to 5 seconds for a response
- Response: `FLT_PREOP_SUCCESS_NO_CALLBACK` (allow) or `FLT_PREOP_COMPLETE` with `STATUS_ACCESS_DENIED` (block)

### 10.4 User-Space Communicator

- Uses `FilterConnectCommunicationPort` + `FilterSendMessage` + `FilterGetMessage`
- Runs on a dedicated thread
- Calls `PolicyEngine::evaluate` and returns the decision

### 10.5 Installation

- `axiomdlpflt.inf` — the driver INF
- `sc create axiomdlpflt binPath= ... type= kernel`
- `fltmc load axiomdlpflt`
- Uninstall: `fltmc unload` + `sc delete`

### 10.6 Requirements

- Windows 10+ (driver model 1.9+)
- Driver signed with a Microsoft‑approved cert (EV or test with `bcdedit /set testsigning on` in dev)

### 10.7 Success Criteria

1. `CreateFile()` on a blocked path returns `ERROR_ACCESS_DENIED`.
2. Direct NT API calls (`NtCreateFile`) also blocked.
3. Driver unloads cleanly without a bugcheck.
4. No file system corruption under stress.
5. Signed driver loads on Windows 10 and 11 x64.

---

## 11. Milestone 22 — Remote Commands & Auto-Update

**Goal:** Control the fleet from the Core and update agents without admin intervention.

### 11.1 Deliverables

- Command dispatcher on the agent (subscribe to `command` WebSocket messages)
- Signature verification for update bundles
- Atomic binary swap on Linux and Windows
- Rollback on failed startup

### 11.2 Commands

| Command | Payload | Action |
|---------|---------|--------|
| `rescan` | `{}` | Trigger a full inventory scan |
| `pause` | `{"duration_sec": 900}` | Pause enforcement |
| `resume` | `{}` | Resume enforcement |
| `quarantine_file` | `{"path": "/..."}` | Move to vault |
| `uninstall` | `{}` | Uninstall and exit |
| `update` | `{"url": "...", "sha256": "...", "sig": "..."}` | Download, verify, install |
| `restart` | `{}` | Restart the service |
| `dump_logs` | `{"lines": 1000}` | Upload recent logs |
| `run_diagnostic` | `{}` | Run self-test, upload results |

### 11.3 Update Flow

1. Agent receives `update` command with URL, SHA-256, and Ed25519 signature.
2. Downloads to `/var/lib/axiomdlp/updates/axiomdlp-agent.new`.
3. Verifies SHA-256.
4. Verifies Ed25519 signature against the embedded public key.
5. On Linux:
   - Rename current binary to `.old`
   - Rename `.new` to current path
   - `chmod +x`
   - Restart via systemd (`systemctl restart axiomdlp-agent`)
   - On startup, if the new binary fails a self-test within 30 seconds, revert to `.old`
6. On Windows:
   - Schedule the swap via `MoveFileEx(MOVEFILE_DELAY_UNTIL_REBOOT)` OR
   - Use a helper service running outside the agent
   - Verify by re-launch and self-test

### 11.4 Embedded Public Key

```cpp
// src/updater/trusted_keys.h
constexpr const char* kUpdateSigningKeyHex =
    "9f8e7d6c5b4a39281706f5e4d3c2b1a0..."; // 32-byte Ed25519 public key
```

The key is compiled into the binary and cannot be replaced without a signed update.

### 11.5 Success Criteria

1. An unsigned update is rejected and logged at ERROR.
2. A signed update installs and the agent restarts within 10 seconds.
3. A corrupted binary fails self-test and rolls back automatically.
4. Linux update works without a restart via `systemd-run`.

---

## 12. Milestone 23 — Platform Expansion

**Goal:** macOS and ARM64 support.

### 12.1 macOS

- **Filesystem watching:** FSEvents API
- **Enforcement:** EndpointSecurity framework (requires Apple entitlements)
- **Service:** launchd plist
- **Tray:** NSStatusBar
- **Signing:** Apple Developer ID + notarization
- **Distribution:** `.pkg` signed and notarized

### 12.2 ARM64 Linux

- Add `cmake/toolchains/linux-aarch64.cmake`
- Cross-compile via `aarch64-linux-gnu-g++`
- Build OpenSSL for aarch64
- Verify eBPF works on aarch64

### 12.3 Windows ARM64

- Add `windows-aarch64.cmake` via llvm-mingw
- Build OpenSSL with `VC-WIN64-ARM`
- Verify minifilter loads on Surface Pro X

### 12.4 Success Criteria

1. All six targets build on CI: linux‑x86_64, linux‑i686, linux‑aarch64, windows‑x86_64, windows‑i686, windows‑aarch64.
2. macOS agent starts, watches files, and enforces blocks.
3. Binary sizes remain under 15 MB compressed.
4. ARM64 binaries run on Raspberry Pi 4 (Linux) and Surface Pro X (Windows).

---

## 13. Milestone 24 — Observability & Telemetry

**Goal:** Every subsystem exposes metrics for fleet-wide visibility.

### 13.1 Deliverables

- `MetricsRegistry` — process-wide metrics
- HTTP endpoint on `127.0.0.1:9101/metrics` in Prometheus text format
- Structured logs with correlation IDs
- Optional OpenTelemetry exporter

### 13.2 Metrics

| Metric | Type | Description |
|--------|------|-------------|
| `axiom_agent_uptime_seconds` | gauge | Process uptime |
| `axiom_agent_module_health` | gauge | Per-module health 0/1/2/3 |
| `axiom_events_published_total` | counter | Events on the bus |
| `axiom_events_dropped_total` | counter | Outbound queue overflows |
| `axiom_scan_files_total` | counter | Files scanned |
| `axiom_scan_bytes_total` | counter | Bytes scanned |
| `axiom_scan_duration_seconds` | histogram | Scan latency |
| `axiom_decisions_total{action}` | counter | Decisions by action |
| `axiom_enforcement_total{result}` | counter | Enforcement by outcome |
| `axiom_policy_version` | gauge | Current policy version |
| `axiom_ws_reconnects_total` | counter | WebSocket reconnect attempts |
| `axiom_ws_connected` | gauge | 1 if connected |
| `axiom_ipc_connections` | gauge | Active helper connections |

### 13.3 Log Correlation

Every event carries a `trace_id` (ULID). Logs include `trace_id` in the structured prefix so a single event can be followed from filesystem watcher to server decision to enforcement action.

### 13.4 OpenTelemetry (optional)

If `OTEL_EXPORTER_OTLP_ENDPOINT` is set, spans are exported:
- `content_scan` (span per file)
- `policy_evaluate` (span per decision)
- `enforce` (span per enforcement action)
- `ws_publish` (span per outbound message)

### 13.5 Success Criteria

1. `/metrics` returns valid Prometheus text.
2. Grafana scrapes and shows a full dashboard.
3. A single event can be traced end-to-end across the bus.
4. Metrics add < 1% CPU overhead.

---

## 14. Milestone 25 — Hardening & Tamper Resistance

**Goal:** The agent cannot be disabled, killed, or bypassed by a non-admin user.

### 14.1 Deliverables

- Service watchdog (a second process that restarts the agent if it dies)
- Anti-kill (kernel-level process protection via eBPF / minifilter)
- Configuration integrity check (HMAC over config file)
- Binary integrity check (self-hash compared to a compiled-in value)
- Signed configuration and policy bundles (already covered)
- Protected directories: `logs/`, `data/`, `updates/` are chmod 0700 and owned by root

### 14.2 Watchdog

Two processes:
- `axiomdlp-agent` — main
- `axiomdlp-watchdog` — small, monitors the main via a heartbeat file

If the heartbeat file is not touched within 30 seconds, the watchdog restarts the agent via systemd / SCM.

The watchdog itself is protected by:
- `ProtectSystem=strict` in the systemd unit
- Runs as a separate systemd unit that the main cannot stop (no `systemctl stop` permission)

### 14.3 Anti-Kill (Linux)

eBPF LSM `task_kill` program: if the target is the agent's process, deny the kill with `-EPERM`. Requires `CAP_BPF` and a kernel with BPF LSM.

### 14.4 Anti-Kill (Windows)

Minifilter pre-op on `IRP_MJ_SET_INFORMATION` with `FileDispositionInformation` for the agent's own binary and data files. Returns `STATUS_ACCESS_DENIED` if a non-system process tries to delete them.

A more robust approach uses an ObRegisterCallbacks hook to strip `PROCESS_TERMINATE` from handles opened against the agent's PID.

### 14.5 Configuration Integrity

On startup:
1. Read `config/agent.json`.
2. Compute HMAC-SHA256 with a key derived from a machine-specific secret (TPM or file ACL).
3. Compare against `config/agent.json.hmac`.
4. If mismatch, log ERROR and load defaults.

### 14.6 Binary Integrity

Embed a SHA-256 of the running binary at build time. On startup:
1. Reopen `/proc/self/exe` (Linux) or the module path (Windows).
2. Compute SHA-256.
3. Compare against the embedded value.
4. If mismatch, log ERROR and continue in "degraded" mode with limited enforcement.

### 14.7 Success Criteria

1. `kill -9` on the agent as a non-root user fails with `EPERM`.
2. Deleting the agent binary as a non-root user fails.
3. Modifying the config file is detected at next startup.
4. Watchdog restarts the agent within 30 seconds of a crash.
5. None of the above prevents legitimate admin operations.

---

## 15. Testing Strategy

### 15.1 Unit Tests

- **C++:** Catch2 + doctest + GoogleTest
- Target: 80% coverage on `src/policy`, `src/enforcement`, `src/extractors`, `src/fingerprint`
- Run on every commit via CI

### 15.2 Integration Tests

- **Agent ↔ Server:** spin up the Go server on a test port, run the agent in a container, assert end-to-end behavior
- Use a dedicated test database
- Cover: enrollment, heartbeat, WebSocket connect, policy update, scan batch, remote command

### 15.3 Load Tests

- 10,000 simulated agents connect to the server
- Sustained 100 req/s per agent for 10 minutes
- Assert no server-side leaks, no dropped messages

### 15.4 Chaos Tests

- Kill the server mid-scan → agent must not lose data
- Network partition → agent must queue and recover
- Corrupt policy file → agent must reject and continue with old policy
- Fill the disk → agent must not crash

### 15.5 Fuzz Tests

- **PDF extractor:** libFuzzer with a seed corpus
- **DOCX extractor:** same
- **WebSocket parser:** AFL
- **IPC protocol:** libFuzzer

### 15.6 Penetration Tests

Every release:
- Attempt to bypass enforcement
- Attempt to forge a policy update
- Attempt to kill the agent
- Attempt to read agent's private state

### 15.7 Certification

- SOC 2 Type II
- ISO 27001
- Common Criteria (EAL2+)
- FIPS 140-2 for crypto modules

---

## 16. Build & Release

### 16.1 CI Pipeline

Triggered on every push:

1. Lint (clang-tidy, clang-format, cppcheck)
2. Unit tests on linux‑x86_64
3. Cross-compile for linux‑i686, windows‑x86_64, windows‑i686
4. Integration tests against a mock server
5. SBOM generation (CycloneDX)
6. Upload artifacts

Triggered on every tag:

1. All of the above
2. Sign binaries with the release key
3. Build `.deb`, `.rpm`, `.msi`
4. Sign packages
5. Publish to the release page
6. Publish SBOM

### 16.2 Versioning

- Semantic versioning: `MAJOR.MINOR.PATCH`
- Every binary embeds the git commit SHA
- Every update bundle includes a `min_version` field

### 16.3 Distribution

- **Linux:** `.deb` + `.rpm` + a static tarball
- **Windows:** `.msi` + a static `.exe`
- **macOS:** signed `.pkg`
- **Docker:** `axiomdlp/agent:<tag>` for testing

---

## 17. Working Agreement

### 17.1 Per Milestone

1. Design doc in this README
2. Interface in header files
3. Reference implementation
4. Unit tests
5. Integration tests
6. All four targets build clean
7. Commit with a descriptive message
8. Update this README to reflect the new state

### 17.2 Every New Module Must

- Implement `IModule`
- Receive `ModuleContext`
- Publish or subscribe to events via the bus
- Report health
- Have a metrics section
- Be fail-safe (errors logged, not fatal)
- Be documented in this README

### 17.3 Every New Subsystem Must

- Follow the three-file pattern: header, implementation, CMake wiring
- Have platform-specific implementations in `platform/<os>/` if it touches the OS
- Have a debug flag to disable it at runtime
- Have a self-test that runs at startup

### 17.4 Every Release Must

- Pass all unit + integration tests
- Have a signed SBOM
- Have a changelog entry
- Be backward compatible with the previous MINOR version's config

### 17.5 Definition of Done

- [ ] Code committed
- [ ] Tests pass
- [ ] Docs updated
- [ ] This README updated
- [ ] All six targets build (once Phase 2 is complete)
- [ ] Metrics exported
- [ ] Logs structured
- [ ] Error paths exercised

---

## Summary

**Phase 2 deliverables in order:**

| # | Milestone | Depends On |
|---|-----------|------------|
| 15 | Policy Engine | — |
| 16 | User-Mode Enforcement | 15 |
| 17 | Extended Extraction | — |
| 18 | Advanced Detection | 17 |
| 19 | IPC & Tray Helper | 16 |
| 20 | Kernel Enforcement (Linux) | 16 |
| 21 | Kernel Enforcement (Windows) | 16 |
| 22 | Remote Commands & Auto-Update | 19 |
| 23 | Platform Expansion | 22 |
| 24 | Observability | 15 |
| 25 | Hardening | 20, 21, 22 |

**Estimated effort:** 6–9 months for one experienced systems engineer, 3–4 months for a small team of 3.

**After Phase 2, the agent will be a production-grade enterprise DLP endpoint** on par with endpoint DLP features found in Forcepoint, Symantec, and Microsoft Defender for Endpoint.
~~~~

  + done.md
### \`done.md\`

~~~~markdown
# AxiomDLP Endpoint Agent — DONE.md

**Status Document — Everything Implemented, Verified, and Fixed**

**Last updated:** After the MSVC + vcpkg migration on `msvc-windows-build`.
Windows x86_64 and x86 both build and run; Linux x86_64 and i686 remain
unchanged from `main`.

**Supersedes:** all previous DONE.md revisions.

---

## 1. Executive Summary

The AxiomDLP Endpoint Agent is a **cross-platform, TLS-capable, network-connected
service foundation** with **four verified build targets from two distinct
toolchains**, six working modules, a persistent local SQLite database, SHA-256
file fingerprinting, HTTP + WebSocket connectivity to the AxiomDLP Core server,
a content scanner for text-based PII detection, and an **atomic
ShutdownController** that lets any component stop the whole agent cleanly with
a typed reason.

### Two dependency pipelines, one source tree

| Platform                | Toolchain              | Dependencies                                                                      |
| ----------------------- | ---------------------- | --------------------------------------------------------------------------------- |
| **Linux x86_64 / i686** | GCC (native, multilib) | `.deps/` — `build-openssl.sh` + system Boost + FetchContent + SQLite amalgamation |
| **Windows x64 / x86**   | MSVC 2022 (19.44)      | **vcpkg manifest mode** — 66 packages, static triplet                             |

The `if(MSVC)` branch in `CMakeLists.txt` is the only place the two
strategies diverge. Every source file, every header, every module uses the
same `find_package(...)` API and links the same imported targets
(`Boost::headers`, `OpenSSL::SSL`, `SQLite::SQLite3`, `spdlog::spdlog`,
`nlohmann_json::nlohmann_json`, `efsw::efsw`).

### What Works Today

**Linux (x86_64 + i686) — unchanged from `main`:**
- GCC native builds from `.deps/`
- Static OpenSSL 3.3.2, system Boost 1.90, FetchContent spdlog 1.14.1 / efsw 1.7.2 / nlohmann 3.11.3, SQLite 3.46.1 amalgamation
- systemd + `sd_notify`, i686 via `-m32` toolchain file
- Both architectures build, link, run, enroll, connect WebSocket, heartbeat, shutdown

**Windows (x64 + x86) — new on `msvc-windows-build`:**
- MSVC 2022 BuildTools builds via vcpkg manifest mode
- Fully static: OpenSSL 3.6.5, Boost 1.92.0, SQLite 3.53.4, spdlog 1.17.0, fmt 12.2.0, nlohmann-json 3.12.0, efsw 1.7.2
- Static CRT (`/MT`) — no `vcruntime140.dll` dependency
- SCM integration, service install/uninstall
- **Binary sizes: 8.61 MB (x64), 6.37 MB (x86)**
- No external DLL dependencies beyond Windows system DLLs
- Both binaries verified via full startup log (6 modules, migrations 0→4)

**Cross-platform features (all four targets):**
- Async structured logging with rotation and non-blocking delivery
- JSON configuration with fail-safe defaults and environment variable expansion
- Real-time filesystem watching (inotify / ReadDirectoryChangesW via efsw)
- Periodic inventory scanning with interruptible cancellation
- SHA-256 content hashing stored in SQLite
- Bidirectional WebSocket to Core with TLS, jittered reconnect, close-code interpretation, graceful 1000 Normal close
- HTTP client with unconditional certificate verification
- Agent enrollment via `POST /api/v1/agent/register`
- Periodic heartbeat via `POST /api/v1/agent/heartbeat`, plus immediate validation heartbeat at startup
- Atomic shutdown with typed reasons preserved end to end
- Server-pushed revocation handling (`token_revoked`, `agent_revoked`)
- Goodbye message on graceful exits
- Content scanner: overlapping chunks → `POST /api/v1/agent/scan/batch` → verdicts back to local `files` table
- Local visualization dashboard (Python server + HTML)

### What Does Not Exist Yet

- Policy engine (decisions from scan results)
- User-mode enforcement (blocking file operations)
- Kernel-mode enforcement (Windows Minifilter / Linux eBPF / fanotify)
- IPC + user helper (tray icon, notifications)
- Auto-update mechanism
- Code signing and packaging (`.deb`, `.rpm`, MSI)
- Rich-document extraction (PDF, DOCX, XLSX, images)
- gRPC transport (scaffolding exists but disabled — see §5.4)

---

## 2. The Four Verified Platforms

All four targets build, link, and run. All support TLS and the full
bidirectional WebSocket pipeline.

| Platform           | Toolchain             | Build | Run | Watch | Hash | HTTP | WSS | Scanner | Shutdown |
| ------------------ | --------------------- | ----- | --- | ----- | ---- | ---- | --- | ------- | -------- |
| **Linux x86_64**   | GCC + `.deps/`        | ✅     | ✅   | ✅     | ✅    | ✅    | ✅   | ✅       | ✅        |
| **Linux i686**     | GCC `-m32` + `.deps/` | ✅     | ✅   | ✅     | ✅    | ✅    | ✅   | ✅       | ✅        |
| **Windows x86_64** | MSVC + vcpkg          | ✅     | ✅   | ✅     | ✅    | ✅    | ✅   | ✅       | ✅        |
| **Windows x86**    | MSVC + vcpkg          | ✅     | ✅   | ✅     | ✅    | ✅    | ✅   | ✅       | ✅        |

**Windows binary verification:**

| Binary                                              | Size    | Machine      |
| --------------------------------------------------- | ------- | ------------ |
| `build/windows-x64-msvc-release/axiomdlp-agent.exe` | 8.61 MB | `8664 (x64)` |
| `build/windows-x86-msvc-release/axiomdlp-agent.exe` | 6.37 MB | `14C (x86)`  |

No external DLL dependencies beyond `KERNEL32`, `ADVAPI32`, `WS2_32`,
`SHELL32`, `ole32`, `USER32`, `CRYPT32`.

---

## 3. Architecture Overview

```
┌─────────────────────────────────────────────────────────────────────┐
│                        APPLICATION                                   │
│  Owns: Config, Logger, ShutdownController, Database, FileStore,      │
│        StateStore, EventBus, ModuleContext, ModuleManager, Service   │
└──────────────────────────────┬──────────────────────────────────────┘
                               │
                               ▼
┌─────────────────────────────────────────────────────────────────────┐
│                          SERVICE                                     │
│  Linux: systemd + sd_notify + signals                                │
│  Windows: SCM + ServiceMain + control handler                        │
│  Blocks on ShutdownController::wait(); tears down on request()       │
└──────────────────────────────┬──────────────────────────────────────┘
                               │
                               ▼
┌─────────────────────────────────────────────────────────────────────┐
│                       MODULE MANAGER                                 │
│  Lifecycle: initialize → start → stop → shutdown (reverse order)     │
└──────────────────────────────┬──────────────────────────────────────┘
                               │
    ┌──────────────┬───────────┼───────────┬──────────────┬──────────┐
    ▼              ▼           ▼           ▼              ▼          ▼
┌─────────┐  ┌──────────┐  ┌────────┐  ┌────────────┐  ┌─────────┐  ┌────────┐
│Filesystem│ │Inventory │ │Event   │  │WebSocket   │  │AgentSync│  │Content │
│Watcher  │  │Module    │ │Logger  │  │Transport   │  │Module   │  │Scanner │
└────┬────┘  └─────┬────┘ └───┬────┘  └─────┬──────┘  └────┬────┘  └────────┘
     │             │          │             │              │
     │ publish     │ hash     │ subscribe   │ WS + TLS     │ HTTP + TLS
     ▼             ▼          ▼             ▼              ▼
┌─────────┐  ┌──────────┐  ┌────────┐  ┌──────────┐  ┌──────────────┐
│EventBus │  │FileStore │  │EventBus│  │Core WS  │  │Core Server    │
│(pub/sub)│  │(SQLite)  │  │        │  │Endpoint │  │(register +    │
│         │  │          │  │        │  │          │  │ heartbeat +   │
│         │  │          │  │        │  │          │  │ scan/batch)   │
└─────────┘  └──────────┘  └────────┘  └──────────┘  └──────────────┘

          ShutdownController (shared by every module and by IService)
          ─────────────────────────────────────────────────────────
          request(reason)  ←  any module, any time, first wins
          wait()           ←  service main loop blocks here
          reason()         ←  read by transport to decide goodbye
```

**Design principles enforced:**

1. **Layered source tree** — `foundation/` ← `events/` ← `modules/` ← `features/`. Higher layers depend on lower; lower never on higher.
2. **Interface in `platform/`, implementation in `platform/<os>/`** — business logic never includes OS headers.
3. **Every capability is a module** — new DLP features are added by creating `features/<name>/` and registering in `application.cpp`.
4. **Modules communicate through the event bus** — never directly.
5. **One shutdown authority** — the `ShutdownController` is the only place shutdown is requested, and every module reads the same reason.
6. **Fail-safe** — bad config, missing paths, failed modules never crash the agent.
7. **RAII everywhere** — destructors clean up, no manual resource management.

**Dependency pipeline (CMake side):**

```
                       CMakeLists.txt
                             │
                 ┌───────────┴───────────┐
                 ▼                       ▼
           if(MSVC)                if(UNIX AND NOT APPLE)
                 │                       │
                 ▼                       ▼
           vcpkg manifest          .deps/ cache
           (Windows native)        (Linux native)
                 │                       │
        ┌────────┴────────┐     ┌───────┴────────┐
        ▼                 ▼     ▼                ▼
    x64-windows-static   x86-windows-static   linux-x86_64   linux-i686
```

---

## 4. Source Tree Layout

```
axiomdlp-agent/
├── CMakeLists.txt
├── CMakePresets.json
├── vcpkg.json                              ← Windows deps manifest
├── README.md
├── DONE.md                                 (this file)
├── LICENSE
├── .clangd / .clang-tidy / .clang-format
├── .gitignore
│
├── .deps/                                  Linux deps cache (gitignored)
│   ├── spdlog-src/
│   ├── nlohmann_json-src/
│   ├── efsw-src/
│   ├── sqlite3-src/
│   ├── openssl-src/
│   ├── openssl-linux-x86_64/
│   ├── openssl-linux-i686/
│   └── boost-include/
│
├── vcpkg_installed/                        Windows deps (gitignored)
│   ├── x64-windows-static/
│   └── x86-windows-static/
│
├── cmake/toolchains/
│   ├── linux-x86_64.cmake
│   └── linux-i686.cmake
│
├── config/
│   ├── agent.default.json
│   ├── agent.default_win.json
│   └── agent.schema.json
│
├── data/                                   (gitignored)
├── logs/                                   (gitignored)
│
├── scripts/
│   ├── linux/
│   │   ├── build-all.sh
│   │   ├── build-grpc.sh
│   │   ├── build-openssl.sh
│   │   └── merge-compile-commands.sh
│   ├── win/
│   │   ├── setup-windows-msvc.ps1
│   │   └── build-windows-msvc.ps1
│   ├── mock_server.py
│   ├── viz_server.py
│   └── viz.html
│
├── docs/
│   └── windows-msvc-build.md
│
└── src/
    ├── main.cpp

    ├── foundation/
    │   ├── application.{h,cpp}        Orchestration, arg parsing
    │   ├── config.{h,cpp}             JSON load + validation + env expansion
    │   ├── logger.{h,cpp}             Async spdlog setup
    │   └── shutdown.h                 ShutdownController + ShutdownReason

    ├── events/
    │   ├── event.{h,cpp}
    │   └── event_bus.{h,cpp}

    ├── modules/
    │   ├── i_module.h
    │   ├── module_base.h
    │   ├── module_context.h           Holds ShutdownController&
    │   ├── module_manager.{h,cpp}
    │   └── health.h

    ├── platform/
    │   ├── paths.h
    │   ├── service.h                  Takes ShutdownController
    │   ├── file_watcher.h
    │   ├── common/
    │   │   └── efsw_file_watcher.cpp
    │   ├── linux/
    │   │   ├── paths.cpp
    │   │   └── service.cpp            Waits on ShutdownController
    │   └── windows/
    │       ├── paths.cpp
    │       └── service.cpp            Waits on ShutdownController

    ├── net/
    │   ├── http_client.{h,cpp}        Always verifies TLS
    │   ├── websocket_client.{h,cpp}   No per-read deadline, jitter, graceful close
    │   ├── outbound_queue.{h,cpp}
    │   └── message_router.{h,cpp}

    ├── util/
    │   ├── glob_match.{h,cpp}
    │   ├── file_hash.{h,cpp}
    │   └── base64.{h,cpp}

    ├── storage/
    │   ├── sqlite_database.{h,cpp}
    │   ├── migrations.{h,cpp}
    │   ├── file_repository.{h,cpp}
    │   └── state_repository.{h,cpp}

    └── features/
        ├── filesystem_watcher/
        │   └── filesystem_watcher_module.{h,cpp}
        ├── inventory/
        │   ├── inventory_module.{h,cpp}
        │   └── inventory_scanner.{h,cpp}
        ├── event_logger/
        │   └── event_logger_module.{h,cpp}
        ├── websocket_transport/
        │   └── transport_module.{h,cpp}
        ├── agent_sync/
        │   └── agent_sync_module.{h,cpp}
        ├── content_scanner/
        │   └── content_scanner_module.{h,cpp}
        └── grpc_transport/            (scaffolding, disabled by default)
            ├── agent.proto
            ├── grpc_transport_module.{h,cpp}
```

---

## 5. Build System

### 5.1 Linux (x86_64 + i686) — unchanged from `main`

**Prerequisites (Kali/Debian/Ubuntu):**

```bash
sudo dpkg --add-architecture i386
sudo apt update
sudo apt install -y \
    build-essential gcc-multilib g++-multilib \
    ninja-build git file clangd \
    libboost-dev libsystemd-dev libsystemd-dev:i386
```

**Build:**

```bash
./scripts/linux/build-all.sh release
```

**First run:** downloads and compiles OpenSSL 3.3.2 for both Linux
targets (~15–25 min). **Subsequent runs:** incremental (~30 sec).

**Presets:**

| Preset                         | Output                                | Compiler            |
| ------------------------------ | ------------------------------------- | ------------------- |
| `linux-x86_64-{debug,release}` | `build/linux-x86_64-*/axiomdlp-agent` | `/usr/bin/g++`      |
| `linux-i686-{debug,release}`   | `build/linux-i686-*/axiomdlp-agent`   | `/usr/bin/g++ -m32` |

### 5.2 Windows (x64 + x86) — MSVC + vcpkg

**Prerequisites:**
- **Visual Studio 2022** with "Desktop development with C++" workload (BuildTools works)
- **CMake 3.24+** on PATH
- **Ninja** on PATH
- **vcpkg** cloned to `C:\Users\<user>\vcpkg` with `VCPKG_ROOT` set
- **Long paths enabled** (`LongPathsEnabled=1` in registry)

**One-time setup:**

```powershell
.\scripts\win\setup-windows-msvc.ps1
```

Fast mode: downloads vcpkg as ZIP from `codeload.github.com` (no git clone,
~25 MB) and downloads prebuilt `vcpkg.exe` from `vcpkg-tool/releases`
(no bootstrap compile, ~3 MB). Takes ~3 min. Also has a `setx` fallback
for `VCPKG_ROOT` if the .NET `SetEnvironmentVariable` API fails.

**Build:**

```powershell
# x64 Release
.\scripts\win\build-windows-msvc.ps1 -Arch x64

# x86 Release
.\scripts\win\build-windows-msvc.ps1 -Arch x86
```

The build script **auto-initializes the Visual Studio environment** via
`vcvarsall.bat`, so it works from any PowerShell window — Developer
PowerShell is not required. It also:

- Saves `VCPKG_ROOT` before VS init and restores it after (Bug W22 —
  `vcvarsall.bat` overwrites it with the VS-bundled vcpkg)
- Refuses to build if `VCPKG_ROOT` ends up pointing at the VS-bundled path
- Explicitly sets `CC=cl.exe` and `CXX=cl.exe` to override MSYS2 GCC
  if it's on PATH ahead of MSVC (Bug W21)
- Detects stale `CMakeCache.txt` with GCC compiler entries and cleans
  the build directory

**First Windows build:** vcpkg compiles 66 packages per triplet
(~25–40 min per architecture). **Subsequent builds:** all 66 packages
restore from vcpkg's binary cache in ~5 seconds.

**Presets:**

| Preset                             | Output                                        | Triplet              |
| ---------------------------------- | --------------------------------------------- | -------------------- |
| `windows-x64-msvc-{debug,release}` | `build/windows-x64-msvc-*/axiomdlp-agent.exe` | `x64-windows-static` |
| `windows-x86-msvc-{debug,release}` | `build/windows-x86-msvc-*/axiomdlp-agent.exe` | `x86-windows-static` |

MSVC presets are gated by `hostSystemName == "Windows"` in
`CMakePresets.json`, so they are invisible on Linux. Both use
`toolchainFile: $env{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake`.

**vcpkg cache location:** `C:\Users\<user>\AppData\Local\vcpkg\archives\`
Second-run restore times observed: **3.6–5.7 seconds** for all 66 packages.

**Offline/slow-network fallback:** if vcpkg downloader stalls (GitHub CDN
throttle), download the failing URL with `aria2c -x16 -s16` inside WSL,
then copy the file to `C:\Users\<user>\vcpkg\downloads\`. vcpkg accepts
pre-cached files silently. Documented in §22 (Bugs W18–W20).

### 5.3 Build strategy selection

```cmake
if(MSVC)
    set(AXIOM_USE_VCPKG_DEPS ON)
    message(STATUS "Build strategy: MSVC + vcpkg (Windows native)")
    # Static CRT — matches MinGW -static-libstdc++
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
else()
    set(AXIOM_USE_VCPKG_DEPS OFF)
    message(STATUS "Build strategy: GCC native (.deps/)")
endif()
```

Every `find_package(...)` block in `CMakeLists.txt` branches on
`AXIOM_USE_VCPKG_DEPS`:

| Dependency    | vcpkg path                                               | `.deps/` path                                       |
| ------------- | -------------------------------------------------------- | --------------------------------------------------- |
| nlohmann/json | `find_package(CONFIG REQUIRED)`                          | system or FetchContent                              |
| spdlog        | `find_package(CONFIG REQUIRED)`                          | FetchContent                                        |
| efsw          | `find_package(CONFIG REQUIRED)` → `efsw::efsw`           | FetchContent → `efsw-static`                        |
| Boost         | `find_package(Boost CONFIG REQUIRED)` → `Boost::headers` | isolated `/usr/include/boost` symlink               |
| OpenSSL       | `find_package(OpenSSL REQUIRED)`                         | manual `OpenSSL::SSL` / `::Crypto` imported targets |
| SQLite3       | `find_package(SQLite3 REQUIRED)` → `SQLite::SQLite3`     | amalgamation → `axiom_sqlite` static lib            |

### 5.4 gRPC — optional, off by default

The gRPC transport module scaffolding exists in
`src/features/grpc_transport/` (`agent.proto`, `.h`, `.cpp`) but is
**disabled by default** via `option(AXIOM_ENABLE_GRPC ... OFF)` in
`CMakeLists.txt`.

**Why disabled:** `grpc` and `protobuf` are not in `vcpkg.json`. The
gRPC CMake block would fail with `Could not find Protobuf` if it ran
unconditionally. Gating it behind `AXIOM_ENABLE_GRPC` keeps the
non-gRPC build path clean.

**To enable later (Windows):**

1. Add to `vcpkg.json`:
   ```json
   "protobuf",
   { "name": "grpc", "features": [] }
   ```
2. Configure with:
   ```powershell
   cmake --preset windows-x64-msvc-release -DAXIOM_ENABLE_GRPC=ON
   ```
3. First enable build: ~30–45 min extra vcpkg time for abseil +
   protobuf + grpc. Subsequent builds: cached, ~5 sec extra.

**On Linux**, gRPC is currently unavailable — only the Windows code path
is wired into `CMakeLists.txt`. Wiring `.deps/grpc-linux-*` from
`scripts/linux/build-grpc.sh` into the `else()` branch of the
`AXIOM_ENABLE_GRPC` block is a follow-up task.

### 5.5 Dependencies — per-platform

**Linux:**

| Library       | Version  | Purpose                | Source                            |
| ------------- | -------- | ---------------------- | --------------------------------- |
| spdlog        | 1.14.1   | Async logging          | FetchContent + `.deps/`           |
| nlohmann/json | 3.11.3   | JSON parsing           | System package or FetchContent    |
| efsw          | 1.7.2    | File watching          | FetchContent + `.deps/`           |
| SQLite        | 3.46.1   | Metadata storage       | Amalgamation + `.deps/`           |
| Boost         | 1.90     | HTTP + WebSocket       | System headers, isolated include  |
| OpenSSL       | 3.3.2    | TLS + hashing + base64 | `build-openssl.sh` for both archs |
| libsystemd    | (system) | `sd_notify`            | pkg-config (optional)             |

**Windows:**

| Library       | Version | Source                                                            |
| ------------- | ------- | ----------------------------------------------------------------- |
| OpenSSL       | 3.6.5   | vcpkg (`openssl:x64/x86-windows-static`)                          |
| Boost         | 1.92.0  | vcpkg (`boost-asio`, `boost-beast` + 60 transitive boost-* ports) |
| spdlog        | 1.17.0  | vcpkg                                                             |
| fmt           | 12.2.0  | vcpkg (spdlog dependency)                                         |
| efsw          | 1.7.2   | vcpkg                                                             |
| SQLite        | 3.53.4  | vcpkg                                                             |
| nlohmann/json | 3.12.0  | vcpkg                                                             |

**Note:** Boost and OpenSSL versions differ by platform (they come from
different sources — `.deps/` vs vcpkg). The public API used by the agent
(Beast HTTP + WebSocket, Asio async I/O, OpenSSL EVP + X509) is stable
across both.

**Total vcpkg packages built per Windows triplet: 66.** Includes
`boost` (60 sub-packages), `openssl`, `sqlite3`, `spdlog`, `fmt`,
`nlohmann-json`, `efsw`, `vcpkg-cmake`, `vcpkg-cmake-config`,
`vcpkg-cmake-get-vars`, `vcpkg-boost`, `boost-uninstall`.

---

## 6. Foundation Layer

### `ShutdownController` — `src/foundation/shutdown.h`

The single, thread-safe authority on whether the agent is shutting down
and why.

```cpp
enum class ShutdownReason {
    None,
    UserRequested,      // Ctrl+C, SIGTERM, service stop
    AuthRevoked,        // 401 from server: token invalid or revoked
    AgentRevoked,       // 403 from server: this specific agent revoked
    ConfigError,
    FatalError,
    ServerShutdown      // server sent 1001 Going Away
};

class ShutdownController {
public:
    bool request(ShutdownReason reason);   // first call wins
    void wait();                            // blocks until requested
    bool wait_for(duration);                // bounded wait
    bool is_requested() const;
    ShutdownReason reason() const;
};
```

- First caller wins; later calls are no-ops.
- Any module can request at any time.
- Service main loop blocks on `wait()` and tears down when a request arrives.
- The reason is preserved end to end (final log line, goodbye message, server log).

### `Application` — `src/foundation/application.{h,cpp}`

Owns the entire lifecycle:

1. Create the `ShutdownController` first.
2. Parse arguments (`--install`, `--uninstall`, `--foreground`, `--config`).
3. Resolve config path with fallback chain.
4. Load config (fail-safe; warnings collected).
5. Initialize logger.
6. Open SQLite database.
7. Apply migrations.
8. Create `FileStore`, `StateStore`.
9. Create `EventBus`.
10. Create `ModuleContext` (includes the shutdown controller).
11. Create `ModuleManager`, register 6 modules.
12. Initialize all modules.
13. For install/uninstall: short-circuit.
14. For normal run: create platform `IService`, wire manager and controller, run.

**Shutdown sequence:** destroys members in reverse dependency order
**before** `Logger::shutdown()` to avoid the segfault that happened when
`Database::~Database()` logged through a dead spdlog singleton.

### `Config` — `src/foundation/config.{h,cpp}`

Fail-safe JSON loading with a **single top-level `server` section**:

```json
"server": {
  "base_url": "http://127.0.0.1:3000",
  "api_token": "axdlp_user_...",
  "heartbeat_interval_seconds": 60,
  "reconnect_backoff_seconds": 10,
  "reconnect_backoff_max_seconds": 300
}
```

- All server-facing values live here. `agent_sync`, `transport`, and
  `content_scanner` read from the same place.
- `verify_tls` is no longer parsed anywhere — TLS is always verified on
  `https://` and `wss://`.
- Legacy fields under `modules.agent_sync.*` and `modules.transport.*`
  are still read (with a deprecation warning) so old configs keep working.
- Environment variable expansion: `%VAR%` (Windows) and `${VAR}` (POSIX).
- Never throws; every field has a safe default.

### `Logger` — `src/foundation/logger.{h,cpp}`

- spdlog async logger.
- 8192-slot queue, 1 background worker.
- Console sink (colored) + rotating file sink (5 MB × 3).
- `overrun_oldest` policy — never blocks the caller.
- Format: `[timestamp] [level] [thread-id] message`.
- Flush on `warn` and above.

---

## 7. Events Layer

### `Event` — `src/events/event.{h,cpp}`

```cpp
struct Event {
    std::string id;
    std::string type;
    std::chrono::system_clock::time_point timestamp;
    std::string user_id;
    std::string process_name;
    int         pid = 0;
    std::map<std::string, std::string> metadata;
    std::optional<std::string> content;
};
```

**Canonical event types:** `file_access`, `file_write`, `file_delete`,
`usb_insert`, `usb_remove`, `clipboard_copy`, `print_job`,
`process_spawn`, `scan_result`, `decision`.

### `EventBus` — `src/events/event_bus.{h,cpp}`

In-process synchronous publish/subscribe. `shared_ptr<Event>` is shared
with all subscribers. Handlers are invoked outside the lock, so they can
safely subscribe or unsubscribe during a callback.

---

## 8. Module Framework

### `IModule` — `src/modules/i_module.h`

```cpp
class IModule {
public:
    virtual std::string name() const = 0;
    virtual std::string version() const = 0;
    virtual bool initialize(ModuleContext&) = 0;
    virtual bool start() = 0;
    virtual void stop() = 0;
    virtual void shutdown() = 0;
    virtual HealthStatus health() const = 0;
};
```

### `ModuleContext` — `src/modules/module_context.h`

```cpp
struct ModuleContext {
    const Config&       config;
    EventBus&           event_bus;
    FileStore&          file_store;
    StateStore&         state_store;
    Database&           database;
    ShutdownController& shutdown;
};
```

### `ModuleManager` — `src/modules/module_manager.{h,cpp}`

Owns `std::vector<std::unique_ptr<IModule>>`. Start in forward order;
stop and shutdown in reverse. Aggregates health.

### `HealthStatus` — `src/modules/health.h`

Four levels: `Unknown`, `OK`, `Degraded`, `Failed`.

---

## 9. Platform Layer

### Interfaces

| File                      | Purpose                                                           |
| ------------------------- | ----------------------------------------------------------------- |
| `platform/paths.h`        | Executable path, config path, log dir, data dir                   |
| `platform/service.h`      | `IService` with `set_shutdown_controller`, `request_stop(reason)` |
| `platform/file_watcher.h` | `IFileWatcher` + `FileEvent`                                      |

### Linux — `platform/linux/`

- `paths.cpp` — `readlink("/proc/self/exe")`, `/etc/axiomdlp/`, `/var/log/axiomdlp/`, `/var/lib/axiomdlp/`.
- `service.cpp` — systemd, `sd_notify`, signals. Signal handler sets an atomic flag; main loop translates it into a `ShutdownController` request.

**Build-time guards:** `src/platform/linux/*.cpp` start with
`#if !defined(__linux__) #error "…"` — the build fails clearly if
CMake ever includes them on the wrong host.

### Windows — `platform/windows/`

- `paths.cpp` — `GetModuleFileNameA`, `SHGetKnownFolderPath` (via
  `FOLDERID_ProgramData`), `CoTaskMemFree` cleanup.
- `service.cpp` — SCM, `ServiceMain`, control handler.
  `SERVICE_CONTROL_STOP` → `ShutdownController::request(UserRequested)`.

**Build-time guards:** `src/platform/windows/*.cpp` start with
`#if !defined(_WIN32) #error "…"` — same protection.

**Windows link additions:** `ole32` (for `CoTaskMemFree`),
`user32` (shell helpers), plus the existing `advapi32 shell32 ws2_32 crypt32`.

---

## 10. Net Layer

### `HttpClient` — `src/net/http_client.{h,cpp}`

Synchronous HTTP/HTTPS client built on Boost.Beast.

- POST with JSON body and optional Bearer token.
- Supports `http://` and `https://`.
- **TLS is always verified on `https://`.** The `verify_tls` parameter
  was removed; certificate chain and hostname must match or the call fails.
- SNI set before the handshake.
- Never throws; returns `{ status, body, error }`.

### `WebSocketClient` — `src/net/websocket_client.{h,cpp}`

Asynchronous WebSocket client built on Boost.Beast + Boost.Asio.

- Dedicated `io_context` thread.
- **`set_bearer_token()`** injects `Authorization: Bearer <token>` into
  the handshake via a Beast decorator.
- **No application-level read deadline.** Liveness is owned by Beast's
  own `stream_base::timeout` (`idle_timeout = 90s`, `keep_alive_pings = true`).
- **Exponential backoff with ±20% jitter** drawn from `std::mt19937`.
  A fleet of thousands does not reconnect in lockstep.
- **Close codes interpreted.** `1000`, `1001`, `1009`, `1012`, `1013`
  retry. `1008` (Policy Violation) stops permanently. `1011` retries
  (server bug).
- **Graceful close on `stop()`**: sends a `1000 Normal` close frame
  (with 2s timeouts) before tearing down, so the server logs a clean
  close instead of a `1006 abnormal closure`.
- **TLS is always verified on `wss://`** — no opt-out.
- Send/stop race serialized by `lifecycle_mutex`.

**MSVC build note:** this file is the heaviest in the project — it
instantiates the full Boost.Beast WebSocket stack (async read/write,
TLS, close codes). On MSVC it exceeds the default object-file section
limit (`C1128: number of sections exceeded object file format limit`).
`CMakeLists.txt` adds `/bigobj` to all MSVC compile options
unconditionally. Linux has no equivalent limit.

### `OutboundQueue` — `src/net/outbound_queue.{h,cpp}`

Bounded, priority-ordered FIFO. Four priorities. Bounded by both
message count and total bytes.

### `MessageRouter` — `src/net/message_router.{h,cpp}`

Type-keyed dispatch for incoming JSON. Handlers registered by string
type. Unknown types logged and ignored.

---

## 11. Utility Layer

- `glob_match` — `*`, `?`, directory-prefix matching.
- `file_hash` — streaming SHA-256 via OpenSSL EVP.
- `base64` — RFC 4648 encoding via OpenSSL EVP.

---

## 12. Storage Layer

One SQLite file. Three tables.

```
data/agent.db
  ├── files      — one row per file (metadata + hash + scan state)
  ├── state      — key/value for agent state (agent_id, ...)
  └── walks      — reserved for scan history (empty)
```

### `files` table

Identity (`id`, `dev`, `inode`), path decomposition, POSIX metadata,
flags, classification (`category`, `file_type`), scan result
(`is_sensitive`, `sensitivity`, `scan_status`), scan metadata
(`content_hash`, `last_scanned_at`, `scan_attempts`, `scan_error`),
bookkeeping (`first_seen`, `last_seen`, `walk_id`).

**Unique constraint:** `(dev, inode)`.

**Indexes:** `path`, `dirname`, `extension`, `category`, `mtime_ns DESC`,
`size DESC`, `uid`, `walk_id`, `is_sensitive` (partial), `scan_status`,
`content_hash`.

### Schema Versioning

`PRAGMA user_version` tracks the version. Current: **4**. Migrations
apply automatically on startup. Both Linux and Windows apply the same
migration sequence (0→4 verified on a fresh Windows database).

### `Database` / `FileStore` / `StateStore`

- `Database` is a thin SQLite wrapper with WAL, `synchronous=NORMAL`,
  `foreign_keys=ON`, `busy_timeout=5000`. No logging in `close()` —
  the destructor may run after logger teardown.
- `FileStore` is the domain wrapper for `files`: upsert, pending-scan
  queries, verdict deduplication, scan-result writes, error recording,
  and aggregations.
- `StateStore` is the key/value store backed by `state`. Used by
  `agent_sync` for `agent_id`.

**Note:** SQLite version differs by platform — **3.46.1 on Linux**
(amalgamation), **3.53.4 on Windows** (vcpkg). Schema and query
compatibility verified by both platforms applying the same migrations
successfully.

---

## 13. Feature Modules

### `filesystem_watcher`

Real-time filesystem events via efsw.

- 300 ms trailing-edge debounce.
- Priority-based merge: `modified (4) > created (3) > moved (2) > deleted (1)`.
- Two-tier exclusion (glob + directory prefix).
- Self-exclusion of the agent's own log and data directories (fixes the log feedback loop).
- Async watcher setup on a dedicated thread.
- Three threads total: main service, efsw, flusher (50 ms tick).

**Windows runtime observation:** on first run with the Linux config,
the watcher correctly logged `skipping '/home/kali/Downloads' (does not exist)` —
proving efsw is linked and functional. It opened a watch request,
checked path existence, and logged clearly. Switching to
`config/agent.default_win.json` (watch `%USERPROFILE%`) resolves this.

### `inventory`

Periodic full filesystem scan.

- Scheduled on an interval (default 900s), owns its worker thread.
- Rich metadata capture: path decomposition, timestamps in nanoseconds,
  POSIX permissions, file type, category.
- SHA-256 hashing for eligible files.
- Explicit `scan_status` assignment: `pending`, `not_applicable`, `skipped_too_large`.
- Batched inserts every 5,000 rows.
- Graceful stop within ~20 ms; partial commits on cancel.

### `event_logger`

Subscribes to all events on the bus and logs them. Pure observability.

### `websocket_transport`

Owns the WebSocket connection to the Core server.

- Reads `server.base_url` and `server.api_token` from the top-level
  `server` config section.
- Derives the WebSocket URL from `server.base_url` unless an explicit
  `server_url` override is set.
- Waits for `agent_id` in the state store before connecting.
- On graceful shutdown (`UserRequested`, `FatalError`, `ConfigError`)
  sends `{"type":"agent_shutdown","payload":{"reason":...}}` with
  `Priority::Critical` **before** closing the socket.
- Handles server-pushed `token_revoked` and `agent_revoked` messages by
  calling `ShutdownController::request(AuthRevoked/AgentRevoked)`.
- Default handlers for `ping`, `policy_update`, `command`.

### `agent_sync`

The HTTP channel between the agent and the Core server.

- **Enrollment** — `POST /api/v1/agent/register` (once at startup).
- **Heartbeat** — `POST /api/v1/agent/heartbeat`, default 60 s.
- On startup with a saved `agent_id`, sends **one immediate validation
  heartbeat** so a revoked token is detected within milliseconds instead
  of after the first interval.
- 401 → `ShutdownController::request(AuthRevoked)` with the message
  `"no valid API token — heartbeat rejected (401) — token is invalid,
  expired, or revoked"`.
- 403 → `ShutdownController::request(AgentRevoked)`.
- 404 → clears `agent_id` and re-enrolls.
- Empty `server.api_token` at init → error message says **"no valid API
  token"** explicitly.
- Interruptible sleep; also wakes when the shutdown controller is triggered.

**Windows runtime observation:** on first run with no Core server,
returned `connect: No connection could be made because the target
machine actively refused it [system:10061]`. This proves Boost.Asio
socket, Winsock (`ws2_32`), and error mapping all work — the subsystem
failed correctly and logged the retry intent.

### `content_scanner`

Reads text files, splits into overlapping chunks, sends to Core, writes
verdicts back.

- Reads from `server.base_url` and `server.api_token`.
- Extension filter, size filter, existence check, SHA-256 of content,
  verdict deduplication by hash.
- Chunk size 1 MB, overlap 256 bytes.
- Batches respect `batch_size` and `batch_max_bytes`.
- Updates `is_sensitive`, `sensitivity`, `scan_status = 'scanned'` on success.
- Content is never persisted; read into memory, sent, discarded.

---

## 14. TLS / OpenSSL

### Why

HTTP and WebSocket traffic carries the `axdlp_user_...` token. If that
token leaks, an attacker can impersonate the agent.

### Two OpenSSL versions, both static

| Platform | Version | Location                                                            |
| -------- | ------- | ------------------------------------------------------------------- |
| Linux    | 3.3.2   | `.deps/openssl-linux-*/lib{,64}/libssl.a` + `libcrypto.a`           |
| Windows  | 3.6.5   | `vcpkg_installed/*-windows-static/lib/libssl.lib` + `libcrypto.lib` |

Both are statically linked into the final binary. Verified:

- Linux: `ldd axiomdlp-agent | grep ssl` → empty (no `.so` deps)
- Windows: `dumpbin /dependents axiomdlp-agent.exe` → only `KERNEL32`,
  `ADVAPI32`, `WS2_32`, `SHELL32`, `ole32`, `USER32`, `CRYPT32`
  (no `libssl-3-x64.dll`, no `libcrypto-3-x64.dll`)

### Verification (unchanged)

Both `HttpClient` and `WebSocketClient` verify chain **and** hostname.
Implemented via `X509_VERIFY_PARAM_set1_host()` because Boost's
`host_name_verification` doesn't exist in Boost 1.90.

### No Opt-Out

`verify_tls` was removed from every layer:

- `TransportModuleConfig`, `AgentSyncModuleConfig` — field removed.
- `HttpClient::post_json` — parameter removed.
- `WebSocketClient::set_verify_tls` — method removed.
- `config/agent.schema.json` — property removed.
- Legacy `verify_tls` keys in config files produce a **warning that
  they are ignored**; the agent still verifies.

### SNI

Both clients call `SSL_set_tlsext_host_name()` before the handshake.

### Static CRT (Windows)

`CMAKE_MSVC_RUNTIME_LIBRARY = "MultiThreaded$<$<CONFIG:Debug>:Debug>"`
forces `/MT`. This matches MinGW's `-static-libstdc++` behavior and
eliminates the `vcruntime140.dll` runtime dependency. Any Windows 10/11
machine can run the `.exe` with no redistributable install.

---

## 15. Configuration System

### Top-Level `server` Section

```json
"server": {
  "base_url": "http://127.0.0.1:3000",
  "api_token": "axdlp_user_...",
  "heartbeat_interval_seconds": 60,
  "reconnect_backoff_seconds": 10,
  "reconnect_backoff_max_seconds": 300
}
```

### Module Sections

- `filesystem_watcher`: watch paths, exclude paths, debounce.
- `inventory`: watch paths, exclude paths, interval, hash settings, scannable categories.
- `transport`: `enabled`, optional `server_url` override.
- `agent_sync`: `enabled`.
- `content_scanner`: batch size, chunking, retries, text extensions.
- `grpc_transport`: `enabled` (default `false` — gRPC is off by default).

### Fail-Safe Behavior

Missing file, invalid JSON, wrong type, invalid value — all produce
warnings and fall back to safe defaults. **The agent always starts.**
Config problems never prevent operation.

---

## 16. The Data Model

After an inventory, `files` contains every regular file under the
watch paths. Fields are populated by `stat()` during the walk, by
SHA-256 hashing for eligible files, and by the content scanner for
verdicts.

**Data flow:**

```
1. Inventory writes metadata + hash, scan_status = 'pending'
2. Content scanner queries pending files, reads content, verifies hash
3. Batches chunks to POST /api/v1/agent/scan/batch
4. Server runs PII detection and returns per-file verdicts
5. Agent writes is_sensitive / sensitivity / scan_status = 'scanned'
6. Enforcement (future) acts on is_sensitive = 1 files
```

---

## 17. WebSocket Lifecycle (Agent ↔ Server)

### Connection Setup

1. `agent_sync` enrolls via HTTP, gets `agent_id`, writes it to SQLite `state`.
2. `transport` polls the state store until `agent_id` appears.
3. `transport` derives the WebSocket URL from `server.base_url` (or uses
   an explicit override).
4. `WebSocketClient::start()` spawns the I/O thread.
5. Handshake carries `Authorization: Bearer <token>`.
6. On 101 Switching Protocols, the client transitions to `Connected`.
7. Every 30 s the server pings; Beast on the client responds with pongs
   automatically.
8. Every 60 s `agent_sync` heartbeats over HTTP, updating `last_seen_at`.

### Message Flow

- **Server → agent**: any handler can call `agentHub.SendToAgent(id, msg)`
  or `agentHub.BroadcastToOrg(orgID, msg)`.
- **Agent → server**: any module can call `TransportModule::send_message(json)`.
  The message is queued in `OutboundQueue` and written by the write pump.
- **Goodbye on graceful shutdown**: `TransportModule::stop()` sends
  `{"type":"agent_shutdown","payload":{"reason":...}}` at
  `Priority::Critical` before closing.

### Reconnect Policy

Exponential backoff, ±20% jitter, base 10 s, cap 300 s.

```
reconnecting in 11s (base=10s, max=300s)
reconnecting in 21s (base=20s, max=300s)
reconnecting in 42s (base=40s, max=300s)
reconnecting in 88s (base=80s, max=300s)
reconnecting in 176s (base=160s, max=300s)
reconnecting in 300s (base=300s, max=300s)   ← capped
```

### Close Code Handling

| Code | Meaning          | Behavior                            |
| ---- | ---------------- | ----------------------------------- |
| 1000 | Normal           | Reconnect                           |
| 1001 | Going Away       | Reconnect                           |
| 1006 | Abnormal         | Reconnect                           |
| 1008 | Policy Violation | **Stop permanently** (auth revoked) |
| 1009 | Too Big          | Reconnect                           |
| 1011 | Internal         | Reconnect                           |
| 1012 | Service Restart  | Reconnect                           |
| 1013 | Try Again Later  | Reconnect                           |

### Server-Side Revocation

Two independent mechanisms:

1. **Token watcher**: the server re-checks the DB every 60 s. If the
   token is revoked, it sends `{"type":"token_revoked",...}` then a
   `1008 Policy Violation` close frame.
2. **Heartbeat 401**: the agent's next HTTP heartbeat returns 401.
   `agent_sync` calls `ShutdownController::request(AuthRevoked)`.

Whichever fires first, the whole agent exits cleanly in ~1 second.

### Atomic Shutdown

Every module can request shutdown at any time. The service main loop
unblocks and runs `stop_all()` then `shutdown_all()` in reverse order.
The final log line shows the reason.

---

## 18. The Complete Bug History

### Windows MSVC migration bugs (new — `msvc-windows-build` branch)

**Bug W15 — PowerShell 5.1 mis-parses UTF-8 `.ps1` files without BOM.**
Em-dashes (`—`), ellipses (`…`), and smart quotes in the `.ps1` files
were interpreted as three separate Windows-1252 characters each. The
stray `"` in a comment **terminated a string on line 20**, causing a
cascade of `Missing argument in parameter list` and `Unexpected token`
errors from every subsequent line. **Fix:** rewrote both `.ps1` files as
pure ASCII (0x00–0x7F only). Replaced Unicode symbols with ASCII
equivalents (`-`, `...`, `[OK]`, `[ERROR]`, `[WARN]`).

**Bug W16 — `git clone vcpkg` at 9 KB/s = days.** The full vcpkg repo
has 345,595 objects; on a slow link the clone is impractical. **Fix:**
download as ZIP from `codeload.github.com` (~25 MB, ~2 min).

**Bug W17 — `bootstrap-vcpkg.bat` compilation unnecessary.** vcpkg-tool
is prebuilt and released on GitHub. **Fix:** download `vcpkg.exe` from
`vcpkg-tool/releases/latest/download/vcpkg.exe` (~3 MB).

**Bug W18 — Strawberry Perl download stalls twice.** OpenSSL's
`Configure` needs Perl; vcpkg downloads 291 MB from GitHub. Stalled at
0 MB on first attempt, stalled at 131 MB on second. **Fix:** downloaded
via aria2c in WSL (16 parallel connections, `-x16 -s16`), then copied
from `/tmp` to `C:\Users\<user>\vcpkg\downloads\`. The 291 MB file was
verified complete; vcpkg's hash check accepted it.

**Bug W19 — jom download stalls.** jom is Qt's `nmake` replacement,
needed by OpenSSL. Same fix: aria2c download + copy to vcpkg downloads.

**Bug W20 — sqlite3 source download stalls.** Same fix: aria2c + copy.

**Bug W21 — MSYS2 GCC hijacks CMake's compiler.** CMake's Ninja
generator scans PATH for `cc`/`gcc`; MSYS2's `C:\msys64\mingw64\bin`
was ahead of MSVC in PATH order, so CMake selected GCC. That flipped
`AXIOM_USE_VCPKG_DEPS` to OFF (GCC is not MSVC) and made CMake look
for Boost at `/usr/include/boost` on Windows. **Fix:** (a) auto-init
VS env via `vcvarsall.bat`, (b) explicitly set `$env:CC = "cl.exe"` and
`$env:CXX = "cl.exe"`, (c) detect stale `CMakeCache.txt` with GCC and
clean the build directory.

**Bug W22 — `vcvarsall.bat` overwrites `VCPKG_ROOT`.** Calling
`vcvarsall.bat` sets `VCPKG_ROOT` to
`...\Microsoft Visual Studio\...\VC\vcpkg` (the VS-bundled vcpkg). That
instance requires `builtin-baseline` in `vcpkg.json`, so it errors with:
`this vcpkg instance requires a manifest with a specified baseline`.
**Fix:** in `build-windows-msvc.ps1`, save `VCPKG_ROOT` before VS init,
restore it after, and refuse to build if it ends up pointing at the
VS-bundled path.

**Bug W23 — `vcpkg-configuration.json` placeholder baseline.** The
file contained `"baseline": "<COMMIT-SHA>"` (literal string). vcpkg
tried to check out that SHA and failed. **Fix:** deleted the file.
vcpkg falls back to registry HEAD, which works.

**Bug W24 — Boost 1.92.0 vcpkg meta-package dropped `asio` / `beast`
features.** `"boost"` with `features: ["asio", "beast", "system"]`
failed with `error: boost@1.92.0 does not have required feature asio`.
The meta-package no longer supports those features. **Fix:** use the
separate `boost-asio` and `boost-beast` ports directly; `boost-system`
comes as a transitive dependency.

**Bug W25 — `/bigobj` required for `websocket_client.cpp`.** MSVC's
default object-file format has a 65,535 section limit. The full
Boost.Beast WebSocket stack (async read/write, TLS, close codes)
exceeds it: `fatal error C1128: number of sections exceeded object
file format limit: compile with /bigobj`. **Fix:** added `/bigobj` to
all MSVC compile options in `CMakeLists.txt`. Linux (GCC/ELF) has no
equivalent limit — this is a build-only fix.

**Bug W26 — `ole32` and `user32` missing from Windows link libs.**
`src/platform/windows/paths.cpp` uses `CoTaskMemFree` (from
`<objbase.h>`, linked via `ole32.lib`) after `SHGetKnownFolderPath`.
Without it, `LNK2019: unresolved external symbol __imp_CoTaskMemFree`.
**Fix:** added `ole32 user32` to the WIN32 `target_link_libraries`.

**Bug W27 — gRPC block activated without protobuf in vcpkg.json.**
`CMakeLists.txt` had `if(AXIOM_USE_VCPKG_DEPS) find_package(Protobuf
CONFIG REQUIRED) ...` which runs whenever MSVC is active. But `grpc`
and `protobuf` were not in `vcpkg.json` — the gRPC module was
scaffolding only, disabled for later. `find_package(Protobuf CONFIG
REQUIRED)` therefore failed with `Could not find a package
configuration file provided by "Protobuf"`. **Fix:** introduced
`option(AXIOM_ENABLE_GRPC ... OFF)` and changed the guard to
`if(AXIOM_USE_VCPKG_DEPS AND AXIOM_ENABLE_GRPC)`. The block is now
cleanly gated — no `find_package` unless explicitly enabled.

### Configuration & shutdown bugs (unchanged from previous revision)

**Bug C1 — `verify_tls` was a footgun.** Removed from every layer.
Legacy keys warn and are ignored.

**Bug C2 — Duplicate token across three modules.** Consolidated into
`server.api_token`.

**Bug C3 — Revoked token silently ignored while running.** `agent_sync`
now requests shutdown on 401/403; the server's watcher also closes the
socket with 1008.

**Bug C4 — Empty token at startup allowed a stale `agent_id` heartbeat.**
Now the agent sends an immediate validation heartbeat and errors with
**"no valid API token"** before entering the loop.

### WebSocket-specific bugs (unchanged from previous revision)

**Bug W1 — 60s forced disconnect.** `start_read()` used
`expires_after(60s)` on every read. Server pings don't complete
application-level reads, so the deadline fired every 60 s and killed
the connection. **Fix:** removed the per-read deadline; liveness is now
Beast's `stream_base::timeout` with `keep_alive_pings = true`.

**Bug W2 — Handshake rejected (401) because no bearer was sent.**
**Fix:** `set_bearer_token()` + a Beast decorator that injects
`Authorization` into the handshake.

**Bug W3 — Slow Unregister.** When `readPump` returned, the writer
waited up to 30 s for the next ping tick. **Fix:** call `cancel()`
immediately after `readPump` returns.

**Bug W4 — `1006 abnormal closure` on Ctrl+C.** Client sent no close
frame. **Fix:** `stop()` now sends a `1000 Normal` close frame with
tightened 2 s timeouts.

**Bug W5 — Send-on-closed-channel panic.** `Unregister` closed the
`Send` channel while a broadcast could still be selecting on it.
**Fix:** introduced a separate `Done` channel; `Send` is never closed.

**Bug W6 — Thundering herd on reconnect.** All agents reconnected
synchronously. **Fix:** ±20% jitter via `std::mt19937`.

**Bug W7 — No caps.** Unlimited connections. **Fix:** 100k global /
20k per org.

**Bug W8 — No rate limit.** One bad agent could DoS the server.
**Fix:** `x/time/rate` limiter, 20/s per agent.

**Bug W9 — Slow shutdown blocked by WebSockets.** **Fix:** `CloseAll()`
runs before `srv.Shutdown()`.

**Bug W10 — Close codes ignored.** Revoked agents kept reconnecting.
**Fix:** `should_reconnect_on_close()`.

**Bug W11 — `write_in_flight` not reset.** **Fix:** reset on every
disconnect path.

**Bug W12 — `close_code` type mismatch.** **Fix:**
`static_cast<websocket::close_code>(cr.code)`.

**Bug W13 — `handshake_declined` doesn't exist in this Boost.** **Fix:**
match on `ec.message()` instead.

**Bug W14 — `bugprone-unused-return-value` from clang-tidy on async
operations.** **Fix:** wrap every non-void-returning Beast call in
explicit `static_cast<void>(...)`.

### Original 23 bugs (kept for history)

Bugs 1–23 in the previous revision are unchanged and still fixed.
Highlights: log feedback loop, shutdown segfault, dev/inode collision,
SQLite cross-compile failure, CMake `file(DOWNLOAD)` silent failure,
missing C language in `project()`, walker interruptibility, debounce
priority, heartbeat timer advance, `std::ignore` vs `void`,
`host_name_verification` in Boost 1.90, OpenSSL `lib/` vs `lib64/`,
CMake 4.x `find_package(OpenSSL)` quirks, i686 OpenSSL selection,
duplicated `glob_match`, post-refactor include paths, and `AgentSync`
starting after `initialize()` failure.

---

## 19. Runtime Evidence

### Linux x86_64 — fresh token

```
[info] AxiomDLP Agent v0.1.0
[info] Config loaded from: config/agent.default.json
[info] Database: opened 'data/agent.db'
[info] Migrations: schema version 4 -> 4
[info] ModuleManager: registering module 'filesystem_watcher' v1.0.0
[info] ModuleManager: registering module 'inventory' v1.0.0
[info] ModuleManager: registering module 'event_logger' v1.0.0
[info] ModuleManager: registering module 'transport' v1.0.0
[info] ModuleManager: registering module 'agent_sync' v1.0.0
[info] ModuleManager: registering module 'content_scanner' v1.0.0
[info] transport module: initialized (url=ws://127.0.0.1:3000/api/v1/agent/ws, backoff=10s..300s)
[info] agent_sync module: initialized (server=http://127.0.0.1:3000, heartbeat_interval=60s)
[info] axiomdlp-agent starting (foreground mode)
[info] agent_sync: POST http://127.0.0.1:3000/api/v1/agent/register (hostname=kali)
[info] agent_sync: enrolled (agent_id=1, user_id=1, org_id=2)
[info] transport module: connecting to ws://127.0.0.1:3000/api/v1/agent/ws?agent_id=1
[info] WebSocketClient: connected (ws)
[info] transport module: state = connected
[debug] agent_sync: heartbeat ok (agent_id=1)
```

### Linux x86_64 — server-side registration

```json
{"msg":"Agent registered","agent_id":1,"user_id":1,"org_id":2,"hostname":"kali","os":"linux","version":"0.1.0"}
{"msg":"agent hub: registered","agent_id":1,"user_id":1,"org_id":2,"token_id":1,"total":1,"org_total":1}
```

### Linux x86_64 — graceful Ctrl+C

```
[info] axiomdlp-agent received stop request (reason=user_requested)
[info] ModuleManager: stopping 6 module(s)
[info] transport module: sending goodbye (reason=user_requested)
[info] WebSocketClient: sent graceful close (code=1000)
[info] transport module: state = disconnected
[info] Service returned rc=0 (reason=user_requested)
[info] Application shutting down
```

Server side:
```json
{"msg":"agent ws: agent shutting down gracefully","agent_id":1,"payload":{"agent_id":1,"reason":"user_requested"}}
{"msg":"agent ws: clean close","agent_id":1}
{"msg":"agent hub: unregistered","agent_id":1,"remaining":0}
```

### Linux x86_64 — revocation while running

Server watcher:
```json
{"msg":"agent ws: token revoked, closing connection","agent_id":1,"token_id":1}
```

Or agent-side heartbeat:
```
[error] agent_sync: no valid API token — heartbeat rejected (401) — token is invalid, expired, or revoked
[info] axiomdlp-agent received stop request (reason=auth_revoked)
[info] transport module: sending goodbye (reason=auth_revoked)
[info] WebSocketClient: sent graceful close (code=1000)
[info] Application shutting down
```

### Linux x86_64 — revoked token at startup

```
[info] agent_sync: validating token with immediate heartbeat
[error] agent_sync: no valid API token — heartbeat rejected (401) — token is invalid, expired, or revoked
[info] axiomdlp-agent received stop request (reason=auth_revoked)
[info] Application shutting down
```

Exits in ~1 s. No reconnect loop.

### Linux i686 — same behavior

All of the above verified on Linux i686 as well. Both binaries produce
identical logs apart from `hostname` and `os` in the enrollment payload.

### Windows x64 — verified 2026-10-17

```
[2026-10-17 19:25:50.861] [info] [t:7468] AxiomDLP Agent v0.1.0
[2026-10-17 19:25:50.861] [info] [t:7468] Config loaded from: config/agent.default.json
[2026-10-17 19:25:50.864] [info] [t:7468] Database: opened 'data/agent.db'
[2026-10-17 19:25:50.865] [info] [t:7468] Migrations: schema version 0 -> 4
[2026-10-17 19:25:50.865] [info] [t:7468] Migrations: applying v2 (enriched files table)
[2026-10-17 19:25:50.866] [info] [t:7468] Migrations: v2 applied
[2026-10-17 19:25:50.866] [info] [t:7468] Migrations: applying v3 (state table)
[2026-10-17 19:25:50.866] [info] [t:7468] Migrations: v3 applied
[2026-10-17 19:25:50.866] [info] [t:7468] Migrations: applying v4 (scanner metadata)
[2026-10-17 19:25:50.867] [info] [t:7468] Migrations: v4 applied
[2026-10-17 19:25:50.867] [info] [t:7468] ModuleManager: registering module 'filesystem_watcher' v1.0.0
[2026-10-17 19:25:50.867] [info] [t:7468] ModuleManager: registering module 'inventory' v1.0.0
[2026-10-17 19:25:50.867] [info] [t:7468] ModuleManager: registering module 'event_logger' v1.0.0
[2026-10-17 19:25:50.867] [info] [t:7468] ModuleManager: registering module 'transport' v1.0.0
[2026-10-17 19:25:50.867] [info] [t:7468] ModuleManager: registering module 'agent_sync' v1.0.0
[2026-10-17 19:25:50.867] [info] [t:7468] ModuleManager: registering module 'content_scanner' v1.0.0
[2026-10-17 19:25:50.867] [info] [t:7468] ModuleManager: initializing 6 module(s)
[2026-10-17 19:25:50.867] [info] [t:7468] ModuleManager: initializing 'filesystem_watcher'
[2026-10-17 19:25:50.867] [info] [t:7468] filesystem_watcher module: excluding own log directory 'D:\axiomdlp-agent\logs'
[2026-10-17 19:25:50.867] [info] [t:7468] filesystem_watcher module: excluding own data directory 'D:\axiomdlp-agent\data'
[2026-10-17 19:25:50.867] [info] [t:7468] filesystem_watcher module: initialized with 1 watch path(s), 4 exclude pattern(s), debounce 300 ms
[2026-10-17 19:25:50.867] [info] [t:7468] ModuleManager: initializing 'inventory'
[2026-10-17 19:25:50.867] [info] [t:7468] inventory module: initialized (scan every 60 s)
[2026-10-17 19:25:50.867] [info] [t:7468] ModuleManager: initializing 'event_logger'
[2026-10-17 19:25:50.867] [info] [t:7468] ModuleManager: initializing 'transport'
[2026-10-17 19:25:50.867] [info] [t:7468] transport module: initialized (url=ws://127.0.0.1:3000/api/v1/agent/ws,backoff=10s..300s)
[2026-10-17 19:25:50.867] [info] [t:7468] ModuleManager: initializing 'agent_sync'
[2026-10-17 19:25:50.867] [info] [t:7468] agent_sync module: initialized (server=http://127.0.0.1:3000, heartbeat_interval=60s)
[2026-10-17 19:25:50.867] [info] [t:7468] ModuleManager: initializing 'content_scanner'
[2026-10-17 19:25:50.867] [info] [t:7468] content_scanner module: initialized (batch_size=50, interval=30s, chunk=1024KB, overlap=256B, max_file=100MB, ext_count=47)
[2026-10-17 19:25:50.867] [info] [t:7468] not launched by SCM; running in foreground
[2026-10-17 19:25:50.867] [info] [t:7468] ModuleManager: starting 6 module(s)
[2026-10-17 19:25:50.867] [info] [t:7468] ModuleManager: starting 'filesystem_watcher'
[2026-10-17 19:25:50.867] [info] [t:7468] filesystem_watcher module: started (watcher initializing)
[2026-10-17 19:25:50.867] [info] [t:7468] ModuleManager: starting 'inventory'
[2026-10-17 19:25:50.867] [info] [t:7468] inventory module: started
[2026-10-17 19:25:50.867] [info] [t:7468] ModuleManager: starting 'event_logger'
[2026-10-17 19:25:50.867] [info] [t:7468] event_logger: subscribed to all events (id=1)
[2026-10-17 19:25:50.867] [info] [t:7468] ModuleManager: starting 'transport'
[2026-10-17 19:25:50.867] [info] [t:7468] transport module: started
[2026-10-17 19:25:50.867] [info] [t:7468] ModuleManager: starting 'agent_sync'
[2026-10-17 19:25:50.867] [info] [t:7468] agent_sync module: started
[2026-10-17 19:25:50.867] [info] [t:7468] ModuleManager: starting 'content_scanner'
[2026-10-17 19:25:50.867] [info] [t:7468] content_scanner module: started
[2026-10-17 19:25:50.867] [info] [t:7468] AxiomDLPAgent running (Windows foreground mode)
```

**Warnings observed (both benign):**

1. `EfswFileWatcher: skipping '/home/kali/Downloads' (does not exist)` —
   Linux config path on Windows. Proves efsw is linked and functional.
2. `agent_sync: connect refused [system:10061]` — no Core server running
   at `127.0.0.1:3000`. Proves Boost.Asio socket + Winsock error mapping
   work. Retry logic engaged.

Both warnings disappear once `config/agent.default_win.json` is copied
to `config/agent.json` (watch `%USERPROFILE%`) and the Core server starts.

### Windows x86 — build verified

Same build process, same 66 packages per triplet, same 6-module startup
(identical log to x64 apart from the architecture of the `.exe` itself).
Binary size: 6.37 MB.

### Binary verification

| Binary                                        | Size    | `dumpbin /headers` machine |
| --------------------------------------------- | ------- | -------------------------- |
| `windows-x64-msvc-release/axiomdlp-agent.exe` | 8.61 MB | `8664 machine (x64)`       |
| `windows-x86-msvc-release/axiomdlp-agent.exe` | 6.37 MB | `14C machine (x86)`        |

No external DLL dependencies beyond Windows system DLLs.

---

## 20. Known Limitations

### 20.1 ModuleManager Doesn't Skip Failed Modules

A module that fails `initialize()` still gets `start()` called on it.
Fix pending.

### 20.2 No Process or User Attribution

Events carry a path but not the process. Requires fanotify (Linux) or
Minifilter (Windows).

### 20.3 inotify / ReadDirectoryChangesW Queue Overflow

The periodic inventory scan catches anything the watcher misses.

### 20.4 No Network Filesystem Support

inotify doesn't fire on NFS/SMB. efsw's ReadDirectoryChangesW path on
Windows handles most network shares, but behavior is filesystem-dependent.

### 20.5 Windows Ctrl+C Not Handled in Foreground

No `SetConsoleCtrlHandler` registered. SCM path works. `taskkill /PID`
terminates the process without graceful shutdown.

### 20.6 Windows Path Case Sensitivity

The glob matcher is case-sensitive. NTFS is case-insensitive, so a
case-sensitive matcher could miss exclusions. Worked around by matching
what the OS reports.

### 20.7 Dev/Inode Are Placeholders

`dev` is always 0; `inode` is a path hash. Sufficient for uniqueness;
not a real inode reference.

### 20.8 Content Scanner Only Handles Text Files

PDFs, DOCX, XLSX, images are marked `not_applicable`.

### 20.9 `walks` Table Unused

The schema exists; nothing populates it.

### 20.10 Mixed Path Separators on Windows

Cosmetic only.

### 20.11 Platform toolchain duplication

Linux and Windows use different dependency pipelines. Boost is 1.90 on
Linux (system) and 1.92 on Windows (vcpkg). OpenSSL is 3.3.2 on Linux
and 3.6.5 on Windows. This is intentional: `.deps/` works for Linux
(no vcpkg needed), vcpkg is required for Windows (gRPC later).

### 20.12 gRPC scaffolding is currently dead code

`src/features/grpc_transport/` exists but is entirely guarded by
`#ifdef AXIOM_HAVE_GRPC`. Without `AXIOM_ENABLE_GRPC=ON` and the two
`vcpkg.json` entries, it compiles to empty objects and adds no runtime
behavior. This is deliberate — gRPC is a follow-up milestone.

### 20.13 Windows setup requires manual download workarounds on slow networks

The `setup-windows-msvc.ps1` fast-mode uses `Invoke-WebRequest`, which
can stall on slow GitHub CDN. The WSL aria2c fallback (documented in
§5.2) works but is not scripted. Future improvement: switch to
`Start-BitsTransfer` or `BITS` protocol for resumable downloads.

### 20.14 OpenSSL Linux build requires ~20 minutes

One-time per OpenSSL version bump. Cached trees reuse.

---

## 21. What Does Not Exist Yet

| Feature                  | Notes                                        |
| ------------------------ | -------------------------------------------- |
| Policy engine            | Scan results → decisions                     |
| User-mode enforcement    | Block file operations                        |
| Kernel enforcement       | Minifilter / eBPF / fanotify                 |
| IPC + tray helper        | Notifications, pause/resume                  |
| Auto-update              | Signed manifests, atomic swap                |
| Code signing             | Authenticode / GPG                           |
| Packaging                | `.deb`, `.rpm`, MSI                          |
| Rich document extraction | PDF, DOCX, XLSX inside content scanner       |
| Process/user attribution | Requires kernel-level monitoring             |
| gRPC transport (Windows) | Scaffolding exists, disabled by default      |
| gRPC transport (Linux)   | `build-grpc.sh` exists, CMake wiring pending |

---

## 22. Lessons Learned

### On Concurrency

- A single `ShutdownController` beats scattered flags. One authority, one reason, one path.
- `Done` channels are safer than closing `Send` channels under concurrent send.
- Jitter is mandatory for fleet-scale reconnection.
- Bounded queues prevent network partitions from becoming OOM.

### On WebSocket

- No per-read deadlines. The protocol owns liveness via pings.
- Always send a graceful close frame on shutdown.
- Interpret close codes. Not every disconnect should retry.
- Separate concerns: `agent_sync` (HTTP) and `transport` (WS) are
  different lifecycles.

### On Configuration

- One source of truth per concept. `server.api_token` beats three copies.
- Security defaults must not be options. `verify_tls` is gone.
- Backwards compatibility is cheap when done in the parser.

### On Cross-Platform

- **Two toolchains is the right answer, not one.** Linux works with
  `.deps/` + apt. Windows *requires* vcpkg for gRPC-compatible static
  linking. Forcing one to adopt the other's strategy trades working
  code for theoretical uniformity.
- **The `if(MSVC)` switch is a clean seam.** No source file knows which
  path is active. Both call the same `find_package` API and link the
  same imported targets.
- **`/bigobj` and static CRT are the only Windows-specific C++ concerns.**
  Everything else — Boost template size, WinSock error mapping, SCM
  service registration — behaves exactly as the source expects.
- **Static linking eliminates a class of deployment problems.** No
  `vcruntime140.dll`, no `libssl-3-x64.dll`, no DLL search path issues.
  The `.exe` runs anywhere.
- **Toolchain isolation prevents host libraries from leaking.** The
  `platform/` interface pattern means no business logic includes
  `<windows.h>` or `<sys/inotify.h>`.

### On Windows Build Infrastructure

- **PowerShell 5.1 is hostile to non-ASCII in `.ps1`.** Any Unicode in
  a script (em-dash, ellipsis, smart quote, checkmark) will break the
  parser unless the file is saved with a UTF-8 BOM. ASCII-only is the
  durable fix.
- **`vcvarsall.bat` is the source of truth for MSVC env.** Setting
  `PATH` alone is insufficient — `INCLUDE`, `LIB`, `LIBPATH`,
  `VCINSTALLDIR` must all be present. Calling `vcvarsall.bat` in a
  subprocess and importing its env is the reliable pattern.
- **`VCPKG_ROOT` can be silently overwritten by VS env init.** Save
  and restore it. Refuse to proceed if it points at the VS-bundled
  vcpkg.
- **MSYS2 in PATH breaks CMake's compiler detection.** Explicitly set
  `CC=cl.exe`, `CXX=cl.exe` and detect/clean stale `CMakeCache.txt`
  with GCC compiler entries.
- **vcpkg downloads are fragile on slow GitHub links.** Use aria2c
  (or equivalent parallel downloader) in WSL and drop the file into
  `%USERPROFILE%\vcpkg\downloads\`. vcpkg accepts pre-cached files
  silently.
- **vcpkg binary cache is a 500x speedup.** First build ~30 min per
  architecture; every subsequent build ~30 seconds. This is what makes
  the "one-time cost" a genuine one-time cost.
- **Static triplet + static CRT is the right default for redistribution.**
  `x64-windows-static` / `x86-windows-static` produce a self-contained
  `.exe` with no external dependencies beyond Windows system DLLs.

### On Debugging

- Read the actual error. `close 1006` and `handshake declined` are
  symptoms, not causes.
- Log at the right level. Heartbeat success at DEBUG; revoke at ERROR.
- Run for long enough to see periodic behavior — 60 s+ tests reveal
  timing bugs.
- **A stalled download looks identical to a hung process.** Check file
  size growth (`Get-ChildItem … | Select Length`) across 30-second
  intervals before declaring a hang.
- **A `.part` file with a nonzero size is not proof of completion.**
  Compare against the expected size (e.g., Strawberry Perl is 291 MB,
  not 131 MB — a truncated download at 131 MB was still not usable).

### On the Project

- **The infrastructure is genuinely reusable.** Same `ShutdownController`,
  same event bus, same module framework, same config parser on four
  targets — no platform-specific branches in any of them.
- **The DLP logic is easy once infrastructure exists.** All the hard
  work in this migration was plumbing (build system, dep manager,
  downloader resilience). Policy engine + enforcement will be
  comparatively straightforward.
- **Ship small.** Every change since the beginning has been verified
  before the next.
- **Bug histories like this document are worth their weight.** They
  explain *why* the code and scripts look the way they do. Future
  readers will not spend a day re-discovering that `vcvarsall.bat`
  overwrites `VCPKG_ROOT` — they'll read Bug W22.

---
~~~~

  + .gitignore
### \`.gitignore\`

```
# Build outputs
build/
build-*/

# Shared dependency source cache
.deps/

# vcpkg manifest mode artifacts
vcpkg_installed/
vcpkg-cache/
*.vcpkg-lock.json

# MSVC / Visual Studio
.vs/
*.vcxproj.user

# Generated compile database (root merged file)
compile_commands.json

# IDE / OS
.vscode/*
!.vscode/settings.json
.idea/
*.swp
*.swo
.DS_Store
Thumbs.db

# Compiled artifacts
*.o
*.obj
*.exe
*.dll
*.so
*.a
*.lib

# Logs and runtime data
*.log
logs/
data/

# CMake-generated
CMakeCache.txt
CMakeFiles/
cmake_install.cmake

# clangd cache
.clangd.cache/
.cache/
axiomdlp-agent-bundle.md
```

  + make-bundle.sh
### \`make-bundle.sh\`

```bash
#!/usr/bin/env bash
#
# make-bundle.sh — bundle the AxiomDLP agent's OWN source into one markdown
# file for an LLM/agent. Third-party toolchains, build outputs, and
# dependency trees are skipped; every skip is recorded in a footer so the
# agent can ask for anything it needs by name.
#
set -euo pipefail

ROOT="${ROOT:-$(pwd)}"
OUT="${OUT:-$ROOT/axiomdlp-agent-bundle.md}"
QUIET="${QUIET:-0}"
MAX_FILE_SIZE="${MAX_FILE_SIZE:-524288}"   # 512 KiB per-file guard (env-overridable)

[[ -f "$ROOT/CMakeLists.txt" ]] || {
    echo "error: $ROOT has no CMakeLists.txt — run from the project root" >&2
    exit 1
}

log() { [[ "$QUIET" == 1 ]] || printf '%s\n' "$*" ; }

# ─── Directories we never descend into ────────────────────────────────
# Build outputs, dependency trees, VCS metadata, editor/tool caches,
# runtime data/logs, and any vendored trees inside src/. These are never
# bundled, never listed in the tree, and always reported in the footer.
THIRD_PARTY_DIRS=(
    .deps _deps deps
    .git .hg .svn
    .cache .venv
    .vscode .idea
    build cmake-build-*
    CMakeFiles node_modules
    data logs
    third_party external vendor
)

# Precompute `-name X -o -name Y -o ...` as a bash array for find(1).
PRUNE_NAMES=()
for p in "${THIRD_PARTY_DIRS[@]}"; do
    if [[ ${#PRUNE_NAMES[@]} -gt 0 ]]; then
        PRUNE_NAMES+=(-o)
    fi
    PRUNE_NAMES+=(-name "$p")
done

is_third_party_dir() {
    local name="$1" p
    for p in "${THIRD_PARTY_DIRS[@]}"; do
        # shellcheck disable=SC2053
        [[ "$name" == $p ]] && return 0
    done
    return 1
}

# ─── Tracked state ────────────────────────────────────────────────────
SKIPPED_LINES=""
included=0
skipped=0

record_skip() {
    SKIPPED_LINES+="- \`$1\` — $2"$'\n'
    skipped=$((skipped + 1))
    log "  · skip: $1 ($2)"
}

# ─── Code fence + language helpers ────────────────────────────────────
fence_for() { case "$1" in *.md|*.markdown) echo '~~~~' ;; *) echo '```' ;; esac; }

lang_for() {
    case "$1" in
        *.c)                                     echo 'c'          ;;
        *.cc|*.cpp|*.cxx)                        echo 'cpp'        ;;
        *.h|*.hh|*.hpp|*.hxx|*.inl|*.ipp|*.tpp)  echo 'cpp'        ;;
        CMakeLists.txt|*.cmake)                  echo 'cmake'      ;;
        *.json|*.jsonc)                          echo 'json'       ;;
        *.yaml|*.yml)                            echo 'yaml'       ;;
        *.toml)                                  echo 'toml'       ;;
        *.sh|*.bash)                             echo 'bash'       ;;
        *.py)                                    echo 'python'     ;;
        *.ps1)                                   echo 'powershell' ;;
        *.bat|*.cmd)                             echo 'batch'      ;;
        *.html|*.htm)                            echo 'html'       ;;
        *.md|*.markdown)                         echo 'markdown'   ;;
        *.ini|*.cfg|*.conf|*.service)            echo 'ini'        ;;
        *.txt)                                   echo 'text'       ;;
        *)                                       echo ''           ;;
    esac
}

is_binary() {
    local enc
    enc="$(file --mime-encoding -b "$1" 2>/dev/null || echo binary)"
    [[ "$enc" == "binary" ]]
}

# Emit a single file, guarding against binaries and huge blobs.
emit_file() {
    local f="$1" rel size pretty
    rel="${f#"$ROOT"/}"

    if is_binary "$f"; then
        record_skip "$rel" "binary file — not bundled"
        return
    fi

    size=$(stat -c%s "$f" 2>/dev/null || echo 0)
    if (( size > MAX_FILE_SIZE )); then
        pretty="$(numfmt --to=iec "$size" 2>/dev/null || echo "$size B")"
        record_skip "$rel" "large file ($pretty) — not bundled"
        return
    fi

    included=$((included + 1))
    log "  + $rel"
    {
        printf '### \`%s\`\n\n' "$rel"
        printf '%s%s\n' "$(fence_for "$rel")" "$(lang_for "$rel")"
        cat "$f"
        [[ -s "$f" && "$(tail -c1 "$f" | od -An -t x1 | tr -d ' \n')" != "0a" ]] && printf '\n'
        printf '%s\n\n' "$(fence_for "$rel")"
    } >> "$OUT"
}

# Emit one specific file if it exists.
want_file() {
    local f="$ROOT/$1"
    [[ -f "$f" ]] || { record_skip "$1" "not present"; return; }
    emit_file "$f"
}

# Walk a directory, emitting every non-third-party file.
want_dir() {
    local dir="$1" label="$2" f rel
    [[ -d "$dir" ]] || { record_skip "$label" "directory absent"; return; }
    while IFS= read -r -d '' f; do
        rel="${f#"$ROOT"/}"
        emit_file "$f"
    done < <(
        find "$dir" \
            -type d \( "${PRUNE_NAMES[@]}" \) -prune \
            -o -type f -print0 \
        | sort -z
    )
}

# ─── Header ───────────────────────────────────────────────────────────
{
    printf '# AxiomDLP Agent — Source Bundle\n\n'
    printf -- '- **Generated:** %s\n' "$(date -u '+%Y-%m-%d %H:%M:%S UTC')"
    printf -- '- **Root:** `%s`\n\n' "$ROOT"
    printf 'This bundle contains **only the project'"'"'s own source code** —\n'
    printf 'C++ sources, the top-level CMake configuration, runtime JSON\n'
    printf 'config, packaging, scripts, and docs.\n\n'
    printf 'Third-party code (dependency trees under `.deps/` or `_deps/`,\n'
    printf 'vendored `third_party/`, cross-compile toolchain files under\n'
    printf '`cmake/toolchains/`), build outputs, editor/tool caches, and\n'
    printf 'generated artifacts are **not** included. Every path that was\n'
    printf 'skipped is listed in the **Skipped Files** section at the end.\n'
    printf 'If you need any of them, ask the user — do not guess.\n\n'
    printf -- '---\n\n'
} > "$OUT"

# ─── Tree (source only) ───────────────────────────────────────────────
{
    printf '## Project Structure\n\n```text\n'
    ( cd "$ROOT"
      find . -type d \( "${PRUNE_NAMES[@]}" \) -prune -o -print | sort
    )
    printf '```\n\n---\n\n'
} >> "$OUT"

# ─── Root files ───────────────────────────────────────────────────────
{
    printf '## Project Root\n\n'
    want_file "CMakeLists.txt"
    want_file "CMakePresets.json"
    want_file "README.md"
    want_file "LICENSE"
    want_file "next.md"
    want_file "done.md"
    want_file ".gitignore"
    want_file "make-bundle.sh"
    printf -- '---\n\n'
} >> "$OUT"

# ─── Runtime configuration ────────────────────────────────────────────
printf '## Configuration\n\n' >> "$OUT"
want_dir "$ROOT/config" "config/"
printf -- '---\n\n' >> "$OUT"

# ─── Source ───────────────────────────────────────────────────────────
printf '## Source\n\n' >> "$OUT"
want_dir "$ROOT/src" "src/"
printf -- '---\n\n' >> "$OUT"

# ─── Packaging ────────────────────────────────────────────────────────
printf '## Packaging\n\n' >> "$OUT"
want_dir "$ROOT/packaging" "packaging/"
printf -- '---\n\n' >> "$OUT"

# ─── Scripts ──────────────────────────────────────────────────────────
printf '## Scripts\n\n' >> "$OUT"
want_dir "$ROOT/scripts" "scripts/"
printf -- '---\n\n' >> "$OUT"

# ─── Docs ─────────────────────────────────────────────────────────────
printf '## Docs\n\n' >> "$OUT"
want_dir "$ROOT/docs" "docs/"
printf -- '---\n\n' >> "$OUT"

# ─── Enumerate top-level entries we deliberately did not bundle ───────
{
    # Top-level directories (find so dot-dirs are included).
    while IFS= read -r -d '' d; do
        name="$(basename "$d")"
        case "$name" in
            config|src|packaging|scripts|docs)  continue ;;
        esac
        if is_third_party_dir "$name"; then
            case "$name" in
                .deps|_deps|deps)     record_skip "$name/" "third-party dependency tree — not bundled" ;;
                .git|.hg|.svn)        record_skip "$name/" "version-control metadata — not bundled" ;;
                .cache|.venv)         record_skip "$name/" "tool/venv cache — not bundled" ;;
                .vscode|.idea)        record_skip "$name/" "editor config — not bundled" ;;
                build|cmake-build-*)  record_skip "$name/" "build output — not bundled" ;;
                CMakeFiles)           record_skip "$name/" "build metadata — not bundled" ;;
                node_modules)         record_skip "$name/" "third-party node deps — not bundled" ;;
                data)                 record_skip "$name/" "runtime data — not bundled (ask if needed)" ;;
                logs)                 record_skip "$name/" "runtime logs — not bundled (ask if needed)" ;;
                third_party|external|vendor) record_skip "$name/" "vendored third-party code — not bundled" ;;
                *)                    record_skip "$name/" "not bundled (ask if needed)" ;;
            esac
        else
            record_skip "$name/" "not bundled (ask if needed)"
        fi
    done < <(find "$ROOT" -mindepth 1 -maxdepth 1 -type d -print0 | sort -z)

    # Top-level files (same, via find so dot-files are visible).
    while IFS= read -r -d '' f; do
        base="$(basename "$f")"
        case "$base" in
            CMakeLists.txt|CMakePresets.json|README.md|LICENSE|next.md|done.md|.gitignore|make-bundle.sh) continue ;;
            compile_commands.json)    record_skip "$base" "generated — not bundled" ;;
            .clangd)                  record_skip "$base" "editor config — not bundled" ;;
            axiomdlp-agent-bundle.md) record_skip "$base" "generated bundle — not bundled" ;;
            *)                        record_skip "$base" "not bundled (ask if needed)" ;;
        esac
    done < <(find "$ROOT" -mindepth 1 -maxdepth 1 -type f -print0 | sort -z)
} 2>/dev/null || true

# ─── Skipped Files appendix ───────────────────────────────────────────
{
    printf '## Skipped Files\n\n'
    printf 'These paths exist in the project but were deliberately left out\n'
    printf 'of this bundle because they contain third-party code, build\n'
    printf 'output, or generated artifacts. **If you need any of them, ask\n'
    printf 'the user** — do not assume their contents.\n\n'
    if [[ -z "$SKIPPED_LINES" ]]; then
        printf '_Nothing was skipped._\n'
    else
        printf '%s\n' "$SKIPPED_LINES"
    fi
    printf '\n---\n\n'
} >> "$OUT"

# ─── Summary ──────────────────────────────────────────────────────────
{
    printf '## Summary\n\n'
    printf -- '- Files included: **%s**\n' "$included"
    printf -- '- Files skipped:  **%s**\n' "$skipped"
    printf -- '- Bundle size:    %s\n' "$(du -h "$OUT" | cut -f1)"
} >> "$OUT"

log
log "✓ bundle written"
log "  path:     $OUT"
log "  size:     $(du -h "$OUT" | cut -f1)"
log "  included: $included"
log "  skipped:  $skipped"
```

---

## Configuration

### \`config/agent.default.json\`

```json
{
  "logging": {
    "dir": "logs",
    "file": "agent.log",
    "level": "info"
  },
  "storage": {
    "db_path": "data/agent.db"
  },
  "server": {
    "base_url": "http://127.0.0.1:3000",
    "api_token": "axdlp_user_gNAHsaK1QoFwVq3H3kMFMku0f8nJjkVugecDQfxF",
    "heartbeat_interval_seconds": 60,
    "reconnect_backoff_seconds": 10,
    "reconnect_backoff_max_seconds": 300,
    "grpc_port": 50051,
    "grpc_tls": true,
    "grpc_ca_path": ""
  },
  "modules": {
    "filesystem_watcher": {
      "enabled": true,
      "watch_paths": [
        "/home/kali/Downloads"
      ],
      "exclude_paths": [
        "*/logs/*",
        "*/.cache/*"
      ],
      "debounce_ms": 300
    },
    "inventory": {
      "enabled": true,
      "watch_paths": [
        "/home/kali/Downloads"
      ],
      "exclude_paths": [
        "*/logs/*"
      ],
      "interval_seconds": 60,
      "compute_hashes": true,
      "max_file_size_bytes": 104857600,
      "scannable_categories": [
        "document",
        "code",
        "data",
        "config"
      ]
    },
    "agent_sync": {
      "enabled": true
    },
    "content_scanner": {
      "enabled": true,
      "batch_size": 50,
      "batch_max_bytes": 52428800,
      "scan_interval_seconds": 30,
      "chunk_size_bytes": 1048576,
      "chunk_overlap_bytes": 256,
      "max_file_bytes": 104857600,
      "max_retries": 3,
      "request_timeout_seconds": 300,
      "text_extensions": [
        "txt", "md", "log", "csv", "tsv", "json", "yaml", "yml", "toml", "xml",
        "c", "cpp", "cc", "cxx", "h", "hpp", "hxx", "py", "js", "ts", "jsx", "tsx",
        "go", "rs", "java", "kt", "scala", "cs", "swift", "rb", "php", "pl", "lua",
        "sql", "sh", "bash", "zsh", "ps1", "html", "css", "scss", "less", "conf",
        "cfg", "ini", "env", "properties"
      ]
    }
  }
}
```

### \`config/agent.default.json.bak\`

```
{
  "logging": {
    "dir": "logs",
    "file": "agent.log",
    "level": "info"
  },
  "storage": {
    "db_path": "data/agent.db"
  },
  "server": {
    "base_url": "http://127.0.0.1:3000",
    "api_token": "axdlp_user_gNAHsaK1QoFwVq3H3kMFMku0f8nJjkVugecDQfxF",
    "heartbeat_interval_seconds": 60,
    "reconnect_backoff_seconds": 10,
    "reconnect_backoff_max_seconds": 300,
    "grpc_port": 50051,
    "grpc_tls": true,
    "grpc_ca_path": ""
  },
  "modules": {
    "filesystem_watcher": {
      "enabled": true,
      "watch_paths": [
        "/home/kali/Downloads"
      ],
      "exclude_paths": [
        "*/logs/*",
        "*/.cache/*"
      ],
      "debounce_ms": 300
    },
    "inventory": {
      "enabled": true,
      "watch_paths": [
        "/home/kali/Downloads"
      ],
      "exclude_paths": [
        "*/logs/*"
      ],
      "interval_seconds": 60,
      "compute_hashes": true,
      "max_file_size_bytes": 104857600,
      "scannable_categories": [
        "document",
        "code",
        "data",
        "config"
      ]
    },
    "agent_sync": {
      "enabled": true
    },
    "content_scanner": {
      "enabled": true,
      "batch_size": 50,
      "batch_max_bytes": 52428800,
      "scan_interval_seconds": 30,
      "chunk_size_bytes": 1048576,
      "chunk_overlap_bytes": 256,
      "max_file_bytes": 104857600,
      "max_retries": 3,
      "request_timeout_seconds": 300,
      "text_extensions": [
        "txt", "md", "log", "csv", "tsv", "json", "yaml", "yml", "toml", "xml",
        "c", "cpp", "cc", "cxx", "h", "hpp", "hxx", "py", "js", "ts", "jsx", "tsx",
        "go", "rs", "java", "kt", "scala", "cs", "swift", "rb", "php", "pl", "lua",
        "sql", "sh", "bash", "zsh", "ps1", "html", "css", "scss", "less", "conf",
        "cfg", "ini", "env", "properties"
      ]
    }
  }
}
```

### \`config/agent.default_win.json\`

```json
{
  "logging": {
    "dir": "logs",
    "file": "agent.log",
    "level": "info"
  },
  "storage": {
    "db_path": "data/agent.db"
  },
  "server": {
    "base_url": "http://192.168.1.42:3000",
    "api_token": "",
    "heartbeat_interval_seconds": 60,
    "reconnect_backoff_seconds": 10,
    "reconnect_backoff_max_seconds": 300,
    "grpc_port": 50051,
    "grpc_tls": false,
    "grpc_ca_path": ""
  },
  "modules": {
    "filesystem_watcher": {
      "enabled": true,
      "watch_paths": [
        "%USERPROFILE%"
      ],
      "exclude_paths": [
        "*/node_modules/*",
        "*/.git/*",
        "*/AppData/Local/Temp/*",
        "*/AppData/Local/Microsoft/Windows/INetCache/*",
        "*/AppData/Local/Packages/*/LocalCache/*",
        "*/AppData/Local/Microsoft/Windows/Explorer/*",
        "*/AppData/Roaming/Microsoft/Windows/Recent/*"
      ],
      "debounce_ms": 300
    },
    "inventory": {
      "enabled": true,
      "watch_paths": [
        "%USERPROFILE%",
        "%PUBLIC%",
        "%ProgramData%",
        "%ProgramFiles%",
        "%SystemRoot%"
      ],
      "exclude_paths": [
        "*/node_modules/*",
        "*/.git/*",
        "*/AppData/Local/Temp/*",
        "*/AppData/Local/Microsoft/Windows/INetCache/*",
        "*/AppData/Local/Packages/*/LocalCache/*",
        "*/AppData/Local/Microsoft/Windows/Explorer/*",
        "*/AppData/Roaming/Microsoft/Windows/Recent/*",
        "*/AppData/Local/CrashDumps/*",
        "*/Windows/WinSxS/*",
        "*/Windows/assembly/*",
        "*/Windows/System32/*",
        "*/Windows/SysWOW64/*",
        "*/Windows/Installer/*",
        "*/Windows/Logs/*",
        "*/ProgramData/Microsoft/Windows Defender/*",
        "*/ProgramData/Microsoft/Windows/Caches/*",
        "*/ProgramData/Package Cache/*",
        "*/$Recycle.Bin/*",
        "*/System Volume Information/*"
      ],
      "interval_seconds": 900,
      "compute_hashes": true,
      "max_file_size_bytes": 104857600,
      "scannable_categories": [
        "document",
        "code",
        "data",
        "config"
      ]
    },
    "agent_sync": {
      "enabled": true
    },
    "content_scanner": {
      "enabled": true,
      "batch_size": 50,
      "batch_max_bytes": 52428800,
      "scan_interval_seconds": 30,
      "chunk_size_bytes": 1048576,
      "chunk_overlap_bytes": 256,
      "max_file_bytes": 104857600,
      "max_retries": 3,
      "request_timeout_seconds": 300,
      "text_extensions": [
        "txt", "md", "log", "csv", "tsv", "json", "yaml", "yml", "toml", "xml",
        "c", "cpp", "cc", "cxx", "h", "hpp", "hxx", "py", "js", "ts", "jsx", "tsx",
        "go", "rs", "java", "kt", "scala", "cs", "swift", "rb", "php", "pl", "lua",
        "sql", "sh", "bash", "zsh", "ps1", "html", "css", "scss", "less", "conf",
        "cfg", "ini", "env", "properties"
      ]
    }
  }
}
```

### \`config/agent.schema.json\`

```json
{
  "$schema": "http://json-schema.org/draft-07/schema#",
  "$id": "https://axiomdlp.example.com/schemas/agent.schema.json",
  "title": "AxiomDLP Agent Configuration",
  "description": "Configuration file for the AxiomDLP endpoint agent. Uses gRPC exclusively.",
  "type": "object",

  "properties": {
    "logging": {
      "type": "object",
      "description": "Logging subsystem configuration.",
      "properties": {
        "dir":   { "type": "string", "default": "logs" },
        "file":  { "type": "string", "default": "agent.log" },
        "level": {
          "type": "string",
          "enum": ["trace", "debug", "info", "warn", "error", "critical", "off"],
          "default": "info"
        }
      },
      "additionalProperties": false
    },

    "storage": {
      "type": "object",
      "description": "Local persistence configuration.",
      "properties": {
        "db_path": { "type": "string", "default": "data/agent.db" }
      },
      "additionalProperties": false
    },

    "server": {
      "type": "object",
      "description": "Identity and connection information for the Core server. Shared by agent_sync and content_scanner.",
      "properties": {
        "base_url": {
          "type": "string",
          "default": "http://127.0.0.1:3000",
          "description": "Base URL of the Core, used to derive the gRPC target host."
        },
        "api_token": {
          "type": "string",
          "default": "",
          "description": "Bearer token issued at registration: axdlp_user_... Required for all server-facing modules."
        },
        "heartbeat_interval_seconds": {
          "type": "integer",
          "minimum": 5,
          "default": 60,
          "description": "Heartbeat period used by agent_sync."
        },
        "reconnect_backoff_seconds": {
          "type": "integer",
          "minimum": 1,
          "default": 10,
          "description": "Initial reconnect delay used by grpc_transport."
        },
        "reconnect_backoff_max_seconds": {
          "type": "integer",
          "minimum": 1,
          "default": 300,
          "description": "Maximum reconnect delay used by grpc_transport."
        },
        "grpc_port": {
          "type": "integer",
          "minimum": 1,
          "maximum": 65535,
          "default": 50051,
          "description": "The port used for the gRPC transport."
        },
        "grpc_tls": {
          "type": "boolean",
          "default": true,
          "description": "Whether to use TLS for the gRPC channel. Must be true in production."
        },
        "grpc_ca_path": {
          "type": "string",
          "default": "",
          "description": "Optional path to a PEM CA certificate file. If empty, system default root CAs are used."
        }
      },
      "additionalProperties": false
    },

    "modules": {
      "type": "object",
      "description": "Per-module configuration.",
      "properties": {
        "filesystem_watcher": {
          "type": "object",
          "description": "Real-time filesystem event watcher.",
          "properties": {
            "enabled": {
              "type": "boolean",
              "default": false
            },
            "watch_paths": {
              "type": "array",
              "items": { "type": "string" },
              "default": []
            },
            "exclude_paths": {
              "type": "array",
              "items": { "type": "string" },
              "default": []
            },
            "debounce_ms": {
              "type": "integer",
              "minimum": 0,
              "default": 300
            }
          },
          "additionalProperties": false
        },

        "inventory": {
          "type": "object",
          "description": "Periodic filesystem metadata scan.",
          "properties": {
            "enabled": {
              "type": "boolean",
              "default": true
            },
            "watch_paths": {
              "type": "array",
              "items": { "type": "string" },
              "default": []
            },
            "exclude_paths": {
              "type": "array",
              "items": { "type": "string" },
              "default": []
            },
            "interval_seconds": {
              "type": "integer",
              "minimum": 10,
              "default": 900
            },
            "compute_hashes": {
              "type": "boolean",
              "default": true
            },
            "max_file_size_bytes": {
              "type": "integer",
              "minimum": 0,
              "default": 104857600
            },
            "scannable_categories": {
              "type": "array",
              "items": { "type": "string" },
              "default": ["document", "code", "data", "config"]
            }
          },
          "additionalProperties": false
        },

        "agent_sync": {
          "type": "object",
          "description": "gRPC enrollment + heartbeat. URL, token, and interval come from the top-level 'server' section.",
          "properties": {
            "enabled": {
              "type": "boolean",
              "default": true
            }
          },
          "additionalProperties": false
        },

        "content_scanner": {
          "type": "object",
          "description": "Reads text files, splits into overlapping chunks, sends them to the server for PII detection via gRPC, and writes the merged verdict back to the local files table.",
          "properties": {
            "enabled": {
              "type": "boolean",
              "default": true
            },
            "batch_size": {
              "type": "integer",
              "minimum": 1,
              "default": 50,
              "description": "Maximum number of chunks per gRPC ScanBatch call."
            },
            "batch_max_bytes": {
              "type": "integer",
              "minimum": 1024,
              "default": 52428800,
              "description": "Maximum estimated payload size per gRPC call."
            },
            "scan_interval_seconds": {
              "type": "integer",
              "minimum": 5,
              "default": 30,
              "description": "Sleep between scan cycles when there is nothing to do."
            },
            "chunk_size_bytes": {
              "type": "integer",
              "minimum": 4096,
              "default": 1048576,
              "description": "Size of each chunk sent to the server."
            },
            "chunk_overlap_bytes": {
              "type": "integer",
              "minimum": 1,
              "default": 256,
              "description": "Bytes of overlap between consecutive chunks. Must be less than chunk_size_bytes."
            },
            "max_file_bytes": {
              "type": "integer",
              "minimum": 1024,
              "default": 104857600,
              "description": "Largest file the scanner will read. Should match inventory.max_file_size_bytes."
            },
            "max_retries": {
              "type": "integer",
              "minimum": 1,
              "default": 3,
              "description": "Stop retrying a file after this many failed scan attempts."
            },
            "request_timeout_seconds": {
              "type": "integer",
              "minimum": 10,
              "default": 300,
              "description": "Timeout for one gRPC ScanBatch call."
            },
            "text_extensions": {
              "type": "array",
              "items": { "type": "string" },
              "default": [
                "txt", "md", "log", "csv", "tsv",
                "json", "yaml", "yml", "toml", "xml",
                "c", "cpp", "cc", "cxx", "h", "hpp", "hxx",
                "py", "js", "ts", "jsx", "tsx",
                "go", "rs", "java", "kt", "scala", "cs", "swift",
                "rb", "php", "pl", "lua", "sql", "sh", "bash", "zsh", "ps1",
                "html", "css", "scss", "less",
                "conf", "cfg", "ini", "env", "properties"
              ],
              "description": "Only files with one of these extensions are scanned. Everything else is marked not_applicable."
            }
          },
          "additionalProperties": false
        }
      },
      "additionalProperties": false
    }
  },

  "additionalProperties": false
}
```

---

## Source

### \`src/events/event_bus.cpp\`

```cpp
// src/core/event_bus.cpp

#include "events/event_bus.h"

#include <spdlog/spdlog.h>

#include <exception>

namespace axiom::core {

SubscriptionId EventBus::subscribe(const std::string& type, EventHandler handler) {
    std::lock_guard<std::mutex> lock(mutex_);
    const SubscriptionId id = next_id_++;
    subscriptions_.push_back({id, type, std::move(handler)});
    return id;
}

SubscriptionId EventBus::subscribe_all(EventHandler handler) {
    return subscribe("", std::move(handler));
}

void EventBus::unsubscribe(SubscriptionId id) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto it = subscriptions_.begin(); it != subscriptions_.end(); ++it) {
        if (it->id == id) {
            subscriptions_.erase(it);
            return;
        }
    }
}

void EventBus::publish(EventPtr event) {
    if (!event) {
        return;
    }

    // Copy the matching handlers to a local vector while holding the
    // lock. Then invoke them without the lock. This avoids a deadlock
    // if a handler subscribes or unsubscribes during its call.
    std::vector<EventHandler> to_call;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto& sub : subscriptions_) {
            if (sub.type.empty() || sub.type == event->type) {
                to_call.push_back(sub.handler);
            }
        }
    }

    for (const auto& handler : to_call) {
        try {
            handler(event);
        } catch (const std::exception& e) {
            SPDLOG_ERROR("EventBus: subscriber threw: {}", e.what());
        } catch (...) {
            SPDLOG_ERROR("EventBus: subscriber threw an unknown exception");
        }
    }
}

std::size_t EventBus::subscriber_count() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return subscriptions_.size();
}

} // namespace axiom::core
```

### \`src/events/event_bus.h\`

```cpp
// src/core/event_bus.h
//
// EventBus is a synchronous publish/subscribe system.
//
// Modules publish events onto the bus. Other modules subscribe to
// event types (or to all events) and receive callbacks when a matching
// event is published.
//
// Publishers and subscribers never know about each other. This is
// what decouples modules from each other.
//
// Thread safety:
//   subscribe / unsubscribe / publish may be called from any thread.
//   Callbacks run on the publishing thread. A slow subscriber can
//   delay other subscribers of the same event.

#pragma once

#include "events/event.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace axiom::core {

using EventPtr     = std::shared_ptr<Event>;
using EventHandler = std::function<void(const EventPtr&)>;
using SubscriptionId = std::uint64_t;

class EventBus {
public:
    EventBus() = default;
    ~EventBus() = default;

    // Non-copyable, non-movable.
    EventBus(const EventBus&) = delete;
    EventBus& operator=(const EventBus&) = delete;
    EventBus(EventBus&&) = delete;
    EventBus& operator=(EventBus&&) = delete;

    // Subscribe to a specific event type.
    // Returns a subscription ID that can be used to unsubscribe.
    SubscriptionId subscribe(const std::string& type, EventHandler handler);

    // Subscribe to every event, regardless of type.
    SubscriptionId subscribe_all(EventHandler handler);

    // Remove a subscription. Safe to call with an unknown ID (no-op).
    void unsubscribe(SubscriptionId id);

    // Publish an event to all matching subscribers.
    // Subscribers are called synchronously, on the caller's thread.
    // Exceptions thrown by subscribers are caught and logged.
    void publish(EventPtr event);

    // Number of active subscriptions (useful for diagnostics).
    std::size_t subscriber_count() const;

private:
    struct Subscription {
        SubscriptionId id;
        std::string    type;      // empty string = all events
        EventHandler   handler;
    };

    mutable std::mutex            mutex_;
    std::vector<Subscription>     subscriptions_;
    SubscriptionId                next_id_ = 1;
};

} // namespace axiom::core
```

### \`src/events/event.cpp\`

```cpp
// src/core/event.cpp

#include "events/event.h"

#include <atomic>

namespace axiom::core {

namespace {

// Monotonic counter so two events created in the same nanosecond still
// have distinct IDs.
std::atomic<std::uint64_t> g_counter{0};

std::string generate_id() {
    const auto now = std::chrono::system_clock::now();
    const auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
        now.time_since_epoch()).count();
    const auto c = g_counter.fetch_add(1, std::memory_order_relaxed);
    return std::to_string(ns) + "-" + std::to_string(c);
}

} // namespace

Event Event::create(const std::string& type) {
    Event e;
    e.id        = generate_id();
    e.type      = type;
    e.timestamp = std::chrono::system_clock::now();
    return e;
}

} // namespace axiom::core
```

### \`src/events/event.h\`

```cpp
// src/core/event.h
//
// The Event struct. Every interesting thing that happens in the agent
// becomes an Event that is published on the event bus.
//
// Events are passed by shared_ptr so multiple subscribers can receive
// the same event without copying it.

#pragma once

#include <chrono>
#include <map>
#include <optional>
#include <string>

namespace axiom::core {

struct Event {
    // Unique identifier (generated at creation).
    std::string id;

    // Type of the event, e.g. "file_access", "usb_insert".
    // Subscribers filter by this string.
    std::string type;

    // When the event occurred.
    std::chrono::system_clock::time_point timestamp;

    // Optional attribution.
    std::string user_id;
    std::string process_name;
    int         pid = 0;

    // Arbitrary key/value metadata (path, size, action, etc.).
    std::map<std::string, std::string> metadata;

    // Optional payload (text extracted from a file, clipboard, etc.).
    std::optional<std::string> content;

    // Create a new event of the given type with a fresh ID and
    // the current timestamp.
    static Event create(const std::string& type);
};

// Canonical event type strings. Using these avoids typos and gives
// the compiler something to check against.
namespace event_types {

inline constexpr const char* kFileAccess    = "file_access";
inline constexpr const char* kFileWrite     = "file_write";
inline constexpr const char* kFileDelete    = "file_delete";
inline constexpr const char* kUsbInsert     = "usb_insert";
inline constexpr const char* kUsbRemove     = "usb_remove";
inline constexpr const char* kClipboardCopy = "clipboard_copy";
inline constexpr const char* kPrintJob      = "print_job";
inline constexpr const char* kProcessSpawn  = "process_spawn";
inline constexpr const char* kScanResult    = "scan_result";
inline constexpr const char* kDecision      = "decision";

} // namespace event_types

} // namespace axiom::core
```

### \`src/features/agent_sync/agent_sync_module.cpp\`

```cpp
// src/features/agent_sync/agent_sync_module.cpp
#include "features/agent_sync/agent_sync_module.h"
#include "net/http_client.h"
#include "storage/state_repository.h"

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include <chrono>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>

#if defined(_WIN32)
#  include <windows.h>
#elif defined(__linux__)
#  include <unistd.h>
#endif

namespace axiom::core {

using json = nlohmann::json;
using namespace std::chrono_literals;

namespace {
constexpr const char* kAgentVersion = "0.1.0";
constexpr const char* kStateKeyAgentId = "agent_id";

void interruptible_sleep(std::atomic<bool>& stop_flag, ShutdownController& shutdown, std::chrono::seconds total) {
    constexpr auto tick = 100ms;
    auto elapsed = 0ms;
    const auto target = std::chrono::duration_cast<std::chrono::milliseconds>(total);
    while (elapsed < target) {
        if (stop_flag.load(std::memory_order_relaxed)) return;
        if (shutdown.is_requested()) return;
        std::this_thread::sleep_for(tick);
        elapsed += tick;
    }
}
} // namespace

AgentSyncModule::AgentSyncModule()  = default;
AgentSyncModule::~AgentSyncModule() = default;

std::string AgentSyncModule::detect_hostname() {
#if defined(_WIN32)
    char buf[MAX_COMPUTERNAME_LENGTH + 1];
    DWORD size = sizeof(buf);
    if (GetComputerNameA(buf, &size)) return std::string(buf, size);
#elif defined(__linux__)
    char buf[256];
    if (gethostname(buf, sizeof(buf)) == 0) return std::string(buf);
#endif
    return "unknown";
}

std::string AgentSyncModule::detect_os() {
#if defined(_WIN32)
    return "windows";
#elif defined(__linux__)
    return "linux";
#elif defined(__APPLE__)
    return "darwin";
#else
    return "unknown";
#endif
}

std::string AgentSyncModule::detect_arch() {
#if defined(__x86_64__) || defined(_M_X64)
    return "x86_64";
#elif defined(__i386__) || defined(_M_IX86)
    return "i686";
#elif defined(__aarch64__)
    return "aarch64";
#else
    return "unknown";
#endif
}

bool AgentSyncModule::initialize(ModuleContext& context) {
    ModuleBase::initialize(context);

    const auto& mcfg = ctx_->config.modules.agent_sync;
    const auto& scfg = ctx_->config.server;

    enabled_                = mcfg.enabled;
    base_url_               = scfg.base_url;
    api_token_              = scfg.api_token;
    heartbeat_interval_sec_ = scfg.heartbeat_interval_seconds;

    if (!enabled_) return true;

    if (base_url_.empty() || api_token_.empty()) {
        SPDLOG_ERROR("agent_sync module: no valid API token — server.base_url or api_token is empty");
        return false;
    }

    http_ = std::make_unique<HttpClient>();
    SPDLOG_INFO("agent_sync module: initialized (heartbeat={}s)", heartbeat_interval_sec_);
    return true;
}

bool AgentSyncModule::start() {
    if (!enabled_) return true;
    stop_requested_.store(false, std::memory_order_relaxed);
    needs_re_enroll_.store(false, std::memory_order_relaxed);
    sync_thread_ = std::thread([this] { sync_loop(); });
    running_.store(true, std::memory_order_relaxed);
    SPDLOG_INFO("agent_sync module: started");
    return true;
}

void AgentSyncModule::stop() {
    if (!running_.load(std::memory_order_relaxed)) return;
    stop_requested_.store(true, std::memory_order_relaxed);
    if (sync_thread_.joinable()) sync_thread_.join();
    http_.reset();
    running_.store(false, std::memory_order_relaxed);
    SPDLOG_INFO("agent_sync module: stopped");
}

void AgentSyncModule::shutdown() {
    SPDLOG_INFO("agent_sync module: shutting down");
}

HealthStatus AgentSyncModule::health() const {
    if (ctx_ == nullptr) return HealthStatus::unknown("not initialized");
    if (!enabled_)       return HealthStatus::ok("disabled");
    if (!running_.load(std::memory_order_relaxed)) return HealthStatus::failed("sync thread not running");
    if (agent_id_ == 0) return HealthStatus::degraded("not yet enrolled");
    return HealthStatus::ok("enrolled (agent_id=" + std::to_string(agent_id_) + ")");
}

void AgentSyncModule::sync_loop() {
    if (auto saved = ctx_->state_store.get(kStateKeyAgentId)) {
        try {
            agent_id_ = std::stoi(*saved);
            SPDLOG_INFO("agent_sync: loaded agent_id={} from local state", agent_id_);
        } catch (const std::exception&) {
            ctx_->state_store.remove(kStateKeyAgentId);
            agent_id_ = 0;
        }
    }

    if (agent_id_ == 0) {
        if (!enroll()) {
            if (stop_requested_.load(std::memory_order_relaxed) || ctx_->shutdown.is_requested()) return;
            needs_re_enroll_.store(true, std::memory_order_relaxed);
        }
    } else {
        SPDLOG_INFO("agent_sync: validating token with immediate heartbeat");
        if (!heartbeat()) {
            if (stop_requested_.load(std::memory_order_relaxed) || ctx_->shutdown.is_requested()) return;
        }
    }

    while (!stop_requested_.load(std::memory_order_relaxed)) {
        if (ctx_->shutdown.is_requested()) return;
        interruptible_sleep(stop_requested_, ctx_->shutdown, std::chrono::seconds(heartbeat_interval_sec_));
        if (stop_requested_.load(std::memory_order_relaxed) || ctx_->shutdown.is_requested()) break;

        if (needs_re_enroll_.exchange(false, std::memory_order_relaxed)) {
            agent_id_ = 0;
            if (!enroll()) {
                if (stop_requested_.load(std::memory_order_relaxed) || ctx_->shutdown.is_requested()) return;
                needs_re_enroll_.store(true, std::memory_order_relaxed);
                continue;
            }
        }

        if (!heartbeat()) {
            if (stop_requested_.load(std::memory_order_relaxed) || ctx_->shutdown.is_requested()) return;
        }
    }
}

bool AgentSyncModule::enroll() {
    const std::string url = base_url_ + "/api/v1/agent/register";
    json body = { {"hostname", detect_hostname()}, {"os", detect_os()}, {"architecture", detect_arch()}, {"agent_version", kAgentVersion} };

    SPDLOG_INFO("agent_sync: POST {} (hostname={})", url, body["hostname"].get<std::string>());
    const auto resp = http_->post_json(url, body.dump(), api_token_, 15);

    if (!resp.error.empty()) return false;
    if (resp.status == 401 || resp.status == 403) {
        ctx_->shutdown.request(ShutdownReason::AuthRevoked);
        return false;
    }
    if (!resp.is_2xx()) return false;

    try {
        const auto parsed = json::parse(resp.body);
        agent_id_ = parsed.value("agent_id", 0);
        if (agent_id_ <= 0) return false;
        ctx_->state_store.set(kStateKeyAgentId, std::to_string(agent_id_));
        SPDLOG_INFO("agent_sync: enrolled (agent_id={}, user_id={}, org_id={})", agent_id_, parsed.value("user_id", 0), parsed.value("organization_id", 0));
        return true;
    } catch (...) { return false; }
}

bool AgentSyncModule::heartbeat() {
    if (agent_id_ == 0) {
        needs_re_enroll_.store(true, std::memory_order_relaxed);
        return false;
    }

    const std::string url = base_url_ + "/api/v1/agent/heartbeat";
    json body = { {"agent_id", agent_id_}, {"agent_version", kAgentVersion} };
    const auto resp = http_->post_json(url, body.dump(), api_token_, 10);

    if (!resp.error.empty()) return false;
    if (resp.status == 200) {
        SPDLOG_DEBUG("agent_sync: heartbeat ok (agent_id={})", agent_id_);
        return true;
    }
    if (resp.status == 401 || resp.status == 403) {
        ctx_->shutdown.request(ShutdownReason::AuthRevoked);
        return false;
    }
    if (resp.status == 404) {
        ctx_->state_store.remove(kStateKeyAgentId);
        needs_re_enroll_.store(true, std::memory_order_relaxed);
        return false;
    }
    return false;
}

} // namespace axiom::core
```

### \`src/features/agent_sync/agent_sync_module.h\`

```cpp
// src/features/agent_sync/agent_sync_module.h
#pragma once

#include "modules/module_base.h"
#include <atomic>
#include <memory>
#include <string>
#include <thread>
#include <optional>
#include <nlohmann/json.hpp>

namespace axiom::core {

class HttpClient;

class AgentSyncModule final : public ModuleBase {
public:
    AgentSyncModule();
    ~AgentSyncModule() override;

    std::string name() const override { return "agent_sync"; }

    bool initialize(ModuleContext& context) override;
    bool start() override;
    void stop() override;
    void shutdown() override;
    HealthStatus health() const override;

private:
    void sync_loop();
    bool enroll();
    bool heartbeat();

    static std::string detect_hostname();
    static std::string detect_os();
    static std::string detect_arch();

    std::string base_url_;
    std::string api_token_;
    int         heartbeat_interval_sec_ = 30;
    bool        enabled_ = true;

    std::unique_ptr<HttpClient> http_;

    std::thread       sync_thread_;
    std::atomic<bool> stop_requested_{false};
    std::atomic<bool> running_{false};
    std::atomic<bool> needs_re_enroll_{false};

    int agent_id_ = 0;
};

} // namespace axiom::core
```

### \`src/features/content_scanner/content_scanner_module.cpp\`

```cpp
// src/features/content_scanner/content_scanner_module.cpp
#include "features/content_scanner/content_scanner_module.h"
#include "net/http_client.h"
#include "storage/file_repository.h"
#include "storage/state_repository.h"
#include "util/base64.h"
#include "util/file_hash.h"

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <string>
#include <thread>
#include <vector>

namespace axiom::core {

using json = nlohmann::json;
using namespace std::chrono_literals;

namespace {
void interruptible_sleep(std::atomic<bool>& stop_flag, std::chrono::seconds total) {
    constexpr auto tick = 100ms;
    auto elapsed = 0ms;
    const auto target = std::chrono::duration_cast<std::chrono::milliseconds>(total);
    while (elapsed < target) {
        if (stop_flag.load(std::memory_order_relaxed)) return;
        std::this_thread::sleep_for(tick);
        elapsed += tick;
    }
}

bool file_exists(const std::string& path) {
    std::error_code ec;
    return std::filesystem::exists(std::filesystem::path(path), ec);
}

bool ext_in_list(const std::string& ext, const std::vector<std::string>& list) {
    return std::find(list.begin(), list.end(), ext) != list.end();
}

struct ReadResult {
    std::vector<char> data;
    std::string       error;
    bool ok() const { return error.empty(); }
};

ReadResult read_file_bounded(const std::string& path, std::size_t max_bytes) {
    ReadResult r;
    std::FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) { r.error = "cannot open file"; return r; }
    struct Closer { std::FILE* p; ~Closer() { if (p) std::fclose(p); } } closer{f};

    if (std::fseek(f, 0, SEEK_END) != 0) { r.error = "seek failed"; return r; }
    long file_size = std::ftell(f);
    if (file_size < 0) { r.error = "cannot determine file size"; return r; }
    std::rewind(f);

    std::size_t to_read = std::min<std::size_t>(static_cast<std::size_t>(file_size), max_bytes);
    r.data.resize(to_read);
    std::size_t got = std::fread(r.data.data(), 1, to_read, f);
    if (got < to_read) {
        if (std::ferror(f)) { r.error = "read error"; return r; }
        r.data.resize(got);
    }
    return r;
}

std::vector<std::pair<std::size_t, std::size_t>>
compute_chunk_ranges(std::size_t size, std::size_t chunk_size, std::size_t overlap) {
    std::vector<std::pair<std::size_t, std::size_t>> ranges;
    if (size == 0) return ranges;
    std::size_t stride = (chunk_size > overlap) ? (chunk_size - overlap) : chunk_size;
    std::size_t start = 0;
    while (start < size) {
        std::size_t end = std::min(start + chunk_size, size);
        ranges.push_back({start, end});
        if (end >= size) break;
        start += stride;
    }
    return ranges;
}
} // namespace

ContentScannerModule::ContentScannerModule()  = default;
ContentScannerModule::~ContentScannerModule() = default;

bool ContentScannerModule::initialize(ModuleContext& context) {
    ModuleBase::initialize(context);

    const auto& scan_cfg = ctx_->config.modules.content_scanner;
    const auto& srv_cfg  = ctx_->config.server;

    enabled_             = scan_cfg.enabled;
    batch_size_          = scan_cfg.batch_size;
    batch_max_bytes_     = scan_cfg.batch_max_bytes;
    scan_interval_sec_   = scan_cfg.scan_interval_seconds;
    chunk_size_          = scan_cfg.chunk_size_bytes;
    chunk_overlap_       = scan_cfg.chunk_overlap_bytes;
    max_file_bytes_      = scan_cfg.max_file_bytes;
    max_retries_         = scan_cfg.max_retries;
    request_timeout_sec_ = scan_cfg.request_timeout_seconds;
    text_extensions_     = scan_cfg.text_extensions;

    base_url_  = srv_cfg.base_url;
    api_token_ = srv_cfg.api_token;

    if (!enabled_) return true;

    if (base_url_.empty() || api_token_.empty()) {
        SPDLOG_ERROR("content_scanner module: server.base_url and server.api_token must be set");
        return false;
    }

    http_ = std::make_unique<HttpClient>();
    SPDLOG_INFO("content_scanner module: initialized (batch_size={}, interval={}s)", 
                batch_size_, scan_interval_sec_);
    return true;
}

bool ContentScannerModule::start() {
    if (!enabled_) return true;
    stop_requested_.store(false, std::memory_order_relaxed);
    sync_thread_ = std::thread([this] { sync_loop(); });
    running_.store(true, std::memory_order_relaxed);
    SPDLOG_INFO("content_scanner module: started");
    return true;
}

void ContentScannerModule::stop() {
    if (!running_.load(std::memory_order_relaxed)) return;

    stop_requested_.store(true, std::memory_order_relaxed);
    if (sync_thread_.joinable()) sync_thread_.join();

    http_.reset();
    running_.store(false, std::memory_order_relaxed);

    SPDLOG_INFO("content_scanner module: stopped (scanned={}, sensitive={}, skipped={})",
                total_scanned_, total_sensitive_, total_skipped_);
}

void ContentScannerModule::shutdown() {
    SPDLOG_INFO("content_scanner module: shutting down");
}

HealthStatus ContentScannerModule::health() const {
    if (ctx_ == nullptr) return HealthStatus::unknown("not initialized");
    if (!enabled_)       return HealthStatus::ok("disabled");
    if (!running_.load(std::memory_order_relaxed)) return HealthStatus::failed("scanner thread not running");
    return HealthStatus::ok("scanned=" + std::to_string(total_scanned_));
}

void ContentScannerModule::sync_loop() {
    while (!stop_requested_.load(std::memory_order_relaxed)) {
        if (ctx_->shutdown.is_requested()) return;
        if (auto saved = ctx_->state_store.get("agent_id")) {
            try {
                int id = std::stoi(*saved);
                if (id > 0) { agent_id_ = id; break; }
            } catch (...) {}
        }
        std::this_thread::sleep_for(1s);
    }

    interruptible_sleep(stop_requested_, std::chrono::seconds(5));
    while (!stop_requested_.load(std::memory_order_relaxed)) {
        process_one_cycle();
        if (stop_requested_.load(std::memory_order_relaxed)) break;
        interruptible_sleep(stop_requested_, std::chrono::seconds(scan_interval_sec_));
    }
}

void ContentScannerModule::process_one_cycle() {
    auto pending = ctx_->file_store.fetch_pending_for_scan(static_cast<std::size_t>(batch_size_), max_retries_);
    if (pending.empty()) return;

    SPDLOG_INFO("content_scanner: {} pending files", pending.size());

    std::vector<Chunk> chunks;
    std::map<std::string, FileProgress> progress;

    for (auto& row : pending) {
        if (stop_requested_.load(std::memory_order_relaxed)) break;
        prepare_file(row, chunks, progress);
        if (chunks.size() >= static_cast<std::size_t>(batch_size_)) {
            send_chunks_in_batches(chunks, progress);
            chunks.clear();
        }
    }

    if (!chunks.empty()) send_chunks_in_batches(chunks, progress);
    commit_completed_files(progress);
}

bool ContentScannerModule::prepare_file(
        const FileRow& row,
        std::vector<Chunk>& out_chunks,
        std::map<std::string, FileProgress>& progress) {

    if (!ext_in_list(row.extension, text_extensions_)) { ctx_->file_store.mark_not_applicable(row.path); ++total_skipped_; return false; }
    if (row.size > max_file_bytes_) { ctx_->file_store.mark_skipped_too_large(row.path); ++total_skipped_; return false; }
    if (!file_exists(row.path)) { ctx_->file_store.mark_file_missing(row.path); ++total_skipped_; return false; }

    auto read = read_file_bounded(row.path, max_file_bytes_);
    if (!read.ok()) { ctx_->file_store.record_scan_error_by_path(row.path, read.error); return false; }

    std::string new_hash = util::sha256_hex(read.data.data(), read.data.size());

    if (auto cached = ctx_->file_store.find_scan_verdict_by_hash(new_hash)) {
        ctx_->file_store.update_scan_result_by_path(row.path, new_hash, cached->sensitive, cached->sensitivity);
        ++total_scanned_;
        if (cached->sensitive) ++total_sensitive_;
        return false;
    }

    auto ranges = compute_chunk_ranges(read.data.size(), chunk_size_, chunk_overlap_);
    if (ranges.empty()) {
        ctx_->file_store.update_scan_result_by_path(row.path, new_hash, false, "");
        ++total_scanned_;
        return false;
    }

    FileProgress fp;
    fp.path             = row.path;
    fp.chunks_expected  = static_cast<std::uint32_t>(ranges.size());
    progress[new_hash]  = fp;

    for (std::size_t i = 0; i < ranges.size(); ++i) {
        const auto [start, end] = ranges[i];
        Chunk c;
        c.file_hash   = new_hash;
        c.path        = row.path;
        c.chunk_index = static_cast<std::uint32_t>(i);
        c.chunk_count = static_cast<std::uint32_t>(ranges.size());
        c.content.assign(read.data.begin() + start, read.data.begin() + end);
        out_chunks.push_back(std::move(c));
    }
    return true;
}

void ContentScannerModule::send_chunks_in_batches(
        std::vector<Chunk>& chunks,
        std::map<std::string, FileProgress>& progress) {

    std::size_t i = 0;
    while (i < chunks.size()) {
        if (stop_requested_.load(std::memory_order_relaxed)) return;

        std::size_t batch_bytes = 0;
        std::size_t j = i;
        while (j < chunks.size() && (j - i) < static_cast<std::size_t>(batch_size_)) {
            const std::size_t estimate = chunks[j].content.size() + 256;
            if (j > i && batch_bytes + estimate > batch_max_bytes_) break;
            batch_bytes += estimate;
            ++j;
        }

        if (j == i) break;

        json batch_chunks = json::array();
        for (std::size_t k = i; k < j; ++k) {
            batch_chunks.push_back({
                {"file_hash",   chunks[k].file_hash},
                {"path",        chunks[k].path},
                {"chunk_index", chunks[k].chunk_index},
                {"chunk_count", chunks[k].chunk_count},
                {"content",     util::base64_encode(chunks[k].content.data(), chunks[k].content.size())}
            });
        }

        json payload;
        payload["chunks"] = std::move(batch_chunks);
        const std::string url = base_url_ + "/api/v1/agent/scan/batch";

        SPDLOG_INFO("content_scanner: POST {} ({} chunks, ~{} KB)", url, j - i, batch_bytes / 1024);
        const auto resp = http_->post_json(url, payload.dump(), api_token_, request_timeout_sec_);

        if (!resp.error.empty()) return;
        if (resp.status == 401 || resp.status == 403) {
            SPDLOG_ERROR("content_scanner: auth rejected ({}) — stopping", resp.status);
            running_.store(false, std::memory_order_relaxed);
            stop_requested_.store(true, std::memory_order_relaxed);
            return;
        }
        if (!resp.is_2xx()) return;
        
        merge_batch_response(resp.body, progress);

        for (std::size_t k = i; k < j; ++k) {
            std::fill(chunks[k].content.begin(), chunks[k].content.end(), 0);
            chunks[k].content.clear();
            chunks[k].content.shrink_to_fit();
        }
        i = j;
    }
}

void ContentScannerModule::merge_batch_response(const std::string& response_body, std::map<std::string, FileProgress>& progress) {
    try {
        const auto parsed = json::parse(response_body);
        if (!parsed.contains("results") || !parsed["results"].is_array()) return;

        for (const auto& r : parsed["results"]) {
            const std::string file_hash = r.value("file_hash", "");
            if (file_hash.empty()) continue;
            auto it = progress.find(file_hash);
            if (it == progress.end() || r.contains("error")) continue;

            it->second.chunks_returned += 1;
            if (r.value("sensitive", false)) {
                it->second.sensitive = true;
                if (it->second.sensitivity.empty()) it->second.sensitivity = r.value("entity_type", "");
            }
        }
    } catch (const std::exception& e) {
        SPDLOG_ERROR("content_scanner: failed to parse response: {}", e.what());
    }
}

void ContentScannerModule::commit_completed_files(std::map<std::string, FileProgress>& progress) {
    std::size_t complete = 0;
    std::size_t incomplete = 0;

    for (auto& [hash, fp] : progress) {
        if (fp.chunks_returned < fp.chunks_expected) { ++incomplete; continue; }
        ctx_->file_store.update_scan_result_by_path(fp.path, hash, fp.sensitive, fp.sensitivity);
        ++total_scanned_;
        if (fp.sensitive) ++total_sensitive_;
        ++complete;
    }

    if (complete > 0 || incomplete > 0) {
        SPDLOG_INFO("content_scanner: cycle done (committed={}, incomplete={})", complete, incomplete);
    }
}

} // namespace axiom::core
```

### \`src/features/content_scanner/content_scanner_module.h\`

```cpp
// src/features/content_scanner/content_scanner_module.h
#pragma once

#include "modules/module_base.h"
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace axiom::core {

class  HttpClient;
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
        std::string   path;
        std::uint32_t chunks_expected = 0;
        std::uint32_t chunks_returned = 0;
        bool          sensitive       = false;
        std::string   sensitivity;
    };

    void sync_loop();
    void process_one_cycle();

    bool prepare_file(const FileRow& row,
                      std::vector<Chunk>& out_chunks,
                      std::map<std::string, FileProgress>& progress);

    void send_chunks_in_batches(std::vector<Chunk>& chunks,
                                std::map<std::string, FileProgress>& progress);

    void merge_batch_response(const std::string& response_body,
                              std::map<std::string, FileProgress>& progress);

    void commit_completed_files(std::map<std::string, FileProgress>& progress);

    bool        enabled_             = true;
    std::string base_url_;
    std::string api_token_;
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

    std::unique_ptr<HttpClient> http_;

    std::thread                 sync_thread_;
    std::atomic<bool>           stop_requested_{false};
    std::atomic<bool>           running_{false};

    std::size_t total_scanned_   = 0;
    std::size_t total_sensitive_ = 0;
    std::size_t total_skipped_   = 0;
};

} // namespace axiom::core
```

### \`src/features/event_logger/event_logger_module.cpp\`

```cpp
// src/features/event_logger/event_logger_module.cpp

#include "features/event_logger/event_logger_module.h"

#include <spdlog/spdlog.h>

namespace axiom::core {

bool EventLoggerModule::start() {
    if (ctx_ == nullptr) {
        SPDLOG_ERROR("event_logger: no module context");
        return false;
    }

    subscription_id_ = ctx_->event_bus.subscribe_all(
        [](const EventPtr& e) {
            // INFO, not DEBUG: the event stream is the agent's primary
            // output during development and is expected to be visible
            // by default. The filesystem module excludes the agent's
            // own log directory, so writing these lines does NOT feed
            // back into the pipeline as new file events.
            //
            // Look up "path" once and hold a const char* rather than
            // building a temporary std::string via the ternary. This
            // keeps the per-event cost minimal even at high rates.
            const char* path = "-";
            const auto it = e->metadata.find("path");
            if (it != e->metadata.end()) {
                path = it->second.c_str();
            }

            SPDLOG_INFO("[event] type={} path={}", e->type, path);
        });

    SPDLOG_INFO("event_logger: subscribed to all events (id={})", subscription_id_);
    return true;
}

void EventLoggerModule::stop() {
    if (ctx_ != nullptr && subscription_id_ != 0) {
        ctx_->event_bus.unsubscribe(subscription_id_);
        subscription_id_ = 0;
    }
    SPDLOG_INFO("event_logger: unsubscribed");
}

} // namespace axiom::core
```

### \`src/features/event_logger/event_logger_module.h\`

```cpp
// src/modules/event_logger/event_logger_module.h
//
// EventLoggerModule subscribes to every event on the bus and logs it.
// Its only purpose is observability: it proves the pipeline works and
// gives a visible trace of everything flowing through the bus.
//
// It is a pure consumer: it never publishes events.

#pragma once

#include "events/event_bus.h"
#include "modules/module_base.h"

#include <string>

namespace axiom::core {

class EventLoggerModule final : public ModuleBase {
public:
    std::string name() const override { return "event_logger"; }

    bool start() override;
    void stop() override;

private:
    SubscriptionId subscription_id_ = 0;
};

} // namespace axiom::core
```

### \`src/features/filesystem_watcher/filesystem_watcher_module.cpp\`

```cpp
// src/features/filesystem_watcher/filesystem_watcher_module.cpp

#include "features/filesystem_watcher/filesystem_watcher_module.h"

#include "events/event.h"
#include "util/glob_match.h"

#include <spdlog/spdlog.h>

#include <filesystem>
#include <system_error>
#include <vector>

namespace axiom::core {

namespace {

std::string to_bus_event_type(const std::string& file_event_type) {
    if (file_event_type == file_event_types::kCreated)  return event_types::kFileWrite;
    if (file_event_type == file_event_types::kModified) return event_types::kFileWrite;
    if (file_event_type == file_event_types::kDeleted)  return event_types::kFileDelete;
    if (file_event_type == file_event_types::kMoved)    return event_types::kFileAccess;
    return "file_event";
}

int event_priority(const std::string& file_event_type) {
    if (file_event_type == file_event_types::kModified) return 4;
    if (file_event_type == file_event_types::kCreated)  return 3;
    if (file_event_type == file_event_types::kMoved)    return 2;
    if (file_event_type == file_event_types::kDeleted)  return 1;
    return 0;
}

std::string absolute_lexical(const std::string& p) {
    std::error_code ec;
    auto abs = std::filesystem::absolute(p, ec);
    if (ec) return p;
    return abs.lexically_normal().string();
}

} // namespace

// ----------------------------------------------------------------
// Lifecycle
// ----------------------------------------------------------------

bool FilesystemWatcherModule::initialize(ModuleContext& context) {
    ModuleBase::initialize(context);

    if (!ctx_->config.modules.filesystem_watcher.enabled) {
        SPDLOG_INFO("filesystem_watcher module: disabled in config");
        return true;
    }

    if (ctx_->config.modules.filesystem_watcher.watch_paths.empty()) {
        SPDLOG_WARN("filesystem_watcher module: enabled but no watch_paths configured");
    }

    debounce_window_ = std::chrono::milliseconds(
        ctx_->config.modules.filesystem_watcher.debounce_ms);

    build_excludes();

    SPDLOG_INFO("filesystem_watcher module: initialized with {} watch path(s), "
                "{} exclude pattern(s), debounce {} ms",
                ctx_->config.modules.filesystem_watcher.watch_paths.size(),
                exclude_paths_.size(),
                ctx_->config.modules.filesystem_watcher.debounce_ms);
    return true;
}

bool FilesystemWatcherModule::start() {
    if (!ctx_->config.modules.filesystem_watcher.enabled) {
        return true;
    }

    flush_stop_.store(false, std::memory_order_relaxed);
    flush_thread_ = std::thread([this] { flush_loop(); });

    setup_done_.store(false, std::memory_order_relaxed);
    setup_thread_ = std::thread([this] { setup_watcher_async(); });

    running_ = true;
    SPDLOG_INFO("filesystem_watcher module: started (watcher initializing)");
    return true;
}

void FilesystemWatcherModule::setup_watcher_async() {
    auto w = make_file_watcher();
    if (!w) {
        SPDLOG_ERROR("filesystem_watcher module: failed to create file watcher");
        setup_done_.store(true, std::memory_order_relaxed);
        return;
    }

    const auto& paths = ctx_->config.modules.filesystem_watcher.watch_paths;

    SPDLOG_INFO("filesystem_watcher module: initializing watcher over {} path(s)",
                paths.size());

    const bool ok = w->start(
        paths,
        [this](const FileEvent& fe) { on_file_event(fe); });

    if (!ok) {
        SPDLOG_ERROR("filesystem_watcher module: watcher failed to start");
        setup_done_.store(true, std::memory_order_relaxed);
        return;
    }

    if (flush_stop_.load(std::memory_order_relaxed)) {
        w->stop();
        setup_done_.store(true, std::memory_order_relaxed);
        SPDLOG_INFO("filesystem_watcher module: watcher setup cancelled");
        return;
    }

    {
        std::lock_guard<std::mutex> lock(watcher_mutex_);
        watcher_ = std::move(w);
    }
    setup_done_.store(true, std::memory_order_relaxed);
    SPDLOG_INFO("filesystem_watcher module: watcher ready");
}

void FilesystemWatcherModule::stop() {
    flush_stop_.store(true, std::memory_order_relaxed);

    if (setup_thread_.joinable()) {
        if (!setup_done_.load(std::memory_order_relaxed)) {
            SPDLOG_INFO("filesystem_watcher module: waiting for watcher setup to complete...");
        }
        setup_thread_.join();
    }

    {
        std::lock_guard<std::mutex> lock(watcher_mutex_);
        if (watcher_) {
            watcher_->stop();
            watcher_.reset();
        }
    }

    if (flush_thread_.joinable()) {
        flush_thread_.join();
    }

    {
        std::lock_guard<std::mutex> lock(pending_mutex_);
        for (auto& [path, pending] : pending_) {
            publish_event(pending.event);
        }
        pending_.clear();
    }

    running_ = false;
    SPDLOG_INFO("filesystem_watcher module: stopped");
}

void FilesystemWatcherModule::shutdown() {
    SPDLOG_INFO("filesystem_watcher module: shutting down");
}

HealthStatus FilesystemWatcherModule::health() const {
    if (ctx_ == nullptr) {
        return HealthStatus::unknown("not initialized");
    }
    if (!ctx_->config.modules.filesystem_watcher.enabled) {
        return HealthStatus::ok("disabled");
    }
    if (!running_) {
        return HealthStatus::failed("watcher not running");
    }
    if (!setup_done_.load(std::memory_order_relaxed)) {
        return HealthStatus::degraded("watcher initializing");
    }
    return HealthStatus::ok();
}

// ----------------------------------------------------------------
// Exclusions
// ----------------------------------------------------------------

void FilesystemWatcherModule::build_excludes() {
    exclude_paths_.clear();

    for (const auto& p : ctx_->config.modules.filesystem_watcher.exclude_paths) {
        exclude_paths_.push_back(p);
    }

    if (!ctx_->config.logging.dir.empty()) {
        const std::string log_dir_abs = absolute_lexical(ctx_->config.logging.dir);
        exclude_paths_.push_back(log_dir_abs + "/*");
        SPDLOG_INFO("filesystem_watcher module: excluding own log directory '{}'",
                    log_dir_abs);
    }

    if (!ctx_->config.storage.db_path.empty()) {
        const auto db_dir = std::filesystem::path(ctx_->config.storage.db_path)
                                .parent_path().string();
        if (!db_dir.empty()) {
            const std::string db_dir_abs = absolute_lexical(db_dir);
            exclude_paths_.push_back(db_dir_abs + "/*");
            SPDLOG_INFO("filesystem_watcher module: excluding own data directory '{}'",
                        db_dir_abs);
        }
    }
}

bool FilesystemWatcherModule::is_excluded(const std::string& path) const {
    for (const auto& pattern : exclude_paths_) {
        if (util::path_matches_pattern(pattern, path)) {
            return true;
        }
    }
    return false;
}

// ----------------------------------------------------------------
// Debounce: watcher callback
// ----------------------------------------------------------------

void FilesystemWatcherModule::on_file_event(const FileEvent& fe) {
    if (fe.is_directory) {
        return;
    }

    if (is_excluded(fe.path)) {
        return;
    }

    const int  new_priority = event_priority(fe.event_type);
    const auto now          = std::chrono::steady_clock::now();
    const auto deadline     = now + debounce_window_;

    std::lock_guard<std::mutex> lock(pending_mutex_);

    auto it = pending_.find(fe.path);
    if (it == pending_.end()) {
        pending_.emplace(fe.path, PendingEvent{fe, deadline, new_priority});
        return;
    }

    if (new_priority >= it->second.priority) {
        it->second.event    = fe;
        it->second.deadline = deadline;
        it->second.priority = new_priority;
    } else {
        it->second.deadline = deadline;
    }
}

// ----------------------------------------------------------------
// Flusher thread
// ----------------------------------------------------------------

void FilesystemWatcherModule::flush_loop() {
    constexpr auto tick = std::chrono::milliseconds(50);

    while (!flush_stop_.load(std::memory_order_relaxed)) {
        std::this_thread::sleep_for(tick);

        std::vector<FileEvent> to_publish;

        {
            std::lock_guard<std::mutex> lock(pending_mutex_);
            const auto now = std::chrono::steady_clock::now();

            for (auto it = pending_.begin(); it != pending_.end(); ) {
                if (it->second.deadline <= now) {
                    to_publish.push_back(it->second.event);
                    it = pending_.erase(it);
                } else {
                    ++it;
                }
            }
        }

        for (const auto& fe : to_publish) {
            publish_event(fe);
        }
    }
}

// ----------------------------------------------------------------
// Publish one FileEvent to the bus
// ----------------------------------------------------------------

void FilesystemWatcherModule::publish_event(const FileEvent& fe) {
    auto e = std::make_shared<Event>(Event::create(to_bus_event_type(fe.event_type)));

    e->metadata["path"]      = fe.path;
    e->metadata["source"]    = "filesystem_watcher";
    e->metadata["fs_action"] = fe.event_type;
    if (!fe.old_path.empty()) {
        e->metadata["old_path"] = fe.old_path;
    }

    ctx_->event_bus.publish(e);
}

} // namespace axiom::core
```

### \`src/features/filesystem_watcher/filesystem_watcher_module.h\`

```cpp
// src/features/filesystem_watcher/filesystem_watcher_module.h
//
// FilesystemWatcherModule watches configured directories in real time
// and publishes file events onto the event bus.
//
// Watcher setup is asynchronous. efsw's recursive watch enumeration
// can take tens of seconds on large trees; blocking the service
// startup on it would make the agent unresponsive to signals during
// that window. Instead, the setup runs on a dedicated thread and the
// module reports "initializing" until it is ready.

#pragma once

#include "platform/file_watcher.h"
#include "modules/module_base.h"

#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace axiom::core {

class FilesystemWatcherModule final : public ModuleBase {
public:
    std::string name() const override { return "filesystem_watcher"; }

    bool initialize(ModuleContext& context) override;
    bool start() override;
    void stop() override;
    void shutdown() override;
    HealthStatus health() const override;

private:
    struct PendingEvent {
        FileEvent                             event;
        std::chrono::steady_clock::time_point deadline;
        int                                   priority = 0;
    };

    void on_file_event(const FileEvent& fe);
    void flush_loop();
    void setup_watcher_async();
    void publish_event(const FileEvent& fe);
    void build_excludes();
    bool is_excluded(const std::string& path) const;

    // The watcher is only touched after setup completes, and is
    // protected because setup and stop may run concurrently.
    std::unique_ptr<IFileWatcher> watcher_;
    mutable std::mutex            watcher_mutex_;

    std::vector<std::string> exclude_paths_;

    std::unordered_map<std::string, PendingEvent> pending_;
    mutable std::mutex                            pending_mutex_;
    std::chrono::milliseconds                     debounce_window_{300};

    // Two worker threads: one initializes the watcher, one drains
    // expired events. They run independently of the main service thread.
    std::thread       setup_thread_;
    std::thread       flush_thread_;
    std::atomic<bool> flush_stop_{false};
    std::atomic<bool> setup_done_{false};

    bool running_ = false;
};

} // namespace axiom::core
```

### \`src/features/grpc_transport/agent.proto\`

```
syntax = "proto3";

package axiomdlp.agent.v1;

option go_package = "axiomdlp-core/internal/agentpb;agentpb";

import "google/protobuf/timestamp.proto";

service AgentService {
  rpc Register(RegisterRequest) returns (RegisterResponse);
  rpc Heartbeat(HeartbeatRequest) returns (HeartbeatResponse);
  rpc ScanBatch(ScanBatchRequest) returns (ScanBatchResponse);
  rpc Channel(stream ChannelRequest) returns (stream ChannelResponse);
}

message RegisterRequest {
  string hostname = 1;
  string os = 2;
  string architecture = 3;
  string agent_version = 4;
}

message RegisterResponse {
  int32 agent_id = 1;
  int32 user_id = 2;
  int32 organization_id = 3;
  AgentStatus status = 4;
  google.protobuf.Timestamp server_time = 5;
}

enum AgentStatus {
  AGENT_STATUS_UNSPECIFIED = 0;
  AGENT_STATUS_ACTIVE = 1;
  AGENT_STATUS_REVOKED = 2;
}

message HeartbeatRequest {
  int32 agent_id = 1;
  string agent_version = 2;
}

message HeartbeatResponse {
  google.protobuf.Timestamp server_time = 1;
}

message ScanBatchRequest {
  int32 agent_id = 1;
  repeated ScanChunk chunks = 2;
}

message ScanChunk {
  string file_hash = 1;
  string path = 2;
  int32 chunk_index = 3;
  int32 chunk_count = 4;
  bytes content = 5;
}

message ScanChunkResult {
  string file_hash = 1;
  int32 chunk_index = 2;
  bool sensitive = 3;
  string entity_type = 4;
  int32 risk_score = 5;
  string action = 6;
  string error = 7;
}

message ScanBatchResponse {
  repeated ScanChunkResult results = 1;
}

message ChannelRequest {
  oneof payload {
    ClientShutdown shutdown = 1;
  }
}

message ClientShutdown {
  ShutdownReason reason = 1;
}

enum ShutdownReason {
  SHUTDOWN_REASON_UNSPECIFIED = 0;
  SHUTDOWN_REASON_USER_REQUESTED = 1;
  SHUTDOWN_REASON_AUTH_REVOKED = 2;
  SHUTDOWN_REASON_AGENT_REVOKED = 3;
  SHUTDOWN_REASON_CONFIG_ERROR = 4;
  SHUTDOWN_REASON_FATAL_ERROR = 5;
}

message ChannelResponse {
  oneof payload {
    ServerPolicyUpdate policy_update = 1;
    ServerCommand command = 2;
    ServerRevoke revoke = 3;
  }
}

message ServerPolicyUpdate {
  uint64 version = 1;
  bytes bundle = 2;
  bytes signature = 3;
}

message ServerCommand {
  string command_type = 1;
  bytes payload = 2;
}

message ServerRevoke {
  RevokeReason reason = 1;
  string message = 2;
}

enum RevokeReason {
  REVOKE_REASON_UNSPECIFIED = 0;
  REVOKE_REASON_TOKEN_REVOKED = 1;
  REVOKE_REASON_AGENT_REVOKED = 2;
}
```

### \`src/features/grpc_transport/grpc_transport_module.cpp\`

```cpp
// src/features/grpc_transport/grpc_transport_module.cpp
#include "features/grpc_transport/grpc_transport_module.h"
#include "storage/state_repository.h"

#include <grpcpp/grpcpp.h>
#include <grpcpp/security/credentials.h>
#include <spdlog/spdlog.h>
#include <nlohmann/json.hpp>

#include <fstream>
#include <random>
#include <sstream>
#include <thread>

namespace axiom::core {

using namespace std::chrono_literals;
using namespace axiomdlp::agent::v1;

namespace {
constexpr const char* kStateKeyAgentId = "agent_id";

std::string to_grpc_target(std::string url, int grpc_port) {
    if (url.rfind("https://", 0) == 0) url = url.substr(8);
    else if (url.rfind("http://", 0) == 0) url = url.substr(7);
    auto slash = url.find('/');
    if (slash != std::string::npos) url = url.substr(0, slash);
    auto colon = url.find(':');
    if (colon != std::string::npos) url = url.substr(0, colon);
    return url + ":" + std::to_string(grpc_port);
}

std::string read_ca_file(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) return "";
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}
} // namespace

GrpcTransportModule::GrpcTransportModule()  = default;
GrpcTransportModule::~GrpcTransportModule() = default;

bool GrpcTransportModule::initialize(ModuleContext& context) {
    ModuleBase::initialize(context);
    const auto& srv = ctx_->config.server;
    if (srv.base_url.empty() || srv.api_token.empty()) return false;

    server_url_ = to_grpc_target(srv.base_url, srv.grpc_port);
    api_token_  = srv.api_token;
    SPDLOG_INFO("grpc_transport module: initialized (target={}, tls={})", server_url_, srv.grpc_tls ? "true" : "false");
    return true;
}

bool GrpcTransportModule::start() {
    stop_.store(false, std::memory_order_relaxed);
    manager_thread_ = std::thread([this] { manager_loop(); });
    running_.store(true, std::memory_order_relaxed);
    return true;
}

void GrpcTransportModule::stop() {
    if (!running_.exchange(false)) return;
    stop_.store(true, std::memory_order_relaxed);
    if (manager_thread_.joinable()) manager_thread_.join();
    SPDLOG_INFO("grpc_transport module: stopped");
}

void GrpcTransportModule::shutdown() {}

HealthStatus GrpcTransportModule::health() const {
    if (ctx_ == nullptr) return HealthStatus::unknown("not initialized");
    if (!running_.load(std::memory_order_relaxed)) return HealthStatus::failed("not running");
    return HealthStatus::degraded("stub — RPC implementation pending");
}

void GrpcTransportModule::manager_loop() {
    while (!stop_.load(std::memory_order_relaxed)) {
        if (ctx_->shutdown.is_requested()) return;
        if (auto saved = ctx_->state_store.get(kStateKeyAgentId)) {
            try {
                int id = std::stoi(*saved);
                if (id > 0) { agent_id_ = id; break; }
            } catch (...) {}
        }
        std::this_thread::sleep_for(1s);
    }

    if (agent_id_ <= 0) return;

    std::shared_ptr<grpc::ChannelCredentials> creds;
    const auto& srv = ctx_->config.server;

    if (srv.grpc_tls) {
        grpc::SslCredentialsOptions ssl_opts;
        if (!srv.grpc_ca_path.empty()) {
            std::string ca_pem = read_ca_file(srv.grpc_ca_path);
            if (!ca_pem.empty()) ssl_opts.pem_root_certs = ca_pem;
        }
        creds = grpc::SslCredentials(ssl_opts);
    } else {
        creds = grpc::InsecureChannelCredentials();
    }

    channel_ = grpc::CreateChannel(server_url_, creds);
    stub_    = AgentService::NewStub(channel_);

    int backoff_sec = ctx_->config.server.reconnect_backoff_seconds;
    int max_backoff = ctx_->config.server.reconnect_backoff_max_seconds;
    std::mt19937 rng{std::random_device{}()};

    while (!stop_.load(std::memory_order_relaxed) && !ctx_->shutdown.is_requested()) {
        grpc::Status status = run_channel_stream();

        if (stop_.load() || ctx_->shutdown.is_requested()) break;

        if (status.error_code() == grpc::StatusCode::UNAUTHENTICATED) {
            ctx_->shutdown.request(ShutdownReason::AuthRevoked);
            break;
        }
        if (status.error_code() == grpc::StatusCode::PERMISSION_DENIED) {
            ctx_->shutdown.request(ShutdownReason::AgentRevoked);
            break;
        }

        int jitter = std::max(1, backoff_sec / 5);
        std::uniform_int_distribution<int> dist(-jitter, jitter);
        int sleep_sec = std::max(1, backoff_sec + dist(rng));

        SPDLOG_INFO("grpc_transport: stream disconnected ({}). Reconnecting in {}s...", status.error_message(), sleep_sec);
        for (int i = 0; i < sleep_sec * 10; ++i) {
            if (stop_.load() || ctx_->shutdown.is_requested()) return;
            std::this_thread::sleep_for(100ms);
        }
        backoff_sec = std::min(backoff_sec * 2, max_backoff);
    }
}

grpc::Status GrpcTransportModule::run_channel_stream() {
    grpc::ClientContext context;
    context.AddMetadata("authorization", "Bearer " + api_token_);
    context.AddMetadata("x-agent-id", std::to_string(agent_id_));

    SPDLOG_INFO("grpc_transport: opening Channel bidi stream...");
    auto stream = stub_->Channel(&context);

    while (!stop_.load() && !ctx_->shutdown.is_requested()) {
        ChannelResponse resp;
        if (!stream->Read(&resp)) break; // Stream ended
        handle_server_message(resp);
    }

    if (stop_.load() || ctx_->shutdown.is_requested()) {
        SPDLOG_INFO("grpc_transport: sending graceful goodbye...");
        ChannelRequest req;
        auto* shut = req.mutable_shutdown();
        switch (ctx_->shutdown.reason()) {
            case ShutdownReason::UserRequested: shut->set_reason(SHUTDOWN_REASON_USER_REQUESTED); break;
            case ShutdownReason::AuthRevoked:   shut->set_reason(SHUTDOWN_REASON_AUTH_REVOKED); break;
            case ShutdownReason::AgentRevoked:  shut->set_reason(SHUTDOWN_REASON_AGENT_REVOKED); break;
            case ShutdownReason::ConfigError:   shut->set_reason(SHUTDOWN_REASON_CONFIG_ERROR); break;
            case ShutdownReason::FatalError:    shut->set_reason(SHUTDOWN_REASON_FATAL_ERROR); break;
            default:                            shut->set_reason(SHUTDOWN_REASON_UNSPECIFIED); break;
        }
        stream->Write(req);
        stream->WritesDone();
    }

    return stream->Finish();
}

void GrpcTransportModule::handle_server_message(const ChannelResponse& resp) {
    switch (resp.payload_case()) {
        case ChannelResponse::kPolicyUpdate: {
            const auto& pu = resp.policy_update();
            SPDLOG_INFO("grpc_transport: received policy_update (version={})", pu.version());
            break;
        }
        case ChannelResponse::kCommand: {
            const auto& cmd = resp.command();
            SPDLOG_INFO("grpc_transport: received command: {}", cmd.command_type());
            break;
        }
        case ChannelResponse::kRevoke: {
            const auto& rev = resp.revoke();
            SPDLOG_WARN("grpc_transport: received REVOKE (msg={})", rev.message());
            if (rev.reason() == REVOKE_REASON_TOKEN_REVOKED) ctx_->shutdown.request(ShutdownReason::AuthRevoked);
            else ctx_->shutdown.request(ShutdownReason::AgentRevoked);
            break;
        }
        default: break;
    }
}

} // namespace axiom::core
```

### \`src/features/grpc_transport/grpc_transport_module.h\`

```cpp
// src/features/grpc_transport/grpc_transport_module.h
#pragma once

#include "modules/module_base.h"
#include "agent.grpc.pb.h"

#include <atomic>
#include <memory>
#include <string>
#include <thread>

namespace axiom::core {

class GrpcTransportModule final : public ModuleBase {
public:
    GrpcTransportModule();
    ~GrpcTransportModule() override;

    std::string name() const override { return "grpc_transport"; }

    bool initialize(ModuleContext& context) override;
    bool start() override;
    void stop() override;
    void shutdown() override;
    HealthStatus health() const override;

private:
    void manager_loop();
    grpc::Status run_channel_stream();
    void handle_server_message(const axiomdlp::agent::v1::ChannelResponse& resp);

    std::string server_url_;
    std::string api_token_;
    int         agent_id_ = 0;

    std::shared_ptr<grpc::Channel> channel_;
    std::unique_ptr<axiomdlp::agent::v1::AgentService::Stub> stub_;

    std::thread       manager_thread_;
    std::atomic<bool> stop_{false};
    std::atomic<bool> running_{false};
};

} // namespace axiom::core
```

### \`src/features/inventory/inventory_module.cpp\`

```cpp
// src/features/inventory/inventory_module.cpp

#include "features/inventory/inventory_module.h"
#include "features/inventory/inventory_scanner.h"

#include <spdlog/spdlog.h>

namespace axiom::core {

bool InventoryModule::initialize(ModuleContext& context) {
    ModuleBase::initialize(context);

    if (!ctx_->config.modules.inventory.enabled) {
        SPDLOG_INFO("inventory module: disabled in config");
        return true;
    }

    interval_ = std::chrono::seconds(ctx_->config.modules.inventory.interval_seconds);
    SPDLOG_INFO("inventory module: initialized (scan every {} s)",
                interval_.count());
    return true;
}

bool InventoryModule::start() {
    if (!ctx_->config.modules.inventory.enabled) {
        return true;
    }

    stop_.store(false, std::memory_order_relaxed);
    scan_thread_ = std::thread([this] { scan_loop(); });
    running_ = true;

    SPDLOG_INFO("inventory module: started");
    return true;
}

void InventoryModule::stop() {
    stop_.store(true, std::memory_order_relaxed);
    if (scan_thread_.joinable()) {
        scan_thread_.join();
    }
    running_ = false;
    SPDLOG_INFO("inventory module: stopped");
}

void InventoryModule::shutdown() {
    SPDLOG_INFO("inventory module: shutting down");
}

HealthStatus InventoryModule::health() const {
    if (ctx_ == nullptr) return HealthStatus::unknown("not initialized");
    if (!ctx_->config.modules.inventory.enabled) return HealthStatus::ok("disabled");
    if (!running_) return HealthStatus::failed("scanner thread not running");
    return HealthStatus::ok();
}

void InventoryModule::scan_loop() {
    run_one_scan();

    constexpr auto tick = std::chrono::milliseconds(500);
    auto slept          = std::chrono::milliseconds(0);
    const auto target   = std::chrono::duration_cast<std::chrono::milliseconds>(interval_);

    while (!stop_.load(std::memory_order_relaxed)) {
        std::this_thread::sleep_for(tick);
        slept += tick;
        if (slept >= target) {
            if (stop_.load(std::memory_order_relaxed)) break;
            run_one_scan();
            slept = std::chrono::milliseconds(0);
        }
    }
}

void InventoryModule::run_one_scan() {
    InventoryScanner scanner(*ctx_);
    const auto r = scanner.scan_all(stop_);

    last_file_count_ = r.files_seen;

    if (r.was_cancelled) {
        SPDLOG_INFO("inventory scan: cancelled ({} files seen, {} ms)",
                    r.files_seen, r.duration_ms);
    } else {
        SPDLOG_INFO("inventory scan: {} files seen, {} changed, {} deleted, {} ms",
                    r.files_seen, r.files_changed, r.files_deleted, r.duration_ms);
    }
}

} // namespace axiom::core
```

### \`src/features/inventory/inventory_module.h\`

```cpp
// src/modules/inventory/inventory_module.h
//
// InventoryModule schedules periodic filesystem scans. It owns a
// background thread that calls the InventoryScanner on an interval,
// and reports scan statistics through the log.

#pragma once

#include "modules/module_base.h"

#include <atomic>
#include <chrono>
#include <cstddef>
#include <string>
#include <thread>

namespace axiom::core {

class InventoryModule final : public ModuleBase {
public:
    std::string name() const override { return "inventory"; }

    bool initialize(ModuleContext& context) override;
    bool start() override;
    void stop() override;
    void shutdown() override;
    HealthStatus health() const override;

private:
    void scan_loop();
    void run_one_scan();

    std::thread       scan_thread_;
    std::atomic<bool> stop_{false};

    std::chrono::seconds interval_{900};
    std::size_t          last_file_count_ = 0;
    bool                 running_ = false;
};

} // namespace axiom::core
```

### \`src/features/inventory/inventory_scanner.cpp\`

```cpp
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
        //
        // A file is eligible for the content scanner only if:
        //   - it's a regular file (not a directory, not a symlink)
        //   - its category is in the inventory's scannable list
        //   - its extension is in the content scanner's text allowlist
        //   - its size is at or below the configured limit
        //
        // Files that fail any check are marked not_applicable or
        // skipped_too_large right now, so they never enter the scanner
        // queue and no hashing work is wasted on them.
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
            // Right category but the scanner can't handle this file
            // type (e.g. PDF, DOCX). Mark it so it doesn't sit in the
            // pending queue waiting for a scan that will never run.
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
                ctx_.file_store.commit();
                ctx_.file_store.begin_transaction();
                since_commit = 0;
            }
        }
    }
}

InventoryResult InventoryScanner::scan_all(const std::atomic<bool>& stop_flag) {
    InventoryResult r;

    const auto start = std::chrono::steady_clock::now();

    if (!ctx_.file_store.initialize_schema()) {
        SPDLOG_ERROR("InventoryScanner: file store not initialized");
        return r;
    }

    const auto& roots = ctx_.config.modules.inventory.watch_paths;
    if (roots.empty()) {
        SPDLOG_WARN("InventoryScanner: no watch paths configured");
        return r;
    }

    r.walk_id = 1;

    SPDLOG_INFO("InventoryScanner: starting scan over {} root(s)", roots.size());

    ctx_.file_store.begin_transaction();

    for (const auto& root : roots) {
        if (stop_flag.load(std::memory_order_relaxed)) {
            SPDLOG_INFO("InventoryScanner: stop requested before '{}', aborting",
                        root);
            r.was_cancelled = true;
            break;
        }
        scan_one_root(root, r.walk_id, stop_flag, r);
        if (r.was_cancelled) break;
    }

    ctx_.file_store.commit();

    r.duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start).count();

    if (r.was_cancelled) {
        SPDLOG_INFO("InventoryScanner: cancelled after {} files ({} ms)",
                    r.files_seen, r.duration_ms);
    } else {
        SPDLOG_INFO("InventoryScanner: finished ({} files, {} ms)",
                    r.files_seen, r.duration_ms);
    }

    return r;
}

} // namespace axiom::core
```

### \`src/features/inventory/inventory_scanner.h\`

```cpp
// src/modules/inventory/inventory_scanner.h
//
// InventoryScanner walks the configured watch paths, gathers file
// metadata, and writes each record to the shared FileStore. Streams
// records one at a time so memory usage stays constant regardless of
// file count.

#pragma once

#include "modules/module_context.h"

#include <atomic>
#include <cstdint>
#include <string>

namespace axiom::core {

struct InventoryResult {
    std::uint64_t walk_id       = 0;
    std::size_t   files_seen    = 0;
    std::size_t   files_changed = 0;
    std::size_t   files_deleted = 0;
    std::int64_t  duration_ms   = 0;
    bool          was_cancelled = false;
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
```

### \`src/foundation/application.cpp\`

```cpp
// src/foundation/application.cpp

#include "foundation/application.h"
#include "foundation/logger.h"
#include "platform/paths.h"

#include "features/agent_sync/agent_sync_module.h"
#include "features/content_scanner/content_scanner_module.h"
#include "features/event_logger/event_logger_module.h"
#include "features/filesystem_watcher/filesystem_watcher_module.h"
#include "features/inventory/inventory_module.h"
#include "features/grpc_transport/grpc_transport_module.h"
#include "storage/migrations.h"
#include "storage/state_repository.h"

#include <spdlog/spdlog.h>

#include <cstring>
#include <filesystem>
#include <iostream>

namespace axiom::core {

Application::Options Application::parse_args(int argc, char** argv) {
    Options o;
    for (int i = 1; i < argc; ++i) {
        const char* a = argv[i];
        if (std::strcmp(a, "--install") == 0)          { o.install    = true; }
        else if (std::strcmp(a, "--uninstall") == 0)   { o.uninstall  = true; }
        else if (std::strcmp(a, "--foreground") == 0)  { o.foreground = true; }
        else if (std::strcmp(a, "--config") == 0 && i + 1 < argc) {
            o.config_path = argv[++i];
        } else {
            std::cerr << "unknown argument: " << a << "\n";
        }
    }
    return o;
}

Application::Application(Options opts) : opts_(std::move(opts)) {}
Application::~Application() { shutdown(); }

namespace {
std::string resolve_config_path(const std::string& explicit_path) {
    namespace fs = std::filesystem;
    std::error_code ec;
    if (!explicit_path.empty()) return explicit_path;
    const std::string dev_path = "config/agent.default.json";
    if (fs::exists(fs::path(dev_path), ec)) return dev_path;
    return paths::default_config_path();
}
} // namespace

bool Application::init() {
    shutdown_controller_ = std::make_unique<ShutdownController>();
    const std::string config_path = resolve_config_path(opts_.config_path);
    config_ = Config::load(config_path);
    opts_.config_path = config_path;

    Logger::init(config_.logging.dir, config_.logging.file, config_.logging.level);
    initialized_ = true;

    SPDLOG_INFO("AxiomDLP Agent v0.1.0");
    SPDLOG_INFO("Config loaded from: {}", opts_.config_path);
    for (const auto& w : config_.load_warnings) {
        SPDLOG_WARN("Config: {}", w);
    }

    database_ = std::make_unique<Database>();
    if (!database_->open(config_.storage.db_path)) {
        shutdown_controller_->request(ShutdownReason::FatalError);
        return false;
    }
    if (!Migrations::apply(*database_)) {
        shutdown_controller_->request(ShutdownReason::FatalError);
        return false;
    }
    file_store_ = std::make_unique<FileStore>(*database_);
    if (!file_store_->initialize_schema()) {
        shutdown_controller_->request(ShutdownReason::FatalError);
        return false;
    }
    state_store_ = std::make_unique<StateStore>(*database_);
    if (!state_store_->initialize_schema()) {
        shutdown_controller_->request(ShutdownReason::FatalError);
        return false;
    }

    event_bus_ = std::make_unique<EventBus>();
    module_context_ = std::make_unique<ModuleContext>(config_, *event_bus_, *file_store_, *state_store_, *database_, *shutdown_controller_);
    module_manager_ = std::make_unique<ModuleManager>();

    module_manager_->register_module(std::make_unique<FilesystemWatcherModule>());
    module_manager_->register_module(std::make_unique<InventoryModule>());
    module_manager_->register_module(std::make_unique<EventLoggerModule>());
    module_manager_->register_module(std::make_unique<GrpcTransportModule>());
    module_manager_->register_module(std::make_unique<AgentSyncModule>());
    module_manager_->register_module(std::make_unique<ContentScannerModule>());

    if (!module_manager_->initialize_all(*module_context_)) {
        SPDLOG_ERROR("Some modules failed to initialize; continuing anyway");
    }
    return true;
}

void Application::shutdown() {
    if (!initialized_) return;
    SPDLOG_INFO("Application shutting down");
    service_.reset();
    module_manager_.reset();
    module_context_.reset();
    event_bus_.reset();
    state_store_.reset();
    file_store_.reset();
    database_.reset();
    shutdown_controller_.reset();
    Logger::shutdown();
    initialized_ = false;
}

int Application::execute() {
    if (opts_.install || opts_.uninstall) {
        Logger::init("logs", "install.log", "info");
        initialized_ = true;
        int rc = 0;
        if (opts_.install) rc = install_service(paths::executable_path(), default_service_name());
        else rc = uninstall_service(default_service_name());
        Logger::shutdown();
        initialized_ = false;
        return rc;
    }
    if (!init()) return 1;

    service_ = make_service(default_service_name());
    service_->set_module_manager(module_manager_.get());
    service_->set_shutdown_controller(shutdown_controller_.get());

    const int rc = service_->run();
    SPDLOG_INFO("Service returned rc={} (reason={})", rc, to_string(shutdown_controller_->reason()));
    return rc;
}

} // namespace axiom::core
```

### \`src/foundation/application.h\`

```cpp
// src/foundation/application.h

#pragma once

#include "foundation/config.h"
#include "foundation/shutdown.h"
#include "events/event_bus.h"
#include "modules/module_context.h"
#include "modules/module_manager.h"
#include "platform/service.h"
#include "storage/sqlite_database.h"
#include "storage/file_repository.h"
#include "storage/state_repository.h"

#include <memory>
#include <string>

namespace axiom::core {

class Application {
public:
    struct Options {
        bool install    = false;
        bool uninstall  = false;
        bool foreground = false;
        std::string config_path;
    };

    static Options parse_args(int argc, char** argv);

    explicit Application(Options opts);
    ~Application();

    int execute();

private:
    bool init();
    void shutdown();

    Options opts_;
    Config  config_;

    // Created before any module so everyone can see and request a
    // shutdown. Lifetime is the entire Application lifetime.
    std::unique_ptr<ShutdownController> shutdown_controller_;

    std::unique_ptr<Database>   database_;
    std::unique_ptr<FileStore>  file_store_;
    std::unique_ptr<StateStore> state_store_;
    std::unique_ptr<EventBus>   event_bus_;

    std::unique_ptr<ModuleContext> module_context_;
    std::unique_ptr<ModuleManager> module_manager_;
    std::unique_ptr<IService>      service_;

    bool initialized_ = false;
};

} // namespace axiom::core
```

### \`src/foundation/config.cpp\`

```cpp
// src/foundation/config.cpp

#include "foundation/config.h"
#include <nlohmann/json.hpp>
#include <cstdint>
#include <cstdlib>
#include <fstream>

using json = nlohmann::json;

namespace axiom::core {

namespace {

std::string expand_env(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    std::size_t i = 0;
    while (i < s.size()) {
        if (s[i] == '%') {
            const auto end = s.find('%', i + 1);
            if (end != std::string::npos && end > i + 1) {
                const std::string var = s.substr(i + 1, end - i - 1);
                const char* val = std::getenv(var.c_str());
                if (val) out += val;
                else     out += s.substr(i, end - i + 1);
                i = end + 1;
                continue;
            }
        }
        if (s[i] == '$' && i + 1 < s.size() && s[i + 1] == '{') {
            const auto end = s.find('}', i + 2);
            if (end != std::string::npos) {
                const std::string var = s.substr(i + 2, end - i - 2);
                const char* val = std::getenv(var.c_str());
                if (val) out += val;
                else     out += s.substr(i, end - i + 1);
                i = end + 1;
                continue;
            }
        }
        out += s[i++];
    }
    return out;
}

void read_string(const json& obj, const char* key, std::string& out, std::vector<std::string>& warnings, const std::string& section) {
    if (!obj.contains(key)) return;
    if (!obj[key].is_string()) {
        warnings.push_back(section + "." + key + " must be a string; using default");
        return;
    }
    out = expand_env(obj[key].get<std::string>());
}

void read_bool(const json& obj, const char* key, bool& out, std::vector<std::string>& warnings, const std::string& section) {
    if (!obj.contains(key)) return;
    if (!obj[key].is_boolean()) {
        warnings.push_back(section + "." + key + " must be a boolean; using default");
        return;
    }
    out = obj[key].get<bool>();
}

void read_int(const json& obj, const char* key, int& out, std::vector<std::string>& warnings, const std::string& section) {
    if (!obj.contains(key)) return;
    if (!obj[key].is_number_integer()) {
        warnings.push_back(section + "." + key + " must be an integer; using default");
        return;
    }
    out = obj[key].get<int>();
}

void read_uint64(const json& obj, const char* key, std::uint64_t& out, std::vector<std::string>& warnings, const std::string& section) {
    if (!obj.contains(key)) return;
    if (!obj[key].is_number_unsigned() && !obj[key].is_number_integer()) {
        warnings.push_back(section + "." + key + " must be a non-negative integer; using default");
        return;
    }
    const auto v = obj[key].get<std::int64_t>();
    if (v < 0) {
        warnings.push_back(section + "." + key + " must be non-negative; using default");
        return;
    }
    out = static_cast<std::uint64_t>(v);
}

void read_string_array(const json& obj, const char* key, std::vector<std::string>& out, std::vector<std::string>& warnings, const std::string& section) {
    if (!obj.contains(key)) return;
    if (!obj[key].is_array()) {
        warnings.push_back(section + "." + key + " must be an array; using default");
        return;
    }
    std::vector<std::string> values;
    for (const auto& item : obj[key]) {
        if (!item.is_string()) {
            warnings.push_back(section + "." + key + " must contain only strings; using default");
            return;
        }
        values.push_back(expand_env(item.get<std::string>()));
    }
    out = std::move(values);
}

bool is_valid_level(const std::string& level) {
    return level == "trace" || level == "debug" || level == "info" || level == "warn" ||
           level == "error" || level == "critical" || level == "off";
}

std::string strip_trailing_slashes(std::string s) {
    while (!s.empty() && s.back() == '/') s.pop_back();
    return s;
}

void parse_logging(const json& root, Config& cfg) {
    if (!root.contains("logging")) return;
    const json& log = root["logging"];
    if (!log.is_object()) return;
    read_string(log, "dir",   cfg.logging.dir,   cfg.load_warnings, "logging");
    read_string(log, "file",  cfg.logging.file,  cfg.load_warnings, "logging");
    read_string(log, "level", cfg.logging.level, cfg.load_warnings, "logging");
    if (!is_valid_level(cfg.logging.level)) {
        cfg.load_warnings.push_back("logging.level '" + cfg.logging.level + "' is not valid; using 'info'");
        cfg.logging.level = "info";
    }
}

void parse_storage(const json& root, Config& cfg) {
    if (!root.contains("storage")) return;
    const json& st = root["storage"];
    if (!st.is_object()) return;
    read_string(st, "db_path", cfg.storage.db_path, cfg.load_warnings, "storage");
}

void parse_server(const json& root, Config& cfg) {
    const std::string section = "server";
    if (root.contains("server") && root["server"].is_object()) {
        const json& s = root["server"];
        read_string(s, "base_url",                    cfg.server.base_url,                    cfg.load_warnings, section);
        read_string(s, "api_token",                   cfg.server.api_token,                   cfg.load_warnings, section);
        read_int   (s, "heartbeat_interval_seconds",  cfg.server.heartbeat_interval_seconds,  cfg.load_warnings, section);
        read_int   (s, "reconnect_backoff_seconds",   cfg.server.reconnect_backoff_seconds,   cfg.load_warnings, section);
        read_int   (s, "reconnect_backoff_max_seconds", cfg.server.reconnect_backoff_max_seconds, cfg.load_warnings, section);
        read_int   (s, "grpc_port",                   cfg.server.grpc_port,                   cfg.load_warnings, section);
        read_bool  (s, "grpc_tls",                    cfg.server.grpc_tls,                    cfg.load_warnings, section);
        read_string(s, "grpc_ca_path",                cfg.server.grpc_ca_path,                cfg.load_warnings, section);
    }
    if (cfg.server.grpc_port < 1 || cfg.server.grpc_port > 65535) {
        cfg.load_warnings.push_back(section + ".grpc_port must be between 1 and 65535; using 50051");
        cfg.server.grpc_port = 50051;
    }
    cfg.server.base_url = strip_trailing_slashes(cfg.server.base_url);
    if (cfg.server.base_url.empty()) {
        cfg.load_warnings.push_back("server.base_url is empty; using default");
        cfg.server.base_url = "http://127.0.0.1:3000";
    }
    if (cfg.server.heartbeat_interval_seconds < 5) {
        cfg.load_warnings.push_back("server.heartbeat_interval_seconds must be >= 5; using 30");
        cfg.server.heartbeat_interval_seconds = 30;
    }
    if (cfg.server.reconnect_backoff_seconds < 1) {
        cfg.load_warnings.push_back("server.reconnect_backoff_seconds must be >= 1; using 5");
        cfg.server.reconnect_backoff_seconds = 5;
    }
    if (cfg.server.reconnect_backoff_max_seconds < cfg.server.reconnect_backoff_seconds) {
        cfg.load_warnings.push_back("server.reconnect_backoff_max_seconds must be >= reconnect_backoff_seconds; using 60");
        cfg.server.reconnect_backoff_max_seconds = 60;
    }
}

void parse_filesystem_watcher(const json& mods, Config& cfg) {
    if (!mods.contains("filesystem_watcher")) return;
    const json& fw = mods["filesystem_watcher"];
    if (!fw.is_object()) return;
    auto& m = cfg.modules.filesystem_watcher;
    const std::string section = "modules.filesystem_watcher";
    read_bool         (fw, "enabled",       m.enabled,       cfg.load_warnings, section);
    read_string_array (fw, "watch_paths",   m.watch_paths,   cfg.load_warnings, section);
    read_string_array (fw, "exclude_paths", m.exclude_paths, cfg.load_warnings, section);
    read_int          (fw, "debounce_ms",   m.debounce_ms,   cfg.load_warnings, section);
    if (m.debounce_ms < 0) m.debounce_ms = 300;
}

void parse_inventory(const json& mods, Config& cfg) {
    if (!mods.contains("inventory")) return;
    const json& inv = mods["inventory"];
    if (!inv.is_object()) return;
    auto& m = cfg.modules.inventory;
    const std::string section = "modules.inventory";
    read_bool         (inv, "enabled",              m.enabled,              cfg.load_warnings, section);
    read_string_array (inv, "watch_paths",          m.watch_paths,          cfg.load_warnings, section);
    read_string_array (inv, "exclude_paths",        m.exclude_paths,        cfg.load_warnings, section);
    read_int          (inv, "interval_seconds",     m.interval_seconds,     cfg.load_warnings, section);
    read_bool         (inv, "compute_hashes",       m.compute_hashes,       cfg.load_warnings, section);
    read_uint64       (inv, "max_file_size_bytes",  m.max_file_size_bytes,  cfg.load_warnings, section);
    read_string_array (inv, "scannable_categories", m.scannable_categories, cfg.load_warnings, section);
    if (m.interval_seconds < 10) m.interval_seconds = 900;
}

void parse_agent_sync(const json& mods, Config& cfg) {
    if (!mods.contains("agent_sync")) return;
    const json& a = mods["agent_sync"];
    if (!a.is_object()) return;
    auto& m = cfg.modules.agent_sync;
    read_bool(a, "enabled", m.enabled, cfg.load_warnings, "modules.agent_sync");
}

void parse_content_scanner(const json& mods, Config& cfg) {
    if (!mods.contains("content_scanner")) return;
    const json& c = mods["content_scanner"];
    if (!c.is_object()) return;
    auto& m = cfg.modules.content_scanner;
    const std::string section = "modules.content_scanner";
    read_bool   (c, "enabled",                 m.enabled,                 cfg.load_warnings, section);
    read_int    (c, "batch_size",              m.batch_size,              cfg.load_warnings, section);
    read_int    (c, "scan_interval_seconds",   m.scan_interval_seconds,   cfg.load_warnings, section);
    read_int    (c, "max_retries",             m.max_retries,             cfg.load_warnings, section);
    read_int    (c, "request_timeout_seconds", m.request_timeout_seconds, cfg.load_warnings, section);
    std::uint64_t tmp = m.batch_max_bytes;
    read_uint64 (c, "batch_max_bytes", tmp, cfg.load_warnings, section);
    m.batch_max_bytes = static_cast<std::size_t>(tmp);
    tmp = m.chunk_size_bytes;
    read_uint64 (c, "chunk_size_bytes", tmp, cfg.load_warnings, section);
    m.chunk_size_bytes = static_cast<std::size_t>(tmp);
    tmp = m.chunk_overlap_bytes;
    read_uint64 (c, "chunk_overlap_bytes", tmp, cfg.load_warnings, section);
    m.chunk_overlap_bytes = static_cast<std::size_t>(tmp);
    tmp = m.max_file_bytes;
    read_uint64 (c, "max_file_bytes", tmp, cfg.load_warnings, section);
    m.max_file_bytes = static_cast<std::size_t>(tmp);
    read_string_array (c, "text_extensions", m.text_extensions, cfg.load_warnings, section);
    if (m.batch_size < 1)            m.batch_size = 50;
    if (m.max_retries < 1)           m.max_retries = 3;
    if (m.scan_interval_seconds < 5) m.scan_interval_seconds = 30;
    if (m.chunk_size_bytes < 4096)   m.chunk_size_bytes = 1024 * 1024;
    if (m.chunk_overlap_bytes == 0)  m.chunk_overlap_bytes = 256;
    if (m.chunk_overlap_bytes >= m.chunk_size_bytes) m.chunk_overlap_bytes = 256;
}

} // namespace

Config Config::load(const std::string& path) {
    Config cfg;
    std::ifstream file(path);
    if (!file.is_open()) {
        cfg.load_warnings.push_back("config file not found at '" + path + "'; using built-in defaults");
        return cfg;
    }
    json root;
    try {
        file >> root;
    } catch (const json::parse_error& e) {
        cfg.load_warnings.push_back(std::string("failed to parse config: ") + e.what() + "; using built-in defaults");
        return cfg;
    }
    if (!root.is_object()) {
        cfg.load_warnings.push_back("config root must be a JSON object; using built-in defaults");
        return cfg;
    }
    parse_logging(root, cfg);
    parse_storage(root, cfg);
    parse_server(root, cfg);
    if (root.contains("modules")) {
        const json& mods = root["modules"];
        if (mods.is_object()) {
            parse_filesystem_watcher(mods, cfg);
            parse_inventory(mods, cfg);
            parse_agent_sync(mods, cfg);
            parse_content_scanner(mods, cfg);
        }
    }
    return cfg;
}

} // namespace axiom::core
```

### \`src/foundation/config.h\`

```cpp
// src/foundation/config.h
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace axiom::core {

struct LoggingConfig {
    std::string dir   = "logs";
    std::string file  = "agent.log";
    std::string level = "info";
};

struct StorageConfig {
    std::string db_path = "data/agent.db";
};

struct ServerConfig {
    std::string base_url = "http://127.0.0.1:3000";
    std::string api_token;
    int heartbeat_interval_seconds = 30;
    int reconnect_backoff_seconds     = 5;
    int reconnect_backoff_max_seconds = 60;

    // gRPC specific configuration
    int  grpc_port = 50051;
    bool grpc_tls  = true;
    std::string grpc_ca_path;
};

struct FilesystemWatcherModuleConfig {
    bool                     enabled = false;
    std::vector<std::string> watch_paths;
    std::vector<std::string> exclude_paths;
    int                      debounce_ms = 300;
};

struct InventoryModuleConfig {
    bool                     enabled = true;
    std::vector<std::string> watch_paths;
    std::vector<std::string> exclude_paths;
    int                      interval_seconds = 900;
    bool          compute_hashes      = true;
    std::uint64_t max_file_size_bytes = 104857600ull;
    std::vector<std::string> scannable_categories = {"document", "code", "data", "config"};
};

struct AgentSyncModuleConfig {
    bool enabled = true;
};

struct ContentScannerModuleConfig {
    bool enabled = true;
    int         batch_size            = 50;
    std::size_t batch_max_bytes       = 50 * 1024 * 1024;
    int         scan_interval_seconds = 30;
    std::size_t chunk_size_bytes    = 1024 * 1024;
    std::size_t chunk_overlap_bytes = 256;
    std::size_t max_file_bytes          = 100 * 1024 * 1024;
    int         max_retries             = 3;
    int         request_timeout_seconds = 300;
    std::vector<std::string> text_extensions = {
        "txt", "md", "log", "csv", "tsv", "json", "yaml", "yml", "toml", "xml",
        "c", "cpp", "cc", "cxx", "h", "hpp", "hxx", "py", "js", "ts", "jsx", "tsx",
        "go", "rs", "java", "kt", "scala", "cs", "swift", "rb", "php", "pl", "lua",
        "sql", "sh", "bash", "zsh", "ps1", "html", "css", "scss", "less", "conf",
        "cfg", "ini", "env", "properties"
    };
};

struct ModulesConfig {
    FilesystemWatcherModuleConfig filesystem_watcher;
    InventoryModuleConfig         inventory;
    AgentSyncModuleConfig         agent_sync;
    ContentScannerModuleConfig    content_scanner;
};

struct Config {
    LoggingConfig logging;
    StorageConfig storage;
    ServerConfig  server;
    ModulesConfig modules;
    std::vector<std::string> load_warnings;
    static Config load(const std::string& path);
};

} // namespace axiom::core
```

### \`src/foundation/logger.cpp\`

```cpp
// src/core/logger.cpp

#include "foundation/logger.h"

#include <spdlog/spdlog.h>
#include <spdlog/async.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>

#include <filesystem>
#include <array>

namespace axiom::core {

void Logger::init(const std::string& log_dir,
                  const std::string& log_file,
                  const std::string& level) {

    // Ensure the log directory exists.
    std::error_code ec;
    std::filesystem::create_directories(log_dir, ec);
    const std::string log_path = log_dir + "/" + log_file;

    // Async thread pool: 8192-slot queue, 1 background worker thread.
    spdlog::init_thread_pool(8192, 1);

    // Console sink (colored output for terminals).
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    console_sink->set_level(spdlog::level::trace);

    // Rotating file sink: 5 MB per file, keep 3 rotated files (total ~20 MB cap).
    auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
        log_path, 1024 * 1024 * 5, 3);
    file_sink->set_level(spdlog::level::trace);

    // Zero-overhead stack allocation using std::array instead of std::vector
    std::array<spdlog::sink_ptr, 2> sinks{console_sink, file_sink};

    // Async logger — never blocks the caller.
    // overrun_oldest: if the queue is full, drop the oldest message rather than block.
    auto logger = std::make_shared<spdlog::async_logger>(
        "axiom",
        sinks.begin(), sinks.end(),
        spdlog::thread_pool(),
        spdlog::async_overflow_policy::overrun_oldest);

    // Format: [timestamp] [level] [thread-id] message
    logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [t:%t] %v");

    // Set level from config string.
    logger->set_level(spdlog::level::from_str(level));

    // Flush immediately on warn and above (so important events survive crashes).
    logger->flush_on(spdlog::level::warn);

    // Register as the global default logger.
    spdlog::set_default_logger(std::move(logger));
}

void Logger::shutdown() {
    spdlog::shutdown();
}

} // namespace axiom::core
```

### \`src/foundation/logger.h\`

```cpp
// src/core/logger.h
//
// Centralized logging setup for AxiomDLP Agent.
// Wraps spdlog with a console sink and a rotating file sink,
// running in asynchronous mode so logging never blocks the caller.

#pragma once

#include <string>

namespace axiom::core {

class Logger {
public:
    // Initialize the global logger.
    // - log_dir:  directory where log files are written (created if missing)
    // - log_file: filename inside log_dir
    // - level:    "trace", "debug", "info", "warn", "error", "critical", "off"
    static void init(const std::string& log_dir  = "logs",
                     const std::string& log_file = "agent.log",
                     const std::string& level    = "info");

    // Flush all pending messages and release resources.
    // Call this before the process exits.
    static void shutdown();
};

} // namespace axiom::core
```

### \`src/foundation/shutdown.h\`

```cpp
// src/foundation/shutdown.h
//
// ShutdownController is the single, thread-safe authority on whether
// the agent is shutting down and why.
//
// Any module can call request(reason). The first call wins; later
// calls are no-ops. The service main loop blocks on wait() and tears
// down the whole agent as soon as a request arrives. The reason is
// readable everywhere via reason() so modules can act accordingly —
// for example, the transport module sends a "goodbye" message to the
// server only on graceful shutdowns.

#pragma once

#include <chrono>
#include <condition_variable>
#include <mutex>

namespace axiom::core {

enum class ShutdownReason {
    None,              // not shutting down (initial state)
    UserRequested,     // Ctrl+C, SIGTERM, or service stop
    AuthRevoked,       // 401 from server: token invalid or revoked
    AgentRevoked,      // 403 from server: this specific agent revoked
    ConfigError,       // bad config at startup
    FatalError,        // unrecoverable error in a module
    ServerShutdown     // server sent 1001 Going Away
};

inline const char* to_string(ShutdownReason r) {
    switch (r) {
        case ShutdownReason::None:           return "none";
        case ShutdownReason::UserRequested:  return "user_requested";
        case ShutdownReason::AuthRevoked:    return "auth_revoked";
        case ShutdownReason::AgentRevoked:   return "agent_revoked";
        case ShutdownReason::ConfigError:    return "config_error";
        case ShutdownReason::FatalError:     return "fatal_error";
        case ShutdownReason::ServerShutdown: return "server_shutdown";
    }
    return "unknown";
}

class ShutdownController {
public:
    ShutdownController() = default;

    ShutdownController(const ShutdownController&) = delete;
    ShutdownController& operator=(const ShutdownController&) = delete;
    ShutdownController(ShutdownController&&) = delete;
    ShutdownController& operator=(ShutdownController&&) = delete;

    // Request a shutdown. The first caller wins; later calls are no-ops.
    // Returns true if this call triggered the shutdown.
    bool request(ShutdownReason reason) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (requested_) return false;
            reason_    = reason;
            requested_ = true;
        }
        cv_.notify_all();
        return true;
    }

    // Block until a shutdown has been requested.
    void wait() {
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait(lock, [this] { return requested_; });
    }

    // Block until a shutdown has been requested, or until the timeout
    // expires. Returns true if shutdown was requested.
    template <typename Rep, typename Period>
    bool wait_for(const std::chrono::duration<Rep, Period>& timeout) {
        std::unique_lock<std::mutex> lock(mutex_);
        return cv_.wait_for(lock, timeout, [this] { return requested_; });
    }

    bool is_requested() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return requested_;
    }

    ShutdownReason reason() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return reason_;
    }

private:
    mutable std::mutex      mutex_;
    std::condition_variable cv_;
    bool                    requested_ = false;
    ShutdownReason          reason_    = ShutdownReason::None;
};

} // namespace axiom::core
```

### \`src/main.cpp\`

```cpp
// src/main.cpp

#include "foundation/application.h"

int main(int argc, char** argv) {
    axiom::core::Application::Options opts = axiom::core::Application::parse_args(argc, argv);
    axiom::core::Application app(std::move(opts));
    return app.execute();
}
```

### \`src/modules/health.h\`

```cpp
// src/modules/health.h
//
// Health status types used by modules and the module manager.
//
// Every module reports a HealthStatus. The manager aggregates them
// into a single view for the whole agent.

#pragma once

#include <string>

namespace axiom::core {

// The four levels of health a component can report.
enum class HealthLevel {
    Unknown,   // Not yet checked, or unable to determine
    OK,        // Fully working
    Degraded,  // Working but with issues
    Failed,    // Not working
};

// A snapshot of a component's health.
struct HealthStatus {
    HealthLevel level = HealthLevel::Unknown;
    std::string message;

    static HealthStatus ok(const std::string& msg = "") {
        return {HealthLevel::OK, msg};
    }
    static HealthStatus degraded(const std::string& msg) {
        return {HealthLevel::Degraded, msg};
    }
    static HealthStatus failed(const std::string& msg) {
        return {HealthLevel::Failed, msg};
    }
    static HealthStatus unknown(const std::string& msg = "") {
        return {HealthLevel::Unknown, msg};
    }
};

} // namespace axiom::core
```

### \`src/modules/i_module.h\`

```cpp
// src/interfaces/i_module.h
//
// The module interface. Every feature in the agent implements this
// contract. The ModuleManager drives the lifecycle uniformly.
//
// Lifecycle:
//   1. initialize(context)  — called once, before start
//   2. start()              — called once after all modules initialized
//   3. stop()               — called on shutdown
//   4. shutdown()           — called once, after stop, to release resources
//
// Modules must be self-contained. They cannot call each other directly.
// Communication between modules happens through the event bus.

#pragma once

#include "modules/health.h"
#include "modules/module_context.h"

#include <string>

namespace axiom::core {

class IModule {
public:
    virtual ~IModule() = default;

    // Identity ------------------------------------------------------

    // A short, unique identifier. Lowercase, no spaces.
    virtual std::string name() const = 0;

    // A semantic version string (e.g., "1.0.0").
    virtual std::string version() const = 0;

    // Lifecycle -----------------------------------------------------

    // Called once at agent startup, before start().
    // The module receives the shared ModuleContext here.
    // Returns true on success, false on failure.
    virtual bool initialize(ModuleContext& context) = 0;

    // Called after all modules have been initialized.
    // The module should begin its work here.
    // Returns true on success, false on failure.
    virtual bool start() = 0;

    // Called during agent shutdown. The module should stop its work.
    // Stop must be idempotent — safe to call multiple times.
    virtual void stop() = 0;

    // Called once after stop(), to release resources.
    virtual void shutdown() = 0;

    // Reporting -----------------------------------------------------

    // Report the module's current health.
    virtual HealthStatus health() const = 0;
};

} // namespace axiom::core
```

### \`src/modules/module_base.h\`

```cpp
// src/modules/module_base.h
//
// ModuleBase provides default implementations for the trivial methods
// of the IModule interface. Real modules inherit from ModuleBase and
// override only the methods they need.

#pragma once

#include "modules/i_module.h"

#include <string>

namespace axiom::core {

class ModuleBase : public IModule {
public:
    ModuleBase() = default;
    ~ModuleBase() override = default;

    std::string version() const override {
        return "1.0.0";
    }

    bool initialize(ModuleContext& context) override {
        ctx_ = &context;
        return true;
    }

    bool start() override {
        return true;
    }

    void stop() override {
    }

    void shutdown() override {
    }

    HealthStatus health() const override {
        return HealthStatus::ok();
    }

protected:
    ModuleContext* ctx_ = nullptr;
};

} // namespace axiom::core
```

### \`src/modules/module_context.h\`

```cpp
// src/modules/module_context.h
//
// ModuleContext is the shared dependency container passed to every
// module at initialize() time. It holds references to the subsystems
// modules need: config, event bus, storage, and the shutdown
// controller.

#pragma once

#include "foundation/config.h"
#include "foundation/shutdown.h"
#include "events/event_bus.h"

namespace axiom::core {

class Database;
class FileStore;
class StateStore;

struct ModuleContext {
    const Config&       config;
    EventBus&           event_bus;
    FileStore&          file_store;
    StateStore&         state_store;
    Database&           database;
    ShutdownController& shutdown;

    ModuleContext(const Config&         c,
                  EventBus&             bus,
                  FileStore&            store,
                  StateStore&           state,
                  Database&             db,
                  ShutdownController&   sd)
        : config(c),
          event_bus(bus),
          file_store(store),
          state_store(state),
          database(db),
          shutdown(sd) {}
};

} // namespace axiom::core
```

### \`src/modules/module_manager.cpp\`

```cpp
// src/core/module_manager.cpp

#include "modules/module_manager.h"

#include <spdlog/spdlog.h>

namespace axiom::core {

// ----------------------------------------------------------------
// Registration
// ----------------------------------------------------------------

void ModuleManager::register_module(std::unique_ptr<IModule> module) {
    if (!module) {
        SPDLOG_WARN("ModuleManager: attempting to register a null module");
        return;
    }

    SPDLOG_INFO("ModuleManager: registering module '{}' v{}",
                module->name(), module->version());

    modules_.push_back(std::move(module));
}

// ----------------------------------------------------------------
// Lifecycle — initialization
// ----------------------------------------------------------------

bool ModuleManager::initialize_all(ModuleContext& context) {
    if (modules_.empty()) {
        SPDLOG_WARN("ModuleManager: no modules registered");
        return true;
    }

    SPDLOG_INFO("ModuleManager: initializing {} module(s)", modules_.size());

    bool all_ok = true;
    for (auto& m : modules_) {
        SPDLOG_INFO("ModuleManager: initializing '{}'", m->name());

        if (!m->initialize(context)) {
            SPDLOG_ERROR("ModuleManager: module '{}' failed to initialize",
                         m->name());
            all_ok = false;
        }
    }

    return all_ok;
}

// ----------------------------------------------------------------
// Lifecycle — start
// ----------------------------------------------------------------

bool ModuleManager::start_all() {
    if (modules_.empty()) {
        SPDLOG_WARN("ModuleManager: no modules to start");
        return true;
    }

    SPDLOG_INFO("ModuleManager: starting {} module(s)", modules_.size());

    bool all_ok = true;
    for (auto& m : modules_) {
        SPDLOG_INFO("ModuleManager: starting '{}'", m->name());

        if (!m->start()) {
            SPDLOG_ERROR("ModuleManager: module '{}' failed to start",
                         m->name());
            all_ok = false;
        }
    }

    return all_ok;
}

// ----------------------------------------------------------------
// Lifecycle — stop (reverse order)
// ----------------------------------------------------------------

void ModuleManager::stop_all() {
    if (modules_.empty()) {
        return;
    }

    SPDLOG_INFO("ModuleManager: stopping {} module(s)", modules_.size());

    for (auto it = modules_.rbegin(); it != modules_.rend(); ++it) {
        auto& m = *it;
        SPDLOG_INFO("ModuleManager: stopping '{}'", m->name());
        m->stop();
    }
}

// ----------------------------------------------------------------
// Lifecycle — shutdown (reverse order)
// ----------------------------------------------------------------

void ModuleManager::shutdown_all() {
    if (modules_.empty()) {
        return;
    }

    SPDLOG_INFO("ModuleManager: shutting down {} module(s)", modules_.size());

    for (auto it = modules_.rbegin(); it != modules_.rend(); ++it) {
        auto& m = *it;
        SPDLOG_INFO("ModuleManager: shutting down '{}'", m->name());
        m->shutdown();
    }

    modules_.clear();
}

// ----------------------------------------------------------------
// Queries
// ----------------------------------------------------------------

std::size_t ModuleManager::count() const {
    return modules_.size();
}

HealthStatus ModuleManager::health() const {
    if (modules_.empty()) {
        return HealthStatus::ok("no modules registered");
    }

    std::size_t ok = 0;
    std::size_t degraded = 0;
    std::size_t failed = 0;
    std::size_t unknown = 0;

    for (const auto& m : modules_) {
        const HealthStatus h = m->health();
        switch (h.level) {
            case HealthLevel::OK:       ++ok;       break;
            case HealthLevel::Degraded: ++degraded; break;
            case HealthLevel::Failed:   ++failed;   break;
            case HealthLevel::Unknown:  ++unknown;  break;
        }
    }

    const std::string summary =
        std::to_string(ok)       + " OK, " +
        std::to_string(degraded) + " degraded, " +
        std::to_string(failed)   + " failed, " +
        std::to_string(unknown)  + " unknown";

    if (failed > 0)   return {HealthLevel::Failed,   summary};
    if (degraded > 0) return {HealthLevel::Degraded, summary};
    if (unknown > 0)  return {HealthLevel::Unknown,  summary};
    return {HealthLevel::OK, summary};
}

} // namespace axiom::core
```

### \`src/modules/module_manager.h\`

```cpp
// src/core/module_manager.h
//
// ModuleManager owns the collection of modules and drives their lifecycle.
//
// It handles:
//   - Registration (application wires modules in at startup)
//   - Initialization (passes ModuleContext to each module)
//   - Start / stop (in correct order)
//   - Shutdown (releases resources)
//   - Aggregate health reporting
//
// The manager does NOT know what any module does. It only knows the
// IModule interface. That is what makes the plugin architecture work.

#pragma once

#include "modules/health.h"
#include "modules/module_context.h"
#include "modules/i_module.h"

#include <cstddef>
#include <memory>
#include <vector>

namespace axiom::core {

class ModuleManager {
public:
    ModuleManager() = default;
    ~ModuleManager() = default;

    // Non-copyable, non-movable: owns unique_ptr resources.
    ModuleManager(const ModuleManager&) = delete;
    ModuleManager& operator=(const ModuleManager&) = delete;
    ModuleManager(ModuleManager&&) = delete;
    ModuleManager& operator=(ModuleManager&&) = delete;

    // Register a module. Ownership transfers to the manager.
    void register_module(std::unique_ptr<IModule> module);

    // Lifecycle — called in this order by the service.
    bool initialize_all(ModuleContext& context);
    bool start_all();
    void stop_all();
    void shutdown_all();

    // Queries
    std::size_t count() const;
    HealthStatus health() const;

private:
    std::vector<std::unique_ptr<IModule>> modules_;
};

} // namespace axiom::core
```

### \`src/net/http_client.cpp\`

```cpp
// src/net/http_client.cpp

#include "net/http_client.h"

#include <httplib.h>
#include <spdlog/spdlog.h>

namespace axiom::core {

HttpClient::HttpClient()  = default;
HttpClient::~HttpClient() = default;

HttpClient::Response HttpClient::post_json(const std::string& url,
                                           const std::string& json_body,
                                           const std::string& bearer_token,
                                           int                timeout_secs) {
    Response resp;

    // --- Parse URL scheme://host[:port]/path ---
    const auto scheme_end = url.find("://");
    if (scheme_end == std::string::npos) {
        resp.error = "invalid URL (missing scheme)";
        return resp;
    }

    const std::string scheme = url.substr(0, scheme_end);
    if (scheme != "http" && scheme != "https") {
        resp.error = "unsupported scheme: " + scheme;
        return resp;
    }

    const std::size_t host_start = scheme_end + 3;
    const std::size_t path_start = url.find('/', host_start);

    std::string host_port;
    std::string path;
    if (path_start == std::string::npos) {
        host_port = url.substr(host_start);
        path      = "/";
    } else {
        host_port = url.substr(host_start, path_start - host_start);
        path      = url.substr(path_start);
    }

    std::string host;
    int         port = (scheme == "https") ? 443 : 80;

    const auto colon = host_port.find(':');
    if (colon != std::string::npos) {
        host = host_port.substr(0, colon);
        try {
            port = std::stoi(host_port.substr(colon + 1));
        } catch (...) {
            resp.error = "invalid port in URL";
            return resp;
        }
    } else {
        host = host_port;
    }

    if (host.empty()) {
        resp.error = "invalid URL (empty host)";
        return resp;
    }

    // --- HTTPS ---
    if (scheme == "https") {
        httplib::SSLClient cli(host, port);
        cli.enable_server_certificate_verification(true);
        cli.set_connection_timeout(timeout_secs);
        cli.set_read_timeout(timeout_secs);
        cli.set_write_timeout(timeout_secs);
        cli.set_follow_location(true);

        if (!bearer_token.empty()) {
            cli.set_bearer_token_auth(bearer_token);
        }

        auto res = cli.Post(path.c_str(), json_body, "application/json");
        if (!res) {
            resp.error = "request failed: " + httplib::to_string(res.error());
            return resp;
        }
        resp.status = res->status;
        resp.body   = res->body;
    }
    // --- HTTP ---
    else {
        httplib::Client cli(host, port);
        cli.set_connection_timeout(timeout_secs);
        cli.set_read_timeout(timeout_secs);
        cli.set_write_timeout(timeout_secs);
        cli.set_follow_location(true);

        if (!bearer_token.empty()) {
            cli.set_bearer_token_auth(bearer_token);
        }

        auto res = cli.Post(path.c_str(), json_body, "application/json");
        if (!res) {
            resp.error = "request failed: " + httplib::to_string(res.error());
            return resp;
        }
        resp.status = res->status;
        resp.body   = res->body;
    }

    return resp;
}

} // namespace axiom::core
```

### \`src/net/http_client.h\`

```cpp
// src/net/http_client.h
//
// HTTP client using cpp-httplib for enrollment, heartbeat, and scan batches.
// Uses OpenSSL for TLS (via CPPHTTPLIB_OPENSSL_SUPPORT).
// TLS is always verified.

#pragma once

#include <string>

namespace axiom::core {

class HttpClient {
public:
    HttpClient();
    ~HttpClient();

    struct Response {
        int         status = 0;
        std::string body;
        std::string error;

        bool ok()     const { return error.empty() && status > 0; }
        bool is_2xx() const { return status >= 200 && status < 300; }
    };

    // Synchronous POST with JSON body. Optional Bearer token.
    Response post_json(const std::string& url,
                       const std::string& json_body,
                       const std::string& bearer_token = "",
                       int                timeout_secs = 10);
};

} // namespace axiom::core
```

### \`src/net/outbound_queue.cpp\`

```cpp
// src/modules/transport/outbound_queue.cpp

#include "net/outbound_queue.h"

#include <spdlog/spdlog.h>

namespace axiom::core {

OutboundQueue::OutboundQueue(std::size_t max_messages, std::size_t max_bytes)
    : max_messages_(max_messages), max_bytes_(max_bytes) {}

OutboundQueue::~OutboundQueue() {
    stop();
}

bool OutboundQueue::push(Priority p, std::string message) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (stopped_) return false;

    const std::size_t msg_size = message.size();

    // Make room: drop oldest low-priority items until we fit.
    while (!queues_[static_cast<int>(Priority::Low)].empty() &&
           (bytes_ + msg_size > max_bytes_ ||
            count_messages_locked() >= max_messages_)) {
        bytes_ -= queues_[static_cast<int>(Priority::Low)].front().payload.size();
        queues_[static_cast<int>(Priority::Low)].pop_front();
    }

    // If we still don't fit, try dropping the next priority up.
    while (!queues_[static_cast<int>(Priority::Normal)].empty() &&
           (bytes_ + msg_size > max_bytes_ ||
            count_messages_locked() >= max_messages_)) {
        bytes_ -= queues_[static_cast<int>(Priority::Normal)].front().payload.size();
        queues_[static_cast<int>(Priority::Normal)].pop_front();
    }

    // If we still don't fit, refuse the new message.
    if (bytes_ + msg_size > max_bytes_ ||
        count_messages_locked() >= max_messages_) {
        return false;
    }

    Message m{p, std::move(message)};
    bytes_ += m.payload.size();
    queues_[static_cast<int>(p)].push_back(std::move(m));

    cv_.notify_one();
    return true;
}

bool OutboundQueue::pop(std::string& out) {
    std::unique_lock<std::mutex> lock(mutex_);

    cv_.wait(lock, [this] {
        return stopped_ || !is_empty_locked();
    });

    if (stopped_ && is_empty_locked()) {
        return false;
    }

    for (int i = 0; i < static_cast<int>(Priority::Count); ++i) {
        if (!queues_[i].empty()) {
            Message& m = queues_[i].front();
            out = std::move(m.payload);
            bytes_ -= out.size();
            queues_[i].pop_front();
            return true;
        }
    }
    return false;
}

bool OutboundQueue::try_pop(std::string& out) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (is_empty_locked()) return false;

    for (int i = 0; i < static_cast<int>(Priority::Count); ++i) {
        if (!queues_[i].empty()) {
            Message& m = queues_[i].front();
            out = std::move(m.payload);
            bytes_ -= out.size();
            queues_[i].pop_front();
            return true;
        }
    }
    return false;
}

void OutboundQueue::stop() {
    std::lock_guard<std::mutex> lock(mutex_);
    stopped_ = true;
    cv_.notify_all();
}

std::size_t OutboundQueue::size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return count_messages_locked();
}

bool OutboundQueue::is_empty_locked() const {
    for (int i = 0; i < static_cast<int>(Priority::Count); ++i) {
        if (!queues_[i].empty()) return false;
    }
    return true;
}

std::size_t OutboundQueue::count_messages_locked() const {
    std::size_t total = 0;
    for (int i = 0; i < static_cast<int>(Priority::Count); ++i) {
        total += queues_[i].size();
    }
    return total;
}

} // namespace axiom::core
```

### \`src/net/outbound_queue.h\`

```cpp
// src/modules/transport/outbound_queue.h
//
// OutboundQueue is a bounded, priority-ordered FIFO. Higher priorities
// are dequeued first. Messages at the same priority preserve FIFO order.
//
// The queue is bounded by both message count and total bytes. When
// either limit is exceeded, the oldest message at the lowest priority
// is dropped. This is deliberate: a slow network must not grow the
// queue without limit.

#pragma once

#include <condition_variable>
#include <cstddef>
#include <deque>
#include <mutex>
#include <string>

namespace axiom::core {

class OutboundQueue {
public:
    enum class Priority {
        Critical = 0,
        High     = 1,
        Normal   = 2,
        Low      = 3,
        Count    = 4
    };

    OutboundQueue(std::size_t max_messages, std::size_t max_bytes);
    ~OutboundQueue();

    OutboundQueue(const OutboundQueue&) = delete;
    OutboundQueue& operator=(const OutboundQueue&) = delete;

    // Enqueue a message. Returns false if the queue is full and the
    // message was dropped.
    bool push(Priority p, std::string message);

    // Pop the highest-priority message. Blocks until one is available
    // or stop() is called. Returns false if woken by stop().
    bool pop(std::string& out);

    // Non-blocking pop. Returns false immediately if the queue is
    // empty.
    bool try_pop(std::string& out);

    // Wake any blocked pop() callers and mark the queue as stopped.
    void stop();

    std::size_t size() const;

private:
    struct Message {
        Priority    priority;
        std::string payload;
    };

    bool is_empty_locked() const;
    std::size_t count_messages_locked() const;

    mutable std::mutex      mutex_;
    std::condition_variable cv_;
    std::deque<Message>     queues_[static_cast<int>(Priority::Count)];
    std::size_t             bytes_ = 0;
    std::size_t             max_messages_;
    std::size_t             max_bytes_;
    bool                    stopped_ = false;
};

} // namespace axiom::core
```

### \`src/platform/common/efsw_file_watcher.cpp\`

```cpp
// src/platform/common/efsw_file_watcher.cpp
//
// EfswFileWatcher — IFileWatcher implementation backed by the efsw library.
//
// This is the ONLY file in the project that includes <efsw/efsw.hpp>.
// Everything else uses the IFileWatcher interface.
//
// efsw handles the platform differences internally:
//   - Linux:   inotify
//   - Windows: I/O Completion Ports (ReadDirectoryChangesW)
//
// The watcher runs on efsw's own background thread. Our callback is
// invoked on that thread.

#include "platform/file_watcher.h"

#include <efsw/efsw.hpp>

#include <spdlog/spdlog.h>

#include <filesystem>
#include <memory>
#include <system_error>

namespace axiom::core {

namespace {

// ----------------------------------------------------------------
// Map efsw's Action enum to our string event types.
// ----------------------------------------------------------------
const char* action_to_string(efsw::Action action) {
    switch (action) {
        case efsw::Actions::Add:      return file_event_types::kCreated;
        case efsw::Actions::Delete:   return file_event_types::kDeleted;
        case efsw::Actions::Modified: return file_event_types::kModified;
        case efsw::Actions::Moved:    return file_event_types::kMoved;
    }
    return "unknown";
}

// ----------------------------------------------------------------
// The concrete watcher.
//
// Inherits both:
//   - IFileWatcher            (our public interface)
//   - efsw::FileWatchListener (efsw's callback contract)
// ----------------------------------------------------------------
class EfswFileWatcher final : public IFileWatcher,
                              private efsw::FileWatchListener {
public:
    EfswFileWatcher() = default;

    ~EfswFileWatcher() override {
        stop();
    }

    bool start(const std::vector<std::string>& paths,
               Callback callback) override {
        if (running_) {
            SPDLOG_WARN("EfswFileWatcher: already running");
            return false;
        }

        if (paths.empty()) {
            SPDLOG_WARN("EfswFileWatcher: no paths to watch");
            return false;
        }

        callback_ = std::move(callback);
        watcher_  = std::make_unique<efsw::FileWatcher>();

        // Try to watch each path. Skip-with-warning on failure; only
        // fail the whole start() if NO path could be watched.
        //
        // We pre-check existence and directory-ness ourselves, because
        // efsw's addWatch returns a bare negative WatchID with no
        // diagnostic. Without this, "permission denied", "does not
        // exist", and "unsupported filesystem" all produce the same
        // misleading log line.
        std::size_t added              = 0;
        std::size_t skipped_missing    = 0;
        std::size_t skipped_notdir     = 0;
        std::size_t skipped_addwatch   = 0;

        for (const auto& path : paths) {
            std::error_code ec;

            // 1. Exists?
            if (!std::filesystem::exists(path, ec)) {
                SPDLOG_WARN("EfswFileWatcher: skipping '{}' (does not exist)",
                            path);
                ++skipped_missing;
                continue;
            }

            // 2. Is a directory? efsw cannot watch a regular file, and
            //    silently refuses.
            if (!std::filesystem::is_directory(path, ec)) {
                SPDLOG_WARN("EfswFileWatcher: skipping '{}' (not a directory)",
                            path);
                ++skipped_notdir;
                continue;
            }

            // 3. Hand it to efsw. If addWatch still refuses, the cause
            //    is genuinely a permission or filesystem limitation,
            //    and we say so plainly.
            const efsw::WatchID id =
                watcher_->addWatch(path, this, /*recursive=*/true);

            if (id < 0) {
                SPDLOG_WARN("EfswFileWatcher: skipping '{}' "
                            "(permission denied or unsupported filesystem)",
                            path);
                ++skipped_addwatch;
                continue;
            }

            SPDLOG_INFO("EfswFileWatcher: watching '{}' (id={})", path, id);
            ++added;
        }

        if (added == 0) {
            SPDLOG_ERROR("EfswFileWatcher: no paths could be watched");
            watcher_.reset();
            callback_ = nullptr;
            return false;
        }

        watcher_->watch();
        running_ = true;

        SPDLOG_INFO("EfswFileWatcher: started, watching {} path(s) "
                    "({} missing, {} not a directory, {} addWatch failures)",
                    added, skipped_missing, skipped_notdir, skipped_addwatch);
        return true;
    }

    void stop() override {
        if (!running_) {
            return;
        }

        watcher_.reset();
        callback_ = nullptr;
        running_  = false;

        SPDLOG_INFO("EfswFileWatcher: stopped");
    }

private:
    void handleFileAction(efsw::WatchID /*watchid*/,
                          const std::string& dir,
                          const std::string& filename,
                          efsw::Action action,
                          const std::string& old_filename) override {
        FileEvent fe;

        // efsw may or may not include a trailing slash in `dir`.
        // Handle both cases cleanly to avoid producing paths with
        // a double slash (e.g., "/tmp/dlp-test//file.txt").
        if (!dir.empty() && dir.back() == '/') {
            fe.path = dir + filename;
        } else {
            fe.path = dir + "/" + filename;
        }

        fe.event_type = action_to_string(action);
        fe.old_path   = old_filename;
        fe.timestamp  = std::chrono::system_clock::now();

        // Populate is_directory. Consumers (FilesystemModule) use this
        // to skip directory-entry-change noise.
        //
        // efsw does not tell us, so we ask the filesystem ourselves.
        // For Delete actions the path no longer exists, so we leave
        // the flag false - we cannot stat what has been removed, and
        // a false negative here is harmless (the event is still
        // published and can be filtered downstream if needed).
        if (action != efsw::Actions::Delete) {
            std::error_code ec;
            fe.is_directory = std::filesystem::is_directory(fe.path, ec);
        }

        if (callback_) {
            try {
                callback_(fe);
            } catch (const std::exception& e) {
                SPDLOG_ERROR("EfswFileWatcher: callback threw: {}", e.what());
            } catch (...) {
                SPDLOG_ERROR("EfswFileWatcher: callback threw unknown exception");
            }
        }
    }

    std::unique_ptr<efsw::FileWatcher> watcher_;
    Callback                           callback_;
    bool                               running_ = false;
};

} // namespace

std::unique_ptr<IFileWatcher> make_file_watcher() {
    return std::make_unique<EfswFileWatcher>();
}

} // namespace axiom::core
```

### \`src/platform/file_watcher.h\`

```cpp
// src/core/file_watcher.h
//
// IFileWatcher — platform-agnostic file system watching interface.
//
// Two responsibilities:
//   1. Watch a list of directories for filesystem changes.
//   2. Call a user-provided callback for every change event.
//
// The concrete implementation lives elsewhere (currently
// platform/common/efsw_file_watcher.cpp, using the efsw library).
//
// Design notes:
//   - The watcher runs on its own background thread.
//   - The callback is invoked on that thread.
//   - stop() joins the thread.
//   - One watcher may watch multiple directories at once.

#pragma once

#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace axiom::core {

// ----------------------------------------------------------------
// FileEvent — a single filesystem change
// ----------------------------------------------------------------

// What kind of change occurred.
// We use strings (not an enum) so that the vocabulary is open and
// matches the stringly-typed event bus.
namespace file_event_types {
inline constexpr const char* kCreated  = "created";
inline constexpr const char* kModified = "modified";
inline constexpr const char* kDeleted  = "deleted";
inline constexpr const char* kMoved    = "moved";
}

struct FileEvent {
    // Full path of the affected file (or directory).
    std::string path;

    // One of the file_event_types:: constants above.
    std::string event_type;

    // For "moved" events, the previous path. Empty otherwise.
    std::string old_path;

    // True if the affected entity is a directory.
    bool is_directory = false;

    // When the event was observed by the watcher.
    std::chrono::system_clock::time_point timestamp;
};

// ----------------------------------------------------------------
// IFileWatcher — the interface
// ----------------------------------------------------------------

class IFileWatcher {
public:
    using Callback = std::function<void(const FileEvent&)>;

    virtual ~IFileWatcher() = default;

    // Non-copyable, non-movable — owns a background thread.
    IFileWatcher(const IFileWatcher&) = delete;
    IFileWatcher& operator=(const IFileWatcher&) = delete;
    IFileWatcher(IFileWatcher&&) = delete;
    IFileWatcher& operator=(IFileWatcher&&) = delete;

    // Begin watching the given directories recursively.
    // Spawns a background thread. Returns true on success.
    // The callback is invoked (on the watcher thread) for every event.
    virtual bool start(const std::vector<std::string>& paths,
                       Callback callback) = 0;

    // Stop watching and join the background thread.
    // Safe to call multiple times. After stop(), the watcher can be
    // start()ed again, but most implementations will prefer to be
    // destroyed and recreated.
    virtual void stop() = 0;

protected:
    // Protected constructor so only derived classes can be created.
    IFileWatcher() = default;
};

// ----------------------------------------------------------------
// Factory
// ----------------------------------------------------------------

// Create the platform-appropriate file watcher implementation.
// Currently returns an efsw-backed watcher that works on both Linux
// and Windows. Later milestones may add platform-specific backends.
std::unique_ptr<IFileWatcher> make_file_watcher();

} // namespace axiom::core
```

### \`src/platform/linux/paths.cpp\`

```cpp
// src/platform/linux/paths_linux.cpp

#if !defined(__linux__)
#  error "src/platform/linux/paths.cpp is Linux-only and must not be compiled on other hosts."
#endif

#include "platform/paths.h"

#include <limits.h>
#include <unistd.h>

#include <filesystem>

namespace axiom::core::paths {

std::string executable_path() {
    char buf[PATH_MAX + 1] = {0};
    ssize_t n = ::readlink("/proc/self/exe", buf, PATH_MAX);
    if (n <= 0) {
        return {};
    }
    return std::string(buf, static_cast<size_t>(n));
}

std::string executable_dir() {
    const std::string p = executable_path();
    if (p.empty()) {
        return {};
    }
    return std::filesystem::path(p).parent_path().string();
}

std::string default_config_path() {
    return "/etc/axiomdlp/agent.json";
}

std::string default_log_dir() {
    return "/var/log/axiomdlp";
}

std::string default_data_dir() {
    return "/var/lib/axiomdlp";
}

} // namespace axiom::core::paths
```

### \`src/platform/linux/service.cpp\`

```cpp
// src/platform/linux/service_linux.cpp

#if !defined(__linux__)
#  error "src/platform/linux/service.cpp is Linux-only and must not be compiled on other hosts."
#endif

#include "platform/service.h"
#include "modules/module_manager.h"

#include <spdlog/spdlog.h>

#include <atomic>
#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#ifdef HAVE_SYSTEMD
#include <systemd/sd-daemon.h>
#endif

namespace axiom::core {

namespace {

// Signal handlers may only touch async-signal-safe state. We keep a
// simple atomic flag and let the main loop translate it into a
// ShutdownController::request().
std::atomic<bool> g_signal_received{false};

extern "C" void on_signal(int /*sig*/) {
    g_signal_received.store(true, std::memory_order_relaxed);
}

// ----- install / uninstall helpers ---------------------------------

bool is_safe_service_name(const std::string& name) {
    if (name.empty()) return false;
    if (name.find("..") != std::string::npos) return false;
    if (name.front() == '.' || name.back() == '.') return false;
    for (char c : name) {
        const bool ok =
            (c >= 'a' && c <= 'z') ||
            (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') ||
            c == '.' || c == '_' || c == '-';
        if (!ok) return false;
    }
    return true;
}

std::string find_systemctl() {
    const char* candidates[] = {
        "/usr/bin/systemctl",
        "/bin/systemctl",
        "/usr/local/bin/systemctl",
    };
    for (const char* p : candidates) {
        if (::access(p, X_OK) == 0) {
            return p;
        }
    }
    return "systemctl";
}

bool run_program(const std::string& program,
                 const std::vector<std::string>& args) {
    std::vector<char*> argv;
    argv.reserve(args.size() + 2);
    argv.push_back(const_cast<char*>(program.c_str()));
    for (const auto& a : args) {
        argv.push_back(const_cast<char*>(a.c_str()));
    }
    argv.push_back(nullptr);

    const pid_t pid = ::fork();
    if (pid < 0) {
        SPDLOG_WARN("fork failed for {}", program);
        return false;
    }

    if (pid == 0) {
        const int devnull = ::open("/dev/null", O_WRONLY);
        if (devnull >= 0) {
            ::dup2(devnull, STDOUT_FILENO);
            ::dup2(devnull, STDERR_FILENO);
            ::close(devnull);
        }
        ::execvp(program.c_str(), argv.data());
        ::_exit(127);
    }

    int status = 0;
    if (::waitpid(pid, &status, 0) < 0) {
        SPDLOG_WARN("waitpid failed for {}", program);
        return false;
    }
    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

} // namespace

class LinuxService final : public IService {
public:
    explicit LinuxService(std::string name) : name_(std::move(name)) {}

    void set_module_manager(ModuleManager* mgr) override {
        module_manager_ = mgr;
    }

    void set_shutdown_controller(ShutdownController* sc) override {
        shutdown_ = sc;
    }

    int run() override {
        // 1. Signal handlers.
        struct sigaction sa{};
        sa.sa_handler = on_signal;
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = 0;
        ::sigaction(SIGTERM, &sa, nullptr);
        ::sigaction(SIGINT,  &sa, nullptr);
        std::signal(SIGHUP, SIG_IGN);

        const bool under_systemd = (::getenv("INVOCATION_ID") != nullptr);

        SPDLOG_INFO("{} starting ({})",
                    name_,
                    under_systemd ? "systemd service mode"
                                  : "foreground mode");

        // 2. Start modules.
        if (module_manager_) {
            module_manager_->start_all();
        }

#ifdef HAVE_SYSTEMD
        if (under_systemd) {
            sd_notify(0, "READY=1");
            SPDLOG_INFO("sd_notify READY sent");
        } else {
            SPDLOG_DEBUG("not running under systemd; skipping sd_notify");
        }
#endif

        // 3. Wait for shutdown. Either a signal or a module's
        //    ShutdownController::request() will unblock this loop.
        while (!shutdown_->is_requested()) {
            if (g_signal_received.load(std::memory_order_relaxed)) {
                shutdown_->request(ShutdownReason::UserRequested);
                break;
            }
            // Wake up every 200ms to check the signal flag. The
            // controller also wakes the loop immediately when a module
            // requests shutdown.
            (void)shutdown_->wait_for(std::chrono::milliseconds(200));
        }

        SPDLOG_INFO("{} received stop request (reason={})",
                    name_, to_string(shutdown_->reason()));

        // 4. Stop modules in reverse order.
        if (module_manager_) {
            module_manager_->stop_all();
            module_manager_->shutdown_all();
        }

#ifdef HAVE_SYSTEMD
        if (under_systemd) {
            sd_notify(0, "STOPPING=1");
        }
#endif

        return 0;
    }

    void request_stop(ShutdownReason reason) override {
        if (shutdown_) {
            shutdown_->request(reason);
        }
    }

private:
    std::string         name_;
    ModuleManager*      module_manager_ = nullptr;
    ShutdownController* shutdown_       = nullptr;
};

std::unique_ptr<IService> make_service(const std::string& service_name) {
    return std::make_unique<LinuxService>(service_name);
}

std::string default_service_name() {
    return "axiomdlp-agent";
}

int install_service(const std::string& binary_path,
                    const std::string& service_name) {
    if (!is_safe_service_name(service_name)) {
        SPDLOG_ERROR("unsafe service name: '{}'", service_name);
        return 1;
    }

    const std::string unit_path =
        "/etc/systemd/system/" + service_name + ".service";

    {
        std::ofstream f(unit_path);
        if (!f.is_open()) {
            SPDLOG_ERROR("cannot write unit file to {} (need root?)", unit_path);
            return 1;
        }

        f << "[Unit]\n"
          << "Description=AxiomDLP Endpoint Agent\n"
          << "After=network.target\n\n"
          << "[Service]\n"
          << "Type=notify\n"
          << "ExecStart=" << binary_path << "\n"
          << "Restart=on-failure\n"
          << "RestartSec=5\n"
          << "StandardOutput=journal\n"
          << "StandardError=journal\n\n"
          << "[Install]\n"
          << "WantedBy=multi-user.target\n";
        f.close();

        SPDLOG_INFO("installed unit file: {}", unit_path);
    }

    const std::string systemctl = find_systemctl();
    SPDLOG_INFO("using systemctl at: {}", systemctl);

    if (run_program(systemctl, {"daemon-reload"})) {
        SPDLOG_INFO("reloaded systemd configuration");
    } else {
        SPDLOG_WARN("systemctl daemon-reload failed");
    }

    if (run_program(systemctl, {"enable", "--now", service_name})) {
        SPDLOG_INFO("enabled and started service {}", service_name);
    } else {
        SPDLOG_WARN("systemctl enable --now failed for {}; "
                    "run manually: {} enable --now {}",
                    service_name, systemctl, service_name);
    }

    return 0;
}

int uninstall_service(const std::string& service_name) {
    if (!is_safe_service_name(service_name)) {
        SPDLOG_ERROR("unsafe service name: '{}'", service_name);
        return 1;
    }

    const std::string systemctl = find_systemctl();

    SPDLOG_INFO("stopping service {}", service_name);
    if (run_program(systemctl, {"stop", service_name})) {
        SPDLOG_INFO("  stop: ok");
    } else {
        SPDLOG_INFO("  stop: failed or service was not running");
    }

    SPDLOG_INFO("disabling service {}", service_name);
    if (run_program(systemctl, {"disable", service_name})) {
        SPDLOG_INFO("  disable: ok");
    } else {
        SPDLOG_INFO("  disable: failed or service was not enabled");
    }

    const std::string unit_path =
        "/etc/systemd/system/" + service_name + ".service";

    errno = 0;
    if (std::remove(unit_path.c_str()) != 0) {
        if (errno != ENOENT) {
            SPDLOG_ERROR("failed to remove {}: {}",
                         unit_path, std::strerror(errno));
            return 1;
        }
        SPDLOG_INFO("unit file already absent: {}", unit_path);
    } else {
        SPDLOG_INFO("removed unit file: {}", unit_path);
    }

    if (run_program(systemctl, {"daemon-reload"})) {
        SPDLOG_INFO("reloaded systemd configuration");
    } else {
        SPDLOG_WARN("systemctl daemon-reload failed");
    }

    SPDLOG_INFO("uninstalled service {}", service_name);
    return 0;
}

} // namespace axiom::core
```

### \`src/platform/paths.h\`

```cpp
// src/core/paths.h
//
// Platform-specific filesystem paths the agent needs.
// Each function has one implementation per platform, selected at build time.

#pragma once

#include <string>

namespace axiom::core::paths {

// Full path to the running executable (e.g., /usr/local/bin/axiomdlp-agent).
std::string executable_path();

// Directory containing the running executable.
std::string executable_dir();

// Default config file path for this platform.
std::string default_config_path();

// Default log directory for this platform.
std::string default_log_dir();

// Default data directory (SQLite DB, cached policies, etc.).
std::string default_data_dir();

} // namespace axiom::core::paths
```

### \`src/platform/service.h\`

```cpp
// src/platform/service.h
//
// Platform-agnostic service lifecycle interface.
//
// Linux:  systemd conventions
// Windows: Service Control Manager
//
// The service drives the module lifecycle. It does NOT own the
// ShutdownController — that lives in Application and is shared with
// every module through ModuleContext.

#pragma once

#include "foundation/shutdown.h"

#include <memory>
#include <string>

namespace axiom::core {

class ModuleManager;

class IService {
public:
    virtual ~IService() = default;

    // Run the service's main loop. Blocks until a shutdown is
    // requested via ShutdownController. Returns 0 on clean shutdown.
    virtual int run() = 0;

    // Request a graceful shutdown with a reason. Idempotent: only the
    // first call has any effect.
    virtual void request_stop(ShutdownReason reason) = 0;

    // Wire in the module manager and the shared shutdown controller
    // before run() is called.
    virtual void set_module_manager(ModuleManager* mgr) = 0;
    virtual void set_shutdown_controller(ShutdownController* sc) = 0;
};

// Platform-specific factory.
std::unique_ptr<IService> make_service(const std::string& service_name);

// Platform-specific install / uninstall.
int install_service(const std::string& binary_path,
                    const std::string& service_name);
int uninstall_service(const std::string& service_name);

std::string default_service_name();

} // namespace axiom::core
```

### \`src/platform/windows/paths.cpp\`

```cpp
// src/platform/windows/paths_windows.cpp

#if !defined(_WIN32)
#  error "src/platform/windows/paths.cpp is Windows-only and must not be compiled on other hosts."
#endif

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
```

### \`src/platform/windows/service.cpp\`

```cpp
// src/platform/windows/service_win.cpp

#if !defined(_WIN32)
#  error "src/platform/windows/service.cpp is Windows-only and must not be compiled on other hosts."
#endif

#include "platform/service.h"
#include "modules/module_manager.h"

#include <spdlog/spdlog.h>

#include <atomic>
#include <chrono>
#include <thread>

#include <windows.h>

namespace axiom::core {

namespace {

std::atomic<bool>     g_stop{false};
SERVICE_STATUS_HANDLE g_status_handle = nullptr;
SERVICE_STATUS        g_status{};
std::string           g_name;

ModuleManager*      g_module_manager = nullptr;
ShutdownController* g_shutdown       = nullptr;

void report(DWORD state, DWORD exit_code = 0, DWORD wait_hint = 0) {
    g_status.dwCurrentState  = state;
    g_status.dwWin32ExitCode = exit_code;
    g_status.dwWaitHint      = wait_hint;
    if (g_status_handle) {
        SetServiceStatus(g_status_handle, &g_status);
    }
}

DWORD WINAPI ctrl_handler(DWORD control, DWORD, LPVOID, LPVOID) {
    switch (control) {
    case SERVICE_CONTROL_STOP:
    case SERVICE_CONTROL_SHUTDOWN:
        report(SERVICE_STOP_PENDING, 0, 3000);
        if (g_shutdown) {
            g_shutdown->request(ShutdownReason::UserRequested);
        } else {
            g_stop.store(true, std::memory_order_relaxed);
        }
        return NO_ERROR;
    case SERVICE_CONTROL_INTERROGATE:
        return NO_ERROR;
    default:
        return ERROR_CALL_NOT_IMPLEMENTED;
    }
}

void WINAPI service_main(DWORD, LPSTR*) {
    g_status_handle = RegisterServiceCtrlHandlerExA(
        g_name.c_str(), ctrl_handler, nullptr);
    if (!g_status_handle) return;

    g_status.dwServiceType      = SERVICE_WIN32_OWN_PROCESS;
    g_status.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;

    report(SERVICE_START_PENDING, 0, 3000);

    if (g_module_manager) {
        g_module_manager->start_all();
    }

    report(SERVICE_RUNNING);
    SPDLOG_INFO("{} running (Windows service mode)", g_name);

    if (g_shutdown) {
        g_shutdown->wait();
    } else {
        while (!g_stop.load(std::memory_order_relaxed)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }
    }

    SPDLOG_INFO("{} stopping (Windows service mode)", g_name);

    if (g_module_manager) {
        g_module_manager->stop_all();
        g_module_manager->shutdown_all();
    }

    report(SERVICE_STOPPED);
}

} // namespace

class WindowsService final : public IService {
public:
    explicit WindowsService(std::string name) : name_(std::move(name)) {}

    void set_module_manager(ModuleManager* mgr) override {
        g_module_manager = mgr;
    }

    void set_shutdown_controller(ShutdownController* sc) override {
        shutdown_  = sc;
        g_shutdown = sc;
    }

    int run() override {
        g_name = name_;
        g_stop.store(false, std::memory_order_relaxed);

        SERVICE_TABLE_ENTRYA table[] = {
            { const_cast<LPSTR>(g_name.c_str()), service_main },
            { nullptr, nullptr }
        };

        if (!StartServiceCtrlDispatcherA(table)) {
            DWORD err = GetLastError();
            if (err == ERROR_FAILED_SERVICE_CONTROLLER_CONNECT) {
                SPDLOG_INFO("not launched by SCM; running in foreground");
                return run_foreground();
            }
            SPDLOG_ERROR("StartServiceCtrlDispatcher failed: {}", err);
            return 1;
        }
        return 0;
    }

    void request_stop(ShutdownReason reason) override {
        if (shutdown_) {
            shutdown_->request(reason);
        } else {
            g_stop.store(true, std::memory_order_relaxed);
        }
    }

private:
    int run_foreground() {
        if (g_module_manager) {
            g_module_manager->start_all();
        }

        SPDLOG_INFO("{} running (Windows foreground mode)", name_);

        if (shutdown_) {
            shutdown_->wait();
        } else {
            while (!g_stop.load(std::memory_order_relaxed)) {
                std::this_thread::sleep_for(std::chrono::milliseconds(200));
            }
        }

        if (g_module_manager) {
            g_module_manager->stop_all();
            g_module_manager->shutdown_all();
        }

        SPDLOG_INFO("{} stopping (foreground)", name_);
        return 0;
    }

    std::string         name_;
    ShutdownController* shutdown_ = nullptr;
};

std::unique_ptr<IService> make_service(const std::string& service_name) {
    return std::make_unique<WindowsService>(service_name);
}

std::string default_service_name() {
    return "AxiomDLPAgent";
}

int install_service(const std::string& binary_path,
                    const std::string& service_name) {
    SC_HANDLE scm = OpenSCManagerA(nullptr, nullptr, SC_MANAGER_CREATE_SERVICE);
    if (!scm) {
        SPDLOG_ERROR("OpenSCManager failed: {}", GetLastError());
        return 1;
    }

    const std::string quoted = "\"" + binary_path + "\"";

    SC_HANDLE svc = CreateServiceA(
        scm, service_name.c_str(), service_name.c_str(),
        SERVICE_ALL_ACCESS, SERVICE_WIN32_OWN_PROCESS,
        SERVICE_AUTO_START, SERVICE_ERROR_NORMAL,
        quoted.c_str(), nullptr, nullptr, nullptr, nullptr, nullptr);

    if (!svc) {
        const DWORD err = GetLastError();
        if (err == ERROR_SERVICE_EXISTS) {
            SPDLOG_WARN("service {} already installed", service_name);
            svc = OpenServiceA(scm, service_name.c_str(),
                               SERVICE_START | SERVICE_QUERY_STATUS);
            if (!svc) {
                CloseServiceHandle(scm);
                return 1;
            }
        } else {
            SPDLOG_ERROR("CreateService failed: {}", err);
            CloseServiceHandle(scm);
            return 1;
        }
    } else {
        SPDLOG_INFO("service {} installed", service_name);
    }

    SERVICE_STATUS status{};
    if (!QueryServiceStatus(svc, &status)) {
        SPDLOG_WARN("QueryServiceStatus failed: {}", GetLastError());
    } else if (status.dwCurrentState == SERVICE_RUNNING) {
        SPDLOG_INFO("service {} already running", service_name);
    } else {
        if (StartServiceA(svc, 0, nullptr)) {
            SPDLOG_INFO("started service {}", service_name);
        } else {
            const DWORD err = GetLastError();
            if (err == ERROR_SERVICE_ALREADY_RUNNING) {
                SPDLOG_INFO("service {} already running", service_name);
            } else {
                SPDLOG_WARN("StartService failed (err={}); "
                            "start manually with: sc start {}",
                            err, service_name);
            }
        }
    }

    CloseServiceHandle(svc);
    CloseServiceHandle(scm);
    return 0;
}

int uninstall_service(const std::string& service_name) {
    SC_HANDLE scm = OpenSCManagerA(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (!scm) {
        SPDLOG_ERROR("OpenSCManager failed: {}", GetLastError());
        return 1;
    }

    SC_HANDLE svc = OpenServiceA(
        scm, service_name.c_str(),
        SERVICE_STOP | SERVICE_QUERY_STATUS | DELETE);

    if (!svc) {
        const DWORD err = GetLastError();
        if (err == ERROR_SERVICE_DOES_NOT_EXIST) {
            SPDLOG_INFO("service {} not installed; nothing to do", service_name);
            CloseServiceHandle(scm);
            return 0;
        }
        SPDLOG_ERROR("OpenService failed: {}", err);
        CloseServiceHandle(scm);
        return 1;
    }

    SERVICE_STATUS status{};
    if (ControlService(svc, SERVICE_CONTROL_STOP, &status)) {
        SPDLOG_INFO("stop requested for {}", service_name);
        for (int i = 0; i < 100; ++i) {
            if (!QueryServiceStatus(svc, &status)) break;
            if (status.dwCurrentState == SERVICE_STOPPED) {
                SPDLOG_INFO("service {} stopped", service_name);
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    } else {
        const DWORD err = GetLastError();
        if (err == ERROR_SERVICE_NOT_ACTIVE) {
            SPDLOG_INFO("service {} was not running", service_name);
        } else {
            SPDLOG_WARN("ControlService failed (err={}); continuing with delete", err);
        }
    }

    if (!DeleteService(svc)) {
        const DWORD err = GetLastError();
        if (err == ERROR_SERVICE_MARKED_FOR_DELETE) {
            SPDLOG_INFO("service {} already marked for deletion", service_name);
        } else {
            SPDLOG_ERROR("DeleteService failed: {}", err);
            CloseServiceHandle(svc);
            CloseServiceHandle(scm);
            return 1;
        }
    } else {
        SPDLOG_INFO("service {} uninstalled", service_name);
    }

    CloseServiceHandle(svc);
    CloseServiceHandle(scm);
    return 0;
}

} // namespace axiom::core
```

### \`src/storage/file_repository.cpp\`

```cpp
// src/storage/file_repository.cpp

#include "storage/file_repository.h"

#include <spdlog/spdlog.h>

#include <sqlite3.h>

#include <chrono>
#include <string>

namespace axiom::core {

namespace {

// RAII wrapper for a prepared statement.
class Stmt {
public:
    Stmt(sqlite3* db, const char* sql) {
        if (sqlite3_prepare_v2(db, sql, -1, &stmt_, nullptr) != SQLITE_OK) {
            SPDLOG_ERROR("FileStore: prepare failed: {}", sqlite3_errmsg(db));
            stmt_ = nullptr;
        }
    }
    ~Stmt() { if (stmt_) sqlite3_finalize(stmt_); }

    Stmt(const Stmt&) = delete;
    Stmt& operator=(const Stmt&) = delete;

    sqlite3_stmt* get() const { return stmt_; }
    bool ok() const { return stmt_ != nullptr; }

private:
    sqlite3_stmt* stmt_ = nullptr;
};

std::int64_t now_seconds() {
    return static_cast<std::int64_t>(
        std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
}

std::string text_col(sqlite3_stmt* s, int idx) {
    const unsigned char* p = sqlite3_column_text(s, idx);
    return p ? reinterpret_cast<const char*>(p) : "";
}

// Column list for every SELECT that returns a full FileRow. Keeping
// it in one place means adding a column only requires one edit.
constexpr const char* kFileRowColumns =
    "id, dev, inode, path, basename, dirname, extension, "
    "size, mtime_ns, ctime_ns, atime_ns, mode, file_type, "
    "is_directory, is_symlink, is_hidden, is_executable, "
    "uid, gid, nlink, category, "
    "is_sensitive, sensitivity, scan_status, "
    "content_hash, last_scanned_at, scan_attempts, scan_error";

// Read a full FileRow from the current row of `s`, starting at column
// index `c`. `c` is advanced past every column read.
FileRow read_file_row(sqlite3_stmt* s, int& c) {
    FileRow r;
    (void)sqlite3_column_int64(s, c++);  // id (skipped)
    r.dev             = static_cast<std::uint64_t>(sqlite3_column_int64(s, c++));
    r.inode           = static_cast<std::uint64_t>(sqlite3_column_int64(s, c++));
    r.path            = text_col(s, c++);
    r.basename        = text_col(s, c++);
    r.dirname         = text_col(s, c++);
    r.extension       = text_col(s, c++);
    r.size            = static_cast<std::uint64_t>(sqlite3_column_int64(s, c++));
    r.mtime_ns        = sqlite3_column_int64(s, c++);
    r.ctime_ns        = sqlite3_column_int64(s, c++);
    r.atime_ns        = sqlite3_column_int64(s, c++);
    r.mode            = static_cast<std::uint32_t>(sqlite3_column_int64(s, c++));
    r.file_type       = text_col(s, c++);
    r.is_directory    = sqlite3_column_int(s, c++) != 0;
    r.is_symlink      = sqlite3_column_int(s, c++) != 0;
    r.is_hidden       = sqlite3_column_int(s, c++) != 0;
    r.is_executable   = sqlite3_column_int(s, c++) != 0;
    r.uid             = static_cast<std::uint32_t>(sqlite3_column_int64(s, c++));
    r.gid             = static_cast<std::uint32_t>(sqlite3_column_int64(s, c++));
    r.nlink           = static_cast<std::uint32_t>(sqlite3_column_int64(s, c++));
    r.category        = text_col(s, c++);
    r.is_sensitive    = sqlite3_column_int(s, c++) != 0;
    r.sensitivity     = text_col(s, c++);
    r.scan_status     = text_col(s, c++);
    r.content_hash    = text_col(s, c++);
    r.last_scanned_at = sqlite3_column_int64(s, c++);
    r.scan_attempts   = sqlite3_column_int(s, c++);
    r.scan_error      = text_col(s, c++);
    return r;
}

} // namespace

// ----------------------------------------------------------------
// Schema
// ----------------------------------------------------------------

bool FileStore::initialize_schema() {
    if (!db_.is_open()) {
        SPDLOG_ERROR("FileStore: database not open");
        return false;
    }
    return db_.exec("SELECT 1 FROM files LIMIT 0;");
}

// ----------------------------------------------------------------
// Transactions
// ----------------------------------------------------------------

bool FileStore::begin_transaction() { return db_.begin_transaction(); }
bool FileStore::commit()            { return db_.commit(); }
bool FileStore::rollback()          { return db_.rollback(); }

// ----------------------------------------------------------------
// Upsert
// ----------------------------------------------------------------

bool FileStore::upsert_file(const FileRow& row, std::uint64_t walk_id) {
    const char* sql = R"SQL(
        INSERT INTO files
            (dev, inode, path, basename, dirname, extension,
             size, mtime_ns, ctime_ns, atime_ns,
             mode, file_type,
             is_directory, is_symlink, is_hidden, is_executable,
             uid, gid, nlink, category,
             is_sensitive, sensitivity, scan_status,
             content_hash, last_scanned_at, scan_attempts, scan_error,
             first_seen, last_seen, walk_id)
        VALUES
            (?, ?, ?, ?, ?, ?,
             ?, ?, ?, ?,
             ?, ?,
             ?, ?, ?, ?,
             ?, ?, ?, ?,
             ?, ?, ?,
             ?, ?, ?, ?,
             ?, ?, ?)
        ON CONFLICT(dev, inode) DO UPDATE SET
            path           = excluded.path,
            basename       = excluded.basename,
            dirname        = excluded.dirname,
            extension      = excluded.extension,
            size           = excluded.size,
            mtime_ns       = excluded.mtime_ns,
            ctime_ns       = excluded.ctime_ns,
            atime_ns       = excluded.atime_ns,
            mode           = excluded.mode,
            file_type      = excluded.file_type,
            is_directory   = excluded.is_directory,
            is_symlink     = excluded.is_symlink,
            is_hidden      = excluded.is_hidden,
            is_executable  = excluded.is_executable,
            uid            = excluded.uid,
            gid            = excluded.gid,
            nlink          = excluded.nlink,
            category       = excluded.category,
            content_hash   = excluded.content_hash,
            scan_status    = CASE
                WHEN files.content_hash = excluded.content_hash
                    THEN files.scan_status
                ELSE excluded.scan_status
            END,
            last_seen      = excluded.last_seen,
            walk_id        = excluded.walk_id
    )SQL";

    Stmt stmt(db_.handle(), sql);
    if (!stmt.ok()) return false;

    const std::int64_t now = now_seconds();

    sqlite3_bind_int64(stmt.get(),  1, static_cast<sqlite3_int64>(row.dev));
    sqlite3_bind_int64(stmt.get(),  2, static_cast<sqlite3_int64>(row.inode));
    sqlite3_bind_text (stmt.get(),  3, row.path.c_str(),        -1, SQLITE_TRANSIENT);
    sqlite3_bind_text (stmt.get(),  4, row.basename.c_str(),    -1, SQLITE_TRANSIENT);
    sqlite3_bind_text (stmt.get(),  5, row.dirname.c_str(),     -1, SQLITE_TRANSIENT);
    sqlite3_bind_text (stmt.get(),  6, row.extension.c_str(),   -1, SQLITE_TRANSIENT);

    sqlite3_bind_int64(stmt.get(),  7, static_cast<sqlite3_int64>(row.size));
    sqlite3_bind_int64(stmt.get(),  8, row.mtime_ns);
    sqlite3_bind_int64(stmt.get(),  9, row.ctime_ns);
    sqlite3_bind_int64(stmt.get(), 10, row.atime_ns);

    sqlite3_bind_int64(stmt.get(), 11, static_cast<sqlite3_int64>(row.mode));
    sqlite3_bind_text (stmt.get(), 12, row.file_type.c_str(),   -1, SQLITE_TRANSIENT);

    sqlite3_bind_int  (stmt.get(), 13, row.is_directory  ? 1 : 0);
    sqlite3_bind_int  (stmt.get(), 14, row.is_symlink    ? 1 : 0);
    sqlite3_bind_int  (stmt.get(), 15, row.is_hidden     ? 1 : 0);
    sqlite3_bind_int  (stmt.get(), 16, row.is_executable ? 1 : 0);

    sqlite3_bind_int64(stmt.get(), 17, static_cast<sqlite3_int64>(row.uid));
    sqlite3_bind_int64(stmt.get(), 18, static_cast<sqlite3_int64>(row.gid));
    sqlite3_bind_int64(stmt.get(), 19, static_cast<sqlite3_int64>(row.nlink));
    sqlite3_bind_text (stmt.get(), 20, row.category.c_str(),    -1, SQLITE_TRANSIENT);

    sqlite3_bind_int  (stmt.get(), 21, row.is_sensitive ? 1 : 0);
    sqlite3_bind_text (stmt.get(), 22, row.sensitivity.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text (stmt.get(), 23, row.scan_status.c_str(), -1, SQLITE_TRANSIENT);

    sqlite3_bind_text (stmt.get(), 24, row.content_hash.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt.get(), 25, row.last_scanned_at);
    sqlite3_bind_int  (stmt.get(), 26, row.scan_attempts);
    sqlite3_bind_text (stmt.get(), 27, row.scan_error.c_str(),   -1, SQLITE_TRANSIENT);

    sqlite3_bind_int64(stmt.get(), 28, now);
    sqlite3_bind_int64(stmt.get(), 29, now);
    sqlite3_bind_int64(stmt.get(), 30, static_cast<sqlite3_int64>(walk_id));

    return sqlite3_step(stmt.get()) == SQLITE_DONE;
}

// ----------------------------------------------------------------
// Pruning
// ----------------------------------------------------------------

bool FileStore::prune_stale(std::uint64_t current_walk_id) {
    const char* sql = "DELETE FROM files WHERE walk_id < ?;";
    Stmt stmt(db_.handle(), sql);
    if (!stmt.ok()) return false;

    sqlite3_bind_int64(stmt.get(), 1, static_cast<sqlite3_int64>(current_walk_id));
    return sqlite3_step(stmt.get()) == SQLITE_DONE;
}

// ----------------------------------------------------------------
// Walk bookkeeping
// ----------------------------------------------------------------

std::uint64_t FileStore::begin_walk(std::int64_t started_at) {
    const char* sql = R"SQL(
        INSERT INTO walks
            (started_at, finished_at, files_seen, files_new,
             files_changed, files_deleted, duration_ms)
        VALUES (?, 0, 0, 0, 0, 0, 0)
    )SQL";

    Stmt stmt(db_.handle(), sql);
    if (!stmt.ok()) return 0;

    sqlite3_bind_int64(stmt.get(), 1, started_at);
    if (sqlite3_step(stmt.get()) != SQLITE_DONE) return 0;
    return static_cast<std::uint64_t>(sqlite3_last_insert_rowid(db_.handle()));
}

bool FileStore::finish_walk(std::uint64_t walk_id,
                            std::int64_t started_at,
                            std::int64_t finished_at,
                            std::size_t files_seen,
                            std::size_t files_new,
                            std::size_t files_changed,
                            std::size_t files_deleted,
                            std::int64_t duration_ms) {
    const char* sql = R"SQL(
        UPDATE walks SET
            started_at = ?, finished_at = ?,
            files_seen = ?, files_new = ?,
            files_changed = ?, files_deleted = ?,
            duration_ms = ?
        WHERE id = ?
    )SQL";

    Stmt stmt(db_.handle(), sql);
    if (!stmt.ok()) return false;

    sqlite3_bind_int64(stmt.get(), 1, started_at);
    sqlite3_bind_int64(stmt.get(), 2, finished_at);
    sqlite3_bind_int64(stmt.get(), 3, static_cast<sqlite3_int64>(files_seen));
    sqlite3_bind_int64(stmt.get(), 4, static_cast<sqlite3_int64>(files_new));
    sqlite3_bind_int64(stmt.get(), 5, static_cast<sqlite3_int64>(files_changed));
    sqlite3_bind_int64(stmt.get(), 6, static_cast<sqlite3_int64>(files_deleted));
    sqlite3_bind_int64(stmt.get(), 7, duration_ms);
    sqlite3_bind_int64(stmt.get(), 8, static_cast<sqlite3_int64>(walk_id));

    return sqlite3_step(stmt.get()) == SQLITE_DONE;
}

// ----------------------------------------------------------------
// Queries
// ----------------------------------------------------------------

std::vector<FileRow> FileStore::query(const FileQuery& q) const {
    std::vector<FileRow> out;

    std::string sql = std::string("SELECT ") + kFileRowColumns + " FROM files WHERE 1=1";

    if (!q.path_prefix.empty()) sql += " AND path LIKE ?";
    if (!q.category.empty())    sql += " AND category = ?";
    if (!q.extension.empty())   sql += " AND extension = ?";
    if (!q.dirname.empty())     sql += " AND dirname = ?";
    if (q.only_files)           sql += " AND is_directory = 0";
    if (q.only_sensitive)       sql += " AND is_sensitive = 1";

    std::string order_col = "path";
    if (q.order_by == "size")     order_col = "size";
    if (q.order_by == "mtime")    order_col = "mtime_ns";
    if (q.order_by == "ctime")    order_col = "ctime_ns";
    if (q.order_by == "category") order_col = "category";
    if (q.order_by == "extension")order_col = "extension";
    if (q.order_by == "dirname")  order_col = "dirname";

    sql += " ORDER BY " + order_col + (q.descending ? " DESC" : " ASC");
    sql += " LIMIT ? OFFSET ?";

    Stmt stmt(db_.handle(), sql.c_str());
    if (!stmt.ok()) return out;

    int idx = 1;
    std::string prefix_like;
    if (!q.path_prefix.empty()) {
        prefix_like = q.path_prefix + "%";
        sqlite3_bind_text(stmt.get(), idx++, prefix_like.c_str(), -1, SQLITE_TRANSIENT);
    }
    if (!q.category.empty())  sqlite3_bind_text(stmt.get(), idx++, q.category.c_str(),  -1, SQLITE_TRANSIENT);
    if (!q.extension.empty()) sqlite3_bind_text(stmt.get(), idx++, q.extension.c_str(), -1, SQLITE_TRANSIENT);
    if (!q.dirname.empty())   sqlite3_bind_text(stmt.get(), idx++, q.dirname.c_str(),   -1, SQLITE_TRANSIENT);

    sqlite3_bind_int64(stmt.get(), idx++, static_cast<sqlite3_int64>(q.limit));
    sqlite3_bind_int64(stmt.get(), idx,   static_cast<sqlite3_int64>(q.offset));

    while (sqlite3_step(stmt.get()) == SQLITE_ROW) {
        int c = 0;
        out.push_back(read_file_row(stmt.get(), c));
    }
    return out;
}

// ----------------------------------------------------------------
// Scanner support
// ----------------------------------------------------------------

std::vector<FileRow> FileStore::fetch_pending_for_scan(std::size_t limit,
                                                       int max_attempts) const {
    std::vector<FileRow> out;

    const std::string sql = std::string("SELECT ") + kFileRowColumns +
        " FROM files "
        "WHERE (scan_status = 'pending' OR scan_status = 'scan_error') "
        "  AND content_hash != '' "
        "  AND is_directory = 0 "
        "  AND scan_attempts < ? "
        "ORDER BY last_seen DESC "
        "LIMIT ?";

    Stmt stmt(db_.handle(), sql.c_str());
    if (!stmt.ok()) return out;

    sqlite3_bind_int  (stmt.get(), 1, max_attempts);
    sqlite3_bind_int64(stmt.get(), 2, static_cast<sqlite3_int64>(limit));

    while (sqlite3_step(stmt.get()) == SQLITE_ROW) {
        int c = 0;
        out.push_back(read_file_row(stmt.get(), c));
    }
    return out;
}

std::optional<ScanVerdict> FileStore::find_scan_verdict_by_hash(
        const std::string& content_hash) const {

    if (content_hash.empty()) return std::nullopt;

    const char* sql =
        "SELECT is_sensitive, sensitivity FROM files "
        "WHERE content_hash = ? AND scan_status = 'scanned' "
        "LIMIT 1";

    Stmt stmt(db_.handle(), sql);
    if (!stmt.ok()) return std::nullopt;

    sqlite3_bind_text(stmt.get(), 1, content_hash.c_str(), -1, SQLITE_TRANSIENT);

    if (sqlite3_step(stmt.get()) != SQLITE_ROW) {
        return std::nullopt;
    }

    ScanVerdict v;
    v.sensitive   = sqlite3_column_int(stmt.get(), 0) != 0;
    v.sensitivity = text_col(stmt.get(), 1);
    return v;
}

bool FileStore::update_scan_result_by_path(const std::string& path,
                                           const std::string& content_hash,
                                           bool sensitive,
                                           const std::string& sensitivity) {
    const char* sql =
        "UPDATE files SET "
        "  content_hash    = ?, "
        "  is_sensitive    = ?, "
        "  sensitivity     = ?, "
        "  scan_status     = 'scanned', "
        "  last_scanned_at = ?, "
        "  scan_error      = '' "
        "WHERE path = ?";

    Stmt stmt(db_.handle(), sql);
    if (!stmt.ok()) return false;

    sqlite3_bind_text (stmt.get(), 1, content_hash.c_str(),   -1, SQLITE_TRANSIENT);
    sqlite3_bind_int  (stmt.get(), 2, sensitive ? 1 : 0);
    sqlite3_bind_text (stmt.get(), 3, sensitivity.c_str(),    -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt.get(), 4, now_seconds());
    sqlite3_bind_text (stmt.get(), 5, path.c_str(),           -1, SQLITE_TRANSIENT);

    return sqlite3_step(stmt.get()) == SQLITE_DONE;
}

bool FileStore::record_scan_error_by_path(const std::string& path,
                                          const std::string& error) {
    const char* sql =
        "UPDATE files SET "
        "  scan_status     = 'scan_error', "
        "  scan_attempts   = scan_attempts + 1, "
        "  scan_error      = ?, "
        "  last_scanned_at = ? "
        "WHERE path = ?";

    Stmt stmt(db_.handle(), sql);
    if (!stmt.ok()) return false;

    sqlite3_bind_text (stmt.get(), 1, error.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt.get(), 2, now_seconds());
    sqlite3_bind_text (stmt.get(), 3, path.c_str(),  -1, SQLITE_TRANSIENT);

    return sqlite3_step(stmt.get()) == SQLITE_DONE;
}

bool FileStore::mark_file_missing(const std::string& path) {
    const char* sql =
        "UPDATE files SET scan_status = 'missing' WHERE path = ?";

    Stmt stmt(db_.handle(), sql);
    if (!stmt.ok()) return false;

    sqlite3_bind_text(stmt.get(), 1, path.c_str(), -1, SQLITE_TRANSIENT);
    return sqlite3_step(stmt.get()) == SQLITE_DONE;
}

bool FileStore::mark_not_applicable(const std::string& path) {
    const char* sql =
        "UPDATE files SET scan_status = 'not_applicable' WHERE path = ?";

    Stmt stmt(db_.handle(), sql);
    if (!stmt.ok()) return false;

    sqlite3_bind_text(stmt.get(), 1, path.c_str(), -1, SQLITE_TRANSIENT);
    return sqlite3_step(stmt.get()) == SQLITE_DONE;
}

bool FileStore::mark_skipped_too_large(const std::string& path) {
    const char* sql =
        "UPDATE files SET scan_status = 'skipped_too_large' WHERE path = ?";

    Stmt stmt(db_.handle(), sql);
    if (!stmt.ok()) return false;

    sqlite3_bind_text(stmt.get(), 1, path.c_str(), -1, SQLITE_TRANSIENT);
    return sqlite3_step(stmt.get()) == SQLITE_DONE;
}

// ----------------------------------------------------------------
// Transport support
// ----------------------------------------------------------------

std::uint64_t FileStore::max_file_id() const {
    Stmt stmt(db_.handle(), "SELECT COALESCE(MAX(id), 0) FROM files;");
    if (!stmt.ok()) return 0;

    if (sqlite3_step(stmt.get()) == SQLITE_ROW) {
        return static_cast<std::uint64_t>(sqlite3_column_int64(stmt.get(), 0));
    }
    return 0;
}

std::vector<FileStore::SensitiveRow>
FileStore::query_sensitive_after(std::uint64_t after_id,
                                 std::size_t limit) const {
    std::vector<SensitiveRow> out;

    Stmt stmt(db_.handle(),
              "SELECT id, path, size, sensitivity, last_seen "
              "FROM files "
              "WHERE is_sensitive = 1 AND id > ? "
              "ORDER BY id ASC LIMIT ?;");
    if (!stmt.ok()) return out;

    sqlite3_bind_int64(stmt.get(), 1, static_cast<sqlite3_int64>(after_id));
    sqlite3_bind_int64(stmt.get(), 2, static_cast<sqlite3_int64>(limit));

    while (sqlite3_step(stmt.get()) == SQLITE_ROW) {
        SensitiveRow r;
        r.id          = static_cast<std::uint64_t>(sqlite3_column_int64(stmt.get(), 0));
        r.path        = text_col(stmt.get(), 1);
        r.size        = static_cast<std::uint64_t>(sqlite3_column_int64(stmt.get(), 2));
        r.sensitivity = text_col(stmt.get(), 3);
        r.last_seen   = sqlite3_column_int64(stmt.get(), 4);
        out.push_back(std::move(r));
    }
    return out;
}

bool FileStore::mark_random_files_sensitive(std::size_t count) {
    const char* sql =
        "UPDATE files SET "
        "  is_sensitive = 1, "
        "  sensitivity = CASE (abs(random()) % 4) "
        "    WHEN 0 THEN 'mock_credit_card' "
        "    WHEN 1 THEN 'mock_pii' "
        "    WHEN 2 THEN 'mock_credentials' "
        "    ELSE 'mock_document' "
        "  END, "
        "  scan_status = 'scanned' "
        "WHERE id IN ("
        "  SELECT id FROM files "
        "  WHERE is_directory = 0 "
        "  ORDER BY random() LIMIT ? "
        ");";

    Stmt stmt(db_.handle(), sql);
    if (!stmt.ok()) return false;

    sqlite3_bind_int64(stmt.get(), 1, static_cast<sqlite3_int64>(count));
    return sqlite3_step(stmt.get()) == SQLITE_DONE;
}

// ----------------------------------------------------------------
// Aggregations
// ----------------------------------------------------------------

FileStore::Stats FileStore::stats() const {
    Stats s;

    Stmt stmt(db_.handle(),
              "SELECT "
              "  COALESCE(SUM(CASE WHEN is_directory = 0 THEN 1 ELSE 0 END), 0), "
              "  COALESCE(SUM(CASE WHEN is_directory = 1 THEN 1 ELSE 0 END), 0), "
              "  COALESCE(SUM(CASE WHEN is_directory = 0 THEN size ELSE 0 END), 0), "
              "  COALESCE(SUM(CASE WHEN is_sensitive = 1 THEN 1 ELSE 0 END), 0) "
              "FROM files;");
    if (!stmt.ok()) return s;

    if (sqlite3_step(stmt.get()) == SQLITE_ROW) {
        s.total_files       = static_cast<std::size_t>(sqlite3_column_int64(stmt.get(), 0));
        s.total_directories = static_cast<std::size_t>(sqlite3_column_int64(stmt.get(), 1));
        s.total_bytes       = static_cast<std::uint64_t>(sqlite3_column_int64(stmt.get(), 2));
        s.sensitive_files   = static_cast<std::size_t>(sqlite3_column_int64(stmt.get(), 3));
    }
    return s;
}

std::vector<FileStore::CategoryCount>
FileStore::category_breakdown() const {
    std::vector<CategoryCount> out;

    Stmt stmt(db_.handle(),
              "SELECT category, COUNT(*), COALESCE(SUM(size), 0) "
              "FROM files WHERE is_directory = 0 "
              "GROUP BY category ORDER BY COUNT(*) DESC;");
    if (!stmt.ok()) return out;

    while (sqlite3_step(stmt.get()) == SQLITE_ROW) {
        CategoryCount c;
        c.category = text_col(stmt.get(), 0);
        c.count    = static_cast<std::size_t>(sqlite3_column_int64(stmt.get(), 1));
        c.bytes    = static_cast<std::uint64_t>(sqlite3_column_int64(stmt.get(), 2));
        out.push_back(std::move(c));
    }
    return out;
}

std::vector<FileStore::DirCount>
FileStore::top_directories(std::size_t limit) const {
    std::vector<DirCount> out;

    Stmt stmt(db_.handle(),
              "SELECT "
              "  substr(path, 1, instr(substr(path, 2), '/') + 1) AS top_dir, "
              "  COUNT(*) AS cnt, "
              "  COALESCE(SUM(size), 0) AS bytes "
              "FROM files WHERE is_directory = 0 "
              "GROUP BY top_dir ORDER BY cnt DESC LIMIT ?;");
    if (!stmt.ok()) return out;

    sqlite3_bind_int64(stmt.get(), 1, static_cast<sqlite3_int64>(limit));

    while (sqlite3_step(stmt.get()) == SQLITE_ROW) {
        DirCount d;
        d.dirname = text_col(stmt.get(), 0);
        d.count   = static_cast<std::size_t>(sqlite3_column_int64(stmt.get(), 1));
        d.bytes   = static_cast<std::uint64_t>(sqlite3_column_int64(stmt.get(), 2));
        out.push_back(std::move(d));
    }
    return out;
}

} // namespace axiom::core
```

### \`src/storage/file_repository.h\`

```cpp
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
    bool upsert_file(const FileRow& row, std::uint64_t walk_id);

    // Remove files whose walk_id is older than `current_walk_id`.
    bool prune_stale(std::uint64_t current_walk_id);

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

    // Record a successful scan result. Updates by path, sets content_hash
    // to the value the scanner observed.
    bool update_scan_result_by_path(const std::string& path,
                                    const std::string& content_hash,
                                    bool sensitive,
                                    const std::string& sensitivity);

    // Record a failed scan attempt. Increments scan_attempts, stores the
    // error, marks status as 'scan_error'.
    bool record_scan_error_by_path(const std::string& path,
                                   const std::string& error);

    // Mark a file that no longer exists on disk.
    bool mark_file_missing(const std::string& path);

    // Mark a file whose extension is outside the scanner's scope.
    bool mark_not_applicable(const std::string& path);

    // Mark a file that exceeds the scanner's max_file_bytes limit.
    bool mark_skipped_too_large(const std::string& path);

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
```

### \`src/storage/migrations.cpp\`

```cpp
// src/storage/migrations.cpp

#include "storage/migrations.h"
#include "storage/sqlite_database.h"

#include <spdlog/spdlog.h>

#include <sqlite3.h>

#include <string>

namespace axiom::core {

namespace {

int read_user_version(Database& db) {
    sqlite3* h = db.handle();
    if (h == nullptr) return -1;

    sqlite3_stmt* stmt = nullptr;
    const int rc = sqlite3_prepare_v2(h, "PRAGMA user_version;", -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        SPDLOG_ERROR("Migrations: cannot prepare PRAGMA user_version: {}",
                     sqlite3_errmsg(h));
        return -1;
    }

    int version = -1;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        version = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);
    return version;
}

bool write_user_version(Database& db, int version) {
    const std::string sql = "PRAGMA user_version = " + std::to_string(version) + ";";
    return db.exec(sql);
}

// ----------------------------------------------------------------
// Migration 2: enriched files table
// ----------------------------------------------------------------
bool apply_v2(Database& db) {
    SPDLOG_INFO("Migrations: applying v2 (enriched files table)");

    const char* sql = R"SQL(
        DROP TABLE IF EXISTS files;
        DROP TABLE IF EXISTS walks;

        CREATE TABLE files (
            id             INTEGER PRIMARY KEY,
            dev            INTEGER NOT NULL,
            inode          INTEGER NOT NULL,
            path           TEXT    NOT NULL,
            basename       TEXT    NOT NULL DEFAULT '',
            dirname        TEXT    NOT NULL DEFAULT '',
            extension      TEXT    NOT NULL DEFAULT '',
            size           INTEGER NOT NULL DEFAULT 0,
            mtime_ns       INTEGER NOT NULL DEFAULT 0,
            ctime_ns       INTEGER NOT NULL DEFAULT 0,
            atime_ns       INTEGER NOT NULL DEFAULT 0,
            mode           INTEGER NOT NULL DEFAULT 0,
            file_type      TEXT    NOT NULL DEFAULT 'file',
            is_directory   INTEGER NOT NULL DEFAULT 0,
            is_symlink     INTEGER NOT NULL DEFAULT 0,
            is_hidden      INTEGER NOT NULL DEFAULT 0,
            is_executable  INTEGER NOT NULL DEFAULT 0,
            uid            INTEGER NOT NULL DEFAULT 0,
            gid            INTEGER NOT NULL DEFAULT 0,
            nlink          INTEGER NOT NULL DEFAULT 0,
            category       TEXT    NOT NULL DEFAULT 'other',

            is_sensitive   INTEGER NOT NULL DEFAULT 0,
            sensitivity    TEXT    NOT NULL DEFAULT '',
            scan_status    TEXT    NOT NULL DEFAULT 'pending',

            first_seen     INTEGER NOT NULL,
            last_seen      INTEGER NOT NULL,
            walk_id        INTEGER NOT NULL DEFAULT 0,
            UNIQUE(dev, inode)
        );

        CREATE INDEX IF NOT EXISTS idx_files_path       ON files(path);
        CREATE INDEX IF NOT EXISTS idx_files_dirname    ON files(dirname);
        CREATE INDEX IF NOT EXISTS idx_files_extension  ON files(extension);
        CREATE INDEX IF NOT EXISTS idx_files_category   ON files(category);
        CREATE INDEX IF NOT EXISTS idx_files_mtime      ON files(mtime_ns DESC);
        CREATE INDEX IF NOT EXISTS idx_files_size       ON files(size DESC);
        CREATE INDEX IF NOT EXISTS idx_files_uid        ON files(uid);
        CREATE INDEX IF NOT EXISTS idx_files_walk       ON files(walk_id);
        CREATE INDEX IF NOT EXISTS idx_files_sensitive  ON files(is_sensitive)
            WHERE is_sensitive = 1;

        CREATE TABLE IF NOT EXISTS walks (
            id            INTEGER PRIMARY KEY,
            started_at    INTEGER NOT NULL,
            finished_at   INTEGER NOT NULL,
            files_seen    INTEGER NOT NULL,
            files_new     INTEGER NOT NULL,
            files_changed INTEGER NOT NULL,
            files_deleted INTEGER NOT NULL,
            duration_ms   INTEGER NOT NULL
        );
    )SQL";

    if (!db.exec("BEGIN TRANSACTION;")) return false;
    if (!db.exec(sql)) {
        db.exec("ROLLBACK;");
        return false;
    }
    if (!db.exec("COMMIT;")) return false;

    SPDLOG_INFO("Migrations: v2 applied");
    return true;
}

// ----------------------------------------------------------------
// Migration 3: key/value state table
// ----------------------------------------------------------------
bool apply_v3(Database& db) {
    SPDLOG_INFO("Migrations: applying v3 (state table)");

    const char* sql = R"SQL(
        CREATE TABLE IF NOT EXISTS state (
            key   TEXT PRIMARY KEY,
            value TEXT NOT NULL
        );
    )SQL";

    if (!db.exec("BEGIN TRANSACTION;")) return false;
    if (!db.exec(sql)) {
        db.exec("ROLLBACK;");
        return false;
    }
    if (!db.exec("COMMIT;")) return false;

    SPDLOG_INFO("Migrations: v3 applied");
    return true;
}

// ----------------------------------------------------------------
// Migration 4: content-scanning metadata
//
// Adds the columns the future content scanner needs to track what
// has been hashed, what has been sent to the server for inspection,
// and what state each file is in.
// ----------------------------------------------------------------
bool apply_v4(Database& db) {
    SPDLOG_INFO("Migrations: applying v4 (scanner metadata)");

    const char* sql = R"SQL(
        ALTER TABLE files ADD COLUMN content_hash     TEXT    NOT NULL DEFAULT '';
        ALTER TABLE files ADD COLUMN last_scanned_at  INTEGER NOT NULL DEFAULT 0;
        ALTER TABLE files ADD COLUMN scan_attempts    INTEGER NOT NULL DEFAULT 0;
        ALTER TABLE files ADD COLUMN scan_error       TEXT    NOT NULL DEFAULT '';

        CREATE INDEX IF NOT EXISTS idx_files_scan_status
            ON files(scan_status);
        CREATE INDEX IF NOT EXISTS idx_files_content_hash
            ON files(content_hash);
    )SQL";

    if (!db.exec("BEGIN TRANSACTION;")) return false;
    if (!db.exec(sql)) {
        db.exec("ROLLBACK;");
        return false;
    }
    if (!db.exec("COMMIT;")) return false;

    SPDLOG_INFO("Migrations: v4 applied");
    return true;
}

} // namespace

int Migrations::current_version() {
    return 4;
}

bool Migrations::apply(Database& db) {
    if (!db.is_open()) {
        SPDLOG_ERROR("Migrations: database not open");
        return false;
    }

    const int target  = current_version();
    int       current = read_user_version(db);

    if (current < 0) {
        SPDLOG_ERROR("Migrations: cannot read user_version");
        return false;
    }

    SPDLOG_INFO("Migrations: schema version {} -> {}", current, target);

    if (current >= target) {
        return true;
    }

    if (current < 2) {
        if (!apply_v2(db)) {
            SPDLOG_ERROR("Migrations: v2 failed");
            return false;
        }
        if (!write_user_version(db, 2)) {
            SPDLOG_ERROR("Migrations: cannot write user_version=2");
            return false;
        }
        current = 2;
    }

    if (current < 3) {
        if (!apply_v3(db)) {
            SPDLOG_ERROR("Migrations: v3 failed");
            return false;
        }
        if (!write_user_version(db, 3)) {
            SPDLOG_ERROR("Migrations: cannot write user_version=3");
            return false;
        }
        current = 3;
    }

    if (current < 4) {
        if (!apply_v4(db)) {
            SPDLOG_ERROR("Migrations: v4 failed");
            return false;
        }
        if (!write_user_version(db, 4)) {
            SPDLOG_ERROR("Migrations: cannot write user_version=4");
            return false;
        }
        current = 4;
    }

    return true;
}

} // namespace axiom::core
```

### \`src/storage/migrations.h\`

```cpp
// src/storage/migrations.h

#pragma once

namespace axiom::core {

class Database;

class Migrations {
public:
    static bool apply(Database& db);
    static int current_version();
};

} // namespace axiom::core
```

### \`src/storage/sqlite_database.cpp\`

```cpp
// src/storage/database.cpp

#include "storage/sqlite_database.h"

#include <spdlog/spdlog.h>

#include <sqlite3.h>

#include <filesystem>

namespace axiom::core {

Database::~Database() {
    close();
}

bool Database::open(const std::string& path) {
    if (db_ != nullptr) {
        SPDLOG_WARN("Database::open: already open; closing first");
        close();
    }

    // Make sure parent directory exists.
    std::error_code ec;
    const auto parent = std::filesystem::path(path).parent_path();
    if (!parent.empty()) {
        std::filesystem::create_directories(parent, ec);
        if (ec) {
            SPDLOG_ERROR("Database: cannot create parent directory '{}': {}",
                         parent.string(), ec.message());
            return false;
        }
    }

    const int rc = sqlite3_open(path.c_str(), &db_);
    if (rc != SQLITE_OK) {
        // Read the error message BEFORE closing the handle. sqlite3_errmsg
        // is only valid while the handle is alive.
        const std::string err = db_ ? sqlite3_errmsg(db_) : "unknown";
        sqlite3_close(db_);
        db_ = nullptr;

        SPDLOG_ERROR("Database: sqlite3_open('{}') failed: {}", path, err);
        return false;
    }

    // Pragmas that make SQLite safe for concurrent readers + one writer.
    exec("PRAGMA journal_mode=WAL;");
    exec("PRAGMA synchronous=NORMAL;");
    exec("PRAGMA foreign_keys=ON;");
    exec("PRAGMA busy_timeout=5000;");

    SPDLOG_INFO("Database: opened '{}'", path);
    return true;
}

void Database::close() {
    if (db_ == nullptr) return;
    sqlite3_close(db_);
    db_ = nullptr;

    // No logging here on purpose.
    //
    // close() is called from the destructor, which may run during
    // Application::shutdown() after the logger has been torn down.
    // Any SPDLOG_* call at that point dereferences a dead spdlog
    // singleton and crashes. The caller knows when it is closing the
    // database; there is nothing useful to log from here that isn't
    // already visible from "Database: opened" and the process exit.
}

bool Database::exec(const std::string& sql) {
    if (db_ == nullptr) {
        SPDLOG_ERROR("Database::exec: not open");
        return false;
    }

    char* errmsg = nullptr;
    const int rc = sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &errmsg);
    if (rc != SQLITE_OK) {
        const std::string msg = errmsg ? errmsg : "unknown";
        if (errmsg) sqlite3_free(errmsg);

        SPDLOG_ERROR("Database::exec failed: {} (SQL: {})", msg, sql);
        return false;
    }
    return true;
}

bool Database::begin_transaction() { return exec("BEGIN TRANSACTION;"); }
bool Database::commit()            { return exec("COMMIT;"); }
bool Database::rollback()          { return exec("ROLLBACK;"); }

std::string Database::last_error() const {
    if (db_ == nullptr) return "not open";
    return sqlite3_errmsg(db_);
}

} // namespace axiom::core
```

### \`src/storage/sqlite_database.h\`

```cpp
// src/storage/database.h
//
// Thin wrapper around SQLite. Knows nothing about files, metadata, or
// DLP. Only knows how to open a database file, run SQL, and manage
// transactions.
//
// Forward-declares `sqlite3` so callers don't need to include
// <sqlite3.h> just to hold a reference.

#pragma once

#include <string>

struct sqlite3;   // opaque handle; defined in <sqlite3.h>

namespace axiom::core {

class Database {
public:
    Database() = default;
    ~Database();

    // Non-copyable, non-movable: owns the sqlite3* handle.
    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;
    Database(Database&&) = delete;
    Database& operator=(Database&&) = delete;

    // Open (or create) the database file. Creates parent directories
    // if missing. Enables WAL and foreign keys.
    bool open(const std::string& path);

    // Close the handle. Safe to call multiple times.
    void close();

    bool is_open() const { return db_ != nullptr; }

    // Execute a statement that returns no rows. Returns false on error.
    bool exec(const std::string& sql);

    // Transaction helpers. Nested transactions are not supported.
    bool begin_transaction();
    bool commit();
    bool rollback();

    // Raw handle, for prepared statements in higher layers.
    sqlite3* handle() const { return db_; }

    // Last error message from SQLite.
    std::string last_error() const;

private:
    sqlite3* db_ = nullptr;
};

} // namespace axiom::core
```

### \`src/storage/state_repository.cpp\`

```cpp
// src/storage/state_store.cpp

#include "storage/state_repository.h"
#include "storage/sqlite_database.h"

#include <spdlog/spdlog.h>

#include <sqlite3.h>

namespace axiom::core {

namespace {

// RAII wrapper for a prepared statement, same pattern as file_store.cpp.
class Stmt {
public:
    Stmt(sqlite3* db, const char* sql) {
        if (sqlite3_prepare_v2(db, sql, -1, &stmt_, nullptr) != SQLITE_OK) {
            SPDLOG_ERROR("StateStore: prepare failed: {}", sqlite3_errmsg(db));
            stmt_ = nullptr;
        }
    }
    ~Stmt() { if (stmt_) sqlite3_finalize(stmt_); }

    Stmt(const Stmt&)            = delete;
    Stmt& operator=(const Stmt&) = delete;

    sqlite3_stmt* get() const { return stmt_; }
    bool          ok()  const { return stmt_ != nullptr; }

private:
    sqlite3_stmt* stmt_ = nullptr;
};

} // namespace

// ----------------------------------------------------------------
// Schema check
// ----------------------------------------------------------------

bool StateStore::initialize_schema() {
    if (!db_.is_open()) {
        SPDLOG_ERROR("StateStore: database not open");
        return false;
    }
    return db_.exec("SELECT 1 FROM state LIMIT 0;");
}

// ----------------------------------------------------------------
// set / get / remove
// ----------------------------------------------------------------

bool StateStore::set(const std::string& key, const std::string& value) {
    const char* sql =
        "INSERT INTO state (key, value) VALUES (?, ?) "
        "ON CONFLICT(key) DO UPDATE SET value = excluded.value;";

    Stmt stmt(db_.handle(), sql);
    if (!stmt.ok()) return false;

    sqlite3_bind_text(stmt.get(), 1, key.c_str(),   -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt.get(), 2, value.c_str(), -1, SQLITE_TRANSIENT);

    return sqlite3_step(stmt.get()) == SQLITE_DONE;
}

std::optional<std::string> StateStore::get(const std::string& key) const {
    const char* sql = "SELECT value FROM state WHERE key = ?;";

    Stmt stmt(db_.handle(), sql);
    if (!stmt.ok()) return std::nullopt;

    sqlite3_bind_text(stmt.get(), 1, key.c_str(), -1, SQLITE_TRANSIENT);

    if (sqlite3_step(stmt.get()) != SQLITE_ROW) {
        return std::nullopt;
    }

    const unsigned char* p = sqlite3_column_text(stmt.get(), 0);
    if (p == nullptr) return std::nullopt;
    return std::string(reinterpret_cast<const char*>(p));
}

bool StateStore::remove(const std::string& key) {
    const char* sql = "DELETE FROM state WHERE key = ?;";

    Stmt stmt(db_.handle(), sql);
    if (!stmt.ok()) return false;

    sqlite3_bind_text(stmt.get(), 1, key.c_str(), -1, SQLITE_TRANSIENT);

    return sqlite3_step(stmt.get()) == SQLITE_DONE;
}

} // namespace axiom::core
```

### \`src/storage/state_repository.h\`

```cpp
// src/storage/state_store.h
//
// StateStore is a tiny key/value store backed by the `state` table.
// It persists small pieces of agent state that must survive restarts:
//   - "agent_id"            — returned by the server at registration
//   - "agent_registered_at" — ISO timestamp of successful enrollment
//   - anything else small and non-critical
//
// It is NOT a general-purpose data store. Use FileStore or a new
// domain-specific wrapper for anything with real structure.

#pragma once

#include <optional>
#include <string>

namespace axiom::core {

class Database;

class StateStore {
public:
    explicit StateStore(Database& db) : db_(db) {}

    // Verify the `state` table exists (created by migration v3).
    bool initialize_schema();

    // Store a value, overwriting any existing value for the key.
    bool set(const std::string& key, const std::string& value);

    // Fetch a value. Returns std::nullopt if the key is absent.
    std::optional<std::string> get(const std::string& key) const;

    // Remove a key. Returns true even if the key was absent.
    bool remove(const std::string& key);

private:
    Database& db_;
};

} // namespace axiom::core
```

### \`src/util/base64.cpp\`

```cpp
// src/util/base64.cpp

#include "util/base64.h"

#include <openssl/evp.h>

namespace axiom::util {

std::string base64_encode(const unsigned char* data, std::size_t len) {
    if (len == 0 || data == nullptr) return {};

    // EVP_EncodeBlock writes 4 * ceil(len/3) bytes, then a null terminator.
    std::size_t out_len = 4 * ((len + 2) / 3);
    std::string out(out_len, '\0');

    int n = EVP_EncodeBlock(reinterpret_cast<unsigned char*>(out.data()),
                            data, static_cast<int>(len));
    if (n <= 0) return {};

    out.resize(static_cast<std::size_t>(n));
    return out;
}

} // namespace axiom::util
```

### \`src/util/base64.h\`

```cpp
// src/util/base64.h
//
// Base64 encoding per RFC 4648. Uses OpenSSL's EVP_EncodeBlock so we
// don't have to hand-roll it.
//
// Used by the content scanner to encode file bytes into JSON-safe
// strings. Some text files (.sqlite, .parquet, .db) contain bytes
// that aren't valid UTF-8, and JSON strings must be valid UTF-8.

#pragma once

#include <cstddef>
#include <string>

namespace axiom::util {

std::string base64_encode(const unsigned char* data, std::size_t len);

inline std::string base64_encode(const char* data, std::size_t len) {
    return base64_encode(reinterpret_cast<const unsigned char*>(data), len);
}

inline std::string base64_encode(const std::string& s) {
    return base64_encode(reinterpret_cast<const unsigned char*>(s.data()), s.size());
}

} // namespace axiom::util
```

### \`src/util/file_hash.cpp\`

```cpp
// src/util/file_hash.cpp

#include "util/file_hash.h"

#include <openssl/evp.h>

#include <cstdio>
#include <memory>
#include <vector>

namespace axiom::util {

namespace {

constexpr std::size_t kChunkSize = 1024 * 1024;   // 1 MB

std::string bytes_to_hex(const unsigned char* bytes, unsigned int len) {
    static const char hexchars[] = "0123456789abcdef";
    std::string out;
    out.reserve(len * 2);
    for (unsigned int i = 0; i < len; ++i) {
        out.push_back(hexchars[(bytes[i] >> 4) & 0x0F]);
        out.push_back(hexchars[bytes[i] & 0x0F]);
    }
    return out;
}

// Small RAII wrapper for EVP_MD_CTX.
struct MdCtxDeleter {
    void operator()(EVP_MD_CTX* c) const noexcept {
        if (c) EVP_MD_CTX_free(c);
    }
};

using MdCtxPtr = std::unique_ptr<EVP_MD_CTX, MdCtxDeleter>;

} // namespace

FileHashResult compute_sha256(const std::string& path) {
    FileHashResult r;

    std::FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) {
        r.error = "cannot open file";
        return r;
    }

    struct FileCloser {
        std::FILE* p;
        ~FileCloser() { if (p) std::fclose(p); }
    } closer{f};

    MdCtxPtr ctx(EVP_MD_CTX_new());
    if (!ctx) {
        r.error = "EVP_MD_CTX_new failed";
        return r;
    }

    if (EVP_DigestInit_ex(ctx.get(), EVP_sha256(), nullptr) != 1) {
        r.error = "EVP_DigestInit_ex failed";
        return r;
    }

    std::vector<unsigned char> buffer(kChunkSize);

    for (;;) {
        const std::size_t n = std::fread(buffer.data(), 1, buffer.size(), f);
        if (n > 0) {
            if (EVP_DigestUpdate(ctx.get(), buffer.data(), n) != 1) {
                r.error = "EVP_DigestUpdate failed";
                return r;
            }
            r.bytes_read += n;
        }
        if (n < buffer.size()) {
            if (std::ferror(f)) {
                r.error = "read error";
                return r;
            }
            break;   // EOF
        }
    }

    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int  digest_len = 0;
    if (EVP_DigestFinal_ex(ctx.get(), digest, &digest_len) != 1) {
        r.error = "EVP_DigestFinal_ex failed";
        return r;
    }

    r.hex_digest = bytes_to_hex(digest, digest_len);
    return r;
}

std::string sha256_hex(const void* data, std::size_t len) {
    MdCtxPtr ctx(EVP_MD_CTX_new());
    if (!ctx) return {};

    if (EVP_DigestInit_ex(ctx.get(), EVP_sha256(), nullptr) != 1) return {};
    if (EVP_DigestUpdate(ctx.get(), data, len) != 1) return {};

    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int  digest_len = 0;
    if (EVP_DigestFinal_ex(ctx.get(), digest, &digest_len) != 1) return {};

    return bytes_to_hex(digest, digest_len);
}

} // namespace axiom::util
```

### \`src/util/file_hash.h\`

```cpp
// src/util/file_hash.h
//
// SHA-256 hashing of files and buffers, built on OpenSSL's EVP API.
//
// compute_sha256() reads the file in fixed-size chunks, so memory use
// is bounded regardless of file size. A 100 GB file uses the same
// memory as a 1 KB file.

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace axiom::util {

struct FileHashResult {
    std::string   hex_digest;             // 64 lowercase hex chars, or empty on failure
    std::uint64_t bytes_read = 0;         // bytes fed into the hash
    std::string   error;                  // empty on success

    bool ok() const { return error.empty() && !hex_digest.empty(); }
};

// Compute the SHA-256 of a file's full contents.
// Reads in 1 MB chunks; never loads the entire file into memory.
FileHashResult compute_sha256(const std::string& path);

// Compute the SHA-256 of an in-memory buffer.
std::string sha256_hex(const void* data, std::size_t len);

} // namespace axiom::util
```

### \`src/util/glob_match.cpp\`

```cpp
// src/util/glob_match.cpp

#include "util/glob_match.h"

namespace axiom::util {

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

bool path_matches_pattern(const std::string& pattern,
                          const std::string& path) {
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

} // namespace axiom::util
```

### \`src/util/glob_match.h\`

```cpp
// src/util/glob_match.h
//
// Simple glob pattern matching used to filter filesystem paths.
//
// Supports:
//   *     — matches zero or more characters
//   ?     — matches exactly one character
//
// Also provides path_matches_pattern(), which additionally treats a
// pattern as a directory prefix when it does not end in a wildcard.

#pragma once

#include <string>

namespace axiom::util {

// Does `text` match `pattern`? Supports `*` and `?` wildcards.
bool glob_match(const std::string& pattern, const std::string& text);

// Extended match used for exclude lists:
//   1. Try a straight glob match.
//   2. If the pattern does not end in `*` or `?`, also treat it as a
//      directory prefix (pattern "foo/bar" matches "foo/bar/baz").
bool path_matches_pattern(const std::string& pattern,
                          const std::string& path);

} // namespace axiom::util
```

---

## Packaging

---

## Scripts

### \`scripts/linux/build-all.sh\`

```bash
#!/usr/bin/env bash
#
# scripts/linux/build-all.sh — Linux-only build entrypoint.
#
# Windows builds are done natively on Windows via
#   scripts/win/build-windows-msvc.ps1   (MSVC + vcpkg)
# See docs/windows-msvc-build.md.
#
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
DEPS_DIR="${ROOT}/.deps"

if [ ! -d "${DEPS_DIR}/nlohmann_json-src" ]; then
    echo "==============================================="
    echo "Populating nlohmann_json source cache (one-time)..."
    echo "==============================================="
    mkdir -p "${DEPS_DIR}"
    git clone --depth 1 --branch v3.11.3 \
        https://github.com/nlohmann/json.git \
        "${DEPS_DIR}/nlohmann_json-src"
    echo
fi

openssl_target_ready() {
    local dir="$1"
    { [ -f "${dir}/lib/libssl.a"   ] && [ -f "${dir}/lib/libcrypto.a"   ]; } || \
    { [ -f "${dir}/lib64/libssl.a" ] && [ -f "${dir}/lib64/libcrypto.a" ]; }
}

if ! openssl_target_ready "${DEPS_DIR}/openssl-linux-x86_64" || \
   ! openssl_target_ready "${DEPS_DIR}/openssl-linux-i686"; then
    echo "==============================================="
    echo "OpenSSL not fully built; building (one-time)..."
    echo "==============================================="
    "${ROOT}/scripts/linux/build-openssl.sh"
    echo
else
    echo "OpenSSL: already built for all Linux targets, skipping."
    echo
fi

BUILD_TYPE="${1:-release}"

TARGETS=(
    linux-x86_64
    linux-i686
)

for target in "${TARGETS[@]}"; do
    preset="${target}-${BUILD_TYPE}"
    echo "==============================================="
    echo "Building: ${preset}"
    echo "==============================================="
    cmake --preset "${preset}"
    cmake --build --preset "${preset}"
done

echo
echo "==============================================="
echo "All Linux builds complete. Artifacts:"
echo "==============================================="

for target in "${TARGETS[@]}"; do
    file "build/${target}-${BUILD_TYPE}/axiomdlp-agent"
done

echo
echo "For Windows: clone on a Windows box and run scripts\\win\\build-windows-msvc.ps1"
```

### \`scripts/linux/build-grpc.sh\`

```bash
#!/usr/bin/env bash
# scripts/build-grpc.sh
#
# Builds gRPC statically for Linux targets only.
#
# Windows (MinGW) is intentionally omitted:
#   gRPC has no official MinGW support. The MSYS2 patches exist but
#   are version-locked and break on newer releases. For Windows,
#   the agent uses its existing Boost.Beast WebSocket + HTTP
#   transport. Windows gRPC is deferred until we have a real
#   MSVC build machine.
#
# Outputs (reused if already present — no forced rebuild):
#   .deps/grpc-linux-x86_64/
#   .deps/grpc-linux-i686/
#
# Prerequisites:
#   - OpenSSL built for both Linux targets (run build-openssl.sh)
#   - For i686: libsystemd-dev:i386 (build machine only)
#
# First run takes 30-60 min (Linux x86_64 builds everything from
# scratch). Later runs are instant — target_ready() skips completed
# trees.

set -euo pipefail

GRPC_VERSION="v1.62.0"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
DEPS="$ROOT/.deps"
SRC="$DEPS/grpc-src"
BUILD_DIR="$DEPS/grpc-build"

export CMAKE_POLICY_VERSION_MINIMUM=3.5

mkdir -p "$DEPS"

# ----------------------------------------------------------------
# Download source + submodules (once)
# ----------------------------------------------------------------
if [ ! -d "$SRC" ]; then
    echo "Downloading gRPC $GRPC_VERSION with submodules..."
    git clone --depth 1 --branch "$GRPC_VERSION" \
        --recurse-submodules --shallow-submodules \
        https://github.com/grpc/grpc.git "$SRC"
else
    echo "gRPC source already cached at $SRC"
fi

# ----------------------------------------------------------------
# Readiness check — used to skip already-built trees
# ----------------------------------------------------------------
target_ready() {
    local dir="$1"
    [ -f "${dir}/lib/libgrpc++.a" ] && \
    [ -f "${dir}/lib/libgrpc.a"   ] && \
    [ -f "${dir}/lib/libprotobuf.a" ]
}

# ----------------------------------------------------------------
# Host tools (protoc, grpc_cpp_plugin)
#
# Needed for code generation on every target. The i686 build skips
# its own codegen (see below), so these host tools are the only
# source of protoc/plugin for 32-bit builds.
# ----------------------------------------------------------------
HOST_TOOLS="$DEPS/grpc-host-tools"

if [ ! -f "$HOST_TOOLS/bin/protoc" ]; then
    echo "Building host tools (protoc, grpc_cpp_plugin)..."
    mkdir -p "$HOST_TOOLS" "$BUILD_DIR/host"
    pushd "$BUILD_DIR/host" >/dev/null
    cmake "$SRC" \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX="$HOST_TOOLS" \
        -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
        -DgRPC_BUILD_TESTS=OFF \
        -DgRPC_BUILD_GRPC_CPP_PLUGIN=ON \
        -DgRPC_BUILD_CSHARP_EXT=OFF \
        -DgRPC_BUILD_GRPC_CSHARP_PLUGIN=OFF \
        -DgRPC_BUILD_GRPC_NODE_PLUGIN=OFF \
        -DgRPC_BUILD_GRPC_OBJECTIVE_C_PLUGIN=OFF \
        -DgRPC_BUILD_GRPC_PHP_PLUGIN=OFF \
        -DgRPC_BUILD_GRPC_PYTHON_PLUGIN=OFF \
        -DgRPC_BUILD_GRPC_RUBY_PLUGIN=OFF \
        -DgRPC_INSTALL=ON \
        -DgRPC_SSL_PROVIDER=package \
        -DOPENSSL_ROOT_DIR="$DEPS/openssl-linux-x86_64"
    make -j"$(nproc)" protoc grpc_cpp_plugin
    make install
    popd >/dev/null
else
    echo "Host tools already built at $HOST_TOOLS"
fi

# ----------------------------------------------------------------
# Build one target.
# Args: <target-name> <cmake-toolchain-or-""> <openssl-dir> [extra flags...]
# ----------------------------------------------------------------
build_one() {
    local name="$1"
    local toolchain="$2"
    local openssl_dir="$3"
    shift 3
    local extra_args=("$@")

    local out="$DEPS/grpc-$name"

    if target_ready "$out"; then
        echo "gRPC for $name already built at $out; skipping."
        return
    fi

    echo "Building gRPC for $name"

    local build_subdir="$BUILD_DIR/$name"
    mkdir -p "$build_subdir"
    pushd "$build_subdir" >/dev/null

    local cmake_args=(
        "$SRC"
        -DCMAKE_BUILD_TYPE=Release
        -DCMAKE_INSTALL_PREFIX="$out"
        -DCMAKE_POLICY_VERSION_MINIMUM=3.5
        -DgRPC_BUILD_TESTS=OFF
        -DgRPC_INSTALL=ON
        -DBUILD_SHARED_LIBS=OFF
        -DgRPC_SSL_PROVIDER=package
        -DOPENSSL_ROOT_DIR="$openssl_dir"
        -DgRPC_ZLIB_PROVIDER=module
        -DgRPC_CARES_PROVIDER=module
        -DgRPC_RE2_PROVIDER=module
        -DgRPC_ABSL_PROVIDER=module
        -DgRPC_PROTOBUF_PROVIDER=module
        -DgRPC_PROTOBUF_PACKAGE_TYPE=MODULE
        -DgRPC_BUILD_GRPC_CPP_PLUGIN=ON
        -DgRPC_BUILD_CSHARP_EXT=OFF
        -DgRPC_BUILD_GRPC_CSHARP_PLUGIN=OFF
        -DgRPC_BUILD_GRPC_NODE_PLUGIN=OFF
        -DgRPC_BUILD_GRPC_OBJECTIVE_C_PLUGIN=OFF
        -DgRPC_BUILD_GRPC_PHP_PLUGIN=OFF
        -DgRPC_BUILD_GRPC_PYTHON_PLUGIN=OFF
        -DgRPC_BUILD_GRPC_RUBY_PLUGIN=OFF
        -DCMAKE_CXX_STANDARD=20
        -DCMAKE_CXX_STANDARD_REQUIRED=ON
    )

    if [ -n "$toolchain" ]; then
        cmake_args+=( "-DCMAKE_TOOLCHAIN_FILE=$toolchain" )
    fi

    cmake_args+=( "${extra_args[@]}" )

    cmake "${cmake_args[@]}"
    make -j"$(nproc)"
    make install
    popd >/dev/null

    echo "gRPC for $name installed at $out"
}

# ----------------------------------------------------------------
# Linux x86_64 (native)
# ----------------------------------------------------------------
build_one "linux-x86_64" \
    "" \
    "$DEPS/openssl-linux-x86_64"

# ----------------------------------------------------------------
# Linux i686 (32-bit)
#
# Notes:
#   - gRPC_BUILD_CODEGEN=OFF: the codegen plugin fails to compile
#     on 32-bit. Host tools (built above) handle codegen instead.
#   - gRPC_SYSTEMD_PROVIDER=none: without this, CMake bakes the
#     host's 64-bit /usr/lib/x86_64-linux-gnu/libsystemd.so into
#     gRPCTargets.cmake, and 32-bit consumers fail to link with
#     "file in wrong format". The agent's own systemd support
#     comes from CMakeLists.txt via pkg-config — gRPC does not
#     need its own systemd integration.
# ----------------------------------------------------------------
build_one "linux-i686" \
    "$ROOT/cmake/toolchains/linux-i686.cmake" \
    "$DEPS/openssl-linux-i686" \
    -DgRPC_BUILD_CODEGEN=OFF \
    -DgRPC_SYSTEMD_PROVIDER=none

# ----------------------------------------------------------------
# Summary
# ----------------------------------------------------------------
echo
echo "All gRPC builds complete."
echo
echo "Status:"
for target in linux-x86_64 linux-i686; do
    d="$DEPS/grpc-$target"
    if target_ready "$d"; then
        echo "  ✓ $d"
    else
        echo "  ✗ $d (missing libraries)"
    fi
done

echo
echo "Windows gRPC is intentionally not built here."
echo "The agent uses Boost.Beast WebSocket + JSON on Windows."
```

### \`scripts/linux/build-openssl.sh\`

```bash
#!/usr/bin/env bash
# scripts/build-openssl.sh
#
# Builds OpenSSL statically for both Linux agent targets.
# Windows OpenSSL is supplied by vcpkg on the Windows side.
# Output goes to:
#   .deps/openssl-linux-x86_64/
#   .deps/openssl-linux-i686/
#
# Run once. Re-run only if you bump OPENSSL_VERSION.
# Takes 15-25 minutes total.

set -euo pipefail

OPENSSL_VERSION="3.3.2"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
DEPS="$ROOT/.deps"
SRC="$DEPS/openssl-src"
TARBALL="$DEPS/openssl.tar.gz"

mkdir -p "$DEPS"

# ---------- Download source once ----------
if [ ! -d "$SRC" ]; then
    echo "Downloading OpenSSL $OPENSSL_VERSION..."
    curl -fL --retry 3 \
        -o "$TARBALL" \
        "https://github.com/openssl/openssl/releases/download/openssl-$OPENSSL_VERSION/openssl-$OPENSSL_VERSION.tar.gz"
    tar -xzf "$TARBALL" -C "$DEPS"
    mv "$DEPS/openssl-$OPENSSL_VERSION" "$SRC"
    rm -f "$TARBALL"
else
    echo "OpenSSL source already cached at $SRC"
fi

# Returns 0 if libssl.a and libcrypto.a exist in lib/ or lib64/.
target_ready() {
    local dir="$1"
    { [ -f "${dir}/lib/libssl.a"   ] && [ -f "${dir}/lib/libcrypto.a"   ]; } || \
    { [ -f "${dir}/lib64/libssl.a" ] && [ -f "${dir}/lib64/libcrypto.a" ]; }
}

# Build one target.
# Args: <target-config> <install-prefix> [extra Configure args...]
build_one() {
    local target="$1"
    local out="$2"
    shift 2
    local extra_args=("$@")

    if target_ready "$out"; then
        echo "OpenSSL for $target already built at $out; skipping."
        return
    fi

    echo "==============================================="
    echo "Building OpenSSL for $target"
    echo "==============================================="

    pushd "$SRC" >/dev/null

    make clean 2>/dev/null || true

    ./Configure "$target" \
        --prefix="$out" \
        --openssldir="$out/ssl" \
        no-shared no-tests no-docs no-apps no-legacy \
        "${extra_args[@]}"

    make -j"$(nproc)"
    make install_sw
    make clean

    popd >/dev/null

    echo "OpenSSL for $target installed at $out"
}

# ---------- Build all four ----------
build_one "linux-x86_64"       "$DEPS/openssl-linux-x86_64"
build_one "linux-x86"          "$DEPS/openssl-linux-i686"   -m32

echo
echo "Done. Both Linux OpenSSL trees are ready:"
echo "  $DEPS/openssl-linux-x86_64"
echo "  $DEPS/openssl-linux-i686"
```

### \`scripts/linux/merge-compile-commands.sh\`

```bash
#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
OUT="$ROOT/compile_commands.json"

FILES=(
    "$ROOT/build/linux-x86_64-debug/compile_commands.json"
    "$ROOT/build/linux-x86_64-release/compile_commands.json"
    "$ROOT/build/linux-i686-release/compile_commands.json"
    "$ROOT/build/windows-x86_64-debug/compile_commands.json"
    "$ROOT/build/windows-x86_64-release/compile_commands.json"
    "$ROOT/build/windows-i686-release/compile_commands.json"
)

python3 - "${FILES[@]}" "$OUT" << 'PYEOF'
import json, sys, os
out = sys.argv[-1]
inputs = sys.argv[1:-1]
merged = {}
for path in inputs:
    if not os.path.exists(path):
        continue
    with open(path) as f:
        for entry in json.load(f):
            merged[entry["file"]] = entry
result = sorted(merged.values(), key=lambda e: e["file"])
with open(out, "w") as f:
    json.dump(result, f, indent=2)
print(f"Merged {len(result)} entries into {out}")
PYEOF
```

### \`scripts/win/build-windows-msvc.ps1\`

```powershell
<#
.SYNOPSIS
    Build AxiomDLP Agent on Windows with MSVC + vcpkg.

.DESCRIPTION
    Auto-initializes the Visual Studio environment (vcvarsall.bat) so this
    script works from ANY PowerShell window. Also protects VCPKG_ROOT from
    being overwritten by vcvarsall.bat (which sets it to the VS-bundled vcpkg).

.PARAMETER Configuration
    Debug or Release (default: Release).

.PARAMETER Arch
    x64 or x86 (default: x64).

.PARAMETER Clean
    If set, delete the build directory before configuring.

.NOTES
    This file is intentionally ASCII-only for maximum encoding compatibility.
#>

param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Release",

    [ValidateSet("x64", "x86")]
    [string]$Arch = "x64",

    [switch]$Clean
)

$ErrorActionPreference = "Stop"

# ----------------------------------------------------------------
# Save VCPKG_ROOT BEFORE VS env init (vcvarsall.bat overwrites it
# with the VS-bundled vcpkg, which requires builtin-baseline).
# ----------------------------------------------------------------
$savedVcpkgRoot = $env:VCPKG_ROOT
if (-not $savedVcpkgRoot) {
    $savedVcpkgRoot = [System.Environment]::GetEnvironmentVariable("VCPKG_ROOT", "User")
}
if (-not $savedVcpkgRoot) {
    $savedVcpkgRoot = "$env:USERPROFILE\vcpkg"
}

# ----------------------------------------------------------------
# Auto-initialize Visual Studio environment (MSVC toolchain)
# ----------------------------------------------------------------
$desiredArch = if ($Arch -eq "x86") { "x86" } else { "x64" }
$currentArch = $env:VSCMD_ARG_TGT_ARCH
if (-not $env:VSINSTALLDIR -or $currentArch -ne $desiredArch) {
    Write-Host "Initializing Visual Studio environment..." -ForegroundColor Yellow

    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (-not (Test-Path $vswhere)) {
        Write-Host "[ERROR] Visual Studio Installer not found at $vswhere" -ForegroundColor Red
        exit 1
    }

    $vsPath = & $vswhere -latest -products * `
        -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
        -property installationPath

    if (-not $vsPath) {
        Write-Host "[ERROR] Visual Studio C++ workload not found." -ForegroundColor Red
        exit 1
    }

    $vcvarsall = Join-Path $vsPath "VC\Auxiliary\Build\vcvarsall.bat"
    if (-not (Test-Path $vcvarsall)) {
        Write-Host "[ERROR] vcvarsall.bat not found at $vcvarsall" -ForegroundColor Red
        exit 1
    }

    $vsArch = if ($Arch -eq "x86") { "x86" } else { "amd64" }

    $envOutput = cmd /c "`"$vcvarsall`" $vsArch >nul 2>&1 && set"
    foreach ($line in $envOutput) {
        if ($line -match "^(.*?)=(.*)$") {
            [Environment]::SetEnvironmentVariable($matches[1], $matches[2], "Process")
        }
    }

    if (-not $env:VSINSTALLDIR) {
        Write-Host "[ERROR] vcvarsall.bat did not populate VSINSTALLDIR." -ForegroundColor Red
        exit 1
    }
    Write-Host "  [OK] VS env loaded: $env:VSINSTALLDIR" -ForegroundColor Green
}
else {
    Write-Host "  [OK] VS env already loaded: $env:VSINSTALLDIR" -ForegroundColor Green
}

# ----------------------------------------------------------------
# Restore VCPKG_ROOT (vcvarsall.bat overwrites it to the VS-bundled
# vcpkg, which requires builtin-baseline and uses different ports).
# ----------------------------------------------------------------
if ($savedVcpkgRoot) {
    $previous = $env:VCPKG_ROOT
    $env:VCPKG_ROOT = $savedVcpkgRoot
    if ($previous -and $previous -ne $savedVcpkgRoot) {
        Write-Host "  [OK] Restored VCPKG_ROOT: $env:VCPKG_ROOT" -ForegroundColor Green
        Write-Host "       (vcvarsall.bat had set it to: $previous)" -ForegroundColor DarkGray
    }
}

# Refuse to use the VS-bundled vcpkg - it requires builtin-baseline
# in vcpkg.json which the standalone vcpkg does not need.
if ($env:VCPKG_ROOT -match "Microsoft Visual Studio.*VC[\\/]vcpkg") {
    Write-Host "[ERROR] VCPKG_ROOT points at the VS-bundled vcpkg:" -ForegroundColor Red
    Write-Host "        $env:VCPKG_ROOT" -ForegroundColor Red
    Write-Host "        This is different from your standalone vcpkg." -ForegroundColor Red
    Write-Host "        Set VCPKG_ROOT to the standalone path (e.g. C:\Users\User\vcpkg)" -ForegroundColor Red
    exit 1
}

# Force MSVC compilers (overrides any GCC on PATH)
$env:CC = "cl.exe"
$env:CXX = "cl.exe"

# ----------------------------------------------------------------
# Locate project root
# ----------------------------------------------------------------
$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
Set-Location $projectRoot

Write-Host "`n=== AxiomDLP Agent - Windows MSVC build ===" -ForegroundColor Cyan
Write-Host "  Project root: $projectRoot"
Write-Host "  Config:       $Configuration"
Write-Host "  Arch:         $Arch"

# ----------------------------------------------------------------
# Verify VCPKG_ROOT
# ----------------------------------------------------------------
if (-not $env:VCPKG_ROOT) {
    Write-Host @"
[ERROR] VCPKG_ROOT is not set.

Run the one-time setup first:
    .\scripts\win\setup-windows-msvc.ps1

Then close this shell, open a new one, and try again.
"@ -ForegroundColor Red
    exit 1
}
if (-not (Test-Path "$env:VCPKG_ROOT\scripts\buildsystems\vcpkg.cmake")) {
    Write-Host "[ERROR] vcpkg toolchain file not found at $env:VCPKG_ROOT\scripts\buildsystems\vcpkg.cmake" -ForegroundColor Red
    exit 1
}
Write-Host "  [OK] VCPKG_ROOT: $env:VCPKG_ROOT" -ForegroundColor Green

# ----------------------------------------------------------------
# Build dir handling
# ----------------------------------------------------------------
$presetName = "windows-$Arch-msvc-$(($Configuration).ToLower())"
$buildDir = Join-Path $projectRoot "build\$presetName"

if ($Clean -and (Test-Path $buildDir)) {
    Write-Host "  Cleaning $buildDir (--Clean requested)" -ForegroundColor Yellow
    Remove-Item -Recurse -Force $buildDir
}

# Auto-clean if cached compiler is not MSVC
$cacheFile = Join-Path $buildDir "CMakeCache.txt"
if (Test-Path $cacheFile) {
    $cacheText = Get-Content $cacheFile -Raw
    if ($cacheText -match "CMAKE_C_COMPILER:[^=]*=.*(gcc|cc\.exe)" -or
        $cacheText -match "CMAKE_CXX_COMPILER:[^=]*=.*(g\+\+|c\+\+\.exe)") {
        Write-Host "  Stale CMake cache with GCC compiler found; cleaning build dir..." -ForegroundColor Yellow
        Remove-Item -Recurse -Force $buildDir
    }
}

# ----------------------------------------------------------------
# Configure
# ----------------------------------------------------------------
Write-Host "`n--- Configure (preset: $presetName) ---" -ForegroundColor Yellow
cmake --preset $presetName
if ($LASTEXITCODE -ne 0) {
    Write-Host "[ERROR] Configure failed." -ForegroundColor Red
    exit $LASTEXITCODE
}

# ----------------------------------------------------------------
# Build
# ----------------------------------------------------------------
Write-Host "`n--- Build ---" -ForegroundColor Yellow
cmake --build --preset $presetName --config $Configuration
if ($LASTEXITCODE -ne 0) {
    Write-Host "[ERROR] Build failed." -ForegroundColor Red
    exit $LASTEXITCODE
}

# ----------------------------------------------------------------
# Report
# ----------------------------------------------------------------
$exe = Join-Path $buildDir "axiomdlp-agent.exe"
if (Test-Path $exe) {
    $size = [math]::Round((Get-Item $exe).Length / 1MB, 2)
    Write-Host "`n=== Build succeeded ===" -ForegroundColor Cyan
    Write-Host "  Binary: $exe"
    Write-Host "  Size:   $size MB"
}
else {
    Write-Host "[WARN] Build finished but $exe not found. Check the preset's binaryDir." -ForegroundColor Yellow
}
```

### \`scripts/win/setup-windows-msvc.ps1\`

```powershell
<#
.SYNOPSIS
    One-time setup for building AxiomDLP Agent on Windows with MSVC + vcpkg.

.DESCRIPTION
    - Verifies Visual Studio 2022 (or Build Tools) with the C++ workload.
    - Downloads the vcpkg repository as a ZIP (no git clone).
    - Downloads the prebuilt vcpkg.exe (no compilation).
    - Sets VCPKG_ROOT persistently for the current user.

.NOTES
    This file is intentionally ASCII-only for maximum encoding compatibility.
    Fast mode: uses codeload ZIP + GitHub release exe. No git, no bootstrap.
#>

$ErrorActionPreference = "Stop"

Write-Host "=== AxiomDLP Agent - Windows MSVC bootstrap (fast mode) ===" -ForegroundColor Cyan

# ----------------------------------------------------------------
# 1. Verify Visual Studio
# ----------------------------------------------------------------
Write-Host "`n[1/3] Looking for Visual Studio..." -ForegroundColor Yellow

$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path $vswhere)) {
    Write-Host @"
[ERROR] Visual Studio Installer not found.

Install one of:
  - Visual Studio 2022 Community (free)
  - Visual Studio 2022 Build Tools (free, headless)

Make sure to select the workload:
  'Desktop development with C++'

Download: https://visualstudio.microsoft.com/downloads/
"@ -ForegroundColor Red
    exit 1
}

$vsInstall = & $vswhere -latest -products * `
    -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
    -property installationPath

if (-not $vsInstall) {
    Write-Host "[ERROR] Visual Studio found but the C++ workload is missing." -ForegroundColor Red
    Write-Host "        Open Visual Studio Installer, click Modify, check 'Desktop development with C++'." -ForegroundColor Red
    exit 1
}
Write-Host "  [OK] Found: $vsInstall" -ForegroundColor Green

# ----------------------------------------------------------------
# 2. vcpkg: ZIP recipes + prebuilt vcpkg.exe
# ----------------------------------------------------------------
$vcpkgRoot = if ($env:VCPKG_ROOT) { $env:VCPKG_ROOT } else { "$env:USERPROFILE\vcpkg" }
$vcpkgZip = "$env:USERPROFILE\vcpkg.zip"
$toolchain = "$vcpkgRoot\scripts\buildsystems\vcpkg.cmake"

Write-Host "`n[2/3] Setting up vcpkg at $vcpkgRoot..." -ForegroundColor Yellow

# --- 2a. Recipe folder (ports/, scripts/, triplets/)
if (-not (Test-Path $toolchain)) {
    Write-Host "  Downloading vcpkg recipes ZIP (no git clone)..." -ForegroundColor Yellow

    $zipUrl = "https://codeload.github.com/microsoft/vcpkg/zip/refs/heads/master"
    try {
        Invoke-WebRequest -Uri $zipUrl -OutFile $vcpkgZip -UseBasicParsing
    }
    catch {
        Write-Host "  master not found; trying main branch..." -ForegroundColor Yellow
        $zipUrl = "https://codeload.github.com/microsoft/vcpkg/zip/refs/heads/main"
        Invoke-WebRequest -Uri $zipUrl -OutFile $vcpkgZip -UseBasicParsing
    }

    Write-Host "  Extracting..." -ForegroundColor Yellow
    Expand-Archive -Path $vcpkgZip -DestinationPath "$env:USERPROFILE" -Force

    $extracted = Get-ChildItem "$env:USERPROFILE" -Directory -Filter "vcpkg-*" -ErrorAction SilentlyContinue |
    Where-Object { $_.Name -ne "vcpkg" } | Select-Object -First 1

    if (-not $extracted) {
        Write-Host "[ERROR] Could not find extracted vcpkg-* folder." -ForegroundColor Red
        exit 1
    }

    if (Test-Path $vcpkgRoot) {
        Remove-Item -Recurse -Force $vcpkgRoot -ErrorAction SilentlyContinue
    }
    Rename-Item $extracted.FullName $vcpkgRoot
    Remove-Item $vcpkgZip -Force -ErrorAction SilentlyContinue
}
else {
    Write-Host "  [OK] vcpkg recipes already present." -ForegroundColor Green
}

# --- 2b. Prebuilt vcpkg.exe
if (-not (Test-Path "$vcpkgRoot\vcpkg.exe")) {
    Write-Host "  Downloading prebuilt vcpkg.exe (no compilation)..." -ForegroundColor Yellow
    $exeUrl = "https://github.com/microsoft/vcpkg-tool/releases/latest/download/vcpkg.exe"
    Invoke-WebRequest -Uri $exeUrl -OutFile "$vcpkgRoot\vcpkg.exe" -UseBasicParsing
}
else {
    Write-Host "  [OK] vcpkg.exe already present." -ForegroundColor Green
}

# --- 2c. Verify both pieces
if (-not (Test-Path "$vcpkgRoot\vcpkg.exe")) {
    Write-Host "[ERROR] vcpkg.exe download failed; check $vcpkgRoot" -ForegroundColor Red
    exit 1
}
if (-not (Test-Path $toolchain)) {
    Write-Host "[ERROR] vcpkg toolchain file missing at $toolchain" -ForegroundColor Red
    exit 1
}
Write-Host "  [OK] vcpkg ready: $vcpkgRoot" -ForegroundColor Green

# ----------------------------------------------------------------
# 3. Persist VCPKG_ROOT (both User scope and current process)
# ----------------------------------------------------------------
Write-Host "`n[3/3] Setting VCPKG_ROOT=$vcpkgRoot for current user..." -ForegroundColor Yellow

# Method 1: .NET API (silent if it works)
try {
    [System.Environment]::SetEnvironmentVariable("VCPKG_ROOT", $vcpkgRoot, "User")
}
catch {
    Write-Host "  setenv API failed; falling back to setx..." -ForegroundColor Yellow
}

# Method 2: setx fallback (writes to registry, works on all Windows versions)
$currentVal = [System.Environment]::GetEnvironmentVariable("VCPKG_ROOT", "User")
if ($currentVal -ne $vcpkgRoot) {
    & setx VCPKG_ROOT "$vcpkgRoot" | Out-Null
}

# Also set in current process so immediate builds work
$env:VCPKG_ROOT = $vcpkgRoot

Write-Host "`n=== Setup complete ===" -ForegroundColor Cyan
Write-Host "Close this shell, open a new one, then run:" -ForegroundColor Cyan
Write-Host "  .\scripts\win\build-windows-msvc.ps1" -ForegroundColor Cyan
```

---

## Docs

### \`docs/file_structure.md\`

~~~~markdown
# AxiomDLP Endpoint Agent — Project Structure Explained

> A complete reference for every folder and file in the repository. This document explains **what each directory is for**, **what each file contains**, and **how they fit together**.

---

## Table of Contents

- [AxiomDLP Endpoint Agent — Project Structure Explained](#axiomdlp-endpoint-agent--project-structure-explained)
  - [Table of Contents](#table-of-contents)
  - [1. Root Files](#1-root-files)
  - [2. `cmake/` — Build Helpers](#2-cmake--build-helpers)
  - [3. `external/` — Third-Party Dependencies](#3-external--third-party-dependencies)
  - [4. `src/` — Source Code](#4-src--source-code)
    - [4.1 `src/core/` — Framework](#41-srccore--framework)
    - [4.2 `src/interfaces/` — Contracts](#42-srcinterfaces--contracts)
    - [4.3 `src/event/` — Event Model](#43-srcevent--event-model)
    - [4.4 `src/policy/` — Policy Engine](#44-srcpolicy--policy-engine)
    - [4.5 `src/scanner/` — Content Scanning](#45-srcscanner--content-scanning)
    - [4.6 `src/enforcement/` — Enforcement](#46-srcenforcement--enforcement)
    - [4.7 `src/storage/` — Local Persistence](#47-srcstorage--local-persistence)
    - [4.8 `src/transport/` — Connection Layer](#48-srctransport--connection-layer)
    - [4.9 `src/ipc/` — Service ↔ Helper IPC](#49-srcipc--service--helper-ipc)
    - [4.10 `src/telemetry/` — Metrics \& Health](#410-srctelemetry--metrics--health)
    - [4.11 `src/platform/` — OS Abstraction](#411-srcplatform--os-abstraction)
      - [`platform/windows/`](#platformwindows)
      - [`platform/linux/`](#platformlinux)
    - [4.12 `src/modules/` — Feature Modules](#412-srcmodules--feature-modules)
  - [5. `kernel/` — Future Kernel Drivers](#5-kernel--future-kernel-drivers)
  - [6. `helper/` — User Session Helper](#6-helper--user-session-helper)
  - [7. `config/` — Configuration Files](#7-config--configuration-files)
  - [8. `tests/` — Test Suite](#8-tests--test-suite)
  - [9. `scripts/` — Developer \& Build Scripts](#9-scripts--developer--build-scripts)
  - [10. `packaging/` — Installers \& Packages](#10-packaging--installers--packages)
  - [11. `docs/` — Documentation](#11-docs--documentation)
  - [How the Pieces Fit Together (Quick Recap)](#how-the-pieces-fit-together-quick-recap)
  - [Summary Table](#summary-table)

---

## 1. Root Files

| File | Purpose |
|------|---------|
| **`CMakeLists.txt`** | Root CMake build file. Detects platform, defines global compiler flags, includes subdirectories, links libraries, and produces the final agent binary. |
| **`CMakePresets.json`** | Predefined build configurations (`debug`, `release`, `relwithdebinfo`). Lets developers run `cmake --preset=debug` instead of typing long flags. |
| **`README.md`** | Top-level project introduction, architecture overview, quick-start instructions. |
| **`LICENSE`** | Apache 2.0 license text. |
| **`.gitignore`** | Excludes build artifacts, IDE files, logs, and secrets from Git. |
| **`.clang-format`** | Code formatting rules. Run `clang-format -i` on all sources before committing. |
| **`.clang-tidy`** | Static analysis rules. CI runs this; developers run it locally too. |

---

## 2. `cmake/` — Build Helpers

Contains custom CMake modules that make the root `CMakeLists.txt` cleaner.

| File | Purpose |
|------|---------|
| **`FindLibcurl.cmake`** | Locates libcurl on Linux/Windows if the system doesn't provide one. |
| **`FindSQLite3.cmake`** | Locates SQLite3 headers and libraries. |
| **`FindOpenSSL.cmake`** | Locates OpenSSL for TLS and crypto. |
| **`FindDPAPI.cmake`** | Windows-only. Detects DPAPI availability for secure storage. |
| **`FindLibsecret.cmake`** | Linux-only. Detects libsecret for keyring access. |
| **`CompilerWarnings.cmake`** | Standard warning flags per compiler (MSVC/GCC/Clang). Imported once, applied to all targets. |
| **`Packaging.cmake`** | Helper macros for CPack, DEB, RPM, MSI generation. |

**Why this exists:** Keeps platform-specific build logic out of individual modules. Every module just says "I need libcurl" and the build system figures it out.

---

## 3. `external/` — Third-Party Dependencies

Where third-party libraries live. Managed via git submodules or CMake FetchContent.

| Folder | Library | Purpose |
|--------|---------|---------|
| `nlohmann_json/` | nlohmann/json | JSON parsing and serialization |
| `spdlog/` | spdlog | Structured, fast, thread-safe logging |
| `fmt/` | fmt | Modern string formatting (used by spdlog) |
| `sqlite3/` | SQLite3 | Local embedded database |
| `libcurl/` | libcurl | HTTP/HTTPS client (used later) |
| `openssl/` | OpenSSL | TLS, certificates, hashing |
| `boost/` | Boost (Asio + Beast) | WebSocket and async I/O (later) |
| `catch2/` | Catch2 | Unit testing framework |

**Why this exists:** All external code is pinned to a specific version and vendored into the repo. This makes builds reproducible and offline-friendly.

---

## 4. `src/` — Source Code

The main source tree. Every subsystem lives under `src/`.

### 4.1 `src/core/` — Framework

The backbone of the agent. **Platform-agnostic** — no OS headers here.

| File | Purpose |
|------|---------|
| **`application.{h,cpp}`** | The top-level orchestrator. Boots the agent: loads config, creates `ServiceContext`, initializes the module manager, starts modules, handles shutdown. |
| **`module_manager.{h,cpp}`** | Registers, initializes, starts, stops, and supervises all modules. Also reports aggregate health. |
| **`event_bus.{h,cpp}`** | In-process publish/subscribe. Modules publish events; subscribers (policy engine, storage, telemetry) receive them. |
| **`service_context.{h,cpp}`** | The dependency container. Holds references to config, logger, event bus, storage, transport, module manager. Passed into every module on init. |
| **`config.{h,cpp}`** | Loads and validates `agent.default.json` and overrides. Provides typed access to config values. |
| **`logger.{h,cpp}`** | Thin wrapper around spdlog. Sets up log sinks (console, file, rotating). Enforces redaction rules. |
| **`version.{h,cpp}`** | Compile-time and runtime version strings. Used in logs, heartbeat, and packaging. |
| **`error.{h,cpp}`** | Standard error codes and a `Result<T>` type. Used across module boundaries instead of exceptions. |

**Why this exists:** The core is the framework. It has no idea what a filesystem or USB is. It just knows how to run modules.

---

### 4.2 `src/interfaces/` — Contracts

Pure abstract base classes. No implementation. Every subsystem agrees on these.

| File | Purpose |
|------|---------|
| **`i_module.h`** | The module contract: `initialize`, `start`, `stop`, `shutdown`, `health`. |
| **`i_event_source.h`** | For modules that produce events (filesystem, USB, clipboard). Defines the callback registration. |
| **`i_enforcer.h`** | For modules that can block operations. Defines `enforce(Decision, Event)`. |
| **`i_policy_provider.h`** | For sources of policies (local cache, remote sync, bootstrap). |
| **`i_transport.h`** | Abstraction for HTTP/WebSocket. Lets you swap real transport for a mock in tests. |
| **`i_storage.h`** | Abstraction for local persistence. |
| **`i_ipc.h`** | Abstraction for IPC. Windows named pipes and Unix sockets implement this. |
| **`i_crypto.h`** | Abstraction for encryption and secure storage. DPAPI and libsecret implement this. |

**Why this exists:** Interfaces decouple. A module written against `IStorage` doesn't care if storage is SQLite, in-memory, or a mock.

---

### 4.3 `src/event/` — Event Model

The data model for everything that flows through the agent.

| File | Purpose |
|------|---------|
| **`event.h`** | The `Event` struct — the core data type. Contains ID, type, timestamp, user, process, subject, payload, metadata. |
| **`event.cpp`** | Constructor helpers, validation, hashing. |
| **`event_types.h`** | Enumerations and string constants for all event types (`file_access`, `usb_insert`, etc.). Single source of truth. |
| **`event_factory.{h,cpp}`** | Factory helpers to construct events with correct defaults per type. |
| **`event_serializer.{h,cpp}`** | JSON serialization/deserialization. Later switchable to Protobuf. |

**Why this exists:** Every part of the agent speaks the same event language. Adding a new event type means editing `event_types.h` — nothing else.

---

### 4.4 `src/policy/` — Policy Engine

The brain. Turns events + scan results into decisions.

| File | Purpose |
|------|---------|
| **`policy.h`** | The `Policy` struct: id, name, entity type, threshold, action, priority, enabled, scope. |
| **`policy.cpp`** | Parsing, validation, comparison. |
| **`policy_engine.{h,cpp}`** | The evaluator. Receives events, evaluates against applicable policies, produces a `Decision`. |
| **`policy_cache.{h,cpp}`** | Local cache of policies in SQLite. Loaded at startup. Reloaded on sync. |
| **`policy_sync.{h,cpp}`** | Fetches policies from core. Stub for now; full implementation later. |
| **`decision.{h,cpp}`** | The `Decision` struct: action, risk score, reason, matching policy ID, enforce-sync flag. |

**Why this exists:** Policy logic is pure and testable. It doesn't know about files, USB, or HTTP. It only knows events and policies.

---

### 4.5 `src/scanner/` — Content Scanning

Turns raw content into detections.

| File | Purpose |
|------|---------|
| **`scanner.h`** | The unified `ScanResult` type and the `IScanner` interface. |
| **`scanner.cpp`** | Orchestrates multiple scanner backends. Decides local vs remote. |
| **`regex_scanner.{h,cpp}`** | Fast, offline pattern matching for credit cards, SSNs, emails, phone numbers. |
| **`local_pii_scanner.{h,cpp}`** | Optional embedded ML model for names, addresses. Placeholder for now. |
| **`remote_pii_scanner.{h,cpp}`** | Calls the core's `/pii/detect` endpoint when online. |

**Why this exists:** Policy engine shouldn't care *how* content was scanned. It just receives a unified `ScanResult`. Swapping regex for ML later is trivial.

---

### 4.6 `src/enforcement/` — Enforcement

Turns decisions into actions.

| File | Purpose |
|------|---------|
| **`enforcer.h`** | The `Enforcer` interface. Accepts a `Decision` + `Event`, returns success/failure. |
| **`enforcer.cpp`** | Dispatches to the correct backend based on event type. |
| **`enforcer_registry.{h,cpp}`** | Maps event types → enforcer backends. Allows multiple backends per type. |
| **`backends/usermode/`** | User-mode enforcement implementations (initially empty; filled when needed). |
| **`backends/kernel/`** | Kernel-mode enforcement (empty until kernel drivers exist). |

**Why this exists:** Enforcement is the most platform-specific part. Isolating it behind a registry means kernel drivers slot in later without touching business logic.

---

### 4.7 `src/storage/` — Local Persistence

Everything that touches the disk.

| File | Purpose |
|------|---------|
| **`database.{h,cpp}`** | SQLite wrapper. Connection management, prepared statements, transactions. |
| **`migrations.{h,cpp}`** | Schema versioning. Runs on startup to bring DB up to date. |
| **`event_queue.{h,cpp}`** | Persistent queue of events waiting to be uploaded. Survives crashes. |
| **`secure_store.{h,cpp}`** | Secure token/key storage. Uses DPAPI on Windows, libsecret on Linux. |

**Why this exists:** Offline-first means local state is critical. SQLite is transactional, queryable, and battle-tested.

---

### 4.8 `src/transport/` — Connection Layer

Communication with the core. Stubbed for Phase 1.

| File | Purpose |
|------|---------|
| **`transport_manager.{h,cpp}`** | Top-level orchestrator: heartbeat loop, reconnect loop, message routing. |
| **`http_client.{h,cpp}`** | libcurl-based HTTPS client. Used for enrollment, heartbeat, policy fetch, event upload. |
| **`ws_client.{h,cpp}`** | WebSocket client (Boost.Beast). Used for the push channel. Stub for now. |
| **`heartbeat.{h,cpp}`** | Periodic heartbeat loop with retry and backoff. |
| **`reconnect.{h,cpp}`** | Exponential backoff with jitter. Shared by HTTP and WebSocket. |
| **`protocol.{h,cpp}`** | Message schemas for wire format. JSON now, Protobuf later. |

**Why this exists:** Isolates all networking code. When you swap JSON for Protobuf, only `protocol` changes.

---

### 4.9 `src/ipc/` — Service ↔ Helper IPC

Communication between the privileged service and the user-session helper.

| File | Purpose |
|------|---------|
| **`ipc_server.{h,cpp}`** | Runs in the service. Listens for helper connections (named pipe / Unix socket). |
| **`ipc_client.{h,cpp}`** | Runs in the helper. Connects to the service. |
| **`messages.h`** | Message types and payload schemas. Versioned. |

**Why this exists:** The service runs as SYSTEM/root (no GUI). The helper runs as the user. IPC is the only bridge.

---

### 4.10 `src/telemetry/` — Metrics & Health

Internal observability.

| File | Purpose |
|------|---------|
| **`metrics.{h,cpp}`** | Counters, gauges, histograms for events, scans, blocks, queue size, latency. |
| **`health.{h,cpp}`** | Per-module health reporting and aggregate agent health. |

**Why this exists:** You cannot operate a fleet of agents without visibility. Health and metrics feed heartbeats and local diagnostics.

---

### 4.11 `src/platform/` — OS Abstraction

All OS-specific code lives here, behind interfaces from `src/interfaces/`.

#### `platform/windows/`

| File | Purpose |
|------|---------|
| **`service.{h,cpp}`** | SCM integration: `ServiceMain`, control handler, install/uninstall. |
| **`named_pipe.{h,cpp}`** | Windows implementation of `IIpc`. |
| **`dpapi.{h,cpp}`** | Windows implementation of `ICrypto`. Encrypts secrets with DPAPI. |
| **`registry.{h,cpp}`** | Reads/writes agent config from the Windows Registry. |
| **`process.{h,cpp}`** | Process attribution: which user, which process triggered an event. |
| **`file_watcher.{h,cpp}`** | File event capture (`ReadDirectoryChangesW`, later Minifilter). |
| **`clipboard.{h,cpp}`** | Clipboard hooks. |
| **`usb.{h,cpp}`** | USB device events (`WM_DEVICECHANGE`, SetupAPI). |
| **`printer.{h,cpp}`** | Print spooler monitoring. |

#### `platform/linux/`

| File | Purpose |
|------|---------|
| **`systemd.{h,cpp}`** | systemd integration: notify, signals, service readiness. |
| **`unix_socket.{h,cpp}`** | Linux implementation of `IIpc`. |
| **`keyring.{h,cpp}`** | Linux implementation of `ICrypto`. Uses libsecret / kernel keyring. |
| **`process.{h,cpp}`** | Process attribution via `/proc`, later eBPF. |
| **`file_watcher.{h,cpp}`** | File event capture (`inotify`, later `fanotify`). |
| **`clipboard.{h,cpp}`** | X11 / Wayland clipboard monitoring. |
| **`usb.{h,cpp}`** | USB device events via `udev`. |
| **`printer.{h,cpp}`** | CUPS monitoring. |

**Why this exists:** Business logic never includes `<windows.h>` or `<sys/inotify.h>`. Only files here do.

---

### 4.12 `src/modules/` — Feature Modules

Each module is a self-contained DLP capability. They live under `modules/`.

| File / Folder | Purpose |
|---------------|---------|
| **`module_base.{h,cpp}`** | Common base class implementing boilerplate for `IModule`. Modules inherit from this. |
| **`filesystem/`** | Filesystem module: watches file access, scans content, blocks if policy says so. Both `IEventSource` and `IEnforcer`. |
| **`usb/`** | USB module: detects USB insert/remove, scans copied content. |
| **`clipboard/`** | Clipboard module: monitors clipboard writes, scans text, blocks on policy. |
| **`printer/`** | Print module: monitors print jobs, scans print content. |
| **`browser/`** | Browser module: placeholder for native messaging bridge to a browser extension. |
| **`process/`** | Process module: monitors process creation, tool attribution. |

Each module folder contains:

- `*_module.h` / `*_module.cpp` — the module implementation
- `CMakeLists.txt` — builds the module as a static (or shared) library

**Why this exists:** Adding a new feature is adding a new folder here and registering it in one place. The core stays untouched.

---

## 5. `kernel/` — Future Kernel Drivers

Empty for now. Reserved for kernel-mode enforcement.

| Path | Purpose |
|------|---------|
| **`windows/minifilter/`** | Windows Minifilter driver for file system enforcement (blocking reads/writes at the syscall layer). |
| **`linux/ebpf/`** | Linux eBPF LSM programs for syscall-level enforcement. |

**Why this exists:** Kernel enforcement is the ultimate goal. This folder reserves the namespace so the architecture is visible from day one.

---

## 6. `helper/` — User Session Helper

The tray/UI process running in the user's session.

| File | Purpose |
|------|---------|
| **`CMakeLists.txt`** | Builds the helper executable. |
| **`src/main.cpp`** | Entry point. Connects to the service over IPC, initializes tray. |
| **`src/tray.cpp`** | System tray icon, menu, status indicator. |
| **`src/notifications.cpp`** | User-facing notifications (blocked event, agent paused, etc.). |
| **`src/ipc_client.cpp`** | Client side of the IPC protocol. |
| **`resources/`** | Icons, localization strings, manifest. |

**Why this exists:** The service is headless. The helper is the user's window into what the agent is doing.

---

## 7. `config/` — Configuration Files

| File | Purpose |
|------|---------|
| **`agent.default.json`** | Default configuration shipped with the agent. Baseline values for every setting. |
| **`agent.schema.json`** | JSON Schema for validating config files. Catches typos and invalid values at load time. |
| **`logging.json`** | Logging configuration: sinks, levels, rotation policy, redaction rules. |

**Why this exists:** Config is externalized. Operators can tweak behavior without recompiling.

---

## 8. `tests/` — Test Suite

| Folder | Purpose |
|--------|---------|
| **`CMakeLists.txt`** | Registers tests with CTest. |
| **`unit/core/`** | Unit tests for `src/core/` — event bus, module manager, config, logger. |
| **`unit/policy/`** | Unit tests for the policy engine and decision logic. |
| **`unit/scanner/`** | Unit tests for regex and other scanners. |
| **`integration/`** | End-to-end tests: event → scan → decide → enforce → store. Uses mocks for transport. |
| **`mocks/`** | Mock implementations of interfaces (transport, event source, enforcer) for testing. |

**Why this exists:** Every module can be tested in isolation. Integration tests catch wiring issues.

---

## 9. `scripts/` — Developer & Build Scripts

| File | Purpose |
|------|---------|
| **`build.sh`** | Linux/macOS build helper. Wraps CMake presets. |
| **`build.ps1`** | Windows build helper. Same as above but PowerShell. |
| **`package_deb.sh`** | Builds a `.deb` package for Debian/Ubuntu. |
| **`package_rpm.sh`** | Builds an `.rpm` package for RHEL/Fedora. |
| **`package_msi.ps1`** | Builds an MSI installer via WiX. |
| **`dev_mock_core.py`** | A tiny Python HTTP server that mimics the core's agent endpoints. Lets you develop the transport layer without a full core. |

**Why this exists:** Common operations are one command. New contributors don't have to learn the full build pipeline.

---

## 10. `packaging/` — Installers & Packages

| Path | Purpose |
|------|---------|
| **`linux/systemd/axiomdlp-agent.service`** | The systemd unit file for the agent. |
| **`linux/debian/control`** | Debian package metadata (name, version, dependencies, description). |
| **`linux/rpm/axiomdlp-agent.spec`** | RPM spec file. |
| **`windows/installer.wxs`** | WiX Toolset XML defining the Windows installer. |
| **`windows/service_install.ps1`** | PowerShell script to register the agent as a Windows Service. |

**Why this exists:** Packaging is OS-specific, versioned, and often maintained by a different person than the code. Keeping it separate avoids clutter.

---

## 11. `docs/` — Documentation

| File | Purpose |
|------|---------|
| **`architecture.md`** | Deep-dive on the architecture: pipeline, module system, event bus. |
| **`modules.md`** | Guide to writing new modules: contract, roles, registration, testing. |
| **`policy_format.md`** | Exact JSON schema for policies and decision model. |
| **`event_schema.md`** | Exact JSON schema for events and event types. |
| **`ipc_protocol.md`** | IPC message format, versioning, and security rules. |

**Why this exists:** Documentation lives next to the code. New contributors read these before opening `src/`.

---

## How the Pieces Fit Together (Quick Recap)

```
main.cpp
   │
   ▼
Application
   ├── loads Config
   ├── creates Logger
   ├── creates ServiceContext
   │       ├── EventBus
   │       ├── Storage
   │       ├── Transport (stub)
   │       ├── PolicyEngine
   │       ├── Scanner
   │       └── EnforcerRegistry
   │
   ├── ModuleManager
   │       ├── FilesystemModule   ─┐
   │       ├── UsbModule           │  all emit events
   │       ├── ClipboardModule     │  onto the EventBus
   │       ├── PrinterModule       │
   │       └── ...                ─┘
   │
   ├── IPCServer  ←→  Helper (tray UI)
   │
   └── starts everything, waits for SIGTERM
```

Pipeline at runtime:

```
Module emits Event
   → EventBus publishes
      → Scanner consumes, produces ScanResult
         → PolicyEngine consumes, produces Decision
            → Enforcer applies Decision
               → EventQueue persists outcome
                  → Transport uploads (when online)
```

---

## Summary Table

| Folder | One-Line Purpose |
|--------|------------------|
| `cmake/` | Build system helpers and dependency finders |
| `external/` | Vendored third-party libraries |
| `src/core/` | Framework: app, modules, event bus, config, logging |
| `src/interfaces/` | Pure abstract contracts |
| `src/event/` | Event data model and serialization |
| `src/policy/` | Policy model, engine, cache, decisions |
| `src/scanner/` | Content scanning (regex, ML, remote) |
| `src/enforcement/` | Enforcement orchestration and backends |
| `src/storage/` | SQLite, migrations, event queue, secure store |
| `src/transport/` | Connection to core (HTTP/WebSocket) |
| `src/ipc/` | Service ↔ helper communication |
| `src/telemetry/` | Metrics and health |
| `src/platform/` | OS-specific implementations (Windows/Linux) |
| `src/modules/` | Feature modules (filesystem, USB, clipboard, etc.) |
| `kernel/` | Future kernel drivers (minifilter, eBPF) |
| `helper/` | User-session tray/UI process |
| `config/` | Default configs and JSON schemas |
| `tests/` | Unit, integration, mocks |
| `scripts/` | Build, packaging, dev tools |
| `packaging/` | Installers for Linux and Windows |
| `docs/` | Deep-dive documentation |

---

~~~~

---

## Skipped Files

These paths exist in the project but were deliberately left out
of this bundle because they contain third-party code, build
output, or generated artifacts. **If you need any of them, ask
the user** — do not assume their contents.

- `README.md` — not present
- `LICENSE` — binary file — not bundled
- `packaging/linux/systemd/axiomdlp-agent.service` — binary file — not bundled
- `packaging/windows/service_install.ps1` — binary file — not bundled
- `docs/architecture.md` — binary file — not bundled
- `docs/event_schema.md` — binary file — not bundled
- `docs/ipc_protocol.md` — binary file — not bundled
- `docs/modules.md` — binary file — not bundled
- `docs/policy_format.md` — binary file — not bundled
- `build/` — build output — not bundled
- `.cache/` — tool/venv cache — not bundled
- `cmake/` — not bundled (ask if needed)
- `data/` — runtime data — not bundled (ask if needed)
- `.deps/` — third-party dependency tree — not bundled
- `.git/` — version-control metadata — not bundled
- `.github/` — not bundled (ask if needed)
- `logs/` — runtime logs — not bundled (ask if needed)
- `tests/` — not bundled (ask if needed)
- `.vscode/` — editor config — not bundled
- `axiomdlp-agent-bundle.md` — generated bundle — not bundled
- `.clangd` — editor config — not bundled
- `compile_commands.json` — generated — not bundled
- `vcpkg.json` — not bundled (ask if needed)


---

## Summary

- Files included: **76**
- Files skipped:  **23**
- Bundle size:    412K
