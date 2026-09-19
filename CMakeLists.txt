cmake_minimum_required(VERSION 3.24)
project(axiomdlp-agent VERSION 0.1.0 LANGUAGES CXX)

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
    src/core/logger.cpp
    ${AXIOM_PLATFORM_SOURCES}
)

target_include_directories(axiomdlp-agent PRIVATE src)

target_link_libraries(axiomdlp-agent PRIVATE
    spdlog::spdlog
    nlohmann_json::nlohmann_json
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
    #
    # Linux targets (32-bit and 64-bit) attempt to link libsystemd via
    # pkg-config. Windows targets skip this block entirely (UNIX is false).
    #
    # For 32-bit builds, PKG_CONFIG_LIBDIR must point at the i386
    # pkg-config directory so that the correct .pc file is used.
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