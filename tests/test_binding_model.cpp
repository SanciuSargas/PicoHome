#include <doctest/doctest.h>

#include <map>
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
using test::FakeDigitalInput;
using test::FakeDigitalOutput;

namespace {

// Builds a system straight from JSON, exactly as the firmware does at boot, so
// the tests exercise the real path: parse -> devices -> bindings -> validation.
struct System {
    DeviceRegistry registry;
    EventRouter router;
    BindingSet bindings;
    RemoteSourceTable remoteSources;
    config::App config;
    std::string error;

    std::vector<std::unique_ptr<FakeDigitalOutput>> outputs;
    std::vector<std::unique_ptr<FakeDigitalInput>> inputs;
    std::map<uint16_t, FakeDigitalInput*> buttonInputs;
    std::map<uint16_t, FakeDigitalInput*> sensorInputs;
    std::map<uint16_t, Relay*> relays;

    uint32_t now = 0;

    bool load(const std::string& json) {
        if (!config::parse(json, config, error)) return false;

        for (const auto& def : config.actuators) {
            auto out = std::make_unique<FakeDigitalOutput>();
            auto* relay = static_cast<Relay*>(
                registry.add(std::make_unique<Relay>(def.type, def.id, def.room, *out)));
            relays[def.id] = relay;
            outputs.push_back(std::move(out));
        }
        for (const auto& def : config.sensors) {
            auto in = std::make_unique<FakeDigitalInput>();
            auto sensor =
                std::make_unique<BinarySensor>(def.type, def.id, def.room, *in, def.debounceMs);
            sensor->attachRouter(&router);
            registry.add(std::move(sensor));
            sensorInputs[def.id] = in.get();
            inputs.push_back(std::move(in));
        }
        for (const auto& def : config.buttons) {
            auto in = std::make_unique<FakeDigitalInput>();
            auto button = std::make_unique<Button>(def.id, def.room, *in, def.kind, def.gestures);
            button->attachRouter(&router);
            registry.add(std::move(button));
            buttonInputs[def.id] = in.get();
            inputs.push_back(std::move(in));
        }
        if (!bindings.build(config, registry, router, remoteSources, error)) return false;
        registry.initializeAll();
        return true;
    }

    void run(uint32_t ms) {
        for (uint32_t i = 0; i < ms; ++i) {
            ++now;
            registry.updateAll(now);
            bindings.updateAll(now);
        }
    }

    // A complete single press of a momentary switch, including the multi-press
    // gap that press_1 must wait out.
    void press(uint16_t buttonId) {
        buttonInputs.at(buttonId)->value = true;
        run(100);
        buttonInputs.at(buttonId)->value = false;
        run(450);
    }

    // One flip of a latching rocker — which direction it ends up in does not
    // matter, the flip itself is the user's action.
    void flip(uint16_t buttonId) {
        auto* input = buttonInputs.at(buttonId);
        input->value = !input->value;
        run(400);
    }

    void setSensor(uint16_t sensorId, bool active) {
        sensorInputs.at(sensorId)->value = active;
        run(80);  // past the default 50 ms debounce
    }
};

// Two lights and two switches; `bindingsJson` fills in the behaviour.
std::string makeConfig(const char* bindingsJson, const char* extraSensors = "") {
    std::string sensors;
    if (*extraSensors != '\0') sensors = std::string("\"sensors\": [") + extraSensors + "],";
    return std::string(R"json({
      "schema_version": 2,
      "building": "test",
      "devices": {
        "actuators": [
          {"type": "light", "id": 29, "room": "kitchen", "backend": {"kind": "gpio", "pin": 2}},
          {"type": "light", "id": 30, "room": "kitchen", "backend": {"kind": "gpio", "pin": 4}}
        ],)json") +
           sensors +
           R"json(
        "buttons": [
          {"id": 29, "room": "kitchen", "kind": "momentary", "gestures": true, "pin": {"pin": 3}},
          {"id": 30, "room": "corridor", "kind": "toggle", "pin": {"pin": 5}},
          {"id": 31, "room": "kitchen", "kind": "momentary", "pin": {"pin": 7}}
        ]
      },
      "bindings": )json" +
           bindingsJson + "}";
}

}  // namespace

