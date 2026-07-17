#include <doctest/doctest.h>

#include "picohome/config/config.h"

using namespace picohome;

namespace {

constexpr char kSampleConfig[] = R"json({
  // comments are allowed in PicoHome configs
  "schema_version": 1,
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
    "binary_actuators": [
      {"type": "light", "id": 29, "room": "kitchen",
       "pin": {"expander": -1, "pin": 2, "active_high": true}},
      {"type": "outlet", "id": 600, "room": "ainius_room",
       "pin": {"expander": 0, "pin": 5, "active_high": false}}
    ],
    "buttons": [
      {"id": 29, "room": "kitchen",
       "pin": {"expander": -1, "pin": 3, "active_high": false, "pull_up": true},
       "controls": {"type": "light", "id": 29}}
    ]
  }
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
    CHECK(app.network.brokerPort == 1883);

    REQUIRE(app.binaryActuators.size() == 2);
    CHECK(app.binaryActuators[0].type == DeviceType::Light);
    CHECK(app.binaryActuators[0].id == 29);
    CHECK(app.binaryActuators[0].room == "kitchen");
    CHECK(app.binaryActuators[0].pin.expander == -1);
    CHECK(app.binaryActuators[0].pin.activeHigh);
    CHECK(app.binaryActuators[1].type == DeviceType::Outlet);
    CHECK(app.binaryActuators[1].pin.expander == 0);
    CHECK_FALSE(app.binaryActuators[1].pin.activeHigh);

    REQUIRE(app.buttons.size() == 1);
    CHECK(app.buttons[0].id == 29);
    CHECK(app.buttons[0].controlsType == DeviceType::Light);
    CHECK(app.buttons[0].controlsId == 29);
    CHECK(app.buttons[0].pin.pullUp);
}

TEST_CASE("rejects wrong schema version") {
    config::App app;
    std::string error;
    CHECK_FALSE(config::parse(R"({"schema_version": 99, "building": "x"})", app, error));
    CHECK(error == "unsupported schema_version");
}

TEST_CASE("rejects missing building") {
    config::App app;
    std::string error;
    CHECK_FALSE(config::parse(R"({"schema_version": 1})", app, error));
    CHECK(error == "missing building");
}

TEST_CASE("rejects unknown actuator type") {
    config::App app;
    std::string error;
    CHECK_FALSE(config::parse(R"({
      "schema_version": 1, "building": "x",
      "devices": {"binary_actuators": [
        {"type": "warp_drive", "id": 1, "room": "engine", "pin": {"pin": 2}}
      ]}
    })",
                              app, error));
}

TEST_CASE("rejects actuator without pin") {
    config::App app;
    std::string error;
    CHECK_FALSE(config::parse(R"({
      "schema_version": 1, "building": "x",
      "devices": {"binary_actuators": [{"type": "light", "id": 1, "room": "a"}]}
    })",
                              app, error));
}

TEST_CASE("rejects invalid json") {
    config::App app;
    std::string error;
    CHECK_FALSE(config::parse("{not json", app, error));
    CHECK_FALSE(error.empty());
}