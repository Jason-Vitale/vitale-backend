find_package(Crow CONFIG REQUIRED)
find_package(libpqxx CONFIG REQUIRED)

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
    libpqxx::pqxx
)
