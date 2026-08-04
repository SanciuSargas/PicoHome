#include <doctest/doctest.h>

#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "mocks/fakes.h"
#include "picohome/config/config.h"
#include "picohome/control/binding_set.h"
#include "picohome/control/event_router.h"
#include "picohome/core/device_registry.h"
#include "picohome/devices/button.h"
#include "picohome/devices/relay.h"
#include "picohome/net/command_router.h"
#include "picohome/net/publisher.h"
#include "picohome/net/remote_sources.h"
#include "picohome/net/topic.h"

using namespace picohome;

namespace {

// Stands in for the inter-core queue: same contract, inspectable afterwards.
struct RecordingSink : IPublishSink {
    std::vector<PublishMessage> messages;
    std::size_t capacity = 100;

    bool push(const PublishMessage& message) override {
        if (messages.size() >= capacity) return false;
        messages.push_back(message);
        return true;
    }
};

InboundMessage deviceCommand(DeviceType type, uint16_t id, const char* command,
                             const char* payload) {
    InboundMessage message;
    message.kind = InboundKind::DeviceCommand;
    message.deviceType = type;
    message.deviceId = id;
    std::strncpy(message.command, command, sizeof(message.command) - 1);
    std::strncpy(message.payload, payload, sizeof(message.payload) - 1);
    return message;
}

}  // namespace

TEST_CASE("publisher builds the full topic and picks QoS per command") {
    RecordingSink sink;
    QueuePublisher publisher(sink, "utility");
    test::FakeDigitalOutput out;
    Relay light(DeviceType::Light, 29, "kitchen", out);
    light.attachPublisher(&publisher);

    light.setBinary(true);

    REQUIRE(sink.messages.size() == 1);
    CHECK(std::string(sink.messages[0].topic) == "utility/kitchen/light/29/state");
    CHECK(std::string(sink.messages[0].payload) == "1");
    CHECK(sink.messages[0].retained);
    CHECK(sink.messages[0].qos == 1);
}

TEST_CASE("a full queue drops messages and counts them instead of blocking") {
    RecordingSink sink;
    sink.capacity = 2;
    QueuePublisher publisher(sink, "utility");
    test::FakeDigitalOutput out;
    Relay light(DeviceType::Light, 29, "kitchen", out);
    light.attachPublisher(&publisher);

    for (int i = 0; i < 5; ++i) light.setBinary(i % 2 == 0);  // on/off/on/off/on

    CHECK(sink.messages.size() == 2);
    CHECK(publisher.droppedCount() == 3);
    CHECK(light.isOn());
    // The relay itself still switched every time — the network is never in the
    // control path.
    CHECK(out.value);
}

TEST_CASE("inbound set_binary reaches the device; unknown devices are counted") {
    DeviceRegistry registry;
    EventRouter router;
    test::FakeDigitalOutput out;
    auto* light = static_cast<Relay*>(
        registry.add(std::make_unique<Relay>(DeviceType::Light, 29, "kitchen", out)));
    registry.initializeAll();

    CommandRouter commands(registry, router);

    commands.handle(deviceCommand(DeviceType::Light, 29, "set_binary", "1"));
    CHECK(light->isOn());
    commands.handle(deviceCommand(DeviceType::Light, 29, "set_binary", "0"));
    CHECK_FALSE(light->isOn());
    CHECK(commands.unroutedCount() == 0);

    commands.handle(deviceCommand(DeviceType::Light, 999, "set_binary", "1"));
    CHECK(commands.unroutedCount() == 1);
}

TEST_CASE("a command carrying the value a device already has says nothing") {
    // Home Assistant re-asserts night_mode on a schedule and may repeat
    // set_binary; neither should put traffic on the broker.
    RecordingSink sink;
    QueuePublisher publisher(sink, "utility");
    DeviceRegistry registry;
    EventRouter router;
    test::FakeDigitalOutput out;
    registry.add(std::make_unique<Relay>(DeviceType::Light, 29, "kitchen", out));
    registry.attachPublisher(&publisher);
    registry.initializeAll();
    CommandRouter commands(registry, router);

    commands.handle(deviceCommand(DeviceType::Light, 29, "set_binary", "0"));      // already off
    commands.handle(deviceCommand(DeviceType::Light, 29, "set_night_mode", "1"));  // already on
    CHECK(sink.messages.empty());

    commands.handle(deviceCommand(DeviceType::Light, 29, "set_binary", "1"));
    CHECK(sink.messages.size() == 1);
}

