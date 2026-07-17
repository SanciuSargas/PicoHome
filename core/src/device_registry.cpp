#include "picohome/core/device_registry.h"

namespace picohome {

Device* DeviceRegistry::add(std::unique_ptr<Device> device) {
    devices_.push_back(std::move(device));
    return devices_.back().get();
}

Device* DeviceRegistry::find(DeviceType type, uint16_t id) {
    for (auto& device : devices_) {
        if (device->type() == type && device->id() == id) return device.get();
    }
    return nullptr;
}

void DeviceRegistry::initializeAll() {
    for (auto& device : devices_) device->initialize();
}

void DeviceRegistry::updateAll(uint32_t nowMs) {
    for (auto& device : devices_) device->update(nowMs);
}

void DeviceRegistry::attachPublisher(IPublisher* publisher) {
    for (auto& device : devices_) device->attachPublisher(publisher);
}

void DeviceRegistry::publishAllStates() {
    for (auto& device : devices_) device->publishState();
}

}  // namespace picohome