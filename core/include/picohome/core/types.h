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
    Button,  // wall switch, momentary or toggle — see SwitchKind
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

// Lights are the only devices motion bindings may drive, and the only ones
// carrying the night_mode gate.
inline bool isLight(DeviceType type) {
    return type == DeviceType::Light || type == DeviceType::DimmableLight;
}

}  // namespace picohome
