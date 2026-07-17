#pragma once

#include <memory>
#include <vector>

#include "picohome/core/device.h"

namespace picohome {

// Owns all devices of one controller. Populated once at boot from the config;
// nothing is added or removed afterwards (no-heap-after-init rule).
class DeviceRegistry {
public:
    Device* add(std::unique_ptr<Device> device);
    Device* find(DeviceType type, uint16_t id);

    void initializeAll();
    void updateAll(uint32_t nowMs);
    void attachPublisher(IPublisher* publisher);
    void publishAllStates();

    std::size_t size() const { return devices_.size(); }

private:
    std::vector<std::unique_ptr<Device>> devices_;
};

}  // namespace picohome