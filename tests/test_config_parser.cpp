#include <doctest/doctest.h>

#include "picohome/config/config.h"

using namespace picohome;

namespace {

constexpr char kSampleConfig[] = R"json({
  // comments are allowed in PicoHome configs
  "schema_version": 2,
  "building": "utility",
  "network": {
    "mac": [18, 237, 103, 254, 84, 237],
    "ip": "192.168.1.201",
    "netmask": "255.255.255.0",
    "gateway": "192.168.1.1",
    "broker": "192.168.1.200",
    "broker_port": 1883
  },
  "devices": {
    "actuators": [
      {"type": "light", "id": 29, "room": "kitchen",
       "backend": {"kind": "gpio", "pin": 2, "active_high": true}},
      {"type": "outlet", "id": 600, "room": "ainius_room",
       "backend": {"kind": "gpio", "pin": 5, "active_high": false}}
    ],
    "sensors": [
      {"type": "pir", "id": 720, "room": "corridor", "pin": {"pin": 8}, "debounce_ms": 30}
    ],
    "buttons": [
      {"id": 29, "room": "kitchen", "kind": "momentary",
       "pin": {"pin": 3, "active_high": false, "pull_up": true}},
      {"id": 30, "room": "corridor", "kind": "toggle", "pin": {"pin": 6}}
    ]
  },
  "bindings": [
    {"name": "kitchen light", "rule": "toggle",
     "sources": [{"button": 29, "gesture": "press_1"}],
     "targets": [{"type": "light", "id": 29}]},
    {"name": "corridor motion", "rule": "motion",
     "sources": [{"sensor": {"type": "pir", "id": 720}}],
     "targets": [{"type": "light", "id": 29}],
     "options": {"off_delay_ms": 120000}}
  ]
})json";

}  // namespace

TEST_CASE("parses a full sample config") {
    config::App app;
    std::string error;
    REQUIRE_MESSAGE(config::parse(kSampleConfig, app, error), error);

    CHECK(app.building == "utility");
    CHECK(app.network.mac[0] == 18);
    CHECK(app.network.mac[5] == 237);
    CHECK(app.network.broker == "192.168.1.200");

    REQUIRE(app.actuators.size() == 2);
    CHECK(app.actuators[0].type == DeviceType::Light);
    CHECK(app.actuators[0].id == 29);
    CHECK(app.actuators[0].room == "kitchen");
    CHECK(app.actuators[0].backend == config::BackendKind::Gpio);
    CHECK(app.actuators[0].pin.activeHigh);
    CHECK(app.actuators[1].type == DeviceType::Outlet);
    CHECK_FALSE(app.actuators[1].pin.activeHigh);

    REQUIRE(app.sensors.size() == 1);
    CHECK(app.sensors[0].type == DeviceType::Pir);
    CHECK(app.sensors[0].debounceMs == 30);

    REQUIRE(app.buttons.size() == 2);
    CHECK(app.buttons[0].id == 29);
    CHECK(app.buttons[0].pin.pullUp);
    CHECK(app.buttons[0].kind == SwitchKind::Momentary);
    CHECK(app.buttons[1].kind == SwitchKind::Toggle);

    REQUIRE(app.bindings.size() == 2);
    CHECK(app.bindings[0].rule == config::RuleKind::Toggle);
    REQUIRE(app.bindings[0].sources.size() == 1);
    CHECK(app.bindings[0].sources[0].kind == config::SourceType::Button);
    CHECK(app.bindings[0].sources[0].buttonId == 29);
    CHECK(app.bindings[0].sources[0].gesture == Gesture::Press1);
    CHECK(app.bindings[0].targets[0].id == 29);

    CHECK(app.bindings[1].rule == config::RuleKind::Motion);
    CHECK(app.bindings[1].sources[0].kind == config::SourceType::Sensor);
    CHECK(app.bindings[1].sources[0].sensor.type == DeviceType::Pir);
    CHECK(app.bindings[1].offDelayMs == 120000);
}

TEST_CASE("button sources default to press_1") {
    config::App app;
    std::string error;
    REQUIRE_MESSAGE(config::parse(R"({
      "schema_version": 2, "building": "x",
      "devices": {
        "actuators": [{"type": "light", "id": 1, "room": "a",
                       "backend": {"kind": "gpio", "pin": 2}}],
        "buttons": [{"id": 1, "room": "a", "kind": "momentary", "pin": {"pin": 3}}]},
      "bindings": [{"rule": "toggle", "sources": [{"button": 1}],
                    "targets": [{"type": "light", "id": 1}]}]})",
                                  app, error),
                    error);
    CHECK(app.bindings[0].sources[0].gesture == Gesture::Press1);
}

TEST_CASE("rejects wrong schema version") {
    config::App app;
    std::string error;
    CHECK_FALSE(config::parse(R"({"schema_version": 1, "building": "x"})", app, error));
    CHECK(error.find("schema_version") != std::string::npos);
}

TEST_CASE("rejects missing building") {
    config::App app;
    std::string error;
    CHECK_FALSE(config::parse(R"({"schema_version": 2})", app, error));
    CHECK(error == "missing building");
}

