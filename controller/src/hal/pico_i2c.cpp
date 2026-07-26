#include "hal/pico_i2c.h"

#include "hardware/gpio.h"
#include "hardware/i2c.h"

#include "board_config.h"

namespace picohome::pico_hal {

namespace {

// A stuck bus must not hang the control loop, so every transfer is bounded.
constexpr uint kTimeoutUs = 2000;

}  // namespace

PicoI2cBus::PicoI2cBus() {
    i2c_init(PICOHOME_I2C_INSTANCE, PICOHOME_I2C_BAUD);
    gpio_set_function(PICOHOME_I2C_SDA_GPIO, GPIO_FUNC_I2C);
    gpio_set_function(PICOHOME_I2C_SCL_GPIO, GPIO_FUNC_I2C);
    gpio_pull_up(PICOHOME_I2C_SDA_GPIO);
    gpio_pull_up(PICOHOME_I2C_SCL_GPIO);
}

bool PicoI2cBus::writeRegister(uint8_t address, uint8_t reg, uint8_t value) {
    const uint8_t payload[2] = {reg, value};
    return i2c_write_timeout_us(PICOHOME_I2C_INSTANCE, address, payload, sizeof(payload), false,
                                kTimeoutUs) == static_cast<int>(sizeof(payload));
}

bool PicoI2cBus::writeRegister16(uint8_t address, uint8_t reg, uint16_t value) {
    // Port A in the low byte, port B in the high byte: with IOCON.BANK = 0 the
    // A and B registers are adjacent, so one transfer covers all 16 pins.
    const uint8_t payload[3] = {reg, static_cast<uint8_t>(value & 0xFF),
                                static_cast<uint8_t>(value >> 8)};
    return i2c_write_timeout_us(PICOHOME_I2C_INSTANCE, address, payload, sizeof(payload), false,
                                kTimeoutUs) == static_cast<int>(sizeof(payload));
}

bool PicoI2cBus::readRegister16(uint8_t address, uint8_t reg, uint16_t& value) {
    if (i2c_write_timeout_us(PICOHOME_I2C_INSTANCE, address, &reg, 1, true, kTimeoutUs) != 1) {
        return false;
    }
    uint8_t payload[2] = {};
    if (i2c_read_timeout_us(PICOHOME_I2C_INSTANCE, address, payload, sizeof(payload), false,
                            kTimeoutUs) != static_cast<int>(sizeof(payload))) {
        return false;
    }
    value = static_cast<uint16_t>(payload[0] | (payload[1] << 8));
    return true;
}

}  // namespace picohome::pico_hal
