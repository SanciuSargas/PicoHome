// PicoHome building controller.
//
// Core 0 (this file) owns control and never blocks: it loads /config.json from
// the SD card, builds the device inventory and bindings described there, then
// runs inputs -> event router -> bindings -> actuators. Core 1 owns Ethernet,
// MQTT and everything else that may stall; the two exchange work through
// lock-free queues, so the wall switches keep working with the cable pulled.
//
// Next milestones:
//  - MCP23017 expander pins behind the HAL interfaces (M3)
//  - Covers (M6), Modbus RTU master (M8), dimming (M10)

#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include "pico/stdlib.h"

#include "board_config.h"
#include "hal/pico_pins.h"
#include "net/net_core.h"
#include "picohome/config/config.h"
#include "picohome/control/binding_set.h"
#include "picohome/control/event_router.h"
#include "picohome/core/device_registry.h"
#include "picohome/devices/binary_sensor.h"
#include "picohome/devices/button.h"
#include "picohome/devices/relay.h"
#include "picohome/net/command_router.h"
#include "picohome/net/publisher.h"
#include "picohome/net/remote_sources.h"
#include "sd_config_loader.h"

namespace {

// Long-lived because core 1 reads them for the whole run.
picohome::config::App gConfig;
picohome::RemoteSourceTable gRemoteSources;

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
        auto sensor =
            std::make_unique<BinarySensor>(def.type, def.id, def.room, *input, def.debounceMs);
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

void blinkForever(uint32_t periodMs, const std::string& reason) {
    uint32_t lastPrintMs = 0;
    bool ledOn = false;
    while (true) {
        const uint32_t now = to_ms_since_boot(get_absolute_time());
        if (now - lastPrintMs >= 2000) {
            lastPrintMs = now;
            std::printf("CONFIG ERROR: %s\n", reason.c_str());
        }
        ledOn = !ledOn;
        gpio_put(PICOHOME_HEARTBEAT_LED_GPIO, ledOn);
        sleep_ms(periodMs);
    }
}

// The config is unusable and we have no network settings either: nothing left
// but to blink and complain on the serial port.
[[noreturn]] void errorLoop(const std::string& reason) {
    blinkForever(100, reason);
    __builtin_unreachable();
}

// The config parsed but the system it describes could not be built. The
// network section is trustworthy, so bring MQTT up anyway and report the fault
// on status/0/error — a panel in a cupboard cannot show a blink code.
[[noreturn]] void errorLoopWithNetwork(const std::string& reason) {
    picohome::net::launch(gConfig, gRemoteSources, reason);
    blinkForever(100, reason);
    __builtin_unreachable();
}

}  // namespace

int main() {
    stdio_init_all();

    gpio_init(PICOHOME_HEARTBEAT_LED_GPIO);
    gpio_set_dir(PICOHOME_HEARTBEAT_LED_GPIO, GPIO_OUT);

    // Give USB serial a moment to enumerate so early boot messages are visible.
    sleep_ms(1500);
    std::printf("PicoHome controller booting, reading %s\n", PICOHOME_CONFIG_PATH);

    const auto sdResult = picohome::pico_hal::readFileFromSd(PICOHOME_CONFIG_PATH);
    if (!sdResult.ok) errorLoop("SD: " + sdResult.error);

    std::string error;
    if (!picohome::config::parse(sdResult.content, gConfig, error)) {
        errorLoop("config parse: " + error);
    }

    picohome::DeviceRegistry registry;
    picohome::EventRouter router;
    picohome::BindingSet bindings;
    picohome::net::initQueues();

    if (!buildDevices(gConfig, registry, router, error)) errorLoopWithNetwork("devices: " + error);
    if (!bindings.build(gConfig, registry, router, gRemoteSources, error)) {
        errorLoopWithNetwork("bindings: " + error);
    }

    picohome::QueuePublisher publisher(picohome::net::publishSink(), gConfig.building);
    registry.attachPublisher(&publisher);
    registry.initializeAll();

    picohome::CommandRouter commands(registry, router);

    std::printf("PicoHome '%s' up: %u devices, %u bindings, %u remote sources\n",
                gConfig.building.c_str(), static_cast<unsigned>(registry.size()),
                static_cast<unsigned>(bindings.size()),
                static_cast<unsigned>(gRemoteSources.size()));

    picohome::net::launch(gConfig, gRemoteSources, /*bootError=*/"");

    picohome::pico_hal::PicoClock clock;
    uint32_t lastBlinkMs = 0;
    bool ledOn = false;

    while (true) {
        const uint32_t now = clock.nowMs();

        // Inbound MQTT first, so a command and a switch press in the same
        // iteration resolve in arrival order. Bounded so a flood of traffic
        // can never starve the inputs.
        picohome::InboundMessage inbound;
        for (int budget = 8; budget > 0 && picohome::net::popInbound(inbound); --budget) {
            commands.handle(inbound);
        }

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
