#pragma once

#include <cstdint>

// Single source of truth for the Modbus RTU register maps on the PicoHome bus.
// Included by the controller (master) and the room nodes (slaves) — the two
// sides can never drift apart.
//
// Bus defaults: 9600 8N1 (controller side configurable via SD config).

namespace picohome::modbus {

// Address plan per building bus.
inline constexpr uint8_t kFirstRelayModuleAddress = 1;   // commercial relay modules
inline constexpr uint8_t kLastRelayModuleAddress = 9;
inline constexpr uint8_t kFirstRoomNodeAddress = 10;     // DIY RP2040 room nodes
inline constexpr uint8_t kLastRoomNodeAddress = 99;

// Room node input registers (read with function 0x04). All values are 16-bit.
// A sensor that is not fitted on a node reports kNotFitted.
enum RoomNodeInputReg : uint16_t {
    kRegHeartbeat = 0,    // increments every second, wraps around
    kRegFirmwareVersion,  // BCD, e.g. 0x0100 = 1.0
    kRegStatusFlags,      // bit 0: sensor error; other bits reserved
    kRegTemperatureC100,  // temperature * 100, signed (2534 = 25.34 °C)
    kRegHumidityPct100,   // relative humidity * 100
    kRegCo2Ppm,           // CO2 in ppm
    kRegSmokeState,       // smoke detector: 0 clear, 1 alarm (digital output)
    kRegLightRaw,         // photoresistor, raw ADC counts 0..4095
    kRoomNodeInputRegCount,
};

inline constexpr int16_t kNotFitted = 0x7FFF;

}  // namespace picohome::modbus