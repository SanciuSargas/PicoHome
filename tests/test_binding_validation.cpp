#include <doctest/doctest.h>

#include <memory>
#include <string>

#include "mocks/fakes.h"
#include "picohome/config/config.h"
#include "picohome/control/binding_set.h"
#include "picohome/control/event_router.h"
#include "picohome/core/device_registry.h"
#include "picohome/devices/binary_sensor.h"
#include "picohome/devices/button.h"
#include "picohome/devices/relay.h"

using namespace picohome;

namespace {

// Builds devices + bindings from JSON and reports where it failed. Boot-time
// validation is the point of these tests: a config that claims something the
// system cannot do must be refused before the control loop starts.
struct BuildResult {
    bool parsed = false;
    bool built = false;
    std::string error;
};

BuildResult build(const std::string& json) {
    BuildResult result;
    static std::vector<std::unique_ptr<test::FakeDigitalOutput>> outputs;
    static std::vector<std::unique_ptr<test::FakeDigitalInput>> inputs;

    config::App config;
    if (!config::parse(json, config, result.error)) return result;
    result.parsed = true;

    DeviceRegistry registry;
    EventRouter router;
    BindingSet bindings;
    RemoteSourceTable remoteSources;

    for (const auto& def : config.actuators) {
        auto out = std::make_unique<test::FakeDigitalOutput>();
        registry.add(std::make_unique<Relay>(def.type, def.id, def.room, *out));
        outputs.push_back(std::move(out));
    }
    for (const auto& def : config.sensors) {
        auto in = std::make_unique<test::FakeDigitalInput>();
        auto sensor = std::make_unique<BinarySensor>(def.type, def.id, def.room, *in,
                                                     def.debounceMs);
        sensor->attachRouter(&router);
        registry.add(std::move(sensor));
        inputs.push_back(std::move(in));
    }
    for (const auto& def : config.buttons) {
        auto in = std::make_unique<test::FakeDigitalInput>();
        auto button = std::make_unique<Button>(def.id, def.room, *in, def.kind);
        button->attachRouter(&router);
        registry.add(std::move(button));
        inputs.push_back(std::move(in));
    }

    result.built = bindings.build(config, registry, router, remoteSources, result.error);
    return result;
}

std::string configWith(const char* bindings) {
    return std::string(R"json({
      "schema_version": 2,
      "building": "test",
      "devices": {
        "actuators": [
          {"type": "light", "id": 29, "room": "kitchen", "backend": {"kind": "gpio", "pin": 2}},
          {"type": "outlet", "id": 600, "room": "kitchen", "backend": {"kind": "gpio", "pin": 6}},
          {"type": "heating_mat", "id": 401, "room": "kitchen",
           "backend": {"kind": "gpio", "pin": 7}}
        ],
        "sensors": [
          {"type": "pir", "id": 720, "room": "corridor", "pin": {"pin": 8}},
          {"type": "reed", "id": 700, "room": "kitchen", "pin": {"pin": 9}}
        ],
        "buttons": [{"id": 29, "room": "kitchen", "kind": "momentary", "pin": {"pin": 3}},
                    {"id": 30, "room": "corridor", "kind": "toggle", "pin": {"pin": 5}}]
      },
      "bindings": )json") +
           bindings + "}";
}

}  // namespace

TEST_CASE("valid config builds") {
    const auto result = build(configWith(R"([
      {"name": "ok", "rule": "toggle",
       "sources": [{"button": 29}], "targets": [{"type": "light", "id": 29}]}])"));
    CHECK(result.parsed);
    CHECK_MESSAGE(result.built, result.error);
}

TEST_CASE("toggle may drive lights, outlets and dampers") {
    const auto result = build(configWith(R"([
      {"name": "outlet", "rule": "toggle",
       "sources": [{"button": 29}], "targets": [{"type": "outlet", "id": 600}]}])"));
    CHECK_MESSAGE(result.built, result.error);
}

TEST_CASE("target types outside the local scope are rejected") {
    // A heating mat is a perfectly good MQTT device; it just has no business
    // being driven by a local binding — that logic belongs to Home Assistant.
    const auto result = build(configWith(R"([
      {"name": "mat", "rule": "toggle",
       "sources": [{"button": 29}], "targets": [{"type": "heating_mat", "id": 401}]}])"));
    CHECK_FALSE(result.parsed);
    CHECK(result.error.find("light, outlet or damper") != std::string::npos);
}

