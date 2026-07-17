#pragma once

#include <cstdint>
#include <string_view>

namespace picohome {

// Device types as they appear in the MQTT topic type segment.
enum class DeviceType : uint8_t {
    Light,
    DimmableLight,
    Blinds,
    Curtains,
    Outlet,
    Damper,
    HeatingMat,
    Rgb,
    Button,
    Toggle,
    Reed,
    WaterLeak,
    Pir,
    Presence,
    Smoke,
    LightLevel,
    Temperature,
    Humidity,
    Co2,
    Status,
    Unknown,
};

std::string_view toSegment(DeviceType type);
DeviceType typeFromSegment(std::string_view segment);

}  // namespace picohome
