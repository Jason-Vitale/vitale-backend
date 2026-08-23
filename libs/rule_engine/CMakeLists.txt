find_package(nlohmann_json CONFIG REQUIRED)

add_library(rule_engine STATIC
    src/rule_registry.cpp
    src/maneuver_detected_rule.cpp
    src/decay_detected_rule.cpp
)

target_include_directories(rule_engine PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(rule_engine PUBLIC cxx_std_20)

target_link_libraries(rule_engine PRIVATE
    nlohmann_json::nlohmann_json
)
