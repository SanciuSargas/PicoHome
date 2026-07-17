#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "picohome/core/types.h"

// Typed model of the SD-card JSON config (one file per building, identical
// firmware binary everywhere). Schema is versioned from day one; the parser
// rejects versions it does not understand.

namespace picohome::config {

inline constexpr uint32_t kSupportedSchemaVersion = 1;

struct PinRef {
    int8_t expander = -1;    // -1 = controller GPIO, 0..7 = MCP23017 index
    uint8_t pin = 0;
    bool activeHigh = true;  // outputs: level that energizes; inputs: level meaning "active"
    bool pullUp = true;      // inputs only
};

struct Network {
    uint8_t mac[6] = {};
    std::string ip;
    std::string netmask;
    std::string gateway;
    std::string broker;
    uint16_t brokerPort = 1883;
};

// Relay-backed devices: light, outlet, heating_mat, damper relays, ...
struct BinaryActuator {
    DeviceType type = DeviceType::Light;
    uint16_t id = 0;
    std::string room;
    PinRef pin;
};

struct ButtonDef {
    uint16_t id = 0;
    std::string room;
    PinRef pin;
    // First-milestone binding: press_1 toggles this target (id 0 = no binding).
    DeviceType controlsType = DeviceType::Light;
    uint16_t controlsId = 0;
};

struct App {
    uint32_t schemaVersion = 0;
    std::string building;
    Network network;
    std::vector<BinaryActuator> binaryActuators;
    std::vector<ButtonDef> buttons;
};

// Parses JSON (comments allowed) into `out`. On failure returns false and
// describes the problem in `error`.
bool parse(std::string_view json, App& out, std::string& error);

}  // namespace picohome::config
