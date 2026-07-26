#include <doctest/doctest.h>

#include "mocks/fakes.h"
#include "picohome/control/event_router.h"
#include "picohome/devices/binary_sensor.h"
#include "picohome/devices/button.h"
#include "picohome/devices/relay.h"

using namespace picohome;
using test::FakeDigitalInput;
using test::FakeDigitalOutput;
using test::RecordingPublisher;

TEST_CASE("relay handles set_binary and publishes retained state") {
    FakeDigitalOutput out;
    Relay relay(DeviceType::Light, 29, "kitchen", out);
    RecordingPublisher publisher;
    relay.attachPublisher(&publisher);
    relay.initialize();
    CHECK_FALSE(out.value);

    CHECK(relay.handleCommand("set_binary", "1"));
    CHECK(out.value);
    CHECK(relay.isOn());
    REQUIRE_FALSE(publisher.messages.empty());
    const auto& msg = publisher.messages.back();
    CHECK(msg.command == "state");
    CHECK(msg.payload == "1");
    CHECK(msg.retained);

    CHECK(relay.handleCommand("set_binary", "0"));
    CHECK_FALSE(out.value);

    CHECK_FALSE(relay.handleCommand("set_binary", "banana"));
    CHECK_FALSE(relay.handleCommand("set_range", "50"));
}

TEST_CASE("relay exposes the binary capability and nothing else") {
    FakeDigitalOutput out;
    Relay relay(DeviceType::Light, 29, "kitchen", out);
    CHECK(relay.asBinary() != nullptr);
    CHECK(relay.asRange() == nullptr);
    CHECK(relay.asPosition() == nullptr);

    relay.asBinary()->toggle();
    CHECK(relay.isOn());
}

TEST_CASE("button publishes gesture events and emits them to the router") {
    FakeDigitalInput in;
    RecordingPublisher publisher;
    EventRouter router;

    struct Recorder : IEventObserver {
        std::vector<SourceEvent> events;
        void onSourceEvent(const SourceEvent& event) override { events.push_back(event); }
    } recorder;
    router.subscribe(SourceKey::button(29), &recorder);

    Button button(29, "kitchen", in);
    button.attachPublisher(&publisher);
    button.attachRouter(&router);

    uint32_t now = 0;
    const auto run = [&](bool pressed, uint32_t ms) {
        in.value = pressed;
        for (uint32_t i = 0; i < ms; ++i) button.update(++now);
    };

    run(true, 100);
    run(false, 450);

    REQUIRE(publisher.messages.size() == 1);
    CHECK(publisher.messages[0].command == "event");
    CHECK(publisher.messages[0].payload == "press_1");
    CHECK_FALSE(publisher.messages[0].retained);

    REQUIRE(recorder.events.size() == 1);
    CHECK(recorder.events[0].kind == EventKind::Gesture);
    CHECK(recorder.events[0].gesture() == Gesture::Press1);
}

TEST_CASE("binary sensor debounces, publishes retained state and emits edges") {
    FakeDigitalInput in;
    RecordingPublisher publisher;
    EventRouter router;

    struct Recorder : IEventObserver {
        std::vector<SourceEvent> events;
        void onSourceEvent(const SourceEvent& event) override { events.push_back(event); }
    } recorder;
    router.subscribe(SourceKey::sensor(DeviceType::Pir, 720), &recorder);

    BinarySensor sensor(DeviceType::Pir, 720, "corridor", in, /*debounceMs=*/50);
    sensor.attachPublisher(&publisher);
    sensor.attachRouter(&router);
    sensor.initialize();

    uint32_t now = 0;
    const auto run = [&](bool active, uint32_t ms) {
        in.value = active;
        for (uint32_t i = 0; i < ms; ++i) sensor.update(++now);
    };

    run(true, 20);  // shorter than the debounce window
    CHECK_FALSE(sensor.isActive());
    CHECK(recorder.events.empty());

    run(true, 60);
    CHECK(sensor.isActive());
    REQUIRE(recorder.events.size() == 1);
    CHECK(recorder.events[0].kind == EventKind::SensorEdge);
    CHECK(recorder.events[0].active());
    CHECK(publisher.messages.back().command == "state");
    CHECK(publisher.messages.back().payload == "1");
    CHECK(publisher.messages.back().retained);

    run(false, 80);
    CHECK_FALSE(sensor.isActive());
    REQUIRE(recorder.events.size() == 2);
    CHECK_FALSE(recorder.events[1].active());
}