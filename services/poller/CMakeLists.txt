find_package(CURL REQUIRED)
find_package(libpqxx CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

add_executable(poller
    main.cpp
    space_track_client.cpp
    db_writer.cpp
    gp_record.cpp
    satcat_record.cpp
    poller_base.cpp
    satcat_poller.cpp
    gp_poller.cpp
)

# The rest of the project stays on the C++20 baseline; this target opts into
# C++23 specifically for std::expected in space_track_client.hpp, where it's
# a clear fit for a fallible HTTP call.
target_compile_features(poller PRIVATE cxx_std_23)

target_include_directories(poller PRIVATE
    ${CMAKE_SOURCE_DIR}/shared
)

target_link_libraries(poller PRIVATE
    rule_engine
    CURL::libcurl
    libpqxx::pqxx
    nlohmann_json::nlohmann_json
)
