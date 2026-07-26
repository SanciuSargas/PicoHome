#pragma once

#include "picohome/control/event_router.h"
#include "picohome/core/device.h"
#include "picohome/core/gesture.h"
#include "picohome/hal/hal.h"

namespace picohome {

// Wall switch — momentary pushbutton or latching toggle, per config (see
// SwitchKind). Publishes gestures on .../button/<id>/event and emits them into
// the EventRouter, where bindings pick them up. The button knows nothing about
// what it controls, and a binding does not care which kind of switch produced a
// press: that is what lets the two be swapped on a plain light.
//
// By convention a button carries the plan number of the device it primarily
// controls; several physical switches of one P-group share the same id, because
// they are one logical button in several places.
class Button : public Device, private IGestureListener {
public:
    Button(uint16_t id, std::string room, hal::IDigitalInput& input,
           SwitchKind kind = SwitchKind::Momentary);
    Button(uint16_t id, std::string room, hal::IDigitalInput& input, SwitchKind kind,
           GestureDetector::Timing timing);

    void attachRouter(EventRouter* router) { router_ = router; }

    SwitchKind switchKind() const { return detector_.kind(); }

    void initialize() override;
    void update(uint32_t nowMs) override;

private:
    void onGesture(Gesture gesture) override;

    hal::IDigitalInput& input_;
    GestureDetector detector_;
    EventRouter* router_ = nullptr;
};

}  // namespace picohome
