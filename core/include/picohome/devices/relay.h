#pragma once

#include "picohome/core/device.h"
#include "picohome/hal/hal.h"

namespace picohome {

// Binary output device: light relay, controlled outlet, heating mat, damper
// relay, external light... The concrete DeviceType comes from the config; the
// behaviour is identical for all of them.
class Relay : public Device {
public:
    Relay(DeviceType type, uint16_t id, std::string room, hal::IDigitalOutput& output);

    void initialize() override;
    void update(uint32_t nowMs) override;
    bool handleCommand(std::string_view command, std::string_view payload) override;
    void publishState() override;

    void set(bool on);
    void toggle();
    bool isOn() const { return on_; }

private:
    hal::IDigitalOutput& output_;
    bool on_ = false;
};

}  // namespace picohome