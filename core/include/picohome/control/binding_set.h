#pragma once

#include <memory>
#include <string>
#include <vector>

#include "picohome/config/config.h"
#include "picohome/control/binding.h"
#include "picohome/control/event_router.h"
#include "picohome/core/device_registry.h"

namespace picohome {

// Owns the bindings of one controller and runs their timers.
//
// build() resolves every source and target against the device registry and
// checks that each rule finds the capability it needs on every target. It is
// the boot-time gate that turns "dimmer rule pointed at a plain relay" into a
// clear config error instead of a surprise at 2 a.m.
class BindingSet {
public:
    bool build(const config::App& config, DeviceRegistry& registry, EventRouter& router,
               std::string& error);

    void updateAll(uint32_t nowMs);

    std::size_t size() const { return bindings_.size(); }

private:
    std::vector<std::unique_ptr<Binding>> bindings_;
};

}  // namespace picohome
