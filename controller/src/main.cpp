// PicoHome building controller.
//
// Core 0 (this file) owns control and never blocks: it loads /config.json from
// the SD card, builds the device inventory and bindings described there, then
// runs inputs -> event router -> bindings -> actuators. Core 1 owns Ethernet,
// MQTT and everything else that may stall; the two exchange work through
// lock-free queues, so the wall switches keep working with the cable pulled.
//
// Next milestones:
//  - Modbus RTU master and the relay module (M8), room nodes (M9)
//  - Dimming (M10), WS2815 strips (M11)

#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include "pico/stdlib.h"

#include "board_config.h"
#include "hal/pico_i2c.h"
#include "hal/pico_pins.h"
#include "net/net_core.h"
#include "picohome/config/config.h"
#include "picohome/control/binding_set.h"
#include "picohome/control/event_router.h"
#include "picohome/core/device_registry.h"
#include "picohome/drivers/mcp23017.h"
#include "picohome/devices/binary_sensor.h"
#include "picohome/devices/button.h"
#include "picohome/devices/cover.h"
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

// MCP23017 port expanders, indexed as the config numbers them.
picohome::pico_hal::PicoI2cBus* gI2c = nullptr;
std::vector<std::unique_ptr<picohome::Mcp23017>> gExpanders;

// GPIO number -> expander, so the shared pin interrupt can find its chip.
picohome::Mcp23017* gExpanderByIntPin[32] = {};

void gpioIrqHandler(uint gpio, uint32_t) {
    // Interrupt context: flag it and get out. The bus read happens in the
    // control loop, where taking ~100 µs is allowed.
    if (gpio < 32 && gExpanderByIntPin[gpio] != nullptr) gExpanderByIntPin[gpio]->onInterrupt();
}

picohome::Mcp23017* expanderFor(int8_t index) {
    for (std::size_t i = 0; i < gConfig.expanders.size(); ++i) {
        if (gConfig.expanders[i].index == index) return gExpanders[i].get();
    }
    return nullptr;  // the parser already proved this cannot happen
}

// Builds the expanders and arms their interrupt lines. Pin objects claim their
// directions afterwards, so the chips are configured once everything is known.
bool buildExpanders(std::string& error) {
    static picohome::pico_hal::PicoI2cBus bus;
    gI2c = &bus;

    for (const auto& def : gConfig.expanders) {
        gExpanders.push_back(std::make_unique<picohome::Mcp23017>(*gI2c, def.address));

        gpio_init(def.intPin);
        gpio_set_dir(def.intPin, GPIO_IN);
        gpio_pull_up(def.intPin);  // the chip drives INT low on a change
        gExpanderByIntPin[def.intPin] = gExpanders.back().get();
        gpio_set_irq_enabled_with_callback(def.intPin, GPIO_IRQ_EDGE_FALL, true, &gpioIrqHandler);
    }
    (void)error;
    return true;
}

