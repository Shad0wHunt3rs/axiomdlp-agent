cmake_minimum_required(VERSION 3.24)
project(axiomdlp-agent VERSION 0.1.0 LANGUAGES C CXX)

# ----------------------------------------------------------------
# Build-time options
# ----------------------------------------------------------------
option(AXIOM_ENABLE_GRPC
       "Enable gRPC transport module (requires 'grpc' and 'protobuf' in vcpkg.json)" OFF)

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
# Dependency acquisition strategy
#
#   MSVC (Windows, native)   -> vcpkg manifest mode
#   MINGW (Windows, cross)   -> existing .deps/ cross-built trees
#   UNIX (Linux)             -> existing .deps/ native trees
# ----------------------------------------------------------------
if(MSVC)
    set(AXIOM_USE_VCPKG_DEPS ON)
    message(STATUS "Build strategy: MSVC + vcpkg (Windows native)")
    if(NOT DEFINED CMAKE_MSVC_RUNTIME_LIBRARY)
        set(CMAKE_MSVC_RUNTIME_LIBRARY
            "MultiThreaded$<$<CONFIG:Debug>:Debug>" CACHE STRING
            "Use static CRT to match MinGW's -static-libstdc++")
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
message(STATUS "Dependency source cache: ${AXIOM_DEPS_DIR}")

# ----------------------------------------------------------------
# Third-party dependencies
# ----------------------------------------------------------------
include(FetchContent)

# --- nlohmann/json (header-only) ---
if(AXIOM_USE_VCPKG_DEPS)
    find_package(nlohmann_json CONFIG REQUIRED)
    message(STATUS "nlohmann_json (from vcpkg): ${nlohmann_json_VERSION}")
else()
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
endif()

# --- spdlog ---
if(AXIOM_USE_VCPKG_DEPS)
    find_package(spdlog CONFIG REQUIRED)
    message(STATUS "spdlog (from vcpkg): ${spdlog_VERSION}")
else()
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
endif()

# --- efsw (file system watcher) ---
if(AXIOM_USE_VCPKG_DEPS)
    find_package(efsw CONFIG REQUIRED)
    if(TARGET efsw::efsw)
        set(AXIOM_EFSW_TARGET efsw::efsw)
    else()
        set(AXIOM_EFSW_TARGET efsw)
    endif()
    message(STATUS "efsw (from vcpkg)")
