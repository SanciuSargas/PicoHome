#pragma once

#include "picohome/control/event_router.h"
#include "picohome/core/device.h"
#include "picohome/hal/hal.h"

namespace picohome {

// Debounced on/off sensor: reed contact, water leak probe, smoke detector, PIR,
// LD2410C presence output. One class covers all five because they differ only
// in how quickly their signal settles — see config::defaultDebounceMs. As with
// Relay, the DeviceType comes from config and decides the MQTT topic; the
// behaviour is identical: publish retained state on change, and emit a
// SensorEdge event so a motion binding can react.
//
// Boot is deliberately silent: initialize() adopts whatever the pin reads and
// publishes it, but emits no event. A PIR that happens to read active at
// power-up would otherwise switch lights on before the retained set_night_mode
// has arrived from Home Assistant — a house lit at noon after a power cut. This
// is the same reasoning as priming a rocker switch to its resting position.
class BinarySensor : public Device {
public:
    BinarySensor(DeviceType type, uint16_t id, std::string room, hal::IDigitalInput& input,
                 uint32_t debounceMs);

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
