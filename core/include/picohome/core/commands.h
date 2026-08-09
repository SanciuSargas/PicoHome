#pragma once

#include <string_view>

// MQTT command-segment vocabulary for the 5-segment topic scheme.

namespace picohome::commands {

inline constexpr std::string_view kSetBinary = "set_binary";
inline constexpr std::string_view kSetRange = "set_range";
// Lights only: high level decides when motion may switch a light on.
inline constexpr std::string_view kSetNightMode = "set_night_mode";
inline constexpr std::string_view kNightMode = "night_mode";
inline constexpr std::string_view kSetPosition = "set_position";
inline constexpr std::string_view kSetTilt = "set_tilt";
inline constexpr std::string_view kStop = "stop";   // covers: halt where you are
inline constexpr std::string_view kTilt = "tilt";   // covers: retained slat angle
inline constexpr std::string_view kState = "state";
inline constexpr std::string_view kMeasurement = "measurement";
inline constexpr std::string_view kEvent = "event";
inline constexpr std::string_view kAlive = "alive";
inline constexpr std::string_view kError = "error";

// The verbs a controller accepts. One list, used both to build the MQTT
// subscriptions and to recognise an inbound message — keeping them apart is
// how `stop` came to be subscribed to and then silently discarded, because it
// is the one command that does not start with "set_".
inline constexpr std::string_view kCommandVerbs[] = {
    kSetBinary, kSetNightMode, kSetPosition, kSetTilt, kStop,
};

inline bool isCommandVerb(std::string_view command) {
    for (const std::string_view verb : kCommandVerbs) {
        if (verb == command) return true;
    }
    return false;
}

}  // namespace picohome::commands