else()
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    set(BUILD_STATIC_LIBS ON  CACHE BOOL "" FORCE)
    set(EFSW_BUILD_TESTS   OFF CACHE BOOL "" FORCE)
    set(EFSW_BUILD_EXAMPLE OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(
        efsw
        GIT_REPOSITORY https://github.com/SpartanJ/efsw.git
        GIT_TAG        1.7.2
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(efsw)
    set(AXIOM_EFSW_TARGET efsw-static)
endif()

# ----------------------------------------------------------------
# Boost (Beast + Asio + System)
# ----------------------------------------------------------------
set(AXIOM_BOOST_ROOT "" CACHE PATH "Override Boost header root")

if(AXIOM_USE_VCPKG_DEPS)
    find_package(Boost CONFIG REQUIRED)
    set(AXIOM_BOOST_INCLUDE_DIRS "")
    message(STATUS "Boost: from vcpkg (CONFIG mode)")
else()
    if(NOT AXIOM_BOOST_ROOT)
        foreach(candidate "/usr/include" "/usr/local/include")
            if(EXISTS "${candidate}/boost/beast.hpp")
                set(AXIOM_BOOST_ROOT "${candidate}")
                break()
            endif()
        endforeach()
    endif()
    if(NOT AXIOM_BOOST_ROOT OR NOT EXISTS "${AXIOM_BOOST_ROOT}/boost/beast.hpp")
        message(FATAL_ERROR
            "Boost headers not found. Install them with:\n"
            "    sudo apt install -y libboost-dev\n"
            "Then verify that /usr/include/boost/beast.hpp exists.")
    endif()
    set(AXIOM_BOOST_ISOLATED_DIR "${AXIOM_DEPS_DIR}/boost-include")
    if(NOT EXISTS "${AXIOM_BOOST_ISOLATED_DIR}/boost/beast.hpp")
        message(STATUS "Creating isolated Boost include directory at ${AXIOM_BOOST_ISOLATED_DIR}")
        file(MAKE_DIRECTORY "${AXIOM_BOOST_ISOLATED_DIR}")
        file(CREATE_LINK
             "${AXIOM_BOOST_ROOT}/boost"
             "${AXIOM_BOOST_ISOLATED_DIR}/boost"
             SYMBOLIC)
    endif()
    file(STRINGS "${AXIOM_BOOST_ROOT}/boost/version.hpp"
         _boost_version_line
         REGEX "#define BOOST_LIB_VERSION ")
    if(_boost_version_line MATCHES "\"([0-9_]+)\"")
        set(_boost_version_raw "${CMAKE_MATCH_1}")
        string(REPLACE "_" "." _boost_version "${_boost_version_raw}")
    else()
        set(_boost_version "unknown")
    endif()
    message(STATUS "Boost headers (isolated): ${AXIOM_BOOST_ISOLATED_DIR}/boost (version ${_boost_version})")
    set(AXIOM_BOOST_INCLUDE_DIRS "${AXIOM_BOOST_ISOLATED_DIR}")
endif()

find_package(Threads REQUIRED)

# ----------------------------------------------------------------
# gRPC + Protobuf (OPTIONAL — gated by AXIOM_ENABLE_GRPC)
#
# Disabled by default because 'grpc' and 'protobuf' are NOT in
# vcpkg.json. To enable later:
#   1. Add "protobuf" and "grpc" to vcpkg.json
#   2. Configure with:  cmake --preset windows-x64-msvc-release -DAXIOM_ENABLE_GRPC=ON
#
# On Linux, .deps/grpc-linux-* wiring would go in the else() branch.
# ----------------------------------------------------------------
set(AXIOM_HAVE_GRPC OFF)
set(AXIOM_GRPC_SOURCES "")
set(AXIOM_GRPC_LIBS "")
set(AXIOM_PROTO_SRC "")
set(AXIOM_PROTO_HDR "")
set(AXIOM_PROTO_GEN_DIR "")

if(AXIOM_USE_VCPKG_DEPS AND AXIOM_ENABLE_GRPC)
    find_package(Protobuf CONFIG REQUIRED)
    find_package(gRPC CONFIG REQUIRED)

    set(AXIOM_HAVE_GRPC ON)
    set(AXIOM_PROTO_DIR     "${CMAKE_CURRENT_SOURCE_DIR}/src/features/grpc_transport")
    set(AXIOM_PROTO_FILE    "${AXIOM_PROTO_DIR}/agent.proto")
    set(AXIOM_PROTO_GEN_DIR "${CMAKE_CURRENT_BINARY_DIR}/gen")
    set(AXIOM_PROTO_PKG_DIR "${AXIOM_PROTO_GEN_DIR}/axiomdlp/agent/v1")

    set(AXIOM_PROTO_SRC
        "${AXIOM_PROTO_PKG_DIR}/agent.pb.cc"
        "${AXIOM_PROTO_PKG_DIR}/agent.grpc.pb.cc"
    )
    set(AXIOM_PROTO_HDR
        "${AXIOM_PROTO_PKG_DIR}/agent.pb.h"
        "${AXIOM_PROTO_PKG_DIR}/agent.grpc.pb.h"
    )

    set(AXIOM_GRPC_SOURCES "src/features/grpc_transport/grpc_transport_module.cpp")
    set(AXIOM_GRPC_LIBS    gRPC::grpc++ protobuf::libprotobuf)

    add_custom_command(
        OUTPUT  ${AXIOM_PROTO_SRC} ${AXIOM_PROTO_HDR}
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${AXIOM_PROTO_PKG_DIR}"
        COMMAND "$<TARGET_FILE:protobuf::protoc>"
            --proto_path=${AXIOM_PROTO_DIR}
            --cpp_out=${AXIOM_PROTO_GEN_DIR}
            --grpc_out=${AXIOM_PROTO_GEN_DIR}
            --plugin=protoc-gen-grpc=$<TARGET_FILE:gRPC::grpc_cpp_plugin>
            ${AXIOM_PROTO_FILE}
        DEPENDS ${AXIOM_PROTO_FILE}
        COMMENT "protoc: generating C++ from agent.proto"
        VERBATIM
    )

    add_custom_target(axiom_grpc_gen DEPENDS ${AXIOM_PROTO_SRC} ${AXIOM_PROTO_HDR})

    message(STATUS "gRPC (from vcpkg): ${gRPC_VERSION}")
    message(STATUS "Protobuf (from vcpkg): ${Protobuf_VERSION}")
else()
    if(NOT AXIOM_ENABLE_GRPC)
        message(STATUS "gRPC: disabled (AXIOM_ENABLE_GRPC=OFF)")
    else()
        message(STATUS "gRPC: requested but vcpkg not in use — grpc_transport excluded")
    endif()
endif()

# ----------------------------------------------------------------
# OpenSSL
# ----------------------------------------------------------------
if(AXIOM_USE_VCPKG_DEPS)
    find_package(OpenSSL REQUIRED)
    message(STATUS "OpenSSL (from vcpkg): ${OPENSSL_VERSION}")
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
            "OpenSSL not found in ${AXIOM_OPENSSL_DIR}.\n"
            "Run ./scripts/build-openssl.sh first.")
    endif()
    set(AXIOM_OPENSSL_INCLUDE_DIR "${AXIOM_OPENSSL_DIR}/include")
    if(NOT EXISTS "${AXIOM_OPENSSL_INCLUDE_DIR}/openssl/ssl.h")
        message(FATAL_ERROR
            "OpenSSL headers not found at ${AXIOM_OPENSSL_INCLUDE_DIR}/openssl/.")
    endif()
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
    message(STATUS "OpenSSL (imported from .deps)")
    message(STATUS "  include: ${AXIOM_OPENSSL_INCLUDE_DIR}")
    message(STATUS "  ssl:     ${AXIOM_OPENSSL_LIB_DIR}/libssl.a")
    message(STATUS "  crypto:  ${AXIOM_OPENSSL_LIB_DIR}/libcrypto.a")
endif()

# ----------------------------------------------------------------
# SQLite3
# ----------------------------------------------------------------
if(AXIOM_USE_VCPKG_DEPS)
    find_package(SQLite3 REQUIRED)
    set(AXIOM_SQLITE_TARGET SQLite::SQLite3)
    message(STATUS "SQLite3 (from vcpkg): ${SQLite3_VERSION}")
else()
    set(AXIOM_SQLITE_DIR "${AXIOM_DEPS_DIR}/sqlite3-src")
    set(AXIOM_SQLITE_C   "${AXIOM_SQLITE_DIR}/sqlite3.c")
    set(AXIOM_SQLITE_ZIP "${AXIOM_DEPS_DIR}/sqlite3.zip")
    set(AXIOM_SQLITE_URL "https://www.sqlite.org/2024/sqlite-amalgamation-3460100.zip")
    set(AXIOM_SQLITE_MIN_BYTES 5000000)
    set(AXIOM_SQLITE_CACHE_OK OFF)
    if(EXISTS "${AXIOM_SQLITE_C}")
        file(SIZE "${AXIOM_SQLITE_C}" cache_size)
        if(cache_size GREATER_EQUAL ${AXIOM_SQLITE_MIN_BYTES})
            set(AXIOM_SQLITE_CACHE_OK ON)
        else()
            message(STATUS "SQLite cache truncated (${cache_size} bytes); re-downloading")
            file(REMOVE_RECURSE "${AXIOM_SQLITE_DIR}")
        endif()
    endif()
    if(NOT AXIOM_SQLITE_CACHE_OK)
        message(STATUS "Downloading SQLite amalgamation")
        file(REMOVE "${AXIOM_SQLITE_ZIP}")
        find_program(AXIOM_CURL_BIN NAMES curl)
        find_program(AXIOM_WGET_BIN NAMES wget)
        if(AXIOM_CURL_BIN)
            execute_process(
                COMMAND "${AXIOM_CURL_BIN}" -fL --retry 3 --retry-delay 2
                        -o "${AXIOM_SQLITE_ZIP}" "${AXIOM_SQLITE_URL}"
                RESULT_VARIABLE dl_result
                ERROR_VARIABLE  dl_error
            )
        elseif(AXIOM_WGET_BIN)
            execute_process(
                COMMAND "${AXIOM_WGET_BIN}" -O "${AXIOM_SQLITE_ZIP}" "${AXIOM_SQLITE_URL}"
                RESULT_VARIABLE dl_result
                ERROR_VARIABLE  dl_error
            )
        else()
            message(FATAL_ERROR "Neither curl nor wget is installed.")
        endif()
        if(NOT dl_result EQUAL 0)
            message(FATAL_ERROR "SQLite download failed: ${dl_error}")
        endif()
        file(SIZE "${AXIOM_SQLITE_ZIP}" zip_size)
        if(zip_size LESS 2000000)
            message(FATAL_ERROR "SQLite download is truncated (${zip_size} bytes).")
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
        file(SIZE "${AXIOM_SQLITE_C}" extracted_size)
        if(extracted_size LESS ${AXIOM_SQLITE_MIN_BYTES})
            message(FATAL_ERROR "Extracted sqlite3.c is truncated (${extracted_size} bytes).")
        endif()
        message(STATUS "SQLite amalgamation cached at ${AXIOM_SQLITE_DIR}")
    else()
        message(STATUS "Using cached SQLite amalgamation from .deps/sqlite3-src/")
    endif()
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
    set(AXIOM_SQLITE_TARGET axiom_sqlite)
endif()

# ----------------------------------------------------------------
# Platform-specific sources
# ----------------------------------------------------------------
if(WIN32)
    set(AXIOM_PLATFORM_SOURCES
        src/platform/windows/paths.cpp
        src/platform/windows/service.cpp
    )
elseif(UNIX AND NOT APPLE)
    set(AXIOM_PLATFORM_SOURCES
        src/platform/linux/paths.cpp
        src/platform/linux/service.cpp
    )
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
    src/net/websocket_client.cpp
    src/net/outbound_queue.cpp
    src/net/message_router.cpp
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
    src/features/websocket_transport/transport_module.cpp
    src/storage/sqlite_database.cpp
    src/storage/migrations.cpp
    src/storage/file_repository.cpp
    src/storage/state_repository.cpp
    ${AXIOM_GRPC_SOURCES}
    ${AXIOM_PROTO_SRC}
    ${AXIOM_PLATFORM_SOURCES}
)

