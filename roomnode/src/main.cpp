// PicoHome room node — placeholder firmware.
// Becomes a Modbus RTU slave (nanomodbus) on an RP2040-Zero, reading I2C
// sensors (temperature/humidity/CO2), the photoresistor's analog input and the
// smoke detector's digital one, exposing them through the register map in
// shared/.

#include "pico/stdlib.h"

#include "picohome/modbus_regmap.h"

static_assert(picohome::modbus::kRoomNodeInputRegCount > 0,
              "shared register map must be visible to the room node");

int main() {
    stdio_init_all();

    // RP2040-Zero has a WS2812 on GPIO16 instead of a plain LED; GPIO25 blink
    // works on Pico-form boards used on the bench. TODO: board definition.
    constexpr uint kLed = 25;
    gpio_init(kLed);
    gpio_set_dir(kLed, GPIO_OUT);

    while (true) {
        gpio_put(kLed, true);
        sleep_ms(1000);
        gpio_put(kLed, false);
        sleep_ms(1000);
    }
}
