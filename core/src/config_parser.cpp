#include "picohome/config/config.h"

#include <ArduinoJson.h>

#include <cstdlib>

namespace picohome::config {

namespace {

// Parse helpers set `error` and return false; the caller aborts the whole load.
// Being strict here is deliberate: a controller running a half-understood
// config can energize the wrong circuits, so anything unrecognised is refused.

bool fail(std::string& error, const std::string& message) {
    error = message;
    return false;
}

void parsePin(JsonObjectConst obj, PinRef& pin) {
    pin.expander = obj["expander"] | static_cast<int8_t>(-1);
    pin.pin = obj["pin"] | static_cast<uint8_t>(0);
    pin.activeHigh = obj["active_high"] | true;
    pin.pullUp = obj["pull_up"] | true;
}

bool parseBackend(JsonObjectConst obj, ActuatorDef& actuator, std::string& error) {
    const std::string_view kind = obj["kind"] | "gpio";
    if (kind == "gpio") {
        actuator.backend = BackendKind::Gpio;
    } else if (kind == "mcp") {
        actuator.backend = BackendKind::Mcp;
    } else if (kind == "pwm") {
        return fail(error, "backend 'pwm': dimming is not implemented yet (M10)");
    } else if (kind == "cover") {
        actuator.backend = BackendKind::Cover;
        JsonObjectConst up = obj["up"].as<JsonObjectConst>();
        JsonObjectConst down = obj["down"].as<JsonObjectConst>();
        if (up.isNull() || down.isNull()) {
            return fail(error, "backend 'cover': needs an 'up' and a 'down' pin");
        }
        parsePin(up, actuator.pin);
        parsePin(down, actuator.pinDown);
        return true;
    } else if (kind == "modbus") {
        return fail(error, "backend 'modbus': Modbus is not implemented yet (M8)");
    } else {
        return fail(error, "unknown backend kind");
    }
    parsePin(obj, actuator.pin);
    return true;
}

// I2C addresses are universally written in hex, so accept "0x20" as well as a
// plain number — a hardware config file is edited by humans reading datasheets.
bool parseI2cAddress(JsonVariantConst value, uint8_t& out, std::string& error) {
    if (value.is<const char*>()) {
        const char* text = value.as<const char*>();
        char* end = nullptr;
        const unsigned long parsed = std::strtoul(text, &end, 0);
        if (end == text || *end != '\0') return fail(error, "expander: bad address");
        out = static_cast<uint8_t>(parsed);
    } else if (value.is<unsigned>()) {
        out = value.as<uint8_t>();
    } else {
        return fail(error, "expander: missing address");
    }
    if (out < 0x20 || out > 0x27) {
        return fail(error, "expander: address must be between 0x20 and 0x27");
    }
    return true;
}

bool parseDeviceRef(JsonObjectConst obj, DeviceRef& ref, const char* what, std::string& error) {
    ref.type = typeFromSegment(obj["type"] | "");
    ref.id = obj["id"] | static_cast<uint16_t>(0);
    if (ref.type == DeviceType::Unknown) return fail(error, std::string(what) + ": unknown type");
    if (ref.id == 0) return fail(error, std::string(what) + ": missing id");
    return true;
}

bool parseRule(std::string_view name, RuleKind& rule, std::string& error) {
    if (name == "toggle") {
        rule = RuleKind::Toggle;
    } else if (name == "motion") {
        rule = RuleKind::Motion;
    } else if (name == "cover") {
        rule = RuleKind::Cover;
    } else {
        return fail(error,
                    "unknown rule (local bindings are toggle, motion and cover; anything "
                    "richer belongs in Home Assistant)");
    }
    return true;
}

// Local bindings exist for the behaviour that must survive a dead broker, so
// both ends are restricted to the device classes that genuinely need it.
bool checkTargetType(RuleKind rule, DeviceType type, std::string& error) {
    switch (rule) {
        case RuleKind::Toggle:
            if (isLight(type) || type == DeviceType::Outlet || type == DeviceType::Damper) {
                return true;
            }
            return fail(error, std::string("toggle target must be a light, outlet or damper, got ") +
                                   std::string(toSegment(type)));
        case RuleKind::Motion:
            if (isLight(type)) return true;
            return fail(error, std::string("motion target must be a light, got ") +
                                   std::string(toSegment(type)));
        case RuleKind::Cover:
            if (type == DeviceType::Blinds) return true;
            if (type == DeviceType::Curtains) {
                return fail(error, "curtains are not implemented yet (undecided hardware)");
            }
            return fail(error, "cover target must be blinds");
    }
    return fail(error, "unknown rule");
}

bool parseSource(JsonObjectConst obj, RuleKind rule, SourceDef& source, std::string& error) {
    if (!obj["button"].isNull()) {
        source.kind = SourceType::Button;
        source.buttonId = obj["button"].as<uint16_t>();
        // Most switches just toggle, so press_1 is the default; press_2/press_3
        // are available for the occasional second function.
        const std::string_view gesture = obj["gesture"] | "press_1";
        if (!gestureFromPayload(gesture, source.gesture)) {
            return fail(error, "binding source: unknown gesture");
        }
        return true;
    }
    if (!obj["sensor"].isNull()) {
        source.kind = SourceType::Sensor;
        if (!parseDeviceRef(obj["sensor"].as<JsonObjectConst>(), source.sensor,
                            "binding source sensor", error)) {
            return false;
        }
        if (!isOccupancySensor(source.sensor.type)) {
            return fail(error,
                        "binding source sensor must be pir or presence (other sensors publish "
                        "their state for Home Assistant instead of driving local bindings)");
        }
        if (rule != RuleKind::Motion) {
            return fail(error, "sensor sources are only valid for the motion rule");
        }
        return true;
    }
    if (!obj["topic"].isNull()) {
        // A device on another controller: the corridor 30PJ/31PJ switches live
        // on the closet panel, and a motion sensor may just as well sit on a
        // different one from the light it drives.
        source.kind = SourceType::Remote;
        source.topic = obj["topic"] | "";
        if (source.topic.empty()) return fail(error, "binding source: empty topic");

        if (rule == RuleKind::Motion) {
            // A remote sensor reports a state, so both edges are needed for the
            // off-delay. The defaults match the "1"/"0" this firmware publishes
            // on its own state topics, so a PicoHome sensor needs no payloads.
            source.payloadActive = obj["payload_active"] | "1";
            source.payloadInactive = obj["payload_inactive"] | "0";
            if (source.payloadActive == source.payloadInactive) {
                return fail(error, "binding source: payload_active and payload_inactive "
                                   "must differ");
            }
            return true;
        }

        source.payload = obj["payload"] | "";
        if (source.payload.empty()) return fail(error, "binding source: topic needs a payload");
        return true;
    }
    return fail(error, "binding source must have button, sensor or topic");
}

bool parseBinding(JsonObjectConst obj, BindingDef& binding, std::string& error) {
    binding.name = obj["name"] | "";
    if (!parseRule(obj["rule"] | "", binding.rule, error)) return false;

    for (JsonVariantConst entry : obj["sources"].as<JsonArrayConst>()) {
        SourceDef source;
        if (!parseSource(entry.as<JsonObjectConst>(), binding.rule, source, error)) return false;
        binding.sources.push_back(std::move(source));
    }
    if (binding.sources.empty()) return fail(error, "binding has no sources");

    for (JsonVariantConst entry : obj["targets"].as<JsonArrayConst>()) {
        DeviceRef target;
        if (!parseDeviceRef(entry.as<JsonObjectConst>(), target, "binding target", error)) {
            return false;
        }
        if (!checkTargetType(binding.rule, target.type, error)) return false;
        binding.targets.push_back(target);
    }
    if (binding.targets.empty()) return fail(error, "binding has no targets");

    JsonObjectConst options = obj["options"].as<JsonObjectConst>();
    if (!options.isNull()) {
        if (!options["lux_below"].isNull()) {
            return fail(error,
                        "binding option 'lux_below' is gone: use each light's night_mode, "
                        "which Home Assistant sets over MQTT");
        }
        binding.offDelayMs = options["off_delay_ms"] | 0u;
    }
    if (binding.rule == RuleKind::Motion && binding.offDelayMs == 0) {
        return fail(error, "motion binding needs options.off_delay_ms");
    }
    if (binding.rule == RuleKind::Motion) {
        for (const SourceDef& source : binding.sources) {
            if (source.kind == SourceType::Button) {
                return fail(error, "motion binding sources must be sensors or topics");
            }
        }
    }
    return true;
}

}  // namespace

uint32_t defaultDebounceMs(DeviceType type) {
    switch (type) {
        case DeviceType::Pir:
        case DeviceType::Presence:
            // Neither has anything to debounce: both drive a clean logic output
            // and hold it for their own on-board delay. Filtering here would
            // only add latency to the one path where it is felt — the light
            // should be on before you have finished walking in.
            return 0;
        case DeviceType::Smoke:
            // A driven digital output, so nothing bounces; the reason to wait
            // at all is the long cable run from the room to the panel, where
            // mains wiring alongside it can induce a spike. Milliseconds of
            // margin cover that, and a fire does not need sub-second reporting.
            return 300;
        case DeviceType::WaterLeak:
            // A conductive probe sits right at its threshold and can chatter
            // for seconds on a thin film. A leak never goes away in one second
            // and a false alarm may shut a valve, so this one waits to be sure.
            return 1000;
        default:
            // A magnetic reed contact bounces like any other mechanical switch.
            return 50;
    }
}

bool parse(std::string_view json, App& out, std::string& error) {
    JsonDocument doc;
    const DeserializationError err = deserializeJson(doc, json.data(), json.size());
    if (err) return fail(error, err.c_str());

    out = App{};

    out.schemaVersion = doc["schema_version"] | 0u;
    if (out.schemaVersion != kSupportedSchemaVersion) {
        return fail(error, "unsupported schema_version (expected 2)");
    }

    const char* building = doc["building"];
    if (building == nullptr || *building == '\0') return fail(error, "missing building");
    out.building = building;

    JsonObjectConst net = doc["network"].as<JsonObjectConst>();
    if (!net.isNull()) {
        JsonArrayConst mac = net["mac"].as<JsonArrayConst>();
        if (!mac.isNull()) {
            if (mac.size() != 6) return fail(error, "network.mac must have 6 bytes");
            std::size_t i = 0;
            for (JsonVariantConst byte : mac) out.network.mac[i++] = byte.as<uint8_t>();
        }
        out.network.ip = net["ip"] | "";
        out.network.netmask = net["netmask"] | "";
        out.network.gateway = net["gateway"] | "";
        out.network.broker = net["broker"] | "";
        out.network.brokerPort = net["broker_port"] | static_cast<uint16_t>(1883);
    }

    JsonObjectConst devices = doc["devices"].as<JsonObjectConst>();

    for (JsonVariantConst entry : devices["expanders"].as<JsonArrayConst>()) {
        JsonObjectConst obj = entry.as<JsonObjectConst>();
        ExpanderDef expander;
        if (obj["index"].isNull()) return fail(error, "expander: missing index");
        expander.index = obj["index"] | static_cast<uint8_t>(0);
        if (expander.index > 7) return fail(error, "expander: index must be 0..7");
        if (!parseI2cAddress(obj["address"], expander.address, error)) return false;
        if (obj["int_pin"].isNull()) return fail(error, "expander: missing int_pin");
        expander.intPin = obj["int_pin"] | static_cast<uint8_t>(0);

        for (const ExpanderDef& existing : out.expanders) {
            if (existing.index == expander.index) return fail(error, "expander: duplicate index");
            if (existing.address == expander.address) {
                return fail(error, "expander: duplicate I2C address");
            }
        }
        out.expanders.push_back(expander);
    }

    for (JsonVariantConst entry : devices["actuators"].as<JsonArrayConst>()) {
        JsonObjectConst obj = entry.as<JsonObjectConst>();
        ActuatorDef actuator;
        actuator.type = typeFromSegment(obj["type"] | "light");
        actuator.id = obj["id"] | static_cast<uint16_t>(0);
        actuator.room = obj["room"] | "";
        if (actuator.type == DeviceType::Unknown) return fail(error, "actuator: unknown type");
        if (actuator.id == 0 || actuator.room.empty()) {
            return fail(error, "actuator: needs id and room");
        }
        JsonObjectConst backend = obj["backend"].as<JsonObjectConst>();
        if (backend.isNull()) return fail(error, "actuator: missing backend");
        if (!parseBackend(backend, actuator, error)) return false;

        if (actuator.backend == BackendKind::Cover) {
            if (actuator.type == DeviceType::Curtains) {
                return fail(error, "curtains are not implemented yet: how they are driven and "
                                   "parked is still undecided, so only blinds may use the cover "
                                   "backend");
            }
            if (actuator.type != DeviceType::Blinds) {
                return fail(error, "backend 'cover' is only for blinds");
            }
            JsonObjectConst options = obj["options"].as<JsonObjectConst>();
            if (options.isNull()) return fail(error, "cover: missing options.travel_ms");
            // A blind falls faster than it climbs. `travel_ms` sets both
            // directions for a cover where that does not matter; measuring the
            // two separately is what `travel_up_ms`/`travel_down_ms` are for.
            const uint32_t travelBoth = options["travel_ms"] | 0u;
            actuator.cover.travelUpMs = options["travel_up_ms"] | travelBoth;
            actuator.cover.travelDownMs = options["travel_down_ms"] | travelBoth;
            actuator.cover.tiltMs = options["tilt_ms"] | 0u;
            actuator.cover.stepMs = options["step_ms"] | 500u;
            if (!options["reverse_delay_ms"].isNull()) {
                // Renamed 2026-08-09: the rest applies to any restart, not only
                // a reversal. Say so rather than quietly using the default.
                return fail(error, "cover: 'reverse_delay_ms' is now 'motor_rest_ms'");
            }
            actuator.cover.motorRestMs = options["motor_rest_ms"] | 500u;

            actuator.cover.homeAtBoot = options["home_at_boot"] | false;

            const std::string_view boot = options["boot_position"] | "open";
            if (boot == "open") {
                actuator.cover.bootOpen = true;
            } else if (boot == "closed") {
                actuator.cover.bootOpen = false;
            } else {
                return fail(error, "cover: boot_position must be \"open\" or \"closed\"");
            }

            if (actuator.cover.travelUpMs == 0 || actuator.cover.travelDownMs == 0) {
                return fail(error, "cover: measure the travel — set options.travel_ms, or "
                                   "travel_up_ms and travel_down_ms");
            }
            const uint32_t shorterTravel = actuator.cover.travelUpMs < actuator.cover.travelDownMs
                                               ? actuator.cover.travelUpMs
                                               : actuator.cover.travelDownMs;
            if (actuator.cover.tiltMs >= shorterTravel) {
                return fail(error, "cover: tilt_ms must be shorter than the travel");
            }
        }
        out.actuators.push_back(std::move(actuator));
    }

    for (JsonVariantConst entry : devices["sensors"].as<JsonArrayConst>()) {
        JsonObjectConst obj = entry.as<JsonObjectConst>();
        SensorDef sensor;
        sensor.type = typeFromSegment(obj["type"] | "");
        sensor.id = obj["id"] | static_cast<uint16_t>(0);
        sensor.room = obj["room"] | "";
        if (sensor.type == DeviceType::Unknown) return fail(error, "sensor: unknown type");
        // devices.sensors describes the panel's own wiring: one pin, one state.
        if (!isBinarySensor(sensor.type)) {
            const std::string what = "'" + std::string(toSegment(sensor.type)) + "'";
            if (isMeasurement(sensor.type)) {
                return fail(error, "sensor " + what +
                                       ": measurements are read by room nodes over Modbus, not "
                                       "from a panel pin (M9)");
            }
            return fail(error, what + " is not a sensor type — devices.sensors takes reed, "
                                      "water_leak, smoke, pir or presence");
        }
        if (sensor.id == 0 || sensor.room.empty()) return fail(error, "sensor: needs id and room");
        sensor.debounceMs = obj["debounce_ms"] | defaultDebounceMs(sensor.type);
        JsonObjectConst pin = obj["pin"].as<JsonObjectConst>();
        if (pin.isNull()) return fail(error, "sensor: missing pin");
        parsePin(pin, sensor.pin);
        out.sensors.push_back(std::move(sensor));
    }

    for (JsonVariantConst entry : devices["buttons"].as<JsonArrayConst>()) {
        JsonObjectConst obj = entry.as<JsonObjectConst>();
        ButtonDef button;
        button.id = obj["id"] | static_cast<uint16_t>(0);
        button.room = obj["room"] | "";
        if (button.id == 0 || button.room.empty()) return fail(error, "button: needs id and room");

        // Deliberately not defaulted: a wrongly assumed switch kind is a silent
        // failure on a live panel, and both kinds are fitted around the house.
        const std::string_view kind = obj["kind"] | "";
        if (kind == "momentary") {
            button.kind = SwitchKind::Momentary;
        } else if (kind == "toggle") {
            button.kind = SwitchKind::Toggle;
        } else {
            return fail(error, "button " + std::to_string(button.id) +
                                   ": kind must be \"momentary\" (spring-return pushbutton) or "
                                   "\"toggle\" (latching rocker)");
        }

        // Opt in to the full grammar; plain switches act on the edge instead.
        button.gestures = (obj["gestures"] | false) ? GestureMode::Full : GestureMode::Instant;

        JsonObjectConst pin = obj["pin"].as<JsonObjectConst>();
        if (pin.isNull()) return fail(error, "button: missing pin");
        parsePin(pin, button.pin);
        out.buttons.push_back(std::move(button));
    }

    for (JsonVariantConst entry : doc["bindings"].as<JsonArrayConst>()) {
        BindingDef binding;
        if (!parseBinding(entry.as<JsonObjectConst>(), binding, error)) return false;
        out.bindings.push_back(std::move(binding));
    }

    // Every pin that claims to sit on an expander must name one that exists,
    // and a pin number must be within the chip. Catching this here means a
    // typo cannot end up driving the wrong circuit.
    const auto checkPin = [&](const PinRef& pin, const std::string& what) {
        if (pin.expander < 0) return true;
        if (pin.pin > 15) return fail(error, what + ": expander pins are 0..15");
        for (const ExpanderDef& expander : out.expanders) {
            if (expander.index == pin.expander) return true;
        }
        return fail(error, what + ": expander " + std::to_string(pin.expander) +
                               " is not declared in devices.expanders");
    };

    for (const ActuatorDef& actuator : out.actuators) {
        const std::string what =
            std::string(toSegment(actuator.type)) + "/" + std::to_string(actuator.id);
        if (actuator.backend == BackendKind::Cover) {
            // A cover's two pins carry their own expander field each, so either
            // relay may live on the controller or on an expander.
            if (!checkPin(actuator.pin, what + " (up)")) return false;
            if (!checkPin(actuator.pinDown, what + " (down)")) return false;
            continue;
        }
        if ((actuator.backend == BackendKind::Mcp) != (actuator.pin.expander >= 0)) {
            return fail(error, what + ": backend 'mcp' needs an expander, 'gpio' must not have one");
        }
        if (!checkPin(actuator.pin, what)) return false;
    }
    for (const SensorDef& sensor : out.sensors) {
        if (!checkPin(sensor.pin,
                      std::string(toSegment(sensor.type)) + "/" + std::to_string(sensor.id))) {
            return false;
        }
    }
    for (const ButtonDef& button : out.buttons) {
        if (!checkPin(button.pin, "button/" + std::to_string(button.id))) return false;
    }

    // MQTT addresses a device as <type>/<id>, so two of them sharing that name
    // means commands only ever reach the first while the second still drives
    // its pin. Buttons are exempt on purpose: a P-group is several physical
    // switches carrying one logical id, which is exactly how a switch group in
    // two rooms is declared.
    std::vector<DeviceRef> declared;
    const auto claimName = [&](DeviceType type, uint16_t id) {
        for (const DeviceRef& taken : declared) {
            if (taken.type == type && taken.id == id) {
                return fail(error, std::string(toSegment(type)) + "/" + std::to_string(id) +
                                       " is declared twice");
            }
        }
        declared.push_back(DeviceRef{type, id});
        return true;
    };
    for (const ActuatorDef& actuator : out.actuators) {
        if (!claimName(actuator.type, actuator.id)) return false;
    }
    for (const SensorDef& sensor : out.sensors) {
        if (!claimName(sensor.type, sensor.id)) return false;
    }

    // Two devices on one pin is a wiring mistake the firmware would otherwise
    // act out: an input and an output fighting over the same line, or one relay
    // silently shadowing another. The lists are short, so a plain scan is
    // enough — and this runs once, at boot.
    struct ClaimedPin {
        int8_t expander;
        uint8_t pin;
        std::string owner;
    };
    std::vector<ClaimedPin> pins;
    const auto claimPin = [&](const PinRef& pin, const std::string& owner) {
        for (const ClaimedPin& taken : pins) {
            if (taken.expander == pin.expander && taken.pin == pin.pin) {
                const std::string where = pin.expander < 0
                                              ? "GPIO " + std::to_string(pin.pin)
                                              : "expander " + std::to_string(pin.expander) +
                                                    " pin " + std::to_string(pin.pin);
                return fail(error, where + " is claimed by both " + taken.owner + " and " + owner);
            }
        }
        pins.push_back(ClaimedPin{pin.expander, pin.pin, owner});
        return true;
    };
    for (const ExpanderDef& expander : out.expanders) {
        PinRef intPin;
        intPin.pin = expander.intPin;
        if (!claimPin(intPin, "expander " + std::to_string(expander.index) + " INT")) return false;
    }
    for (const ActuatorDef& actuator : out.actuators) {
        const std::string what =
            std::string(toSegment(actuator.type)) + "/" + std::to_string(actuator.id);
        if (actuator.backend == BackendKind::Cover) {
            if (!claimPin(actuator.pin, what + " (up)")) return false;
            if (!claimPin(actuator.pinDown, what + " (down)")) return false;
            continue;
        }
        if (!claimPin(actuator.pin, what)) return false;
    }
    for (const SensorDef& sensor : out.sensors) {
        if (!claimPin(sensor.pin,
                      std::string(toSegment(sensor.type)) + "/" + std::to_string(sensor.id))) {
            return false;
        }
    }
    for (const ButtonDef& button : out.buttons) {
        if (!claimPin(button.pin, "button/" + std::to_string(button.id))) return false;
    }

    return true;
}

}  // namespace picohome::config
