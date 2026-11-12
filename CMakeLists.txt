cmake_minimum_required(VERSION 3.24)
project(axiomdlp-agent VERSION 0.1.0 LANGUAGES C CXX)

# ----------------------------------------------------------------
# Build options
# ----------------------------------------------------------------
option(AXIOM_USE_MQTT "Build the MQTT transport module (requires libmosquitto 2.x)" ON)

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

# ----------------------------------------------------------------
# Language settings
# ----------------------------------------------------------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

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
# pkg-config search path for i686 cross-builds
# ----------------------------------------------------------------
if(UNIX AND NOT APPLE AND CMAKE_SIZEOF_VOID_P EQUAL 4)
    set(ENV{PKG_CONFIG_LIBDIR} "/usr/lib/i386-linux-gnu/pkgconfig:/usr/share/pkgconfig")
endif()

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

# ----------------------------------------------------------------
# OpenSSL (file hashing, base64, MQTT TLS)
# ----------------------------------------------------------------
if(AXIOM_USE_VCPKG_DEPS)
    find_package(OpenSSL REQUIRED)
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
        message(FATAL_ERROR
            "OpenSSL not found at ${AXIOM_OPENSSL_DIR}. "
            "Run ./scripts/linux/build-openssl.sh first.")
    endif()

    set(OPENSSL_ROOT_DIR       "${AXIOM_OPENSSL_DIR}"                  CACHE PATH     "" FORCE)
    set(OPENSSL_INCLUDE_DIR    "${AXIOM_OPENSSL_DIR}/include"          CACHE PATH     "" FORCE)
    set(OPENSSL_SSL_LIBRARY    "${AXIOM_OPENSSL_LIB_DIR}/libssl.a"     CACHE FILEPATH "" FORCE)
    set(OPENSSL_CRYPTO_LIBRARY "${AXIOM_OPENSSL_LIB_DIR}/libcrypto.a"  CACHE FILEPATH "" FORCE)
    set(OPENSSL_USE_STATIC_LIBS ON                                     CACHE BOOL     "" FORCE)

    find_package(OpenSSL REQUIRED)

    if(NOT TARGET OpenSSL::SSL OR NOT TARGET OpenSSL::Crypto)
        message(FATAL_ERROR
            "OpenSSL imported targets missing after find_package(OpenSSL). "
            "Check ${AXIOM_OPENSSL_DIR} layout.")
    endif()
endif()

# ----------------------------------------------------------------
# libmosquitto (agent transport) — optional via AXIOM_USE_MQTT
# ----------------------------------------------------------------
if(AXIOM_USE_MQTT)
    if(AXIOM_USE_VCPKG_DEPS)
        find_package(mosquitto CONFIG QUIET)

        if(mosquitto_FOUND)
            if(TARGET mosquitto::mosquitto)
                add_library(AXIOM_MOSQUITTO ALIAS mosquitto::mosquitto)
            elseif(TARGET mosquitto::libmosquitto)
                add_library(AXIOM_MOSQUITTO ALIAS mosquitto::libmosquitto)
            elseif(TARGET mosquitto::mosquitto_static)
                add_library(AXIOM_MOSQUITTO ALIAS mosquitto::mosquitto_static)
            elseif(TARGET mosquitto)
                add_library(AXIOM_MOSQUITTO ALIAS mosquitto)
            else()
                message(WARNING "mosquitto CONFIG found but no recognized target; "
                                "falling back to find_path/find_library")
                unset(mosquitto_FOUND)
            endif()
        endif()

        if(mosquitto_FOUND)
            message(STATUS "libmosquitto: vcpkg config (aliased to AXIOM_MOSQUITTO)")
        else()
            find_path(MOSQUITTO_INCLUDE_DIR
                NAMES mosquitto.h libmosquitto.h
                PATH_SUFFIXES mosquitto)
            find_library(MOSQUITTO_LIBRARY
                NAMES mosquitto libmosquitto mosquitto_static libmosquitto_static)
            if(NOT MOSQUITTO_INCLUDE_DIR OR NOT MOSQUITTO_LIBRARY)
                message(FATAL_ERROR
                    "\n"
                    "libmosquitto not found via vcpkg.\n"
                    "\n"
                    "Make sure 'mosquitto' is listed in vcpkg.json dependencies.\n")
            endif()
            add_library(AXIOM_MOSQUITTO INTERFACE)
            target_include_directories(AXIOM_MOSQUITTO INTERFACE ${MOSQUITTO_INCLUDE_DIR})
            target_link_libraries     (AXIOM_MOSQUITTO INTERFACE ${MOSQUITTO_LIBRARY})
            message(STATUS "libmosquitto: ${MOSQUITTO_LIBRARY} (manual find)")
        endif()
    else()
        find_package(PkgConfig QUIET)
        if(NOT PkgConfig_FOUND)
            message(FATAL_ERROR "pkg-config executable not found. Install: sudo apt install pkg-config")
        endif()

        pkg_check_modules(MOSQUITTO QUIET libmosquitto)
        if(NOT MOSQUITTO_FOUND)
            message(FATAL_ERROR
                "\n"
                "libmosquitto not found via pkg-config.\n"
                "\n"
                "Install it with:\n"
                "    sudo apt install libmosquitto-dev\n"
                "\n"
                "For i686: sudo apt install libmosquitto-dev:i386\n")
        endif()

        if(MOSQUITTO_VERSION VERSION_LESS "2.0")
            message(FATAL_ERROR
                "libmosquitto ${MOSQUITTO_VERSION} is too old. Requires 2.x for MQTT 5.")
        endif()

        add_library(AXIOM_MOSQUITTO INTERFACE)
        target_include_directories(AXIOM_MOSQUITTO INTERFACE ${MOSQUITTO_INCLUDE_DIRS})
        target_link_libraries     (AXIOM_MOSQUITTO INTERFACE ${MOSQUITTO_LIBRARIES})
        if(MOSQUITTO_CFLAGS_OTHER)
            target_compile_options(AXIOM_MOSQUITTO INTERFACE ${MOSQUITTO_CFLAGS_OTHER})
        endif()
        message(STATUS "libmosquitto: ${MOSQUITTO_LIBRARIES} (version ${MOSQUITTO_VERSION})")
    endif()
