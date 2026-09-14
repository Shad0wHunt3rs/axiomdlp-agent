cmake_minimum_required(VERSION 3.24)
project(axiomdlp-agent VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_executable(axiomdlp-agent
    src/main.cpp
)

target_include_directories(axiomdlp-agent PRIVATE src)

if(MSVC)
    target_compile_options(axiomdlp-agent PRIVATE /W4 /permissive-)
else()
    target_compile_options(axiomdlp-agent PRIVATE -Wall -Wextra -Wpedantic)
endif()