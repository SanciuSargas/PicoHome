#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "picohome/core/gesture.h"
#include "picohome/core/types.h"

// Typed model of the SD-card JSON config (one file per building, identical
// firmware binary everywhere). Two independent parts: a device inventory
// ("what exists") and a binding list ("what controls what"). No device ever
// references another device.
//
// Local bindings are deliberately limited to the behaviour that must survive a
// dead broker: wall switches driving lights/outlets/dampers/covers, and motion
// sensors driving lights. Everything richer (scenes, reed-triggered logic,
// schedules, cross-room rules) belongs to Home Assistant, which sees every
// device anyway through the universal state/command topics.

namespace picohome::config {

inline constexpr uint32_t kSupportedSchemaVersion = 2;

// How an actuator is driven. Only Gpio works today; the rest are parsed and
// rejected with an explicit "not implemented until Mx" error so a config can
// never claim behaviour the firmware does not have.
enum class BackendKind : uint8_t { Gpio, Mcp, Pwm, Cover, Modbus };

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

struct ActuatorDef {
    DeviceType type = DeviceType::Light;
    uint16_t id = 0;
    std::string room;
    BackendKind backend = BackendKind::Gpio;
    PinRef pin;
};

struct SensorDef {
    DeviceType type = DeviceType::Pir;
    uint16_t id = 0;
    std::string room;
    PinRef pin;
    uint32_t debounceMs = 50;
};

struct ButtonDef {
    uint16_t id = 0;
    std::string room;
    PinRef pin;
    // Required in the config: getting it wrong fails silently (a toggle switch
    // read as momentary never completes a press, so its light simply never
    // responds), and the house has both kinds fitted.
    SwitchKind kind = SwitchKind::Momentary;
};

struct DeviceRef {
    DeviceType type = DeviceType::Light;
    uint16_t id = 0;
};

enum class SourceType : uint8_t { Button, Sensor, Remote };

struct SourceDef {
    SourceType kind = SourceType::Button;

    uint16_t buttonId = 0;              // Button
    Gesture gesture = Gesture::Press1;  // Button (toggle rule; cover uses the whole grammar)

    DeviceRef sensor;                   // Sensor — motion rule, pir/presence only

    // Remote: a device on another controller. A button event carries one exact
    // payload; a remote sensor carries a state, so it needs both edges — which
    // default to the "1"/"0" this firmware publishes on its own state topics.
    std::string topic;
    std::string payload;                   // action semantics (toggle/cover)
    std::string payloadActive = "1";       // sensor semantics (motion)
    std::string payloadInactive = "0";
};

// toggle: button -> lights / outlets / dampers
// motion: pir+presence -> lights, gated by each light's night_mode
// cover:  button -> blinds / curtains (M6)
enum class RuleKind : uint8_t { Toggle, Motion, Cover };

struct BindingDef {
    std::string name;
    RuleKind rule = RuleKind::Toggle;
    std::vector<SourceDef> sources;
    std::vector<DeviceRef> targets;

    uint32_t offDelayMs = 0;  // motion
};

struct App {
    uint32_t schemaVersion = 0;
    std::string building;
    Network network;
    std::vector<ActuatorDef> actuators;
    std::vector<SensorDef> sensors;
    std::vector<ButtonDef> buttons;
    std::vector<BindingDef> bindings;
};

// Parses JSON (comments allowed) into `out`. On failure returns false and
// describes the problem in `error`.
bool parse(std::string_view json, App& out, std::string& error);

}  // namespace picohome::config
