cmake_minimum_required(VERSION 3.16)
project(axiomdlp_agent CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

FetchContent_Declare(
    quill
    GIT_REPOSITORY https://github.com/odygrd/quill.git
    GIT_TAG        v9.0.2
    GIT_SHALLOW    TRUE
    GIT_PROGRESS   TRUE
)

FetchContent_MakeAvailable(quill)

set(SOURCES
    src/main.cpp
    src/logger/logger.cpp
)

add_executable(axiomdlp_agent ${SOURCES})
target_link_libraries(axiomdlp_agent PRIVATE quill::quill)

if(WIN32)
    target_link_libraries(axiomdlp_agent PRIVATE ws2_32)
else()
    target_link_libraries(axiomdlp_agent PRIVATE pthread)
endif()