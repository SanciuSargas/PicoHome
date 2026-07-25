// PicoHome building controller.
// Boots, loads /config.json from the SD card, builds the device registry and
// runs the core-0 control loop: buttons -> bindings -> relays.
//
// Next milestones:
//  - MCP23017 expander pins behind the HAL interfaces
//  - W5100 Ethernet + MQTT session on core 1 (real IPublisher + CommandRouter;
//    boot errors held in gBootError get published retained on status/0/error)
//  - Modbus RTU master (nanomodbus) on core 1

#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include "pico/stdlib.h"

#include "board_config.h"
#include "hal/pico_pins.h"
#include "picohome/config/config.h"
#include "picohome/control/binding_engine.h"
#include "picohome/core/device_registry.h"
#include "picohome/devices/button.h"
#include "picohome/devices/relay.h"
#include "sd_config_loader.h"

namespace {

// Boot error retained for publishing once MQTT exists (M4).
std::string gBootError;

// Pin objects are owned here; devices hold references (both live forever).
std::vector<std::unique_ptr<picohome::hal::IDigitalOutput>> gOutputs;
std::vector<std::unique_ptr<picohome::hal::IDigitalInput>> gInputs;

void buildDevices(const picohome::config::App& config,
                  picohome::DeviceRegistry& registry,
                  picohome::BindingEngine& bindings) {
    using namespace picohome;

    for (const auto& def : config.binaryActuators) {
        // TODO: def.pin.expander >= 0 -> MCP23017-backed pin (M3)
        auto output = std::make_unique<pico_hal::GpioDigitalOutput>(def.pin.pin, def.pin.activeHigh);
        registry.add(std::make_unique<Relay>(def.type, def.id, def.room, *output));
        gOutputs.push_back(std::move(output));
    }

    for (const auto& def : config.buttons) {
        auto input = std::make_unique<pico_hal::GpioDigitalInput>(def.pin.pin, def.pin.activeHigh,
                                                                  def.pin.pullUp);
        auto button = std::make_unique<Button>(def.id, def.room, *input);
        button->attachListener(&bindings);
        registry.add(std::move(button));
        gInputs.push_back(std::move(input));
        if (def.controlsId != 0) {
            bindings.addToggle({def.id, def.controlsType, def.controlsId});
        }
    }
}

// Config unusable: fast blink forever, repeat the reason on USB serial.
[[noreturn]] void errorLoop(const std::string& reason) {
    gBootError = reason;
    uint32_t lastPrintMs = 0;
    bool ledOn = false;
    while (true) {
        const uint32_t now = to_ms_since_boot(get_absolute_time());
        if (now - lastPrintMs >= 2000) {
            lastPrintMs = now;
            printf("CONFIG ERROR: %s\n", reason.c_str());
        }
        ledOn = !ledOn;
        gpio_put(PICOHOME_HEARTBEAT_LED_GPIO, ledOn);
        sleep_ms(100);
    }
}

}  // namespace

int main() {
    stdio_init_all();

    gpio_init(PICOHOME_HEARTBEAT_LED_GPIO);
    gpio_set_dir(PICOHOME_HEARTBEAT_LED_GPIO, GPIO_OUT);

    // Give USB serial a moment to enumerate so early boot messages are visible.
    sleep_ms(1500);
    printf("PicoHome controller booting, reading %s\n", PICOHOME_CONFIG_PATH);

    const auto sdResult = picohome::pico_hal::readFileFromSd(PICOHOME_CONFIG_PATH);
    if (!sdResult.ok) {
        errorLoop("SD: " + sdResult.error);
    }

    picohome::config::App config;
    std::string parseError;
    if (!picohome::config::parse(sdResult.content, config, parseError)) {
        errorLoop("config parse: " + parseError);
    }

    picohome::DeviceRegistry registry;
    picohome::BindingEngine bindings(registry);
    buildDevices(config, registry, bindings);
    registry.initializeAll();

    printf("PicoHome '%s' up, %u devices (config %u bytes)\n", config.building.c_str(),
           static_cast<unsigned>(registry.size()),
           static_cast<unsigned>(sdResult.content.size()));

    picohome::pico_hal::PicoClock clock;
    uint32_t lastBlinkMs = 0;
    bool ledOn = false;

    while (true) {
        const uint32_t now = clock.nowMs();
        registry.updateAll(now);

        // Heartbeat: visibly proves the control loop never blocks.
        if (now - lastBlinkMs >= 500) {
            lastBlinkMs = now;
            ledOn = !ledOn;
            gpio_put(PICOHOME_HEARTBEAT_LED_GPIO, ledOn);
        }
    }
}