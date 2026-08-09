#include <doctest/doctest.h>

#include <fstream>
#include <sstream>
#include <string>

#include "picohome/config/config.h"
#include "picohome/control/binding_set.h"
#include "picohome/control/event_router.h"
#include "picohome/core/device_registry.h"
#include "picohome/devices/button.h"
#include "picohome/devices/cover.h"
#include "picohome/devices/relay.h"
#include "picohome/drivers/mcp23017.h"
#include "picohome/net/remote_sources.h"

#include "mocks/fakes.h"

using namespace picohome;

// config/testbench.json is copied onto real SD cards, so it is checked here
// like any other input: a stray comma or a pin on an undeclared expander should
// fail the build, not a controller on a wall.

TEST_CASE("the shipped bench config parses and describes a buildable system") {
    std::ifstream file(PICOHOME_TESTBENCH_CONFIG);
    REQUIRE_MESSAGE(file.good(), "cannot open " PICOHOME_TESTBENCH_CONFIG);
    std::stringstream buffer;
    buffer << file.rdbuf();

    config::App app;
    std::string error;
    REQUIRE_MESSAGE(config::parse(buffer.str(), app, error), error);

    CHECK(app.building == "testbench");

    // One device pair on controller GPIOs, one on the expander — the point of
    // the file is that the two are indistinguishable above the HAL.
    REQUIRE(app.expanders.size() == 1);
    CHECK(app.expanders[0].address == 0x20);

    bool sawGpioLight = false;
    bool sawExpanderLight = false;
    for (const auto& actuator : app.actuators) {
        if (actuator.backend == config::BackendKind::Gpio) sawGpioLight = true;
        if (actuator.backend == config::BackendKind::Mcp) {
            sawExpanderLight = true;
            CHECK(actuator.pin.expander == app.expanders[0].index);
        }
    }
    CHECK(sawGpioLight);
    CHECK(sawExpanderLight);

    bool sawMomentary = false;
    bool sawToggle = false;
    for (const auto& button : app.buttons) {
        sawMomentary = sawMomentary || button.kind == SwitchKind::Momentary;
        sawToggle = sawToggle || button.kind == SwitchKind::Toggle;
    }
    CHECK(sawMomentary);
    CHECK(sawToggle);

    // And every binding in it resolves against the devices it declares.
    DeviceRegistry registry;
    EventRouter router;
    BindingSet bindings;
    RemoteSourceTable remoteSources;
    std::vector<std::unique_ptr<test::FakeDigitalOutput>> outputs;
    std::vector<std::unique_ptr<test::FakeDigitalInput>> inputs;

    for (const auto& def : app.actuators) {
        auto out = std::make_unique<test::FakeDigitalOutput>();
        if (def.backend == config::BackendKind::Cover) {
            auto down = std::make_unique<test::FakeDigitalOutput>();
            Cover::Timing timing;
            timing.travelUpMs = def.cover.travelUpMs;
            timing.travelDownMs = def.cover.travelDownMs;
            timing.tiltMs = def.cover.tiltMs;
            timing.stepMs = def.cover.stepMs;
            timing.motorRestMs = def.cover.motorRestMs;
            timing.bootOpen = def.cover.bootOpen;
            registry.add(std::make_unique<Cover>(def.type, def.id, def.room, *out, *down, timing));
            outputs.push_back(std::move(down));
        } else {
            registry.add(std::make_unique<Relay>(def.type, def.id, def.room, *out));
        }
        outputs.push_back(std::move(out));
    }
    for (const auto& def : app.buttons) {
        auto in = std::make_unique<test::FakeDigitalInput>();
        auto button = std::make_unique<Button>(def.id, def.room, *in, def.kind, def.gestures);
        button->attachRouter(&router);
        registry.add(std::move(button));
        inputs.push_back(std::move(in));
    }
    CHECK_MESSAGE(bindings.build(app, registry, router, remoteSources, error), error);
    CHECK(bindings.size() == app.bindings.size());
}
