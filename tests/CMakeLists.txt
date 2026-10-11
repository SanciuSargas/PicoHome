# Host-only unit tests (doctest, vendored header in external/doctest).
add_executable(picohome_tests
    main.cpp
    test_gesture_detector.cpp
    test_topic.cpp
    test_config_parser.cpp
    test_relay_button_binding.cpp
    test_binding_model.cpp
    test_binding_validation.cpp
    test_binary_sensor.cpp
    test_mqtt_plumbing.cpp
    test_mcp23017.cpp
    test_cover.cpp
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
    PRIVATE PICOHOME_TESTBENCH_CONFIG="${CMAKE_SOURCE_DIR}/config/testbench.json"
            PICOHOME_SECRETS_EXAMPLE="${CMAKE_SOURCE_DIR}/config/secrets.example.json")

add_test(NAME picohome_tests COMMAND picohome_tests)

# Integration test of the controller's MQTT session: controller/src/net and the
# vendored Paho client, unmodified, against a real mosquitto, with only the
# RP2040 and the W5100 emulated (mqtt_session/). Linux-only, because the
# emulation reads the broker's FIN from POLLRDHUP. Core 1 never returns, so each
# scenario is a process of its own; without mosquitto they report as skipped.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(PAHO_DIR ${CMAKE_SOURCE_DIR}/external/ioLibrary_Driver/Internet/MQTT)
    find_package(Threads REQUIRED)

    # The firmware's Paho sources minus MQTTFormat.c: it is only debug
    # formatting, and it references server-side code that the firmware's
    # --gc-sections drops but a plain host link would not.
    add_executable(mqtt_session_test
        mqtt_session/test_mqtt_session.cpp
        mqtt_session/w5100_emulation.cpp
        ${CMAKE_SOURCE_DIR}/controller/src/net/net_core.cpp
        ${CMAKE_SOURCE_DIR}/controller/src/net/mqtt_port.cpp
        ${PAHO_DIR}/MQTTClient.c
        ${PAHO_DIR}/MQTTPacket/src/MQTTConnectClient.c
        ${PAHO_DIR}/MQTTPacket/src/MQTTDeserializePublish.c
        ${PAHO_DIR}/MQTTPacket/src/MQTTPacket.c
        ${PAHO_DIR}/MQTTPacket/src/MQTTSerializePublish.c
        ${PAHO_DIR}/MQTTPacket/src/MQTTSubscribeClient.c
        ${PAHO_DIR}/MQTTPacket/src/MQTTUnsubscribeClient.c
    )
    # mqtt_session/stub first: it stands in for the pico-sdk and ioLibrary headers.
    target_include_directories(mqtt_session_test PRIVATE
        mqtt_session/stub
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/controller/src
        ${PAHO_DIR}
        ${PAHO_DIR}/MQTTPacket/src
    )
    # Must match controller/CMakeLists.txt: it sets the size of MQTTClient.
    target_compile_definitions(mqtt_session_test PRIVATE MAX_MESSAGE_HANDLERS=16)
    target_link_libraries(mqtt_session_test PRIVATE picohome_core Threads::Threads)

    find_program(MOSQUITTO_EXECUTABLE mosquitto PATHS /usr/sbin /usr/local/sbin)
    foreach(scenario broker_restart qos1_burst broker_frozen)
        add_test(NAME mqtt_session.${scenario}
                 COMMAND mqtt_session_test ${scenario} ${MOSQUITTO_EXECUTABLE})
        set_tests_properties(mqtt_session.${scenario} PROPERTIES
            LABELS mqtt
            SKIP_RETURN_CODE 77
            TIMEOUT 60)
    endforeach()
endif()
