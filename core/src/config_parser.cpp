#include "picohome/config/config.h"

#include <ArduinoJson.h>

namespace picohome::config {

namespace {

void parsePin(JsonObjectConst obj, PinRef& pin) {
    pin.expander = obj["expander"] | static_cast<int8_t>(-1);
    pin.pin = obj["pin"] | static_cast<uint8_t>(0);
    pin.activeHigh = obj["active_high"] | true;
    pin.pullUp = obj["pull_up"] | true;
}

}  // namespace

bool parse(std::string_view json, App& out, std::string& error) {
    JsonDocument doc;
    const DeserializationError err = deserializeJson(doc, json.data(), json.size());
    if (err) {
        error = err.c_str();
        return false;
    }

    out = App{};

    out.schemaVersion = doc["schema_version"] | 0u;
    if (out.schemaVersion != kSupportedSchemaVersion) {
        error = "unsupported schema_version";
        return false;
    }

    const char* building = doc["building"];
    if (building == nullptr || *building == '\0') {
        error = "missing building";
        return false;
    }
    out.building = building;

    JsonObjectConst net = doc["network"].as<JsonObjectConst>();
    if (!net.isNull()) {
        JsonArrayConst mac = net["mac"].as<JsonArrayConst>();
        if (!mac.isNull()) {
            if (mac.size() != 6) {
                error = "network.mac must have 6 bytes";
                return false;
            }
            std::size_t i = 0;
            for (JsonVariantConst byte : mac) out.network.mac[i++] = byte.as<uint8_t>();
        }
        out.network.ip = net["ip"] | "";
        out.network.netmask = net["netmask"] | "";
        out.network.gateway = net["gateway"] | "";
        out.network.broker = net["broker"] | "";
        out.network.brokerPort = net["broker_port"] | static_cast<uint16_t>(1883);
    }

    for (JsonVariantConst entry : doc["devices"]["binary_actuators"].as<JsonArrayConst>()) {
        JsonObjectConst obj = entry.as<JsonObjectConst>();
        BinaryActuator actuator;
        actuator.type = typeFromSegment(obj["type"] | "light");
        if (actuator.type == DeviceType::Unknown) {
            error = "binary_actuators: unknown type";
            return false;
        }
        actuator.id = obj["id"] | static_cast<uint16_t>(0);
        actuator.room = obj["room"] | "";
        JsonObjectConst pinObj = obj["pin"].as<JsonObjectConst>();
        if (actuator.id == 0 || actuator.room.empty() || pinObj.isNull()) {
            error = "binary_actuators: entry needs id, room and pin";
            return false;
        }
        parsePin(pinObj, actuator.pin);
        out.binaryActuators.push_back(std::move(actuator));
    }

    for (JsonVariantConst entry : doc["devices"]["buttons"].as<JsonArrayConst>()) {
        JsonObjectConst obj = entry.as<JsonObjectConst>();
        ButtonDef button;
        button.id = obj["id"] | static_cast<uint16_t>(0);
        button.room = obj["room"] | "";
        JsonObjectConst pinObj = obj["pin"].as<JsonObjectConst>();
        if (button.id == 0 || button.room.empty() || pinObj.isNull()) {
            error = "buttons: entry needs id, room and pin";
            return false;
        }
        parsePin(pinObj, button.pin);
        JsonObjectConst controls = obj["controls"].as<JsonObjectConst>();
        if (!controls.isNull()) {
            button.controlsType = typeFromSegment(controls["type"] | "light");
            button.controlsId = controls["id"] | static_cast<uint16_t>(0);
            if (button.controlsType == DeviceType::Unknown) {
                error = "buttons: unknown controls.type";
                return false;
            }
        }
        out.buttons.push_back(std::move(button));
    }

    return true;
}

}  // namespace picohome::config