TEST_CASE("one button toggles two lights (fan-out)") {
    System sys;
    REQUIRE_MESSAGE(sys.load(makeConfig(R"([
      {"name": "both", "rule": "toggle",
       "sources": [{"button": 29}],
       "targets": [{"type": "light", "id": 29}, {"type": "light", "id": 30}]}
    ])")),
                    sys.error);

    sys.press(29);
    CHECK(sys.relays[29]->isOn());
    CHECK(sys.relays[30]->isOn());

    sys.press(29);
    CHECK_FALSE(sys.relays[29]->isOn());
    CHECK_FALSE(sys.relays[30]->isOn());
}

TEST_CASE("two buttons toggle one light (fan-in, P-group, mixed switch kinds)") {
    // Button 29 is a momentary pushbutton, button 30 a latching rocker — on a
    // plain light the two are interchangeable, which is the whole point of
    // decoding both into the same press_1.
    System sys;
    REQUIRE_MESSAGE(sys.load(makeConfig(R"([
      {"name": "group", "rule": "toggle",
       "sources": [{"button": 29}, {"button": 30}],
       "targets": [{"type": "light", "id": 29}]}
    ])")),
                    sys.error);

    sys.press(29);
    CHECK(sys.relays[29]->isOn());

    sys.flip(30);  // the rocker on the other wall turns it back off
    CHECK_FALSE(sys.relays[29]->isOn());

    sys.flip(30);  // flipping it back is another press, not a "return to off"
    CHECK(sys.relays[29]->isOn());
}

TEST_CASE("a toggle switch resting closed at boot does not switch anything") {
    System sys;
    REQUIRE_MESSAGE(sys.load(makeConfig(R"([
      {"name": "rocker", "rule": "toggle",
       "sources": [{"button": 30}],
       "targets": [{"type": "light", "id": 29}]}
    ])")),
                    sys.error);

    // Simulate the panel powering up with the rocker already in its "on"
    // position: initialize() primes the detector from the pin.
    sys.buttonInputs[30]->value = true;
    sys.registry.initializeAll();
    sys.run(1000);
    CHECK_FALSE(sys.relays[29]->isOn());

    sys.flip(30);
    CHECK(sys.relays[29]->isOn());
}

TEST_CASE("gesture filter selects which press acts; press_1 is the default") {
    System sys;
    REQUIRE_MESSAGE(sys.load(makeConfig(R"([
      {"name": "double only", "rule": "toggle",
       "sources": [{"button": 29, "gesture": "press_2"}],
       "targets": [{"type": "light", "id": 29}]}
    ])")),
                    sys.error);

    sys.press(29);  // single press: not our gesture
    CHECK_FALSE(sys.relays[29]->isOn());

    sys.buttonInputs[29]->value = true;
    sys.run(60);
    sys.buttonInputs[29]->value = false;
    sys.run(60);
    sys.buttonInputs[29]->value = true;
    sys.run(60);
    sys.buttonInputs[29]->value = false;
    sys.run(450);
    CHECK(sys.relays[29]->isOn());
}

TEST_CASE("an instant switch acts on the press itself, a gesture switch waits") {
    System sys;
    REQUIRE_MESSAGE(sys.load(makeConfig(R"([
      {"name": "instant", "rule": "toggle",
       "sources": [{"button": 31}], "targets": [{"type": "light", "id": 29}]},
      {"name": "full grammar", "rule": "toggle",
       "sources": [{"button": 29}], "targets": [{"type": "light", "id": 30}]}
    ])")),
                    sys.error);

    // Button 31 is still held down here — it has already acted, because there
    // is no second press to rule out.
    sys.buttonInputs[31]->value = true;
    sys.run(40);
    CHECK(sys.relays[29]->isOn());

    // Button 29 decodes the whole grammar, so at the same point it is still
    // waiting to find out what the user meant.
    sys.buttonInputs[29]->value = true;
    sys.run(40);
    CHECK_FALSE(sys.relays[30]->isOn());

    sys.buttonInputs[29]->value = false;
    sys.run(60);
    CHECK_FALSE(sys.relays[30]->isOn());  // released, still inside the window
    sys.run(300);
    CHECK(sys.relays[30]->isOn());        // window closed: press_1 at last
}

