#pragma once

#include <cstddef>
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

// Longest broker credentials accepted. Not an MQTT limit (the protocol allows
// 65535) but a packet-size one: the CONNECT has to fit the controller's send
// buffer alongside the client id and the will topic, and a config error at boot
// is far easier to diagnose than a connect that mysteriously never completes.
inline constexpr std::size_t kMaxCredentialLen = 128;

struct Network {
    uint8_t mac[6] = {};
    std::string ip;
    std::string netmask;
    std::string gateway;
    std::string broker;
    uint16_t brokerPort = 1883;

    // Broker login. Both empty = connect anonymously, which is what a broker
    // without `allow_anonymous false` expects. MQTT sends these in the clear
    // and they sit in the clear on the SD card — see the note in
    // docs/config_reference.md before deciding what account to use.
    std::string username;
    std::string password;
};

// One MCP23017 on the panel's I2C bus. `index` is what a PinRef.expander
// refers to; it is written out rather than implied by list order so that
// reordering the list cannot silently remap every pin in the building.
struct ExpanderDef {
    uint8_t index = 0;
    uint8_t address = 0x20;  // 0x20..0x27, set by the chip's A0..A2 pins
    uint8_t intPin = 0;      // controller GPIO the chip's INT is wired to
};

// Timing calibration for a cover, all in milliseconds of motor run — there is
// no position feedback, so these numbers *are* the position scale.
struct CoverTiming {
    // A blind falls faster than it climbs, so both directions are measured.
    uint32_t travelUpMs = 0;        // full closed -> open, required
    uint32_t travelDownMs = 0;      // full open -> closed, required
    uint32_t tiltMs = 0;            // whole slat sweep (0..90 deg outside, 0..180 inside)
    uint32_t stepMs = 500;          // press_1 nudge
    uint32_t motorRestMs = 500;  // motor rest before reversing
    bool bootOpen = true;           // where to assume it is at power-up
    bool homeAtBoot = false;        // or: drive to the top once and know instead
};

struct ActuatorDef {
    DeviceType type = DeviceType::Light;
    uint16_t id = 0;
    std::string room;
    BackendKind backend = BackendKind::Gpio;
    PinRef pin;       // the single pin, or a cover's "up" pin
    PinRef pinDown;   // cover only
    CoverTiming cover;
};

// How long a binary sensor's pin must hold its new level before the change is
// believed. This number is the entire difference between the sensor types, so
// it is worth choosing per type rather than defaulting them all alike.
uint32_t defaultDebounceMs(DeviceType type);

struct SensorDef {
    DeviceType type = DeviceType::Pir;
    uint16_t id = 0;
    std::string room;
    PinRef pin;
    uint32_t debounceMs = 0;  // the parser fills in defaultDebounceMs(type)
};

struct ButtonDef {
    uint16_t id = 0;
    std::string room;
    PinRef pin;
    // Required in the config: getting it wrong fails silently (a toggle switch
    // read as momentary never completes a press, so its light simply never
    // responds), and the house has both kinds fitted.
    SwitchKind kind = SwitchKind::Momentary;

    // Defaults to Instant, because most switches in the house are plain light
    // switches and the multi-press window is felt as lag on every single press.
    // A switch that needs press_2/press_3/hold — covers, dimmers, or anything
    // Home Assistant wants multi-press from — declares `"gestures": true`, and
    // a binding asking for what an Instant switch cannot produce is refused at
    // boot rather than never firing.
    GestureMode gestures = GestureMode::Instant;
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
    std::vector<ExpanderDef> expanders;
    std::vector<ActuatorDef> actuators;
    std::vector<SensorDef> sensors;
    std::vector<ButtonDef> buttons;
    std::vector<BindingDef> bindings;
};

// Parses JSON (comments allowed) into `out`. On failure returns false and
// describes the problem in `error`.
bool parse(std::string_view json, App& out, std::string& error);

// Overlays broker credentials from a second, separate file:
//
//   {"network": {"username": "utility_panel", "password": "..."}}
//
// This exists so the interesting half of the config — the device inventory and
// the bindings, which is what is worth version control and review — can be
// committed, while the password stays out of the repository. The file is
// optional and only the keys it names are replaced, so a controller with no
// secrets file behaves exactly as before.
bool parseSecrets(std::string_view json, Network& out, std::string& error);

}  // namespace picohome::config
