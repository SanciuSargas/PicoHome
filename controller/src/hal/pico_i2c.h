#pragma once

#include "picohome/hal/hal.h"

namespace picohome::pico_hal {

// Panel-local I2C bus (MCP23017 port expanders). Blocking, but every
// transaction is a couple of bytes at 400 kHz — tens of microseconds — and the
// driver above only touches the bus when something actually changed.
class PicoI2cBus final : public hal::II2cBus {
public:
    // Configures the bus pins from board_config.h.
    PicoI2cBus();

    bool writeRegister(uint8_t address, uint8_t reg, uint8_t value) override;
    bool writeRegister16(uint8_t address, uint8_t reg, uint16_t value) override;
    bool readRegister16(uint8_t address, uint8_t reg, uint16_t& value) override;
};

}  // namespace picohome::pico_hal