TEST_CASE("motion may only drive lights") {
    const auto result = build(configWith(R"([
      {"name": "motion outlet", "rule": "motion",
       "sources": [{"sensor": {"type": "pir", "id": 720}}],
       "targets": [{"type": "outlet", "id": 600}],
       "options": {"off_delay_ms": 1000}}])"));
    CHECK_FALSE(result.parsed);
    CHECK(result.error.find("motion target must be a light") != std::string::npos);
}

TEST_CASE("only pir and presence may be binding sources") {
    const auto result = build(configWith(R"([
      {"name": "door", "rule": "motion",
       "sources": [{"sensor": {"type": "reed", "id": 700}}],
       "targets": [{"type": "light", "id": 29}],
       "options": {"off_delay_ms": 1000}}])"));
    CHECK_FALSE(result.parsed);
    CHECK(result.error.find("pir or presence") != std::string::npos);
}

TEST_CASE("target that does not exist is rejected at boot") {
    const auto result = build(configWith(R"([
      {"name": "ghost", "rule": "toggle",
       "sources": [{"button": 29}], "targets": [{"type": "light", "id": 999}]}])"));
    REQUIRE(result.parsed);
    CHECK_FALSE(result.built);
    CHECK(result.error.find("does not exist") != std::string::npos);
}

TEST_CASE("source button that does not exist is rejected at boot") {
    const auto result = build(configWith(R"([
      {"name": "ghost source", "rule": "toggle",
       "sources": [{"button": 77}], "targets": [{"type": "light", "id": 29}]}])"));
    REQUIRE(result.parsed);
    CHECK_FALSE(result.built);
    CHECK(result.error.find("button 77") != std::string::npos);
}

TEST_CASE("source sensor that does not exist is rejected at boot") {
    const auto result = build(configWith(R"([
      {"name": "ghost sensor", "rule": "motion",
       "sources": [{"sensor": {"type": "presence", "id": 999}}],
       "targets": [{"type": "light", "id": 29}],
       "options": {"off_delay_ms": 1000}}])"));
    REQUIRE(result.parsed);
    CHECK_FALSE(result.built);
    CHECK(result.error.find("does not exist") != std::string::npos);
}

TEST_CASE("bindings without sources or targets are rejected") {
    CHECK_FALSE(build(configWith(R"([
      {"name": "no sources", "rule": "toggle", "sources": [],
       "targets": [{"type": "light", "id": 29}]}])"))
                    .parsed);
    CHECK_FALSE(build(configWith(R"([
      {"name": "no targets", "rule": "toggle",
       "sources": [{"button": 29}], "targets": []}])"))
                    .parsed);
}

TEST_CASE("motion binding without off_delay_ms is rejected") {
    const auto result = build(configWith(R"([
      {"name": "no delay", "rule": "motion",
       "sources": [{"sensor": {"type": "pir", "id": 720}}],
       "targets": [{"type": "light", "id": 29}]}])"));
    CHECK_FALSE(result.parsed);
    CHECK(result.error.find("off_delay_ms") != std::string::npos);
}

TEST_CASE("rules that moved to high level are rejected by name") {
    for (const char* rule : {"follow", "set", "scene"}) {
        std::string json = R"([{"name": "x", "rule": ")";
        json += rule;
        json += R"(", "sources": [{"button": 29}], "targets": [{"type": "light", "id": 29}]}])";
        const auto result = build(configWith(json.c_str()));
        CHECK_FALSE(result.parsed);
        CHECK(result.error.find("Home Assistant") != std::string::npos);
    }
}

TEST_CASE("the old lux_below option points at night_mode instead") {
    const auto result = build(configWith(R"([
      {"name": "motion", "rule": "motion",
       "sources": [{"sensor": {"type": "pir", "id": 720}}],
       "targets": [{"type": "light", "id": 29}],
       "options": {"off_delay_ms": 1000, "lux_below": 40}}])"));
    CHECK_FALSE(result.parsed);
    CHECK(result.error.find("night_mode") != std::string::npos);
}

TEST_CASE("switch kind is required, because guessing it fails silently") {
    const auto result = build(R"({
      "schema_version": 2, "building": "test",
      "devices": {
        "actuators": [{"type": "light", "id": 29, "room": "k",
                       "backend": {"kind": "gpio", "pin": 2}}],
        "buttons": [{"id": 29, "room": "k", "pin": {"pin": 3}}]},
      "bindings": []})");
    CHECK_FALSE(result.parsed);
    CHECK(result.error.find("momentary") != std::string::npos);
    CHECK(result.error.find("toggle") != std::string::npos);
}

