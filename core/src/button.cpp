#include "picohome/devices/button.h"

#include "picohome/core/commands.h"

namespace picohome {

Button::Button(uint16_t id, std::string room, hal::IDigitalInput& input,
               GestureDetector::Timing timing)
    : Device(DeviceType::Button, id, std::move(room)),
      input_(input),
      detector_(*this, timing) {}

void Button::update(uint32_t nowMs) { detector_.update(input_.read(), nowMs); }

void Button::onGesture(Gesture gesture) {
    publish(commands::kEvent, toPayload(gesture), /*retained=*/false);
    if (listener_ != nullptr) listener_->onButtonGesture(*this, gesture);
}

}  // namespace picohome