#include "picohome/devices/binary_sensor.h"

#include "picohome/core/commands.h"

namespace picohome {

BinarySensor::BinarySensor(DeviceType type, uint16_t id, std::string room,
                           hal::IDigitalInput& input, uint32_t debounceMs)
    : Device(type, id, std::move(room)), input_(input), debounceMs_(debounceMs) {}

void BinarySensor::initialize() {
    stable_ = input_.read();
    rawLast_ = stable_;
    publishState();
}

void BinarySensor::update(uint32_t nowMs) {
    const bool raw = input_.read();
    if (raw != rawLast_) {
        rawLast_ = raw;
        rawChangeMs_ = nowMs;
    }
    // A sensor with nothing to debounce — a PIR or presence module drives a
    // clean logic output — must act in this same iteration: that latency is the
    // one thing motion lighting is judged on.
    if (raw == stable_ || nowMs - rawChangeMs_ < debounceMs_) return;

    stable_ = raw;
    publishState();
    if (router_ != nullptr) {
        router_->emit(SourceEvent::fromSensor(SourceKey::sensor(type(), id()), stable_));
    }
}

void BinarySensor::publishState() {
    publish(commands::kState, stable_ ? "1" : "0", /*retained=*/true);
}

}  // namespace picohome
