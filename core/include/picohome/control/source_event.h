#pragma once

#include <cstdint>

#include "picohome/core/gesture.h"
#include "picohome/core/types.h"

// One uniform event type for every signal source in the system: a wall switch
// gesture, a sensor edge and a remote MQTT message all arrive as a SourceEvent,
// so a binding rule never branches on where a signal came from. That uniformity
// is what makes a local button and a button on another panel interchangeable.

namespace picohome {

enum class SourceKind : uint8_t {
    Button,  // local wall switch, identified by button id
    Sensor,  // local sensor, identified by (type, id)
    Remote,  // MQTT topic+payload pair, identified by its index in the boot-built table
};

// Identity of a signal source. Cheap to compare, no strings at runtime:
// remote sources are interned into a table at boot and referenced by index.
struct SourceKey {
    SourceKind kind = SourceKind::Button;
    DeviceType type = DeviceType::Unknown;  // Sensor only
    uint16_t id = 0;                        // button/sensor id, or remote index

    static SourceKey button(uint16_t buttonId) {
        return {SourceKind::Button, DeviceType::Unknown, buttonId};
    }
    static SourceKey sensor(DeviceType sensorType, uint16_t sensorId) {
        return {SourceKind::Sensor, sensorType, sensorId};
    }
    static SourceKey remote(uint16_t index) {
        return {SourceKind::Remote, DeviceType::Unknown, index};
    }

    bool operator==(const SourceKey& other) const {
        return kind == other.kind && type == other.type && id == other.id;
    }
};

enum class EventKind : uint8_t {
    Gesture,     // value is a Gesture
    SensorEdge,  // value is 1 (became active) or 0 (became inactive)
};

struct SourceEvent {
    SourceKey key;
    EventKind kind = EventKind::Gesture;
    int32_t value = 0;

    Gesture gesture() const { return static_cast<Gesture>(value); }
    bool active() const { return value != 0; }

    static SourceEvent fromGesture(SourceKey key, Gesture gesture) {
        return {key, EventKind::Gesture, static_cast<int32_t>(gesture)};
    }
    static SourceEvent fromSensor(SourceKey key, bool active) {
        return {key, EventKind::SensorEdge, active ? 1 : 0};
    }
};

}  // namespace picohome
