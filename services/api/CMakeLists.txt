find_package(Crow CONFIG REQUIRED)

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
