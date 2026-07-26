# Host-only unit tests (doctest, vendored header in external/doctest).
add_executable(picohome_tests
    main.cpp
    test_gesture_detector.cpp
    test_topic.cpp
    test_config_parser.cpp
    test_relay_button_binding.cpp
    test_binding_model.cpp
    test_binding_validation.cpp
    test_mqtt_plumbing.cpp
    test_mcp23017.cpp
    test_shipped_config.cpp
)

target_include_directories(picohome_tests PRIVATE
    .
    ${CMAKE_SOURCE_DIR}/external/doctest
)
target_link_libraries(picohome_tests PRIVATE picohome_core)

# The config actually deployed to SD cards is parsed by the test suite, so a
# typo in it fails the build rather than a controller on a wall.
target_compile_definitions(picohome_tests
    PRIVATE PICOHOME_TESTBENCH_CONFIG="${CMAKE_SOURCE_DIR}/config/testbench.json")

add_test(NAME picohome_tests COMMAND picohome_tests)
