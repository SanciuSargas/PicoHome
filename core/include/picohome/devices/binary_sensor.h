#pragma once

#include "picohome/control/event_router.h"
#include "picohome/core/device.h"
#include "picohome/hal/hal.h"

namespace picohome {

// Debounced on/off sensor: reed contact, water leak detector, PIR, LD2410C
// presence output. As with Relay, the DeviceType comes from config — the
// behaviour is the same for all of them: publish retained state on change and
// emit a SensorEdge event so bindings can react.
//
// M7 adds the sensor-specific extras (per-type debounce defaults, LD2410C
// handling); the class itself is already the shape those need.
class BinarySensor : public Device {
public:
    BinarySensor(DeviceType type, uint16_t id, std::string room, hal::IDigitalInput& input,
                 uint32_t debounceMs = 50);

    void attachRouter(EventRouter* router) { router_ = router; }

    void initialize() override;
    void update(uint32_t nowMs) override;
    void publishState() override;

    bool isActive() const { return stable_; }

private:
    hal::IDigitalInput& input_;
    EventRouter* router_ = nullptr;
    uint32_t debounceMs_;

    bool rawLast_ = false;
    uint32_t rawChangeMs_ = 0;
    bool stable_ = false;
};

}  // namespace picohome
