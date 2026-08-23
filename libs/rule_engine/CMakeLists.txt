find_package(nlohmann_json CONFIG REQUIRED)

add_library(rule_engine STATIC
    src/rule_registry.cpp
    src/object_rule_registry.cpp
    src/maneuver_detected_rule.cpp
    src/raan_shift_rule.cpp
    src/eccentricity_change_rule.cpp
    src/drag_change_rule.cpp
    src/decay_detected_rule.cpp
    src/object_renamed_rule.cpp
    src/object_type_changed_rule.cpp
    src/rcs_size_changed_rule.cpp
)

target_include_directories(rule_engine PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(rule_engine PUBLIC cxx_std_20)

target_link_libraries(rule_engine PRIVATE
    nlohmann_json::nlohmann_json
)

if(BUILD_TESTING)
    add_executable(rule_engine_tests
        tests/rule_tests.cpp
    )

    target_link_libraries(rule_engine_tests PRIVATE
        rule_engine
    )

    add_test(NAME rule_engine_tests COMMAND rule_engine_tests)
endif()
