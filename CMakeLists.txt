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
# OpenSSL (must run BEFORE find_package(gRPC))
#
# gRPC's CMake config file calls find_dependency(OpenSSL). If that
# runs first, it resolves to the host's system OpenSSL. On a 32-bit
# (i686) build the host system OpenSSL is 64-bit and the link fails
# with "file in wrong format". Setting OPENSSL_ROOT_DIR + explicit
# library paths here and calling find_package(OpenSSL) ourselves
# forces gRPC's later find_dependency(OpenSSL) to reuse the correct
# architecture's targets.
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

    # Cache hints consumed by FindOpenSSL.cmake (both our call
    # below and gRPC's internal find_dependency(OpenSSL) call).
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
# gRPC + Protobuf (Mandatory)
#
# Runs after OpenSSL is set up so gRPC's find_dependency(OpenSSL)
# reuses our targets instead of resolving to the system OpenSSL.
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