TEST_CASE("a toggle switch bound to a hold gesture is rejected at boot") {
    // Button 30 is a latching rocker: it can never be held, so this binding
    // would sit there doing nothing.
    const auto result = build(configWith(R"([
      {"name": "hold a rocker", "rule": "toggle",
       "sources": [{"button": 30, "gesture": "hold_start"}],
       "targets": [{"type": "light", "id": 29}]}])"));
    REQUIRE(result.parsed);
    CHECK_FALSE(result.built);
    CHECK(result.error.find("cannot produce hold_start") != std::string::npos);
}

TEST_CASE("a momentary switch may be bound to a hold gesture") {
    const auto result = build(configWith(R"([
      {"name": "hold a pushbutton", "rule": "toggle",
       "sources": [{"button": 29, "gesture": "hold_start"}],
       "targets": [{"type": "light", "id": 29}]}])"));
    CHECK_MESSAGE(result.built, result.error);
}

TEST_CASE("both switch kinds drive a plain light interchangeably") {
    const auto result = build(configWith(R"([
      {"name": "mixed P-group", "rule": "toggle",
       "sources": [{"button": 29}, {"button": 30}],
       "targets": [{"type": "light", "id": 29}]}])"));
    CHECK_MESSAGE(result.built, result.error);
}

TEST_CASE("unimplemented rules name their milestone") {
    const auto cover = build(configWith(R"([
      {"name": "blind", "rule": "cover",
       "sources": [{"button": 29}], "targets": [{"type": "blinds", "id": 500}]}])"));
    CHECK_FALSE(cover.parsed);
    CHECK(cover.error.find("M6") != std::string::npos);
}

TEST_CASE("a remote topic is a valid binding source (cross-panel switch)") {
    const auto result = build(configWith(R"([
      {"name": "cross panel", "rule": "toggle",
       "sources": [{"button": 29},
                   {"topic": "closet/corridor/button/31/event", "payload": "press_1"}],
       "targets": [{"type": "light", "id": 29}]}])"));
    CHECK_MESSAGE(result.built, result.error);
}

TEST_CASE("a remote button event needs an exact payload to match") {
    const auto result = build(configWith(R"([
      {"name": "no payload", "rule": "toggle",
       "sources": [{"topic": "closet/corridor/button/31/event"}],
       "targets": [{"type": "light", "id": 29}]}])"));
    CHECK_FALSE(result.parsed);
    CHECK(result.error.find("payload") != std::string::npos);
}

TEST_CASE("a remote motion sensor defaults to this firmware's own 1/0 state payloads") {
    const auto result = build(configWith(R"([
      {"name": "remote motion", "rule": "motion",
       "sources": [{"topic": "closet/corridor/pir/720/state"}],
       "targets": [{"type": "light", "id": 29}],
       "options": {"off_delay_ms": 1000}}])"));
    CHECK_MESSAGE(result.built, result.error);
}

TEST_CASE("a remote motion sensor may spell out a foreign device's payloads") {
    const auto result = build(configWith(R"([
      {"name": "foreign motion", "rule": "motion",
       "sources": [{"topic": "zigbee2mqtt/hall_sensor/occupancy",
                    "payload_active": "true", "payload_inactive": "false"}],
       "targets": [{"type": "light", "id": 29}],
       "options": {"off_delay_ms": 1000}}])"));
    CHECK_MESSAGE(result.built, result.error);
}

TEST_CASE("a remote motion sensor whose two edges are identical is rejected") {
    const auto result = build(configWith(R"([
      {"name": "same payloads", "rule": "motion",
       "sources": [{"topic": "closet/corridor/pir/720/state",
                    "payload_active": "1", "payload_inactive": "1"}],
       "targets": [{"type": "light", "id": 29}],
       "options": {"off_delay_ms": 1000}}])"));
    CHECK_FALSE(result.parsed);
    CHECK(result.error.find("must differ") != std::string::npos);
}

TEST_CASE("unsupported backends name their milestone") {
    config::App config;
    std::string error;
    CHECK_FALSE(config::parse(R"({
      "schema_version": 2, "building": "t",
      "devices": {"actuators": [
        {"type": "light", "id": 29, "room": "k",
         "backend": {"kind": "mcp", "expander": 0, "pin": 3}}]},
      "bindings": []})",
                              config, error));
    CHECK(error.find("M3") != std::string::npos);
}