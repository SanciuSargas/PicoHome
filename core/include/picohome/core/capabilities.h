#pragma once

#include <cstdint>

// Capability interfaces. Binding rules address their targets through these
// rather than through concrete device classes, so one rule drives a GPIO relay,
// a Modbus relay channel or a dimmer without knowing the difference.
//
// Devices opt in by overriding Device::asBinary()/asRange()/asPosition(), which
// gives a safe, allocation-free capability query without RTTI (disabled on the
// target) and lets the boot-time validator reject "dimmer rule on a plain
// relay" before the control loop ever starts.

namespace picohome {

// On/off devices: light, outlet, heating mat, damper relay, Modbus relay channel.
class IBinaryActuator {
public:
    virtual ~IBinaryActuator() = default;
    virtual void setBinary(bool on) = 0;
    virtual bool isOn() const = 0;
    void toggle() { setBinary(!isOn()); }
};

// Continuously variable devices: dimmable lights (0..100 %).
class IRangeActuator {
public:
    virtual ~IRangeActuator() = default;
    virtual void setRange(uint8_t percent) = 0;
    virtual uint8_t range() const = 0;
};

// Lights that motion bindings may switch. `night_mode` is the gate: motion only
// turns a light on while its night_mode is set. The flag is owned by the light
// and driven from Home Assistant over MQTT (set_night_mode / retained
// night_mode), which is the right place to decide what "night" means — sunset
// times, lux sensors, house mode — while the controller keeps the mechanism.
//
// It defaults to enabled so a controller that has never seen the broker still
// lights corridors on motion.
class IMotionGated {
public:
    virtual ~IMotionGated() = default;
    virtual bool nightMode() const = 0;
    virtual void setNightMode(bool on) = 0;
};

// Covers: blinds and curtains (position 0..100 %, optional tilt).
// The cover gesture grammar is added on top of this with M6.
class IPositionActuator {
public:
    virtual ~IPositionActuator() = default;
    virtual void setPosition(uint8_t percent) = 0;
    virtual void setTilt(uint8_t percent) = 0;
    virtual void stop() = 0;
};

}  // namespace picohome
