#include "picohome/config/config.h"

#include <ArduinoJson.h>

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
        return fail(error, "backend 'mcp': expander pins are not implemented yet (M3)");
    } else if (kind == "pwm") {
        return fail(error, "backend 'pwm': dimming is not implemented yet (M10)");
    } else if (kind == "cover") {
        return fail(error, "backend 'cover': covers are not implemented yet (M6)");
    } else if (kind == "modbus") {
        return fail(error, "backend 'modbus': Modbus is not implemented yet (M8)");
    } else {
        return fail(error, "unknown backend kind");
    }
    parsePin(obj, actuator.pin);
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
        return fail(error, "rule 'cover' is not implemented yet (M6)");
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
            if (type == DeviceType::Blinds || type == DeviceType::Curtains) return true;
            return fail(error, "cover target must be blinds or curtains");
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
        if (source.sensor.type != DeviceType::Pir && source.sensor.type != DeviceType::Presence) {
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
        // Cross-panel bindings: a switch wired to another controller publishes
        // its gesture and this controller reacts (e.g. corridor 30PJ/31PJ).
        source.kind = SourceType::Remote;
        source.topic = obj["topic"] | "";
        source.payload = obj["payload"] | "";
        if (source.topic.empty()) return fail(error, "binding source: empty topic");
        if (source.payload.empty()) return fail(error, "binding source: topic needs a payload");
        return fail(error, "binding source 'topic': remote sources need MQTT (M5)");
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
        out.actuators.push_back(std::move(actuator));
    }

    for (JsonVariantConst entry : devices["sensors"].as<JsonArrayConst>()) {
        JsonObjectConst obj = entry.as<JsonObjectConst>();
        SensorDef sensor;
        sensor.type = typeFromSegment(obj["type"] | "");
        sensor.id = obj["id"] | static_cast<uint16_t>(0);
        sensor.room = obj["room"] | "";
        sensor.debounceMs = obj["debounce_ms"] | 50u;
        if (sensor.type == DeviceType::Unknown) return fail(error, "sensor: unknown type");
        if (sensor.id == 0 || sensor.room.empty()) return fail(error, "sensor: needs id and room");
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

    return true;
}

}  // namespace picohome::config
