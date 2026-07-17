#include "picohome/core/device.h"

namespace picohome {

Device::Device(DeviceType type, uint16_t id, std::string room)
    : type_(type), id_(id), room_(std::move(room)) {}

bool Device::handleCommand(std::string_view, std::string_view) { return false; }

void Device::publish(std::string_view command, std::string_view payload, bool retained) {
    if (publisher_ != nullptr) publisher_->publish(*this, command, payload, retained);
}

}  // namespace picohome