target_include_directories(axiomdlp-agent PRIVATE
    src
    ${AXIOM_BOOST_INCLUDE_DIRS}
)
if(AXIOM_HAVE_GRPC)
    target_include_directories(axiomdlp-agent PRIVATE ${AXIOM_PROTO_GEN_DIR})
endif()

target_compile_definitions(axiomdlp-agent PRIVATE
    SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_TRACE
    AXIOM_AGENT_VERSION="${PROJECT_VERSION}"
)

target_link_libraries(axiomdlp-agent PRIVATE
    spdlog::spdlog
    nlohmann_json::nlohmann_json
    ${AXIOM_EFSW_TARGET}
    ${AXIOM_SQLITE_TARGET}
    OpenSSL::SSL
    OpenSSL::Crypto
    Threads::Threads
)

if(AXIOM_USE_VCPKG_DEPS)
    target_link_libraries(axiomdlp-agent PRIVATE Boost::headers)
endif()

if(AXIOM_HAVE_GRPC)
    target_link_libraries(axiomdlp-agent PRIVATE ${AXIOM_GRPC_LIBS})
    target_compile_definitions(axiomdlp-agent PRIVATE AXIOM_HAVE_GRPC=1)
    add_dependencies(axiomdlp-agent axiom_grpc_gen)
endif()

# ----------------------------------------------------------------
# Platform-specific settings
# ----------------------------------------------------------------
if(WIN32)
    # Windows is MSVC + vcpkg only.
    # ole32   -> CoTaskMemFree  (used by src/platform/windows/paths.cpp)
    # user32  -> Shell helpers
    # ws2_32  -> Winsock  (Boost.Asio/Beast)
    # crypt32 -> Windows crypto helpers
    target_link_libraries(axiomdlp-agent PRIVATE
        advapi32 shell32 ws2_32 crypt32 ole32 user32)
    target_compile_definitions(axiomdlp-agent PRIVATE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
        _WIN32_WINNT=0x0601
        _CRT_SECURE_NO_WARNINGS
    )
endif()

if(UNIX AND NOT APPLE)
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

if(MSVC)
    target_compile_options(axiomdlp-agent PRIVATE
        /W4
        /permissive-
        /Zc:__cplusplus
        /bigobj
    )
else()
    target_compile_options(axiomdlp-agent PRIVATE -Wall -Wextra -Wpedantic)
endif()