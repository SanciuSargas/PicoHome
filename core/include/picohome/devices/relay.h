#pragma once

#include "picohome/core/capabilities.h"
#include "picohome/core/device.h"
#include "picohome/hal/hal.h"

namespace picohome {

// Binary output device: light relay, controlled outlet, heating mat, damper
// relay, external light... The concrete DeviceType comes from the config; the
// behaviour is identical for all of them.
//
// Lights additionally carry the night_mode gate used by motion bindings; other
// types neither publish nor accept it.
class Relay : public Device, public IBinaryActuator, public IMotionGated {
public:
    Relay(DeviceType type, uint16_t id, std::string room, hal::IDigitalOutput& output);

    void initialize() override;
    void update(uint32_t nowMs) override;
    bool handleCommand(std::string_view command, std::string_view payload) override;
    void publishState() override;

    IBinaryActuator* asBinary() override { return this; }
    IMotionGated* asMotionGated() override { return supportsNightMode_ ? this : nullptr; }

    // IBinaryActuator
    void setBinary(bool on) override;
    bool isOn() const override { return on_; }

    // IMotionGated
    bool nightMode() const override { return nightMode_; }
    void setNightMode(bool on) override;

private:
    hal::IDigitalOutput& output_;
    bool on_ = false;
    bool supportsNightMode_ = false;
    bool nightMode_ = true;  // motion works before the broker ever says otherwise
};

}  // namespace picohome
