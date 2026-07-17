#include "picohome/control/binding_engine.h"

#include "picohome/devices/relay.h"

namespace picohome {

BindingEngine::BindingEngine(DeviceRegistry& registry) : registry_(registry) {}

void BindingEngine::addToggle(const ToggleBinding& binding) {
    toggles_.push_back(binding);
}

void BindingEngine::onButtonGesture(const Button& button, Gesture gesture) {
    if (gesture != Gesture::Press1) return;  // richer grammar lands with covers/dimmers
    for (const auto& binding : toggles_) {
        if (binding.buttonId != button.id()) continue;
        Device* target = registry_.find(binding.targetType, binding.targetId);
        if (target == nullptr) continue;
        // Factory invariant: binary device types are always Relay-backed
        // (static_cast because firmware builds run without RTTI).
        static_cast<Relay*>(target)->toggle();
    }
}

}  // namespace picohome