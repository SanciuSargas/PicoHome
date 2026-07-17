#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include "picohome/core/types.h"

namespace picohome {

class Device;

// Sink for everything a device tells the outside world; the MQTT layer
// implements this on core 1. Null publisher = device works locally only.
class IPublisher {
public:
    virtual ~IPublisher() = default;
    virtual void publish(const Device& device, std::string_view command,
                         std::string_view payload, bool retained) = 0;
};

// Base class for every field device. Universal rule: every device
// publishes its state and listens for control — even devices that only
// participate in local bindings are fully observable/controllable over MQTT.
class Device {
public:
    Device(DeviceType type, uint16_t id, std::string room);
    virtual ~Device() = default;

    DeviceType type() const { return type_; }
    uint16_t id() const { return id_; }
    std::string_view room() const { return room_; }

    void attachPublisher(IPublisher* publisher) { publisher_ = publisher; }

    virtual void initialize() {}

    // Called every loop iteration on the owning core. Must never block.
    virtual void update(uint32_t nowMs) = 0;

    // MQTT command addressed to this device. Returns true if handled.
    virtual bool handleCommand(std::string_view command, std::string_view payload);

    // Re-publish current retained state (e.g. after broker reconnect).
    virtual void publishState() {}

protected:
    void publish(std::string_view command, std::string_view payload, bool retained);

private:
    DeviceType type_;
    uint16_t id_;
    std::string room_;
    IPublisher* publisher_ = nullptr;
};

}  // namespace picohome