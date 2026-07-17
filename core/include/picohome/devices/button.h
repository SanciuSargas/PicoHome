#pragma once

#include "picohome/core/device.h"
#include "picohome/core/gesture.h"
#include "picohome/hal/hal.h"

namespace picohome {

class Button;

// Wiring hook for local bindings; the BindingEngine implements this.
class IButtonListener {
public:
    virtual ~IButtonListener() = default;
    virtual void onButtonGesture(const Button& button, Gesture gesture) = 0;
};

// Momentary wall switch. Publishes gestures on .../button/<id>/event and
// forwards them to the local binding engine. By convention a button inherits
// the plan number of the device it primarily controls; several physical
// switches of one P-group share the same id (they are one logical button).
class Button : public Device, private IGestureListener {
public:
    Button(uint16_t id, std::string room, hal::IDigitalInput& input,
           GestureDetector::Timing timing = GestureDetector::Timing{});

    void attachListener(IButtonListener* listener) { listener_ = listener; }

    void update(uint32_t nowMs) override;

private:
    void onGesture(Gesture gesture) override;

    hal::IDigitalInput& input_;
    GestureDetector detector_;
    IButtonListener* listener_ = nullptr;
};

}  // namespace picohome