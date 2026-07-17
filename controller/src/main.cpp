// PicoHome building controller — first-milestone skeleton.
// Boots, parses the (for now embedded) JSON config, builds the device registry
// and runs the core-0 control loop: buttons -> bindings -> relays.
//
// Next milestones:
//  - SD card config loading (no-OS-FatFS)      -> replaces kFallbackConfig
//  - W5100 Ethernet + MQTT session on core 1   -> real IPublisher + CommandRouter
//  - MCP23017 expander pins behind the HAL interfaces
//  - Modbus RTU master (nanomodbus) on core 1

#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include "pico/stdlib.h"

#include "hal/pico_pins.h"
#include "picohome/config/config.h"
#include "picohome/control/binding_engine.h"
#include "picohome/core/device_registry.h"
#include "picohome/devices/button.h"
#include "picohome/devices/relay.h"

namespace {

constexpr uint8_t kHeartbeatLedGpio = 25;  // YD-RP2040 user LED (WS2812 on GPIO23 later)

// Temporary built-in config until SD loading lands; mirrors config/testbench.json.
constexpr char kFallbackConfig[] = R"json({
  "schema_version": 1,
  "building": "testbench",
  "devices": {
    "binary_actuators": [
      {"type": "light", "id": 29, "room": "kitchen",
       "pin": {"expander": -1, "pin": 2, "active_high": true}}
    ],
    "buttons": [
      {"id": 29, "room": "kitchen",
       "pin": {"expander": -1, "pin": 3, "active_high": false, "pull_up": true},
       "controls": {"type": "light", "id": 29}}
    ]
  }
})json";

// Pin objects are owned here; devices hold references (both live forever).
std::vector<std::unique_ptr<picohome::hal::IDigitalOutput>> gOutputs;
std::vector<std::unique_ptr<picohome::hal::IDigitalInput>> gInputs;

void buildDevices(const picohome::config::App& config,
                  picohome::DeviceRegistry& registry,
                  picohome::BindingEngine& bindings) {
    using namespace picohome;

    for (const auto& def : config.binaryActuators) {
        // TODO: def.pin.expander >= 0 -> MCP23017-backed pin
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

}  // namespace

int main() {
    stdio_init_all();

    gpio_init(kHeartbeatLedGpio);
    gpio_set_dir(kHeartbeatLedGpio, GPIO_OUT);

    picohome::config::App config;
    std::string error;
    if (!picohome::config::parse(kFallbackConfig, config, error)) {
        // Broken config: report on USB serial and blink fast forever.
        while (true) {
            printf("config error: %s\n", error.c_str());
            gpio_put(kHeartbeatLedGpio, true);
            sleep_ms(100);
            gpio_put(kHeartbeatLedGpio, false);
            sleep_ms(100);
        }
    }

    picohome::DeviceRegistry registry;
    picohome::BindingEngine bindings(registry);
    buildDevices(config, registry, bindings);
    registry.initializeAll();

    printf("PicoHome '%s' up, %u devices\n", config.building.c_str(),
           static_cast<unsigned>(registry.size()));

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
            gpio_put(kHeartbeatLedGpio, ledOn);
        }
    }
}