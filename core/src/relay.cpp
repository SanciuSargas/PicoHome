#include "picohome/devices/relay.h"

#include "picohome/core/commands.h"

namespace picohome {

Relay::Relay(DeviceType type, uint16_t id, std::string room, hal::IDigitalOutput& output)
    : Device(type, id, std::move(room)), output_(output), supportsNightMode_(isLight(type)) {}

void Relay::initialize() {
    output_.write(false);
    on_ = false;
}

void Relay::update(uint32_t) {}

bool Relay::handleCommand(std::string_view command, std::string_view payload) {
    const bool one = payload == "1";
    const bool zero = payload == "0";
    if (command == commands::kSetBinary) {
        if (!one && !zero) return false;
        setBinary(one);
        return true;
    }
    if (command == commands::kSetNightMode && supportsNightMode_) {
        if (!one && !zero) return false;
        setNightMode(one);
        return true;
    }
    return false;
}

void Relay::setBinary(bool on) {
    on_ = on;
    output_.write(on_);
    publish(commands::kState, on_ ? "1" : "0", /*retained=*/true);
}

void Relay::setNightMode(bool on) {
    nightMode_ = on;
    publish(commands::kNightMode, nightMode_ ? "1" : "0", /*retained=*/true);
}

// Full refresh — used on MQTT (re)connect, so both retained topics are correct
// for a subscriber that has just arrived.
void Relay::publishState() {
    publish(commands::kState, on_ ? "1" : "0", /*retained=*/true);
    if (supportsNightMode_) {
        publish(commands::kNightMode, nightMode_ ? "1" : "0", /*retained=*/true);
    }
}

}  // namespace picohome