else()
    message(STATUS "libmosquitto: MQTT transport disabled (AXIOM_USE_MQTT=OFF)")
endif()

# ----------------------------------------------------------------
# SQLite3
# ----------------------------------------------------------------
if(AXIOM_USE_VCPKG_DEPS)
    find_package(SQLite3 REQUIRED)
    set(AXIOM_SQLITE_TARGET SQLite::SQLite3)
else()
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
    src/util/glob_match.cpp
    src/util/file_hash.cpp
    src/util/base64.cpp
    src/util/ulid.cpp
    src/platform/common/efsw_file_watcher.cpp
    src/features/agent_sync/agent_sync_module.cpp
    src/features/content_scanner/content_scanner_module.cpp
    src/features/event_logger/event_logger_module.cpp
    src/features/filesystem_watcher/filesystem_watcher_module.cpp
    src/features/inventory/inventory_module.cpp
    src/features/inventory/inventory_scanner.cpp
    src/storage/sqlite_database.cpp
    src/storage/migrations.cpp
    src/storage/file_repository.cpp
    src/storage/state_repository.cpp
    ${AXIOM_PLATFORM_SOURCES}
)

if(AXIOM_USE_MQTT)
    target_sources(axiomdlp-agent PRIVATE
        src/features/mqtt_transport/mqtt_transport_module.cpp)
endif()

target_include_directories(axiomdlp-agent PRIVATE src)

target_compile_definitions(axiomdlp-agent PRIVATE
    SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_TRACE
    AXIOM_AGENT_VERSION="${PROJECT_VERSION}"
)

if(AXIOM_USE_MQTT)
    target_compile_definitions(axiomdlp-agent PRIVATE AXIOM_HAVE_MQTT=1)
endif()

target_link_libraries(axiomdlp-agent PRIVATE
    spdlog::spdlog
    nlohmann_json::nlohmann_json
    ${AXIOM_EFSW_TARGET}
    ${AXIOM_SQLITE_TARGET}
    OpenSSL::SSL
    OpenSSL::Crypto
    Threads::Threads
)

if(AXIOM_USE_MQTT)
    target_link_libraries(axiomdlp-agent PRIVATE AXIOM_MOSQUITTO)
endif()

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

    if(CMAKE_SIZEOF_VOID_P EQUAL 4)
        set_target_properties(axiomdlp-agent PROPERTIES
            INSTALL_RPATH "/usr/lib/i386-linux-gnu"
            BUILD_WITH_INSTALL_RPATH TRUE)
    endif()
endif()

if(MSVC)
    target_compile_options(axiomdlp-agent PRIVATE /W4 /permissive- /Zc:__cplusplus /bigobj)
else()
    target_compile_options(axiomdlp-agent PRIVATE -Wall -Wextra -Wpedantic)
endif()