TEST_CASE("retained set_night_mode from high level gates the motion rule") {
    DeviceRegistry registry;
    EventRouter router;
    test::FakeDigitalOutput out;
    auto* light = static_cast<Relay*>(
        registry.add(std::make_unique<Relay>(DeviceType::Light, 29, "corridor", out)));
    registry.initializeAll();
    CommandRouter commands(registry, router);

    CHECK(light->nightMode());  // defaults on, so motion works before the broker speaks
    commands.handle(deviceCommand(DeviceType::Light, 29, "set_night_mode", "0"));
    CHECK_FALSE(light->nightMode());
    commands.handle(deviceCommand(DeviceType::Light, 29, "set_night_mode", "1"));
    CHECK(light->nightMode());
}

TEST_CASE("publishAllStates refreshes state and night_mode together") {
    RecordingSink sink;
    QueuePublisher publisher(sink, "utility");
    DeviceRegistry registry;
    EventRouter router;
    test::FakeDigitalOutput out;
    registry.add(std::make_unique<Relay>(DeviceType::Light, 29, "kitchen", out));
    registry.attachPublisher(&publisher);
    registry.initializeAll();

    CommandRouter commands(registry, router);
    commands.handle(InboundMessage::publishAllStates());

    REQUIRE(sink.messages.size() == 2);
    CHECK(std::string(sink.messages[0].topic) == "utility/kitchen/light/29/state");
    CHECK(std::string(sink.messages[1].topic) == "utility/kitchen/light/29/night_mode");
    CHECK(sink.messages[1].retained);
}

TEST_CASE("remote source table interns action pairs and dedupes topics") {
    RemoteSourceTable table;
    const uint16_t a = table.internAction("closet/corridor/button/31/event", "press_1");
    const uint16_t b = table.internAction("closet/corridor/button/31/event", "press_2");
    const uint16_t again = table.internAction("closet/corridor/button/31/event", "press_1");

    CHECK(a != b);
    CHECK(a == again);
    CHECK(table.size() == 2);
    REQUIRE(table.topics().size() == 1);  // one subscription covers both payloads

    const auto* match = table.find("closet/corridor/button/31/event", "press_1");
    REQUIRE(match != nullptr);
    CHECK(match->sourceIndex == a);
    CHECK(match->kind == EventKind::Gesture);

    CHECK(table.find("closet/corridor/button/31/event", "press_9") == nullptr);
    CHECK(table.find("other/topic", "press_1") == nullptr);
}

TEST_CASE("a remote sensor is one source reported by two payloads") {
    RemoteSourceTable table;
    const uint16_t index = table.internSensor("closet/corridor/pir/720/state", "1", "0");

    CHECK(table.size() == 1);              // one source...
    CHECK(table.topics().size() == 1);     // ...one subscription...

    const auto* active = table.find("closet/corridor/pir/720/state", "1");
    const auto* inactive = table.find("closet/corridor/pir/720/state", "0");
    REQUIRE(active != nullptr);
    REQUIRE(inactive != nullptr);
    CHECK(active->sourceIndex == index);   // ...both edges of the same thing
    CHECK(inactive->sourceIndex == index);
    CHECK(active->kind == EventKind::SensorEdge);
    CHECK(active->value == 1);
    CHECK(inactive->value == 0);
}

TEST_CASE("subscriptions cover the command verbs plus every remote topic") {
    // One wildcard per command verb — their number stays constant however many
    // devices a building has — plus one exact topic per remote source.
    const auto subs = buildSubscriptions("utility", {"closet/corridor/button/31/event"});
    REQUIRE(subs.size() == 6);
    CHECK(subs[0] == "utility/+/+/+/set_binary");
    CHECK(subs[1] == "utility/+/+/+/set_night_mode");
    CHECK(subs[2] == "utility/+/+/+/set_position");
    CHECK(subs[3] == "utility/+/+/+/set_tilt");
    CHECK(subs[4] == "utility/+/+/+/stop");
    CHECK(subs[5] == "closet/corridor/button/31/event");
}

