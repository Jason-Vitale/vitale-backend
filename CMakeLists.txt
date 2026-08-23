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

# services/poller and services/api locate libpqxx via pkg-config (Debian/
# Ubuntu's libpqxx-dev ships no CMake config package, only a .pc file).
# Homebrew's libpq is keg-only, so its .pc file isn't on the default
# pkg-config search path -- add it here so `pkg_check_modules(... libpqxx)`
# can resolve libpqxx's `Requires: libpq` without every contributor having
# to export PKG_CONFIG_PATH by hand. No-op on Linux.
if(APPLE)
    find_program(HOMEBREW_EXECUTABLE brew)
    if(HOMEBREW_EXECUTABLE)
        execute_process(
            COMMAND ${HOMEBREW_EXECUTABLE} --prefix libpq
            OUTPUT_VARIABLE HOMEBREW_LIBPQ_PREFIX
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
        )
        if(HOMEBREW_LIBPQ_PREFIX)
            set(ENV{PKG_CONFIG_PATH} "${HOMEBREW_LIBPQ_PREFIX}/lib/pkgconfig:$ENV{PKG_CONFIG_PATH}")
        endif()
    endif()
endif()

add_subdirectory(libs/rule_engine)
add_subdirectory(services/poller)
add_subdirectory(services/api)
