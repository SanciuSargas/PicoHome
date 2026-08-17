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

// An on/off contact read from a single pin: one state, published retained.
// These are the only types `devices.sensors` accepts. Smoke detectors joined
// this list on 2026-08-09, when the ones being fitted turned out to have a
// digital output rather than the analog one first planned — which makes them,
// to the firmware, indistinguishable from a reed contact.
inline bool isBinarySensor(DeviceType type) {
    return type == DeviceType::Reed || type == DeviceType::WaterLeak || type == DeviceType::Pir ||
           type == DeviceType::Presence || type == DeviceType::Smoke;
}

// Sensors reporting a number rather than a state. The wiring routes these to
// room nodes, so they reach the controller over Modbus rather than from a panel
// pin — see M9.
inline bool isMeasurement(DeviceType type) {
    return type == DeviceType::LightLevel || type == DeviceType::Temperature ||
           type == DeviceType::Humidity || type == DeviceType::Co2;
}

// The two ways occupancy is sensed, and the only sensors a local binding may
// observe. A reed or water-leak contact publishes its state for Home Assistant
// and stops there — triggering anything from it is high-level business.
inline bool isOccupancySensor(DeviceType type) {
    return type == DeviceType::Pir || type == DeviceType::Presence;
}

}  // namespace picohome