TEST_CASE("an instant switch reports one press per press, not per edge") {
    System sys;
    REQUIRE_MESSAGE(sys.load(makeConfig(R"([
      {"name": "instant", "rule": "toggle",
       "sources": [{"button": 31}], "targets": [{"type": "light", "id": 29}]}
    ])")),
                    sys.error);

    // Press and release: the release must not toggle it back.
    sys.buttonInputs[31]->value = true;
    sys.run(40);
    CHECK(sys.relays[29]->isOn());
    sys.buttonInputs[31]->value = false;
    sys.run(200);
    CHECK(sys.relays[29]->isOn());

    sys.buttonInputs[31]->value = true;
    sys.run(40);
    CHECK_FALSE(sys.relays[29]->isOn());
}

TEST_CASE("an instant switch still debounces") {
    System sys;
    REQUIRE_MESSAGE(sys.load(makeConfig(R"([
      {"name": "instant", "rule": "toggle",
       "sources": [{"button": 31}], "targets": [{"type": "light", "id": 29}]}
    ])")),
                    sys.error);

    // Contact chatter shorter than the debounce window is not a press.
    for (int i = 0; i < 5; ++i) {
        sys.buttonInputs[31]->value = true;
        sys.run(5);
        sys.buttonInputs[31]->value = false;
        sys.run(5);
    }
    CHECK_FALSE(sys.relays[29]->isOn());
}

TEST_CASE("an instant rocker acts on either flip, and not at boot") {
    System sys;
    REQUIRE_MESSAGE(sys.load(makeConfig(R"([
      {"name": "rocker", "rule": "toggle",
       "sources": [{"button": 30}], "targets": [{"type": "light", "id": 29}]}
    ])")),
                    sys.error);

    sys.buttonInputs[30]->value = true;
    sys.run(40);
    CHECK(sys.relays[29]->isOn());

    sys.buttonInputs[30]->value = false;  // flipping back is another action
    sys.run(40);
    CHECK_FALSE(sys.relays[29]->isOn());
}

TEST_CASE("motion: two sensors hold the light, off-delay runs after both go quiet") {
    System sys;
    REQUIRE_MESSAGE(
        sys.load(makeConfig(R"([
      {"name": "corridor motion", "rule": "motion",
       "sources": [{"sensor": {"type": "pir", "id": 720}},
                   {"sensor": {"type": "presence", "id": 730}}],
       "targets": [{"type": "light", "id": 29}],
       "options": {"off_delay_ms": 5000}}])",
                        R"({"type": "pir", "id": 720, "room": "corridor", "pin": {"pin": 8}},
                           {"type": "presence", "id": 730, "room": "corridor", "pin": {"pin": 9}})")),
        sys.error);

    sys.setSensor(720, true);
    CHECK(sys.relays[29]->isOn());

    // PIR pulse ends but presence still holds the room occupied.
    sys.setSensor(730, true);
    sys.setSensor(720, false);
    sys.run(8000);
    CHECK(sys.relays[29]->isOn());

    // Both quiet now: the light survives until the off-delay expires.
    sys.setSensor(730, false);
    sys.run(4000);
    CHECK(sys.relays[29]->isOn());
    sys.run(2000);
    CHECK_FALSE(sys.relays[29]->isOn());
}

TEST_CASE("motion retriggers: new activity during the countdown keeps the light on") {
    System sys;
    REQUIRE_MESSAGE(
        sys.load(makeConfig(R"([
      {"name": "corridor motion", "rule": "motion",
       "sources": [{"sensor": {"type": "pir", "id": 720}}],
       "targets": [{"type": "light", "id": 29}],
       "options": {"off_delay_ms": 5000}}])",
                        R"({"type": "pir", "id": 720, "room": "corridor", "pin": {"pin": 8}})")),
        sys.error);

    sys.setSensor(720, true);
    sys.setSensor(720, false);
    sys.run(3000);
    CHECK(sys.relays[29]->isOn());

    sys.setSensor(720, true);  // walked past again
    sys.setSensor(720, false);
    sys.run(3000);
    CHECK(sys.relays[29]->isOn());  // countdown restarted

    sys.run(3000);
    CHECK_FALSE(sys.relays[29]->isOn());
}