TEST_CASE("a cross-panel switch drives a local light end to end") {
    // The corridor case: switch wired to the closet panel, relay on this one.
    const char* json = R"json({
      "schema_version": 2,
      "building": "utility",
      "devices": {
        "actuators": [{"type": "light", "id": 30, "room": "corridor",
                       "backend": {"kind": "gpio", "pin": 2}}],
        "buttons": [{"id": 30, "room": "corridor", "kind": "momentary", "pin": {"pin": 3}}]
      },
      "bindings": [
        {"name": "corridor 30PJ/31PJ", "rule": "toggle",
         "sources": [{"button": 30},
                     {"topic": "closet/corridor/button/31/event", "payload": "press_1"}],
         "targets": [{"type": "light", "id": 30}]}
      ]})json";

    config::App config;
    std::string error;
    REQUIRE_MESSAGE(config::parse(json, config, error), error);

    DeviceRegistry registry;
    EventRouter router;
    BindingSet bindings;
    RemoteSourceTable remoteSources;

    test::FakeDigitalOutput out;
    auto* light = static_cast<Relay*>(
        registry.add(std::make_unique<Relay>(DeviceType::Light, 30, "corridor", out)));
    test::FakeDigitalInput in;
    auto button = std::make_unique<Button>(30, "corridor", in, SwitchKind::Momentary);
    button->attachRouter(&router);
    registry.add(std::move(button));

    REQUIRE_MESSAGE(bindings.build(config, registry, router, remoteSources, error), error);
    registry.initializeAll();

    // The config's remote source produced exactly one subscription.
    REQUIRE(remoteSources.size() == 1);
    CHECK(remoteSources.topics()[0] == "closet/corridor/button/31/event");

    // Core 1 matched an inbound message to that source and handed core 0 the index.
    CommandRouter commands(registry, router);
    const auto* match = remoteSources.find("closet/corridor/button/31/event", "press_1");
    REQUIRE(match != nullptr);

    commands.handle(InboundMessage::remoteEvent(match->sourceIndex, match->kind, match->value));
    CHECK(light->isOn());
    commands.handle(InboundMessage::remoteEvent(match->sourceIndex, match->kind, match->value));
    CHECK_FALSE(light->isOn());
}

TEST_CASE("a motion sensor on another controller drives a local light") {
    // The sensor is wired to a different panel entirely; only its state topic
    // crosses the network, and the motion rule cannot tell the difference.
    const char* json = R"json({
      "schema_version": 2,
      "building": "utility",
      "devices": {
        "actuators": [{"type": "light", "id": 30, "room": "corridor",
                       "backend": {"kind": "gpio", "pin": 2}}]
      },
      "bindings": [
        {"name": "stairwell motion", "rule": "motion",
         "sources": [{"topic": "closet/corridor/pir/720/state"}],
         "targets": [{"type": "light", "id": 30}],
         "options": {"off_delay_ms": 5000}}
      ]})json";

    config::App config;
    std::string error;
    REQUIRE_MESSAGE(config::parse(json, config, error), error);

    DeviceRegistry registry;
    EventRouter router;
    BindingSet bindings;
    RemoteSourceTable remoteSources;
    test::FakeDigitalOutput out;
    auto* light = static_cast<Relay*>(
        registry.add(std::make_unique<Relay>(DeviceType::Light, 30, "corridor", out)));
    REQUIRE_MESSAGE(bindings.build(config, registry, router, remoteSources, error), error);
    registry.initializeAll();

    CommandRouter commands(registry, router);
    const auto* active = remoteSources.find("closet/corridor/pir/720/state", "1");
    const auto* inactive = remoteSources.find("closet/corridor/pir/720/state", "0");
    REQUIRE(active != nullptr);
    REQUIRE(inactive != nullptr);

    commands.handle(InboundMessage::remoteEvent(active->sourceIndex, active->kind, active->value));
    CHECK(light->isOn());

    // The off-delay still applies, exactly as for a local sensor.
    commands.handle(
        InboundMessage::remoteEvent(inactive->sourceIndex, inactive->kind, inactive->value));
    for (uint32_t now = 1; now <= 4000; ++now) bindings.updateAll(now);
    CHECK(light->isOn());
    for (uint32_t now = 4001; now <= 6000; ++now) bindings.updateAll(now);
    CHECK_FALSE(light->isOn());
}