bool buildDevices(const picohome::config::App& config, picohome::DeviceRegistry& registry,
                  picohome::EventRouter& router, std::string& error) {
    using namespace picohome;

    // A pin is either a controller GPIO or a bit on an expander; above this
    // point nothing else in the firmware can tell which.
    const auto makeInput = [](const config::PinRef& pin) -> std::unique_ptr<hal::IDigitalInput> {
        if (pin.expander < 0) {
            return std::make_unique<pico_hal::GpioDigitalInput>(pin.pin, pin.activeHigh,
                                                                pin.pullUp);
        }
        return std::make_unique<McpDigitalInput>(*expanderFor(pin.expander), pin.pin,
                                                 pin.activeHigh, pin.pullUp);
    };
    const auto makeOutput = [](const config::PinRef& pin) -> std::unique_ptr<hal::IDigitalOutput> {
        if (pin.expander < 0) {
            return std::make_unique<pico_hal::GpioDigitalOutput>(pin.pin, pin.activeHigh);
        }
        return std::make_unique<McpDigitalOutput>(*expanderFor(pin.expander), pin.pin,
                                                  pin.activeHigh);
    };

    for (const auto& def : config.actuators) {
        if (def.backend == config::BackendKind::Cover) {
            auto up = makeOutput(def.pin);
            auto down = makeOutput(def.pinDown);
            Cover::Timing timing;
            timing.travelUpMs = def.cover.travelUpMs;
            timing.travelDownMs = def.cover.travelDownMs;
            timing.tiltMs = def.cover.tiltMs;
            timing.stepMs = def.cover.stepMs;
            timing.motorRestMs = def.cover.motorRestMs;
            timing.bootOpen = def.cover.bootOpen;
            timing.homeAtBoot = def.cover.homeAtBoot;
            registry.add(std::make_unique<Cover>(def.type, def.id, def.room, *up, *down, timing));
            gOutputs.push_back(std::move(up));
            gOutputs.push_back(std::move(down));
            continue;
        }
        if (def.backend != config::BackendKind::Gpio && def.backend != config::BackendKind::Mcp) {
            error = "actuator backend not supported yet";  // parser already rejects these
            return false;
        }
        auto output = makeOutput(def.pin);
        registry.add(std::make_unique<Relay>(def.type, def.id, def.room, *output));
        gOutputs.push_back(std::move(output));
    }

    for (const auto& def : config.sensors) {
        auto input = makeInput(def.pin);
        auto sensor =
            std::make_unique<BinarySensor>(def.type, def.id, def.room, *input, def.debounceMs);
        sensor->attachRouter(&router);
        registry.add(std::move(sensor));
        gInputs.push_back(std::move(input));
    }

    for (const auto& def : config.buttons) {
        auto input = makeInput(def.pin);
        auto button = std::make_unique<Button>(def.id, def.room, *input, def.kind, def.gestures);
        button->attachRouter(&router);
        registry.add(std::move(button));
        gInputs.push_back(std::move(input));
    }

    // Every pin has claimed its direction and pull-up by now, so the chips can
    // be configured in one shot.
    for (std::size_t i = 0; i < gExpanders.size(); ++i) {
        if (!gExpanders[i]->initialize(to_ms_since_boot(get_absolute_time()))) {
            error = "MCP23017 at address 0x" +
                    std::to_string(static_cast<unsigned>(gConfig.expanders[i].address)) +
                    " did not answer";
            return false;
        }
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

    if (!buildExpanders(error)) errorLoopWithNetwork("expanders: " + error);
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
    uint32_t lastUnrouted = 0;
    bool ledOn = false;

    while (true) {
        const uint32_t now = clock.nowMs();

        // Pull in whatever the expanders' interrupts flagged, so the pin
        // objects read fresh values this iteration.
        for (auto& expander : gExpanders) expander->refreshInputs(now);

        // Inbound MQTT first, so a command and a switch press in the same
        // iteration resolve in arrival order. Bounded so a flood of traffic
        // can never starve the inputs.
        picohome::InboundMessage inbound;
        for (int budget = 8; budget > 0 && picohome::net::popInbound(inbound); --budget) {
            commands.handle(inbound);
        }
        // A command that reached the right device but was refused (wrong verb
        // for that device type, unparseable payload) is otherwise invisible.
        if (commands.unroutedCount() != lastUnrouted) {
            lastUnrouted = commands.unroutedCount();
            std::printf("control: %u command(s) reached no device\n",
                        static_cast<unsigned>(lastUnrouted));
        }

        // Inputs emit events into the router, which dispatches to bindings
        // synchronously; bindings then run their own timers.
        registry.updateAll(now);
        bindings.updateAll(now);

        // One bus write per expander, however many relays on it moved.
        for (auto& expander : gExpanders) expander->flushOutputs();

        // Heartbeat: visibly proves the control loop never blocks.
        if (now - lastBlinkMs >= 500) {
            lastBlinkMs = now;
            ledOn = !ledOn;
            gpio_put(PICOHOME_HEARTBEAT_LED_GPIO, ledOn);
        }
    }
}
