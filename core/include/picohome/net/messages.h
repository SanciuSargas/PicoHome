#pragma once

#include <cstddef>
#include <cstdint>

#include "picohome/control/source_event.h"
#include "picohome/core/types.h"

// Messages crossing between the two cores. Both are plain PODs with no
// pointers, because the pico-sdk inter-core queue copies them byte for byte
// and the receiving core must not chase memory owned by the sender.

namespace picohome {

inline constexpr std::size_t kMaxTopicLen = 96;
inline constexpr std::size_t kMaxPayloadLen = 64;
inline constexpr std::size_t kMaxCommandLen = 24;

// Core 0 -> core 1: something to publish. The topic is assembled on core 0
// (a cheap snprintf) so core 1 only has to move bytes onto the wire.
struct PublishMessage {
    char topic[kMaxTopicLen] = {};
    char payload[kMaxPayloadLen] = {};
    bool retained = false;
    uint8_t qos = 1;
};

enum class InboundKind : uint8_t {
    DeviceCommand,     // a set_* addressed to one of this controller's devices
    RemoteEvent,       // a configured remote binding source fired
    PublishAllStates,  // (re)connected: refresh every retained state
};

// Core 1 -> core 0. Core 1 parses the topic (it has the time to) but never
// touches a Device: only core 0 may drive devices.
struct InboundMessage {
    InboundKind kind = InboundKind::DeviceCommand;
    DeviceType deviceType = DeviceType::Unknown;
    uint16_t deviceId = 0;
    uint16_t remoteIndex = 0;
    // What the remote source reported: a gesture, or a sensor edge. Carried so
    // a remote sensor drives a motion rule exactly like a local one.
    EventKind remoteKind = EventKind::Gesture;
    int32_t remoteValue = 0;
    char command[kMaxCommandLen] = {};
    char payload[kMaxPayloadLen] = {};

    static InboundMessage publishAllStates() {
        InboundMessage message;
        message.kind = InboundKind::PublishAllStates;
        return message;
    }
    static InboundMessage remoteEvent(uint16_t index, EventKind eventKind, int32_t value) {
        InboundMessage message;
        message.kind = InboundKind::RemoteEvent;
        message.remoteIndex = index;
        message.remoteKind = eventKind;
        message.remoteValue = value;
        return message;
    }
};

}  // namespace picohome
