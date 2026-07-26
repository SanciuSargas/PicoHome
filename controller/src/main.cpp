// PicoHome building controller.
// Boots, loads /config.json from the SD card, builds the device inventory and
// the bindings described there, then runs the core-0 control loop:
// inputs -> event router -> bindings -> actuators.
//
// Next milestones:
//  - MCP23017 expander pins behind the HAL interfaces (M3)
//  - W5100 Ethernet + MQTT session on core 1 (M5): real IPublisher, command
//    router, and remote binding sources; boot errors held in gBootError get
//    published retained on status/0/error
//  - Modbus RTU master (nanomodbus) on core 1 (M8)

#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include "pico/stdlib.h"

#include "board_config.h"
#include "hal/pico_pins.h"
#include "picohome/config/config.h"
#include "picohome/control/binding_set.h"
#include "picohome/control/event_router.h"
#include "picohome/core/device_registry.h"
#include "picohome/devices/binary_sensor.h"
#include "picohome/devices/button.h"
#include "picohome/devices/relay.h"
#include "sd_config_loader.h"

namespace {

// Boot error retained for publishing once MQTT exists (M5).
std::string gBootError;

// Pin objects are owned here; devices hold references (both live forever).
std::vector<std::unique_ptr<picohome::hal::IDigitalOutput>> gOutputs;
std::vector<std::unique_ptr<picohome::hal::IDigitalInput>> gInputs;

bool buildDevices(const picohome::config::App& config, picohome::DeviceRegistry& registry,
                  picohome::EventRouter& router, std::string& error) {
    using namespace picohome;

    for (const auto& def : config.actuators) {
        if (def.backend != config::BackendKind::Gpio) {
            error = "actuator backend not supported yet";  // parser already rejects these
            return false;
        }
        auto output = std::make_unique<pico_hal::GpioDigitalOutput>(def.pin.pin, def.pin.activeHigh);
        registry.add(std::make_unique<Relay>(def.type, def.id, def.room, *output));
        gOutputs.push_back(std::move(output));
    }

    for (const auto& def : config.sensors) {
        auto input = std::make_unique<pico_hal::GpioDigitalInput>(def.pin.pin, def.pin.activeHigh,
                                                                  def.pin.pullUp);
        auto sensor = std::make_unique<BinarySensor>(def.type, def.id, def.room, *input,
                                                     def.debounceMs);
        sensor->attachRouter(&router);
        registry.add(std::move(sensor));
        gInputs.push_back(std::move(input));
    }

    for (const auto& def : config.buttons) {
        auto input = std::make_unique<pico_hal::GpioDigitalInput>(def.pin.pin, def.pin.activeHigh,
                                                                  def.pin.pullUp);
        auto button = std::make_unique<Button>(def.id, def.room, *input, def.kind);
        button->attachRouter(&router);
        registry.add(std::move(button));
        gInputs.push_back(std::move(input));
    }
    return true;
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
    if (!sdResult.ok) errorLoop("SD: " + sdResult.error);

    picohome::config::App config;
    std::string error;
    if (!picohome::config::parse(sdResult.content, config, error)) {
        errorLoop("config parse: " + error);
    }

    picohome::DeviceRegistry registry;
    picohome::EventRouter router;
    picohome::BindingSet bindings;

    if (!buildDevices(config, registry, router, error)) errorLoop("devices: " + error);
    if (!bindings.build(config, registry, router, error)) errorLoop("bindings: " + error);
    registry.initializeAll();

    printf("PicoHome '%s' up: %u devices, %u bindings, %u subscriptions\n",
           config.building.c_str(), static_cast<unsigned>(registry.size()),
           static_cast<unsigned>(bindings.size()),
           static_cast<unsigned>(router.subscriptionCount()));

    picohome::pico_hal::PicoClock clock;
    uint32_t lastBlinkMs = 0;
    bool ledOn = false;

    while (true) {
        const uint32_t now = clock.nowMs();
        // Inputs emit events into the router, which dispatches to bindings
        // synchronously; bindings then run their own timers.
        registry.updateAll(now);
        bindings.updateAll(now);

        // Heartbeat: visibly proves the control loop never blocks.
        if (now - lastBlinkMs >= 500) {
            lastBlinkMs = now;
            ledOn = !ledOn;
            gpio_put(PICOHOME_HEARTBEAT_LED_GPIO, ledOn);
        }
    }
}
