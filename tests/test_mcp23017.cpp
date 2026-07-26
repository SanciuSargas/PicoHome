#include <doctest/doctest.h>

#include <map>

#include "picohome/config/config.h"
#include "picohome/drivers/mcp23017.h"

using namespace picohome;

namespace {

// Enough of an MCP23017 to hold the driver honest: it records what was written,
// counts bus traffic, and lets a test drive the physical input pins.
struct FakeI2cBus : hal::II2cBus {
    std::map<uint8_t, uint16_t> registers;  // by register number
    uint16_t inputPins = 0;                 // what the outside world is doing
    int reads = 0;
    int writes = 0;
    bool failing = false;

    static constexpr uint8_t kAddress = 0x20;
    static constexpr uint8_t kGpioA = 0x12;
    static constexpr uint8_t kIodirA = 0x00;
    static constexpr uint8_t kGppuA = 0x0C;
    static constexpr uint8_t kGpintenA = 0x04;
    static constexpr uint8_t kOlatA = 0x14;

    bool writeRegister(uint8_t address, uint8_t reg, uint8_t value) override {
        if (failing || address != kAddress) return false;
        ++writes;
        registers[reg] = value;
        return true;
    }
    bool writeRegister16(uint8_t address, uint8_t reg, uint16_t value) override {
        if (failing || address != kAddress) return false;
        ++writes;
        registers[reg] = value;
        return true;
    }
    bool readRegister16(uint8_t address, uint8_t reg, uint16_t& value) override {
        if (failing || address != kAddress) return false;
        ++reads;
        value = reg == kGpioA ? inputPins : registers[reg];
        return true;
    }
};

}  // namespace

TEST_CASE("initialize configures directions, pull-ups and interrupt-on-change") {
    FakeI2cBus bus;
    Mcp23017 expander(bus, FakeI2cBus::kAddress);

    McpDigitalInput button(expander, 3, /*activeHigh=*/false, /*pullUp=*/true);
    McpDigitalOutput relay(expander, 8, /*activeHigh=*/true);
    REQUIRE(expander.initialize(0));

    CHECK(bus.registers[FakeI2cBus::kIodirA] == (1u << 3));    // pin 3 in, pin 8 out
    CHECK(bus.registers[FakeI2cBus::kGppuA] == (1u << 3));
    CHECK(bus.registers[FakeI2cBus::kGpintenA] == (1u << 3));  // only inputs interrupt
    CHECK(expander.errorCount() == 0);
}

TEST_CASE("pin objects read the shadow, so they cost no bus traffic") {
    FakeI2cBus bus;
    Mcp23017 expander(bus, FakeI2cBus::kAddress);
    McpDigitalInput button(expander, 3, /*activeHigh=*/false, /*pullUp=*/true);
    REQUIRE(expander.initialize(0));

    bus.inputPins = 0;  // pull-up holds it high... but active_high=false
    expander.onInterrupt();
    expander.refreshInputs(1000);
    const int readsAfterRefresh = bus.reads;

    // A switch pulling the pin to ground reads as "active".
    CHECK(button.read());
    CHECK(button.read());
    CHECK(button.read());
    CHECK(bus.reads == readsAfterRefresh);  // ...and none of that touched I2C
}

TEST_CASE("inputs refresh on interrupt, not on every iteration") {
    FakeI2cBus bus;
    Mcp23017 expander(bus, FakeI2cBus::kAddress);
    McpDigitalInput button(expander, 3, /*activeHigh=*/true, /*pullUp=*/false);
    REQUIRE(expander.initialize(0));
    const int baseline = bus.reads;

    // Nothing happened: a run of quiet iterations must not talk to the chip.
    for (uint32_t now = 1; now < Mcp23017::kSafetyRefreshMs; ++now) expander.refreshInputs(now);
    CHECK(bus.reads == baseline);
    CHECK_FALSE(button.read());

    // The chip pulls INT low; the next iteration picks the change up.
    bus.inputPins = 1u << 3;
    expander.onInterrupt();
    expander.refreshInputs(100);
    CHECK(bus.reads == baseline + 1);
    CHECK(button.read());
}

