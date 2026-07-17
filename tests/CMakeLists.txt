# Host-only unit tests (doctest, vendored header in external/doctest).
add_executable(picohome_tests
    main.cpp
    test_gesture_detector.cpp
    test_topic.cpp
    test_config_parser.cpp
    test_relay_button_binding.cpp
)

target_include_directories(picohome_tests PRIVATE
    .
    ${CMAKE_SOURCE_DIR}/external/doctest
)
target_link_libraries(picohome_tests PRIVATE picohome_core)

add_test(NAME picohome_tests COMMAND picohome_tests)
