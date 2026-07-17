#include "hal/pico_pins.h"

#include "hardware/gpio.h"
#include "pico/time.h"

namespace picohome::pico_hal {

GpioDigitalOutput::GpioDigitalOutput(uint8_t gpio, bool activeHigh)
    : gpio_(gpio), activeHigh_(activeHigh) {
    gpio_init(gpio_);
    gpio_set_dir(gpio_, GPIO_OUT);
    write(false);
}

void GpioDigitalOutput::write(bool active) { gpio_put(gpio_, active == activeHigh_); }

GpioDigitalInput::GpioDigitalInput(uint8_t gpio, bool activeHigh, bool pullUp)
    : gpio_(gpio), activeHigh_(activeHigh) {
    gpio_init(gpio_);
    gpio_set_dir(gpio_, GPIO_IN);
    if (pullUp) {
        gpio_pull_up(gpio_);
    } else {
        gpio_pull_down(gpio_);
    }
}

bool GpioDigitalInput::read() { return gpio_get(gpio_) == activeHigh_; }

uint32_t PicoClock::nowMs() { return to_ms_since_boot(get_absolute_time()); }

}  // namespace picohome::pico_hal