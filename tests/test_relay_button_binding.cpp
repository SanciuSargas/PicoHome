#include <doctest/doctest.h>

#include <memory>

#include "mocks/fakes.h"
#include "picohome/control/binding_engine.h"
#include "picohome/core/device_registry.h"
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

TEST_CASE("press_1 toggles the bound relay end to end") {
    FakeDigitalOutput out;
    FakeDigitalInput in;
    DeviceRegistry registry;
    BindingEngine bindings(registry);

    auto* relay = static_cast<Relay*>(
        registry.add(std::make_unique<Relay>(DeviceType::Light, 29, "kitchen", out)));
    auto button = std::make_unique<Button>(29, "kitchen", in);
    button->attachListener(&bindings);
    registry.add(std::move(button));
    bindings.addToggle({29, DeviceType::Light, 29});
    registry.initializeAll();

    uint32_t now = 0;
    const auto run = [&](bool pressed, uint32_t ms) {
        in.value = pressed;
        for (uint32_t i = 0; i < ms; ++i) registry.updateAll(++now);
    };

    run(true, 100);
    run(false, 450);
    CHECK(relay->isOn());

    run(true, 100);
    run(false, 450);
    CHECK_FALSE(relay->isOn());
}

TEST_CASE("button publishes gesture events") {
    FakeDigitalInput in;
    RecordingPublisher publisher;
    Button button(29, "kitchen", in);
    button.attachPublisher(&publisher);

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
}