TEST_CASE("night_mode off: motion leaves the light completely alone") {
    System sys;
    REQUIRE_MESSAGE(
        sys.load(makeConfig(R"([
      {"name": "corridor motion", "rule": "motion",
       "sources": [{"sensor": {"type": "pir", "id": 720}}],
       "targets": [{"type": "light", "id": 29}],
       "options": {"off_delay_ms": 5000}}])",
                        R"({"type": "pir", "id": 720, "room": "corridor", "pin": {"pin": 8}})")),
        sys.error);

    // Daytime, as decided by Home Assistant.
    sys.relays[29]->setNightMode(false);

    sys.setSensor(720, true);
    CHECK_FALSE(sys.relays[29]->isOn());

    // A light switched on by hand must not be switched off by the motion rule
    // that never turned it on.
    sys.relays[29]->setBinary(true);
    sys.setSensor(720, false);
    sys.run(10000);
    CHECK(sys.relays[29]->isOn());
}

TEST_CASE("night_mode defaults on, so motion works before the broker says anything") {
    System sys;
    REQUIRE_MESSAGE(
        sys.load(makeConfig(R"([
      {"name": "corridor motion", "rule": "motion",
       "sources": [{"sensor": {"type": "pir", "id": 720}}],
       "targets": [{"type": "light", "id": 29}],
       "options": {"off_delay_ms": 1000}}])",
                        R"({"type": "pir", "id": 720, "room": "corridor", "pin": {"pin": 8}})")),
        sys.error);

    CHECK(sys.relays[29]->nightMode());
    sys.setSensor(720, true);
    CHECK(sys.relays[29]->isOn());

    // ...and Home Assistant can switch the gate back off at sunrise.
    sys.relays[29]->setNightMode(true);
    CHECK(sys.relays[29]->nightMode());
}

TEST_CASE("motion gate is per light: one may be night-gated while another is not") {
    System sys;
    REQUIRE_MESSAGE(
        sys.load(makeConfig(R"([
      {"name": "corridor motion", "rule": "motion",
       "sources": [{"sensor": {"type": "pir", "id": 720}}],
       "targets": [{"type": "light", "id": 29}, {"type": "light", "id": 30}],
       "options": {"off_delay_ms": 1000}}])",
                        R"({"type": "pir", "id": 720, "room": "corridor", "pin": {"pin": 8}})")),
        sys.error);

    sys.relays[30]->setNightMode(false);

    sys.setSensor(720, true);
    CHECK(sys.relays[29]->isOn());
    CHECK_FALSE(sys.relays[30]->isOn());

    sys.setSensor(720, false);
    sys.run(2000);
    CHECK_FALSE(sys.relays[29]->isOn());
}

TEST_CASE("a wall switch still works while motion controls the same light") {
    System sys;
    REQUIRE_MESSAGE(
        sys.load(makeConfig(R"([
      {"name": "manual", "rule": "toggle",
       "sources": [{"button": 29}], "targets": [{"type": "light", "id": 29}]},
      {"name": "motion", "rule": "motion",
       "sources": [{"sensor": {"type": "pir", "id": 720}}],
       "targets": [{"type": "light", "id": 29}],
       "options": {"off_delay_ms": 5000}}])",
                        R"({"type": "pir", "id": 720, "room": "corridor", "pin": {"pin": 8}})")),
        sys.error);

    sys.press(29);
    CHECK(sys.relays[29]->isOn());
    sys.press(29);
    CHECK_FALSE(sys.relays[29]->isOn());

    sys.setSensor(720, true);
    CHECK(sys.relays[29]->isOn());
}
