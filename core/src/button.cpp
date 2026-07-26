#include "picohome/devices/button.h"

#include "picohome/core/commands.h"

namespace picohome {

Button::Button(uint16_t id, std::string room, hal::IDigitalInput& input, SwitchKind kind)
    : Device(DeviceType::Button, id, std::move(room)),
      input_(input),
      detector_(*this, kind) {}

Button::Button(uint16_t id, std::string room, hal::IDigitalInput& input, SwitchKind kind,
               GestureDetector::Timing timing)
    : Device(DeviceType::Button, id, std::move(room)),
      input_(input),
      detector_(*this, kind, timing) {}

void Button::initialize() {
    // Adopt the switch's boot position silently. Without this a toggle switch
    // resting in the "on" position would look like a flip on the first update
    // and switch every light bound to it the moment the panel powers up.
    detector_.prime(input_.read());
}

void Button::update(uint32_t nowMs) { detector_.update(input_.read(), nowMs); }

void Button::onGesture(Gesture gesture) {
    publish(commands::kEvent, toPayload(gesture), /*retained=*/false);
    if (router_ != nullptr) {
        router_->emit(SourceEvent::fromGesture(SourceKey::button(id()), gesture));
    }
}

}  // namespace picohome
