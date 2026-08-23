# Crow is header-only and isn't packaged by apt (Homebrew happens to have a
# formula, which is how this worked in local dev, but that's not available
# on the deployed EC2 box or any other Linux host). Fetch it directly so
# building this target has no manual install step on any machine.
include(FetchContent)
FetchContent_Declare(
    Crow
    GIT_REPOSITORY https://github.com/CrowCpp/Crow.git
    GIT_TAG v1.3.3
)
FetchContent_MakeAvailable(Crow)

# See services/poller/CMakeLists.txt: Debian/Ubuntu's libpqxx-dev has no
# CMake config package, only pkg-config, unlike Homebrew's libpqxx.
find_package(PkgConfig REQUIRED)
pkg_check_modules(PQXX REQUIRED IMPORTED_TARGET libpqxx)

add_executable(api
    main.cpp
    routes.cpp
)

# Read-only service: does not need rule_engine.
target_compile_features(api PRIVATE cxx_std_20)

target_include_directories(api PRIVATE
    ${CMAKE_SOURCE_DIR}/shared
)

target_link_libraries(api PRIVATE
    Crow::Crow
    PkgConfig::PQXX
)
