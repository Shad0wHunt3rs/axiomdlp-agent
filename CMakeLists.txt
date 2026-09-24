cmake_minimum_required(VERSION 3.24)
project(axiomdlp-agent VERSION 0.1.0 LANGUAGES C CXX)

# ----------------------------------------------------------------
# Shared source cache for FetchContent dependencies
# ----------------------------------------------------------------
set(AXIOM_DEPS_DIR "${CMAKE_SOURCE_DIR}/.deps" CACHE PATH "Shared FetchContent source cache")
file(MAKE_DIRECTORY "${AXIOM_DEPS_DIR}")

if(EXISTS "${AXIOM_DEPS_DIR}/spdlog-src")
    set(FETCHCONTENT_SOURCE_DIR_SPDLOG "${AXIOM_DEPS_DIR}/spdlog-src"
        CACHE PATH "spdlog source cache" FORCE)
endif()
if(EXISTS "${AXIOM_DEPS_DIR}/nlohmann_json-src")
    set(FETCHCONTENT_SOURCE_DIR_NLOHMANN_JSON "${AXIOM_DEPS_DIR}/nlohmann_json-src"
        CACHE PATH "nlohmann/json source cache" FORCE)
endif()
if(EXISTS "${AXIOM_DEPS_DIR}/efsw-src")
    set(FETCHCONTENT_SOURCE_DIR_EFSW "${AXIOM_DEPS_DIR}/efsw-src"
        CACHE PATH "efsw source cache" FORCE)
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
message(STATUS "Dependency source cache: ${AXIOM_DEPS_DIR}")

# ----------------------------------------------------------------
# Third-party dependencies
# ----------------------------------------------------------------
include(FetchContent)

# --- nlohmann/json (header-only; prefer system) ---
find_package(nlohmann_json QUIET)