TEST_CASE("a missed interrupt cannot strand the shadow forever") {
    FakeI2cBus bus;
    Mcp23017 expander(bus, FakeI2cBus::kAddress);
    McpDigitalInput button(expander, 3, /*activeHigh=*/true, /*pullUp=*/false);
    REQUIRE(expander.initialize(0));
    const int baseline = bus.reads;

    // The pin changed but the edge was lost — no onInterrupt() call at all.
    bus.inputPins = 1u << 3;
    expander.refreshInputs(10);
    CHECK_FALSE(button.read());

    // The safety refresh notices anyway.
    expander.refreshInputs(Mcp23017::kSafetyRefreshMs + 10);
    CHECK(bus.reads == baseline + 1);
    CHECK(button.read());
}

TEST_CASE("outputs are coalesced into one write per iteration") {
    FakeI2cBus bus;
    Mcp23017 expander(bus, FakeI2cBus::kAddress);
    McpDigitalOutput a(expander, 0, /*activeHigh=*/true);
    McpDigitalOutput b(expander, 1, /*activeHigh=*/true);
    McpDigitalOutput c(expander, 2, /*activeHigh=*/false);  // active-low relay board
    REQUIRE(expander.initialize(0));
    const int baseline = bus.writes;

    a.write(true);
    b.write(true);
    c.write(true);
    CHECK(bus.writes == baseline);  // nothing on the bus yet

    expander.flushOutputs();
    CHECK(bus.writes == baseline + 1);  // three relays, one transfer
    // Active-low pin 2 is driven low when energised; the others high.
    CHECK(bus.registers[FakeI2cBus::kOlatA] == 0b011);

    expander.flushOutputs();
    CHECK(bus.writes == baseline + 1);  // nothing changed, nothing sent
}

TEST_CASE("an unchanged output writes nothing") {
    FakeI2cBus bus;
    Mcp23017 expander(bus, FakeI2cBus::kAddress);
    McpDigitalOutput relay(expander, 0, /*activeHigh=*/true);
    REQUIRE(expander.initialize(0));

    relay.write(true);
    expander.flushOutputs();
    const int writes = bus.writes;

    relay.write(true);
    expander.flushOutputs();
    CHECK(bus.writes == writes);
}

TEST_CASE("a failed write is retried on the next iteration") {
    FakeI2cBus bus;
    Mcp23017 expander(bus, FakeI2cBus::kAddress);
    McpDigitalOutput relay(expander, 0, /*activeHigh=*/true);
    REQUIRE(expander.initialize(0));

    bus.failing = true;
    relay.write(true);
    expander.flushOutputs();
    CHECK(expander.errorCount() > 0);
    CHECK(bus.registers[FakeI2cBus::kOlatA] == 0);  // never made it to the chip

    // A loose connector that comes back must not leave the relay stale.
    bus.failing = false;
    expander.flushOutputs();
    CHECK(bus.registers[FakeI2cBus::kOlatA] == 1);
}

TEST_CASE("a chip that does not answer fails initialization loudly") {
    FakeI2cBus bus;
    bus.failing = true;
    Mcp23017 expander(bus, FakeI2cBus::kAddress);
    CHECK_FALSE(expander.initialize(0));
    CHECK(expander.errorCount() > 0);
}

