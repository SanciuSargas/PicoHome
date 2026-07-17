#include "picohome/core/types.h"

namespace picohome {

namespace {

struct Entry {
    DeviceType type;
    std::string_view segment;
};

constexpr Entry kEntries[] = {
    {DeviceType::Light, "light"},
    {DeviceType::DimmableLight, "dimmable_light"},
    {DeviceType::Blinds, "blinds"},
    {DeviceType::Curtains, "curtains"},
    {DeviceType::Outlet, "outlet"},
    {DeviceType::Damper, "damper"},
    {DeviceType::HeatingMat, "heating_mat"},
    {DeviceType::Rgb, "rgb"},
    {DeviceType::Button, "button"},
    {DeviceType::Toggle, "toggle"},
    {DeviceType::Reed, "reed"},
    {DeviceType::WaterLeak, "water_leak"},
    {DeviceType::Pir, "pir"},
    {DeviceType::Presence, "presence"},
    {DeviceType::Smoke, "smoke"},
    {DeviceType::LightLevel, "light_level"},
    {DeviceType::Temperature, "temperature"},
    {DeviceType::Humidity, "humidity"},
    {DeviceType::Co2, "co2"},
    {DeviceType::Status, "status"},
};

}  // namespace

std::string_view toSegment(DeviceType type) {
    for (const auto& entry : kEntries) {
        if (entry.type == type) return entry.segment;
    }
    return "unknown";
}

DeviceType typeFromSegment(std::string_view segment) {
    for (const auto& entry : kEntries) {
        if (entry.segment == segment) return entry.type;
    }
    return DeviceType::Unknown;
}

}  // namespace picohome