TEST_CASE("rejects unknown actuator type") {
    config::App app;
    std::string error;
    CHECK_FALSE(config::parse(R"({
      "schema_version": 2, "building": "x",
      "devices": {"actuators": [
        {"type": "warp_drive", "id": 1, "room": "engine", "backend": {"kind": "gpio", "pin": 2}}
      ]}, "bindings": []})",
                              app, error));
}

TEST_CASE("rejects actuator without backend") {
    config::App app;
    std::string error;
    CHECK_FALSE(config::parse(R"({
      "schema_version": 2, "building": "x",
      "devices": {"actuators": [{"type": "light", "id": 1, "room": "a"}]},
      "bindings": []})",
                              app, error));
    CHECK(error.find("backend") != std::string::npos);
}

TEST_CASE("rejects unknown gesture and unknown rule") {
    config::App app;
    std::string error;
    const char* base = R"({
      "schema_version": 2, "building": "x",
      "devices": {"actuators": [{"type": "light", "id": 1, "room": "a",
                                 "backend": {"kind": "gpio", "pin": 2}}],
                  "buttons": [{"id": 1, "room": "a", "kind": "momentary", "pin": {"pin": 3}}]},
      "bindings": [%s]})";

    char json[1024];
    std::snprintf(json, sizeof json, base,
                  R"({"rule": "toggle", "sources": [{"button": 1, "gesture": "press_9"}],
                      "targets": [{"type": "light", "id": 1}]})");
    CHECK_FALSE(config::parse(json, app, error));
    CHECK(error.find("gesture") != std::string::npos);

    std::snprintf(json, sizeof json, base,
                  R"({"rule": "levitate", "sources": [{"button": 1}],
                      "targets": [{"type": "light", "id": 1}]})");
    CHECK_FALSE(config::parse(json, app, error));
    CHECK(error.find("rule") != std::string::npos);
}

TEST_CASE("rejects invalid json") {
    config::App app;
    std::string error;
    CHECK_FALSE(config::parse("{not json", app, error));
    CHECK_FALSE(error.empty());
}

namespace {

// A config with one declared sensor, so sensor rules can be exercised alone.
std::string withSensors(const char* sensorsJson) {
    return std::string(R"({"schema_version": 2, "building": "x",
      "devices": {"sensors": [)") +
           sensorsJson + R"(]}, "bindings": []})";
}

}  // namespace

TEST_CASE("sensors take the debounce their signal needs, unless told otherwise") {
    config::App app;
    std::string error;
    const std::string json = withSensors(R"(
      {"type": "pir",        "id": 720, "room": "corridor", "pin": {"pin": 8}},
      {"type": "reed",       "id": 700, "room": "kitchen",  "pin": {"pin": 9}},
      {"type": "water_leak", "id": 710, "room": "boiler",   "pin": {"pin": 10}},
      {"type": "smoke",      "id": 740, "room": "office",   "pin": {"pin": 11}},
      {"type": "presence",   "id": 730, "room": "corridor", "pin": {"pin": 12},
       "debounce_ms": 2000})");
    REQUIRE_MESSAGE(config::parse(json, app, error), error);

    REQUIRE(app.sensors.size() == 5);
    CHECK(app.sensors[0].debounceMs == config::defaultDebounceMs(DeviceType::Pir));
    CHECK(app.sensors[1].debounceMs == config::defaultDebounceMs(DeviceType::Reed));
    CHECK(app.sensors[2].debounceMs == config::defaultDebounceMs(DeviceType::WaterLeak));
    CHECK(app.sensors[3].debounceMs == config::defaultDebounceMs(DeviceType::Smoke));
    CHECK(app.sensors[4].debounceMs == 2000);  // a flickering module, quietened by hand
}

TEST_CASE("a measurement cannot be declared on a panel pin") {
    config::App app;
    std::string error;
    CHECK_FALSE(config::parse(
        withSensors(R"({"type": "temperature", "id": 760, "room": "sauna", "pin": {"pin": 8}})"),
        app, error));
    CHECK(error.find("room node") != std::string::npos);
    CHECK(error.find("M9") != std::string::npos);
}

TEST_CASE("devices.sensors refuses anything that is not a sensor") {
    config::App app;
    std::string error;
    CHECK_FALSE(config::parse(
        withSensors(R"({"type": "light", "id": 29, "room": "kitchen", "pin": {"pin": 8}})"), app,
        error));
    CHECK(error.find("not a sensor") != std::string::npos);
}

TEST_CASE("a digital smoke detector is an ordinary binary sensor") {
    config::App app;
    std::string error;
    REQUIRE_MESSAGE(
        config::parse(
            withSensors(R"({"type": "smoke", "id": 740, "room": "office", "pin": {"pin": 8}})"),
            app, error),
        error);
    REQUIRE(app.sensors.size() == 1);
    CHECK(app.sensors[0].type == DeviceType::Smoke);
}

TEST_CASE("smoke and reed may be declared but never drive a local binding") {
    // Smoke- and reed-triggered logic is Home Assistant's business; the
    // controller's job is to make the state visible.
    config::App app;
    std::string error;
    CHECK_FALSE(config::parse(R"({"schema_version": 2, "building": "x",
      "devices": {
        "actuators": [{"type": "light", "id": 1, "room": "a",
                       "backend": {"kind": "gpio", "pin": 2}}],
        "sensors": [{"type": "smoke", "id": 740, "room": "a", "pin": {"pin": 8}}]},
      "bindings": [{"rule": "motion", "sources": [{"sensor": {"type": "smoke", "id": 740}}],
                    "targets": [{"type": "light", "id": 1}],
                    "options": {"off_delay_ms": 1000}}]})",
                              app, error));
    CHECK(error.find("pir or presence") != std::string::npos);
}

