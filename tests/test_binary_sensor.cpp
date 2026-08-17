#include <doctest/doctest.h>

#include <memory>
#include <string>

#include "mocks/fakes.h"
#include "picohome/config/config.h"
#include "picohome/control/event_router.h"
#include "picohome/devices/binary_sensor.h"

using namespace picohome;
using test::FakeDigitalInput;
using test::RecordingPublisher;

namespace {

// One sensor with its pin, publisher and router, driven a millisecond at a time
// so debounce windows are exercised at the resolution the control loop runs at.
struct Harness {
    FakeDigitalInput input;
    RecordingPublisher publisher;
    EventRouter router;
    std::unique_ptr<BinarySensor> sensor;
    uint32_t now = 0;

    explicit Harness(DeviceType type = DeviceType::Reed, uint32_t debounceMs = 50) {
        sensor = std::make_unique<BinarySensor>(type, 700, "kitchen", input, debounceMs);
        sensor->attachPublisher(&publisher);
        sensor->attachRouter(&router);
    }

    void run(uint32_t ms) {
        for (uint32_t i = 0; i < ms; ++i) sensor->update(++now);
    }

    // Counts only state publications, so a test can ignore anything else a
    // device might say.
    std::size_t states() const {
        std::size_t count = 0;
        for (const auto& message : publisher.messages) {
            if (message.command == "state") ++count;
        }
        return count;
    }

    std::string lastState() const {
        for (auto it = publisher.messages.rbegin(); it != publisher.messages.rend(); ++it) {
            if (it->command == "state") return it->payload;
        }
        return "<none>";
    }
};

// Records what a binding would see, without needing a binding.
struct EdgeRecorder final : IEventObserver {
    std::vector<bool> edges;
    void onSourceEvent(const SourceEvent& event) override { edges.push_back(event.active()); }
};

}  // namespace

TEST_CASE("a sensor publishes its state at boot") {
    Harness h;
    h.input.value = true;
    h.sensor->initialize();

    REQUIRE(h.states() == 1);
    CHECK(h.lastState() == "1");
    CHECK(h.publisher.messages[0].retained);
}

TEST_CASE("boot is silent: a PIR already active at power-up emits no event") {
    // Otherwise a power cut at noon lights the whole house: the retained
    // set_night_mode that would have gated it has not arrived from the broker
    // yet, and the gate defaults to on.
    Harness h(DeviceType::Pir, 0);
    EdgeRecorder observer;
    h.router.subscribe(SourceKey::sensor(DeviceType::Pir, 700), &observer);

    h.input.value = true;
    h.sensor->initialize();
    h.run(500);

    CHECK(observer.edges.empty());
    CHECK(h.sensor->isActive());

    // The next real change is reported normally.
    h.input.value = false;
    h.run(5);
    REQUIRE(observer.edges.size() == 1);
    CHECK(observer.edges[0] == false);
}

TEST_CASE("a bouncing reed contact settles into one edge") {
    Harness h(DeviceType::Reed, 50);
    EdgeRecorder observer;
    h.router.subscribe(SourceKey::sensor(DeviceType::Reed, 700), &observer);
    h.sensor->initialize();
    const std::size_t bootStates = h.states();

    for (int i = 0; i < 8; ++i) {  // contact chatter as the magnet leaves
        h.input.value = !h.input.value;
        h.run(5);
    }
    h.input.value = true;
    h.run(60);

    REQUIRE(observer.edges.size() == 1);
    CHECK(observer.edges[0] == true);
    CHECK(h.states() == bootStates + 1);
}

TEST_CASE("a pulse shorter than the debounce window is not reported") {
    Harness h(DeviceType::Reed, 50);
    h.sensor->initialize();
    const std::size_t bootStates = h.states();

    h.input.value = true;
    h.run(20);
    h.input.value = false;
    h.run(100);

    CHECK(h.states() == bootStates);
    CHECK_FALSE(h.sensor->isActive());
}

TEST_CASE("with nothing to debounce, an edge lands in the same iteration") {
    // A PIR's whole value is being early; a debounce here is pure latency on
    // the path that motion lighting is judged on.
    Harness h(DeviceType::Pir, 0);
    EdgeRecorder observer;
    h.router.subscribe(SourceKey::sensor(DeviceType::Pir, 700), &observer);
    h.sensor->initialize();

    h.input.value = true;
    h.run(1);

    REQUIRE(observer.edges.size() == 1);
    CHECK(observer.edges[0] == true);
}

TEST_CASE("state is published on change, not on every iteration") {
    Harness h(DeviceType::Presence, 0);
    h.sensor->initialize();
    const std::size_t bootStates = h.states();

    h.input.value = true;
    h.run(2000);  // someone sitting still: OUT stays high the whole time
    CHECK(h.states() == bootStates + 1);
    CHECK(h.lastState() == "1");

    h.input.value = false;
    h.run(2000);
    CHECK(h.states() == bootStates + 2);
    CHECK(h.lastState() == "0");
}

TEST_CASE("publishState re-sends the current state on demand") {
    // The broker reconnect path: every device re-announces itself.
    Harness h(DeviceType::WaterLeak, 0);
    h.input.value = true;
    h.sensor->initialize();
    h.publisher.messages.clear();

    h.sensor->publishState();
    REQUIRE(h.states() == 1);
    CHECK(h.lastState() == "1");
}

TEST_CASE("each sensor type defaults to the debounce its signal needs") {
    using config::defaultDebounceMs;

    // Clean logic outputs with their own hold time: waiting buys nothing.
    CHECK(defaultDebounceMs(DeviceType::Pir) == 0);
    CHECK(defaultDebounceMs(DeviceType::Presence) == 0);

    // A mechanical contact bounces.
    CHECK(defaultDebounceMs(DeviceType::Reed) > 0);

    // A conductive probe chatters at its threshold, and a false leak alarm is
    // expensive — so it waits longest of all.
    CHECK(defaultDebounceMs(DeviceType::WaterLeak) > defaultDebounceMs(DeviceType::Reed));

    // A smoke detector's output is driven, not bouncing; the margin is there
    // for the long cable run, so it sits between the two.
    CHECK(defaultDebounceMs(DeviceType::Smoke) > defaultDebounceMs(DeviceType::Pir));
    CHECK(defaultDebounceMs(DeviceType::Smoke) < defaultDebounceMs(DeviceType::WaterLeak));
}

TEST_CASE("a water leak probe chattering on a thin film reports once") {
    const uint32_t debounce = config::defaultDebounceMs(DeviceType::WaterLeak);
    Harness h(DeviceType::WaterLeak, debounce);
    h.sensor->initialize();
    const std::size_t bootStates = h.states();

    for (int i = 0; i < 10; ++i) {  // probe dipping in and out of conduction
        h.input.value = true;
        h.run(200);
        h.input.value = false;
        h.run(200);
    }
    CHECK(h.states() == bootStates);  // never held long enough to be believed

    h.input.value = true;  // the puddle arrives properly
    h.run(debounce + 10);
    CHECK(h.states() == bootStates + 1);
    CHECK(h.lastState() == "1");
}

TEST_CASE("an inverted pin is the config's business, not the sensor's") {
    // A reed contact wired to ground with a pull-up reads low when closed;
    // "active_high": false handles it in the pin object, so the device sees
    // plain true/false either way.
    Harness h(DeviceType::Reed, 0);
    h.sensor->initialize();
    CHECK(h.lastState() == "0");

    h.input.value = true;
    h.run(2);
    CHECK(h.lastState() == "1");
}
