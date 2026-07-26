#include "picohome/net/topic.h"

#include <array>
#include <cstdio>
#include <iterator>

#include "picohome/core/commands.h"

namespace picohome {

bool buildTopic(char* buffer, std::size_t size, std::string_view building,
                std::string_view room, DeviceType type, uint16_t id,
                std::string_view command) {
    const std::string_view typeSegment = toSegment(type);
    const int written = std::snprintf(
        buffer, size, "%.*s/%.*s/%.*s/%u/%.*s",
        static_cast<int>(building.size()), building.data(),
        static_cast<int>(room.size()), room.data(),
        static_cast<int>(typeSegment.size()), typeSegment.data(),
        static_cast<unsigned>(id),
        static_cast<int>(command.size()), command.data());
    return written > 0 && static_cast<std::size_t>(written) < size;
}

std::vector<std::string> buildSubscriptions(std::string_view building,
                                            const std::vector<std::string>& remoteTopics) {
    // Command verbs this firmware accepts today; M6 adds set_position/set_tilt
    // and M10 set_range.
    constexpr std::string_view kVerbs[] = {commands::kSetBinary, commands::kSetNightMode};

    std::vector<std::string> subscriptions;
    subscriptions.reserve(std::size(kVerbs) + remoteTopics.size());
    for (const std::string_view verb : kVerbs) {
        subscriptions.emplace_back(std::string(building) + "/+/+/+/" + std::string(verb));
    }
    for (const std::string& topic : remoteTopics) subscriptions.push_back(topic);
    return subscriptions;
}

bool parseTopic(std::string_view topic, ParsedTopic& out) {
    std::array<std::string_view, 5> parts;
    std::size_t count = 0;
    std::size_t pos = 0;
    for (;;) {
        const std::size_t next = topic.find('/', pos);
        const std::string_view part = (next == std::string_view::npos)
                                          ? topic.substr(pos)
                                          : topic.substr(pos, next - pos);
        if (count == parts.size()) return false;  // more than 5 segments
        parts[count++] = part;
        if (next == std::string_view::npos) break;
        pos = next + 1;
    }
    if (count != parts.size()) return false;
    for (const auto& part : parts) {
        if (part.empty()) return false;
    }

    out.building = parts[0];
    out.room = parts[1];
    out.type = typeFromSegment(parts[2]);
    if (out.type == DeviceType::Unknown) return false;

    uint32_t id = 0;
    for (const char c : parts[3]) {
        if (c < '0' || c > '9') return false;
        id = id * 10 + static_cast<uint32_t>(c - '0');
        if (id > UINT16_MAX) return false;
    }
    out.id = static_cast<uint16_t>(id);
    out.command = parts[4];
    return true;
}

}  // namespace picohome