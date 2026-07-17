#pragma once

#include <vector>

#include "picohome/core/device_registry.h"
#include "picohome/devices/button.h"

namespace picohome {

// Local input -> actuator rules, built from config. All bindings come from the
// SD config, none are hardcoded. First milestone supports the simplest rule:
// press_1 on a button toggles a binary actuator. This grows into the full
// binding model later: cover gestures, dimmer ramps, motion bindings with
// off-delay/lux threshold, and remote MQTT button sources.
struct ToggleBinding {
    uint16_t buttonId;
    DeviceType targetType;
    uint16_t targetId;
};

class BindingEngine : public IButtonListener {
public:
    explicit BindingEngine(DeviceRegistry& registry);

    void addToggle(const ToggleBinding& binding);

    void onButtonGesture(const Button& button, Gesture gesture) override;

private:
    DeviceRegistry& registry_;
    std::vector<ToggleBinding> toggles_;
};

}  // namespace picohome