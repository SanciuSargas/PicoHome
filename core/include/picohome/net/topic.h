#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

#include "picohome/core/types.h"

// MQTT topic scheme: <building>/<room>/<device_type>/<device_id>/<command>.
// This is the only place in the code that knows the topic layout.

namespace picohome {

struct ParsedTopic {
    std::string_view building;
    std::string_view room;
    DeviceType type = DeviceType::Unknown;
    uint16_t id = 0;
    std::string_view command;
};

// Builds a topic into `buffer`; returns false if the buffer is too small.
bool buildTopic(char* buffer, std::size_t size, std::string_view building,
                std::string_view room, DeviceType type, uint16_t id,
                std::string_view command);

// Parses an exactly-5-segment PicoHome topic; returns false on malformed input.
// The returned string_views point into `topic`.
bool parseTopic(std::string_view topic, ParsedTopic& out);

}  // namespace picohome