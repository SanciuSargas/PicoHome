#include "picohome/devices/relay.h"

#include "picohome/core/commands.h"

namespace picohome {

Relay::Relay(DeviceType type, uint16_t id, std::string room, hal::IDigitalOutput& output)
    : Device(type, id, std::move(room)), output_(output) {}

void Relay::initialize() {
    output_.write(false);
    on_ = false;
}

void Relay::update(uint32_t) {}

bool Relay::handleCommand(std::string_view command, std::string_view payload) {
    if (command != commands::kSetBinary) return false;
    if (payload == "1") {
        set(true);
    } else if (payload == "0") {
        set(false);
    } else {
        return false;
    }
    return true;
}

void Relay::set(bool on) {
    on_ = on;
    output_.write(on_);
    publishState();
}

void Relay::toggle() { set(!on_); }

void Relay::publishState() {
    publish(commands::kState, on_ ? "1" : "0", /*retained=*/true);
}

}  // namespace picohome