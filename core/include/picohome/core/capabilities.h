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

// Covers: blinds and curtains. Position and tilt are 0..100 %, 0 = closed.
//
// Beyond the absolute commands, a cover carries the wall-switch grammar the old
// Arduino shade code established: it remembers which way it is "currently
// going", so a switch with one button can drive a two-direction motor.
class IPositionActuator {
public:
    virtual ~IPositionActuator() = default;

    // Absolute commands (MQTT / high level).
    virtual void setPosition(uint8_t percent) = 0;
    virtual void setTilt(uint8_t percent) = 0;
    virtual void stop() = 0;
    virtual uint8_t position() const = 0;
    virtual bool isMoving() const = 0;

    // Wall-switch grammar (the cover rule).
    virtual void stepAlongDirection() = 0;   // press_1 while idle: nudge
    virtual void toggleDirection() = 0;      // press_2: reverse
    virtual void moveAlongDirection() = 0;   // hold: move while held
    virtual void moveToEnd() = 0;            // hold_long: commit to the end
    virtual void releaseHold() = 0;          // hold released
};

}  // namespace picohome
