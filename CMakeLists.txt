cmake_minimum_required(VERSION 3.24)
project(axiomdlp-agent VERSION 0.1.0 LANGUAGES CXX)

# ----------------------------------------------------------------
# Language settings
# ----------------------------------------------------------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Position-independent executables for Linux hardening (harmless on Windows).
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# ----------------------------------------------------------------
# Detect target triple for logging
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

# spdlog as a static library — no shared object to ship.
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
# Agent executable
# ----------------------------------------------------------------
add_executable(axiomdlp-agent
    src/main.cpp
    src/core/logger.cpp
)

target_include_directories(axiomdlp-agent PRIVATE src)
target_link_libraries(axiomdlp-agent PRIVATE spdlog::spdlog)

# ----------------------------------------------------------------
# Platform-specific settings
# ----------------------------------------------------------------

# --- Windows (MinGW-w64, both 32-bit and 64-bit) ---
if(WIN32)
    # Fully static binary: no libstdc++-6.dll, no libgcc_s_seh-1.dll, no libwinpthread-1.dll.
    target_link_options(axiomdlp-agent PRIVATE
        -static
        -static-libgcc
        -static-libstdc++
    )

    # Windows headers hygiene.
    target_compile_definitions(axiomdlp-agent PRIVATE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
        _WIN32_WINNT=0x0601   # Windows 7 or later
    )
endif()

# --- Linux ---
if(UNIX AND NOT APPLE)
    # Standard Linux hardening.
    target_compile_options(axiomdlp-agent PRIVATE
        -fstack-protector-strong
        -fPIE
    )
    target_link_options(axiomdlp-agent PRIVATE
        -pie
        -Wl,-z,relro,-z,now
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