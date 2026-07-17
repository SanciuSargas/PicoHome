#pragma once

#include "picohome/config/config.h"
#include "picohome/hal/hal.h"

// pico-sdk implementations of the HAL interfaces for controller-local GPIO.
// MCP23017-backed pins land here later behind the same interfaces.

namespace picohome::pico_hal {

class GpioDigitalOutput final : public hal::IDigitalOutput {
public:
    GpioDigitalOutput(uint8_t gpio, bool activeHigh);
    void write(bool active) override;

private:
    uint8_t gpio_;
    bool activeHigh_;
};

class GpioDigitalInput final : public hal::IDigitalInput {
public:
    GpioDigitalInput(uint8_t gpio, bool activeHigh, bool pullUp);
    bool read() override;

private:
    uint8_t gpio_;
    bool activeHigh_;
};

class PicoClock final : public hal::IClock {
public:
    uint32_t nowMs() override;
};

}  // namespace picohome::pico_hal