#pragma once

#include <string_view>

// MQTT command-segment vocabulary for the 5-segment topic scheme.

namespace picohome::commands {

inline constexpr std::string_view kSetBinary = "set_binary";
inline constexpr std::string_view kSetRange = "set_range";
inline constexpr std::string_view kSetPosition = "set_position";
inline constexpr std::string_view kSetTilt = "set_tilt";
inline constexpr std::string_view kState = "state";
inline constexpr std::string_view kMeasurement = "measurement";
inline constexpr std::string_view kEvent = "event";
inline constexpr std::string_view kAlive = "alive";
inline constexpr std::string_view kError = "error";

}  // namespace picohome::commands
