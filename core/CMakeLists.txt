# Portable core: device model, gesture grammar, control model, topics, config parsing.
# No pico-sdk dependencies — builds for both the firmware and the host tests.
add_library(picohome_core STATIC
    src/types.cpp
    src/device.cpp
    src/device_registry.cpp
    src/gesture_detector.cpp
    src/topic.cpp
    src/relay.cpp
    src/binary_sensor.cpp
    src/button.cpp
    src/event_router.cpp
    src/binding.cpp
    src/binding_set.cpp
    src/config_parser.cpp
    src/mcp23017.cpp
    src/remote_sources.cpp
    src/command_router.cpp
    src/publisher.cpp
)

target_include_directories(picohome_core PUBLIC include)
target_link_libraries(picohome_core PUBLIC ArduinoJson)
target_compile_definitions(picohome_core PUBLIC ARDUINOJSON_ENABLE_COMMENTS=1)
target_compile_features(picohome_core PUBLIC cxx_std_17)