if(NOT nlohmann_json_FOUND)
    message(STATUS "System nlohmann_json not found; fetching via FetchContent")
    set(JSON_BuildTests OFF CACHE BOOL "" FORCE)
    set(JSON_Install    OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(
        nlohmann_json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG        v3.11.3
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(nlohmann_json)
else()
    message(STATUS "Using system nlohmann_json (${nlohmann_json_VERSION})")
endif()

# --- spdlog (compiled; always FetchContent for cross-compile) ---
set(SPDLOG_BUILD_SHARED   OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_EXAMPLE  OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_BENCH    OFF CACHE BOOL "" FORCE)
set(SPDLOG_INSTALL        OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG        v1.14.1
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(spdlog)

# --- efsw (file system watcher) ---
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(BUILD_STATIC_LIBS ON  CACHE BOOL "" FORCE)
set(EFSW_BUILD_TESTS  OFF CACHE BOOL "" FORCE)
set(EFSW_BUILD_EXAMPLE OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    efsw
    GIT_REPOSITORY https://github.com/SpartanJ/efsw.git
    GIT_TAG        1.7.2
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(efsw)

# ----------------------------------------------------------------
# SQLite3 amalgamation
#
# Single path for every target. The amalgamation is a .c + .h pair
# that compiles for every architecture, so one cached copy serves
# linux-x86_64, linux-i686, windows-x86_64, and windows-i686.
#
# Cache location: ${AXIOM_DEPS_DIR}/sqlite3-src/
#
# A cache entry is valid only if sqlite3.c exists AND is larger than
# 5 MB. That threshold catches the failure mode where an interrupted
# download leaves a truncated file behind: CMake would happily use it
# and the compiler would produce hundreds of "unterminated #if"
# errors. With the size check, a truncated file is removed and
# re-downloaded automatically.
#
# Downloads use curl or wget via execute_process. CMake's built-in
# file(DOWNLOAD) silently produces a 0-byte file when the request
# fails for any reason (TLS, redirect, proxy, network), which is
# useless for diagnosing the failure.
# ----------------------------------------------------------------
set(AXIOM_SQLITE_DIR  "${AXIOM_DEPS_DIR}/sqlite3-src")
set(AXIOM_SQLITE_C    "${AXIOM_SQLITE_DIR}/sqlite3.c")
set(AXIOM_SQLITE_ZIP  "${AXIOM_DEPS_DIR}/sqlite3.zip")
set(AXIOM_SQLITE_URL  "https://www.sqlite.org/2024/sqlite-amalgamation-3460100.zip")
set(AXIOM_SQLITE_MIN_BYTES 5000000)   # ~5 MB; real file is ~9 MB

# --- Validate the current cache (if any) ---
set(AXIOM_SQLITE_CACHE_OK OFF)

if(EXISTS "${AXIOM_SQLITE_C}")
    file(SIZE "${AXIOM_SQLITE_C}" cache_size)
    if(cache_size GREATER_EQUAL ${AXIOM_SQLITE_MIN_BYTES})
        set(AXIOM_SQLITE_CACHE_OK ON)
    else()
        message(STATUS "SQLite cache at ${AXIOM_SQLITE_C} is truncated "
                       "(${cache_size} bytes < ${AXIOM_SQLITE_MIN_BYTES}); "
                       "removing and re-downloading")
        file(REMOVE_RECURSE "${AXIOM_SQLITE_DIR}")
    endif()
endif()

# --- Download + extract if the cache is missing or was invalidated ---
if(NOT AXIOM_SQLITE_CACHE_OK)
    message(STATUS "Downloading SQLite amalgamation")
    message(STATUS "  URL: ${AXIOM_SQLITE_URL}")

    file(REMOVE "${AXIOM_SQLITE_ZIP}")

    find_program(AXIOM_CURL NAMES curl)
    find_program(AXIOM_WGET NAMES wget)

    if(AXIOM_CURL)
        message(STATUS "  Downloader: ${AXIOM_CURL}")
        execute_process(
            COMMAND "${AXIOM_CURL}" -fL --retry 3 --retry-delay 2
                    -o "${AXIOM_SQLITE_ZIP}" "${AXIOM_SQLITE_URL}"
            RESULT_VARIABLE dl_result
            ERROR_VARIABLE  dl_error
        )
    elseif(AXIOM_WGET)
        message(STATUS "  Downloader: ${AXIOM_WGET}")
        execute_process(
            COMMAND "${AXIOM_WGET}" -O "${AXIOM_SQLITE_ZIP}" "${AXIOM_SQLITE_URL}"
            RESULT_VARIABLE dl_result
            ERROR_VARIABLE  dl_error
        )
    else()
        message(FATAL_ERROR
            "Neither curl nor wget is installed. Install one:\n"
            "    sudo apt install -y curl\n"
            "or download manually from ${AXIOM_SQLITE_URL}\n"
            "and place the extracted sqlite3.c / sqlite3.h in\n"
            "    ${AXIOM_SQLITE_DIR}/")
    endif()

    if(NOT dl_result EQUAL 0)
        message(FATAL_ERROR
            "SQLite download failed (exit ${dl_result}).\n"
            "Output: ${dl_error}\n"
            "Download manually from ${AXIOM_SQLITE_URL} and place the\n"
            "extracted sqlite3.c / sqlite3.h in ${AXIOM_SQLITE_DIR}/")
    endif()

    # Reject a truncated archive before we try to extract it.
    file(SIZE "${AXIOM_SQLITE_ZIP}" zip_size)
    if(zip_size LESS 2000000)
        message(FATAL_ERROR
            "SQLite download is truncated (${zip_size} bytes). "
            "Delete ${AXIOM_SQLITE_ZIP} and retry, or download manually "
            "from ${AXIOM_SQLITE_URL}")
    endif()

    file(ARCHIVE_EXTRACT INPUT "${AXIOM_SQLITE_ZIP}"
         DESTINATION "${AXIOM_DEPS_DIR}/sqlite3-extract")

    file(GLOB SQLITE_DIRS "${AXIOM_DEPS_DIR}/sqlite3-extract/sqlite-amalgamation-*")
    list(LENGTH SQLITE_DIRS SQLITE_DIRS_LEN)
    if(SQLITE_DIRS_LEN EQUAL 0)
        message(FATAL_ERROR "SQLite amalgamation extraction produced no directory")
    endif()
    list(GET SQLITE_DIRS 0 SQLITE_DIR)

    file(RENAME "${SQLITE_DIR}" "${AXIOM_SQLITE_DIR}")
    file(REMOVE_RECURSE "${AXIOM_DEPS_DIR}/sqlite3-extract")
    file(REMOVE "${AXIOM_SQLITE_ZIP}")

    # Final sanity check on the extracted source.
    file(SIZE "${AXIOM_SQLITE_C}" extracted_size)
    if(extracted_size LESS ${AXIOM_SQLITE_MIN_BYTES})
        message(FATAL_ERROR
            "Extracted sqlite3.c is truncated (${extracted_size} bytes). "
            "Something is wrong with the download or extraction.")
    endif()

    message(STATUS "SQLite amalgamation cached at ${AXIOM_SQLITE_DIR}")
else()
    message(STATUS "Using cached SQLite amalgamation from .deps/sqlite3-src/")
endif()

# --- Build the amalgamation as a static library ---
add_library(axiom_sqlite STATIC "${AXIOM_SQLITE_C}")
target_include_directories(axiom_sqlite PUBLIC "${AXIOM_SQLITE_DIR}")

target_compile_definitions(axiom_sqlite PRIVATE
    SQLITE_THREADSAFE=1
    SQLITE_OMIT_LOAD_EXTENSION=1
    SQLITE_DEFAULT_MEMSTATUS=0
    SQLITE_DQS=0
)

if(UNIX AND NOT APPLE)
    target_link_libraries(axiom_sqlite PUBLIC dl pthread)
endif()

# ----------------------------------------------------------------
# Platform-specific sources
# ----------------------------------------------------------------
if(WIN32)
    set(AXIOM_PLATFORM_SOURCES
        src/platform/windows/paths_windows.cpp
        src/platform/windows/service_win.cpp
    )
elseif(UNIX AND NOT APPLE)
    set(AXIOM_PLATFORM_SOURCES
        src/platform/linux/paths_linux.cpp
        src/platform/linux/service_linux.cpp
    )
endif()

# ----------------------------------------------------------------
# Agent executable
# ----------------------------------------------------------------
add_executable(axiomdlp-agent
    src/main.cpp
    src/core/application.cpp
    src/core/config.cpp
    src/core/event.cpp
    src/core/event_bus.cpp
    src/core/logger.cpp
    src/core/module_manager.cpp
    src/platform/common/efsw_file_watcher.cpp
    src/modules/module_base.cpp
    src/modules/filesystem_watcher/filesystem_watcher_module.cpp
    src/modules/inventory/inventory_module.cpp
    src/modules/inventory/inventory_scanner.cpp
    src/modules/event_logger/event_logger_module.cpp
    src/storage/database.cpp
    src/storage/migrations.cpp
    src/storage/file_store.cpp
    ${AXIOM_PLATFORM_SOURCES}
)

target_include_directories(axiomdlp-agent PRIVATE src)

# Compile all spdlog log macros (TRACE..CRITICAL) regardless of the
# runtime level. Without this, spdlog's SPDLOG_DEBUG and SPDLOG_TRACE
# expand to (void)0, so their arguments are never evaluated. That both
# silences useful diagnostics and produces -Wunused-parameter warnings
# in lambdas whose only statement is a debug log. Runtime filtering is
# still applied by the configured level, so nothing extra is written
# unless logging.level is set to "debug" or "trace".
target_compile_definitions(axiomdlp-agent PRIVATE
    SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_TRACE
)

target_link_libraries(axiomdlp-agent PRIVATE
    spdlog::spdlog
    nlohmann_json::nlohmann_json
    efsw-static
    axiom_sqlite
)

# ----------------------------------------------------------------
# Platform-specific settings
# ----------------------------------------------------------------
if(WIN32)
    target_link_libraries(axiomdlp-agent PRIVATE advapi32 shell32)

    target_link_options(axiomdlp-agent PRIVATE
        -static -static-libgcc -static-libstdc++
    )
    target_compile_definitions(axiomdlp-agent PRIVATE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
        _WIN32_WINNT=0x0601
    )
endif()

if(UNIX AND NOT APPLE)
    # ----------------------------------------------------------------
    # libsystemd (sd_notify) is OPTIONAL.
    # ----------------------------------------------------------------
    set(AXIOM_HAVE_SYSTEMD OFF)

    include(FindPkgConfig QUIET)
    if(PkgConfig_FOUND)
        if(CMAKE_SIZEOF_VOID_P EQUAL 4)
            set(ENV{PKG_CONFIG_LIBDIR}
                "/usr/lib/i386-linux-gnu/pkgconfig:/usr/share/pkgconfig")
        else()
            set(ENV{PKG_CONFIG_LIBDIR}
                "/usr/lib/x86_64-linux-gnu/pkgconfig:/usr/share/pkgconfig")
        endif()

        pkg_check_modules(SYSTEMD QUIET libsystemd)
        if(SYSTEMD_FOUND)
            set(AXIOM_HAVE_SYSTEMD ON)
        endif()
    endif()

    if(AXIOM_HAVE_SYSTEMD)
        message(STATUS "libsystemd found: sd_notify enabled")
        target_compile_definitions(axiomdlp-agent PRIVATE HAVE_SYSTEMD)
        target_include_directories(axiomdlp-agent PRIVATE ${SYSTEMD_INCLUDE_DIRS})
        target_link_libraries(axiomdlp-agent PRIVATE ${SYSTEMD_LIBRARIES})
    else()
        message(STATUS "libsystemd not available: sd_notify disabled")
    endif()

    target_compile_options(axiomdlp-agent PRIVATE
        -fstack-protector-strong -fPIE
    )
    target_link_options(axiomdlp-agent PRIVATE
        -pie -Wl,-z,relro,-z,now
    )
endif()

# ----------------------------------------------------------------
# Compiler warnings
# ----------------------------------------------------------------
if(MSVC)
    target_compile_options(axiomdlp-agent PRIVATE /W4 /permissive-)
else()
    target_compile_options(axiomdlp-agent PRIVATE -Wall -Wextra -Wpedantic)
endif()