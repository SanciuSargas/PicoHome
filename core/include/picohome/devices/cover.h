#pragma once

#include "picohome/core/capabilities.h"
#include "picohome/core/device.h"
#include "picohome/hal/hal.h"

// Blinds driven by two relays (up / down) with no position feedback, so
// position is *integrated from time*: the config calibrates how long a full
// travel takes and the device counts milliseconds of motor run.
//
// One motor drives two axes, and the physical order is what the model follows:
// **the slats rotate first, then the blind moves.** Running the motor up turns
// the slats from closed towards open and only then raises the blind; running
// down turns them back towards closed and only then lowers it. That is also why
// a short press on the wall switch nudges the slats rather than the height —
// exactly what a blind switch is expected to do — and why a blind sitting at
// the bottom opens its slats first, letting light in without raising.
//
// `tilt_ms` is simply how long the motor takes to traverse the whole angle
// range, which is the only difference between the two kinds fitted here:
//   * exterior blinds sweep 0..90 deg, so the far end is "fully open";
//   * interior blinds sweep 0..180 deg, so the slats are open halfway through
//     and closed again at the far end.
// The controller owns the mechanism (how long to run the motor); what a given
// angle *means* is left to Home Assistant.
//
// There is nothing to ask about position at boot, so it is assumed — see
// `bootOpen` — and running to either limit re-synchronises the estimate, which
// is why reaching an end is treated as a calibration point.

namespace picohome {

class Cover : public Device, public IPositionActuator {
public:
    struct Timing {
        uint32_t travelMs = 0;          // full closed -> open, required
        uint32_t tiltMs = 0;            // whole angle sweep; 0 = no tilt
        uint32_t stepMs = 500;          // press_1 nudge
        uint32_t reverseDelayMs = 500;  // motor rest before reversing
        // Blinds normally sit raised, so that is the default assumption; the
        // first gesture then lowers them.
        bool bootOpen = true;
    };

    enum class Direction : uint8_t { Down, Up };

    Cover(DeviceType type, uint16_t id, std::string room, hal::IDigitalOutput& up,
          hal::IDigitalOutput& down, Timing timing);

    void initialize() override;
    void update(uint32_t nowMs) override;
    bool handleCommand(std::string_view command, std::string_view payload) override;
    void publishState() override;

    IPositionActuator* asPosition() override { return this; }

    // IPositionActuator
    void setPosition(uint8_t percent) override;
    void setTilt(uint8_t percent) override;
    void stop() override;
    uint8_t position() const override;
    bool isMoving() const override { return moving_ || reversing_; }
    void stepAlongDirection() override;
    void toggleDirection() override;
    void moveAlongDirection() override;
    void moveToEnd() override;
    void releaseHold() override;

    uint8_t tilt() const;
    Direction direction() const { return direction_; }

private:
    enum class Target : uint8_t { None, Position, Tilt, Duration };

    void requestMove(Direction direction);
    void integrate(uint32_t elapsedMs);
    bool targetReached() const;
    void publishIfDue(bool force);

    hal::IDigitalOutput& up_;
    hal::IDigitalOutput& down_;
    Timing timing_;

    uint32_t positionMs_ = 0;  // 0 = closed (down)
    uint32_t tiltMs_ = 0;      // 0 = slats closed; the sweep opens them going up

    Direction direction_ = Direction::Down;
    Direction pendingDirection_ = Direction::Down;
    bool moving_ = false;
    bool reversing_ = false;
    uint32_t reverseStartMs_ = 0;

    Target target_ = Target::None;
    uint32_t targetPositionMs_ = 0;
    uint32_t targetTiltMs_ = 0;
    uint32_t targetEndMs_ = 0;
    // hold_long commits to the end, so letting go must not cancel it.
    bool committedToEnd_ = false;

    uint32_t nowMs_ = 0;
    uint32_t lastPublishMs_ = 0;
    bool started_ = false;
};

}  // namespace picohome