TEST_CASE("config: expanders are declared with an explicit index and hex address") {
    config::App app;
    std::string error;
    REQUIRE_MESSAGE(config::parse(R"({
      "schema_version": 2, "building": "utility",
      "devices": {
        "expanders": [{"index": 0, "address": "0x20", "int_pin": 6},
                      {"index": 1, "address": 33, "int_pin": 7}],
        "actuators": [{"type": "light", "id": 30, "room": "corridor",
                       "backend": {"kind": "mcp", "expander": 0, "pin": 3}}],
        "buttons": [{"id": 30, "room": "corridor", "kind": "toggle",
                     "pin": {"expander": 1, "pin": 11}}]
      },
      "bindings": []})",
                                  app, error),
                    error);

    REQUIRE(app.expanders.size() == 2);
    CHECK(app.expanders[0].address == 0x20);
    CHECK(app.expanders[0].intPin == 6);
    CHECK(app.expanders[1].address == 0x21);
    CHECK(app.actuators[0].backend == config::BackendKind::Mcp);
    CHECK(app.actuators[0].pin.expander == 0);
    CHECK(app.buttons[0].pin.expander == 1);
}

TEST_CASE("config: a pin on an undeclared expander is rejected") {
    config::App app;
    std::string error;
    CHECK_FALSE(config::parse(R"({
      "schema_version": 2, "building": "utility",
      "devices": {
        "expanders": [{"index": 0, "address": "0x20", "int_pin": 6}],
        "actuators": [{"type": "light", "id": 30, "room": "corridor",
                       "backend": {"kind": "mcp", "expander": 3, "pin": 3}}]
      },
      "bindings": []})",
                              app, error));
    CHECK(error.find("not declared") != std::string::npos);
}

TEST_CASE("config: backend and expander must agree") {
    config::App app;
    std::string error;
    // gpio backend but an expander given
    CHECK_FALSE(config::parse(R"({
      "schema_version": 2, "building": "u",
      "devices": {"expanders": [{"index": 0, "address": "0x20", "int_pin": 6}],
                  "actuators": [{"type": "light", "id": 1, "room": "r",
                                 "backend": {"kind": "gpio", "expander": 0, "pin": 3}}]},
      "bindings": []})",
                              app, error));
    CHECK(error.find("backend") != std::string::npos);

    // mcp backend without one
    CHECK_FALSE(config::parse(R"({
      "schema_version": 2, "building": "u",
      "devices": {"expanders": [{"index": 0, "address": "0x20", "int_pin": 6}],
                  "actuators": [{"type": "light", "id": 1, "room": "r",
                                 "backend": {"kind": "mcp", "pin": 3}}]},
      "bindings": []})",
                              app, error));
    CHECK(error.find("backend") != std::string::npos);
}

TEST_CASE("config: bad expander declarations are rejected") {
    config::App app;
    std::string error;
    const auto parse = [&](const char* expanders) {
        std::string json = R"({"schema_version": 2, "building": "u",
                               "devices": {"expanders": )";
        json += expanders;
        json += R"(}, "bindings": []})";
        return config::parse(json, app, error);
    };

    CHECK_FALSE(parse(R"([{"index": 0, "address": "0x30", "int_pin": 6}])"));  // out of range
    CHECK(error.find("0x20") != std::string::npos);

    CHECK_FALSE(parse(R"([{"index": 0, "int_pin": 6}])"));  // no address
    CHECK_FALSE(parse(R"([{"index": 0, "address": "0x20"}])"));  // no int pin
    CHECK_FALSE(parse(R"([{"index": 0, "address": "0x20", "int_pin": 6},
                          {"index": 0, "address": "0x21", "int_pin": 7}])"));  // duplicate index
    CHECK_FALSE(parse(R"([{"index": 0, "address": "0x20", "int_pin": 6},
                          {"index": 1, "address": "0x20", "int_pin": 7}])"));  // duplicate address
}

TEST_CASE("config: an expander pin number above 15 is rejected") {
    config::App app;
    std::string error;
    CHECK_FALSE(config::parse(R"({
      "schema_version": 2, "building": "u",
      "devices": {"expanders": [{"index": 0, "address": "0x20", "int_pin": 6}],
                  "actuators": [{"type": "light", "id": 1, "room": "r",
                                 "backend": {"kind": "mcp", "expander": 0, "pin": 16}}]},
      "bindings": []})",
                              app, error));
    CHECK(error.find("0..15") != std::string::npos);
}
