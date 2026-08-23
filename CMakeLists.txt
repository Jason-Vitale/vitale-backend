cmake_minimum_required(VERSION 3.20)
project(vitale_backend LANGUAGES CXX)

# C++20 is the project-wide baseline. Individual targets may opt into C++23
# where a specific feature (e.g. std::expected) clearly earns its keep --
# see services/poller/CMakeLists.txt.
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

add_subdirectory(libs/rule_engine)
add_subdirectory(services/poller)
add_subdirectory(services/api)