TEST_CASE("the same device name cannot be declared twice") {
    config::App app;
    std::string error;
    CHECK_FALSE(config::parse(
        withSensors(R"({"type": "pir", "id": 720, "room": "corridor", "pin": {"pin": 8}},
                       {"type": "pir", "id": 720, "room": "office",   "pin": {"pin": 9}})"),
        app, error));
    CHECK(error.find("pir/720") != std::string::npos);
    CHECK(error.find("twice") != std::string::npos);

    CHECK_FALSE(config::parse(R"({"schema_version": 2, "building": "x",
      "devices": {"actuators": [
        {"type": "light", "id": 29, "room": "a", "backend": {"kind": "gpio", "pin": 2}},
        {"type": "light", "id": 29, "room": "b", "backend": {"kind": "gpio", "pin": 3}}]},
      "bindings": []})",
                              app, error));
    CHECK(error.find("light/29") != std::string::npos);
}

TEST_CASE("two devices cannot claim the same pin") {
    config::App app;
    std::string error;
    CHECK_FALSE(config::parse(R"({"schema_version": 2, "building": "x",
      "devices": {
        "actuators": [{"type": "light", "id": 29, "room": "a",
                       "backend": {"kind": "gpio", "pin": 2}}],
        "buttons": [{"id": 29, "room": "a", "kind": "momentary", "pin": {"pin": 2}}]},
      "bindings": []})",
                              app, error));
    CHECK(error.find("GPIO 2") != std::string::npos);
    CHECK(error.find("light/29") != std::string::npos);
    CHECK(error.find("button/29") != std::string::npos);

    // The two namespaces are separate: expander 0 pin 2 is not GPIO 2.
    REQUIRE_MESSAGE(config::parse(R"({"schema_version": 2, "building": "x",
      "devices": {
        "expanders": [{"index": 0, "address": "0x20", "int_pin": 7}],
        "actuators": [{"type": "light", "id": 29, "room": "a",
                       "backend": {"kind": "gpio", "pin": 2}}],
        "buttons": [{"id": 29, "room": "a", "kind": "momentary",
                     "pin": {"expander": 0, "pin": 2}}]},
      "bindings": []})",
                                  app, error),
                    error);
}

TEST_CASE("a cover's two relays are checked against everything else") {
    config::App app;
    std::string error;
    CHECK_FALSE(config::parse(R"({"schema_version": 2, "building": "x",
      "devices": {
        "actuators": [
          {"type": "blinds", "id": 500, "room": "a",
           "backend": {"kind": "cover", "up": {"pin": 6}, "down": {"pin": 8}},
           "options": {"travel_ms": 10000, "tilt_ms": 1500}},
          {"type": "light", "id": 29, "room": "a", "backend": {"kind": "gpio", "pin": 8}}]},
      "bindings": []})",
                              app, error));
    CHECK(error.find("blinds/500 (down)") != std::string::npos);
}

TEST_CASE("an expander's interrupt line is a claimed pin too") {
    config::App app;
    std::string error;
    CHECK_FALSE(config::parse(R"({"schema_version": 2, "building": "x",
      "devices": {
        "expanders": [{"index": 0, "address": "0x20", "int_pin": 7}],
        "actuators": [{"type": "light", "id": 29, "room": "a",
                       "backend": {"kind": "gpio", "pin": 7}}]},
      "bindings": []})",
                              app, error));
    CHECK(error.find("expander 0 INT") != std::string::npos);
}

TEST_CASE("two switches may share one button id — that is what a P-group is") {
    config::App app;
    std::string error;
    REQUIRE_MESSAGE(config::parse(R"({"schema_version": 2, "building": "x",
      "devices": {
        "actuators": [{"type": "light", "id": 30, "room": "corridor",
                       "backend": {"kind": "gpio", "pin": 2}}],
        "buttons": [{"id": 30, "room": "corridor", "kind": "toggle", "pin": {"pin": 3}},
                    {"id": 30, "room": "hall",     "kind": "toggle", "pin": {"pin": 4}}]},
      "bindings": [{"rule": "toggle", "sources": [{"button": 30}],
                    "targets": [{"type": "light", "id": 30}]}]})",
                                  app, error),
                    error);
    CHECK(app.buttons.size() == 2);
}
