/**
 * @file main.cpp
 * @brief Main program for homo automation microcontrollers.
 */

#define ARDUINOJSON_ENABLE_COMMENTS 1

#include "hardware/structs/iobank0.h"
#include <Arduino.h>
#include <functional>
#include <array>
#include <SD.h>
#include <ArduinoJson.h>
#include <Adafruit_MCP23X17.h>
#include <BasicOutputInterface.h>
#include <BasicOutputDevice.h>
#include <BasicMomentarySwitchSensor.h>
#include <LocalPin.h>
#include <ExpanderPin.h>
#include <MQTTCommunication.h>
#include <MCPInterruptHandler.h>
#include <SPI.h>
#include "Ethernet.h"
#include "PubSubClient.h"
#include <string_view>
#include <array>
#include <charconv>   // for from_chars
#include <cstdio>

const char* configFilename = "/config.txt";

// Details from the config.txt file
// MQTT details
const char* mqttClientID;
uint8_t macAddress[6];
uint32_t ipAddr;
uint32_t ipServ;
IPAddress ipAddress;
IPAddress ipServer;
byte mac[]    = {  0x12, 0xED, 0x67, 0xFE, 0x54, 0xED };

unsigned long previousMillisMQTTReconnect = 0;
unsigned long delayForMQTTReconnect = 5000;

// hardware details
const uint8_t ethernetResetPin = 14;
uint8_t ethernetCSPin = 10;
uint8_t SDCSPin = 17;

// interrupt variables
volatile uint32_t triggeredLocalInterrupts = 0;
bool interruptsToCheck[29] = {false};
std::function<void()> localPinInterruptCallbacks[29];


// MCP23X17 i2c extender objects (8 is max that you can have)
size_t mcpDeviceCount = 0;
bool mcpInterruptMatrix[8][16] = {false};
std::array<std::array<std::function<void()>, 16>, 8> mcpInterruptCallbacks;
Adafruit_MCP23X17 mcp[8];
LocalPin* mcpInterruptPins[8];
MCPInterruptHandler** mcpInterruptHandlers = nullptr;

size_t basicOutputDeviceCount = 0;
BasicOutputInterface** basicOutputDevices = nullptr;
size_t digitalSensorCount = 0;
AbstractDigitalSensor** digitalSensors = nullptr;
CommunicationInterface* communicationClient;

void sharedInterruptCallback(uint gpio, uint32_t events) {
    triggeredLocalInterrupts |= (1u << gpio);
}

bool payloadToInt_from_chars(const uint8_t* payload, size_t length, int &out) {
    if (!payload || length == 0) return false;
    const char* begin = reinterpret_cast<const char*>(payload);
    const char* end   = begin + length;
    auto res = std::from_chars(begin, end, out);
    return res.ec == std::errc(); // true if parsed at least one number (no extra trimming)
}

// TODO: maybe implement a queue in the future
void MQTTCallback(char* topic, byte* payload, unsigned int length) {
    // create a string_view over topic (topic must be null-terminated)
    std::string_view tv(topic);

    // Serial.print("Message arrived [");
    // Serial.print(topic);
    // Serial.print("] ");
    // for (int i=0;i<length;i++) {
    //   Serial.print((char)(payload[i]));
    // }
    // Serial.println();

    // fixed-size parts container
    constexpr size_t maxParts = 4;
    std::array<std::string_view, maxParts> parts;
    size_t partsCount = 0;

    size_t pos = 0;
    while (pos <= tv.size() && partsCount < maxParts) {
        size_t next = tv.find('/', pos);
        if (next == std::string_view::npos) {
            parts[partsCount++] = tv.substr(pos);
            break;
        } else {
            parts[partsCount++] = tv.substr(pos, next - pos);
            pos = next + 1;
        }
    }

    // std::string_view room   = parts[0];
    std::string_view type   = parts[1];
    std::string_view id_sv  = parts[2];
    std::string_view action = parts[3];

    int payloadInt = 0;
    if (!payloadToInt_from_chars(reinterpret_cast<const uint8_t*>(payload), length, payloadInt))
    {
        Serial.println("Invalid payload");
    }

    // convert id_sv to integer without allocating
    int id = 0;
    auto res = std::from_chars(id_sv.data(), id_sv.data() + id_sv.size(), id);
    if (res.ec != std::errc()) {
        Serial.println("Invalid id");
        return;
    }


    if (type == "BasicOutput" && action == "control")
    {
        for (int i = 0; i < basicOutputDeviceCount; i++)
        {
            if (basicOutputDevices[i]->getMQTTDeviceId() == id)
            {
                basicOutputDevices[i]->dealWithMQTTCallback(payloadInt);
            }
        }
    }
}

EthernetClient ethClient;
PubSubClient client(ethClient);

void reconnectNetwork()
{
    // Loop until we're reconnected
    if (millis() - previousMillisMQTTReconnect > delayForMQTTReconnect) {
        previousMillisMQTTReconnect = millis();

        Serial.print("Attempting MQTT connection...");
        // Attempt to connect
        if (client.connect(mqttClientID)) {
            Serial.println("connected");
            // Once connected, publish an announcement...
            client.publish("status","hello world");

            char topic[128];
            int needed = snprintf(topic, sizeof(topic), "%s/+/+/control", mqttClientID);
            if (needed < 0 || needed >= (int)sizeof(topic)) {
                Serial.println("Topic truncated or snprintf error");
            } else {
                client.subscribe(topic);
            }
        } else {
            Serial.print("failed, rc=");
            Serial.print(client.state());
            Serial.println(" try again in 5 seconds");
            // Wait 5 seconds before retrying
            delay(5000);
        }
    }
}

// Loads the configuration from a file
// TODO: add checks for each config option (whether it exists)
void loadConfiguration(const char *filename) {
    // Open the file for reading
    File file = SD.open(filename);

    // Allocate a temporary JsonDocument
    // Don't forget to change the capacity to match your requirements.
    // Use arduinojson.org/v7/assistant to compute the capacity.
    JsonDocument doc;

    // Deserialize the JSON document
    DeserializationError error = deserializeJson(doc, file);
    if (error) {
      Serial.print("deserializeJson() failed: ");
      Serial.println(error.c_str());
      return;
    }

    ////////////////////////////////////////////////////////
    // MQTT DETAILS
    ////////////////////////////////////////////////////////
    mqttClientID = doc["mqttClientID"];

    if (doc["macAddress"].is<JsonArray>()) {
        JsonArray macJson = doc["macAddress"].as<JsonArray>();
        if (macJson.size() == 6)
        {
            copyArray(macJson, macAddress, sizeof(macAddress)/sizeof(macAddress[0]));
        } else
        {
            Serial.println("macAddress is not 6 elements long");
        }
    } else
    {
        Serial.println("macAddress is not an array");
    }
    ipAddr = doc["ipAddress"];
    ipServ = doc["serverIPAddress"];
    delayForMQTTReconnect = doc["delayForMQTTReconnect"];

    ////////////////////////////////////////////////////////
    // ASSIGN OUTPUT DEVICE OPTIONS
    ////////////////////////////////////////////////////////
    JsonArray outputDeviceJson = doc["outputDevices"]["basicOutputDevices"]["devices"].as<JsonArray>();
    basicOutputDeviceCount = outputDeviceJson.size();
    basicOutputDevices = new BasicOutputInterface*[basicOutputDeviceCount];
    size_t index = 0;
    for (JsonObject outputDevice : outputDeviceJson) {
        // char* deviceTypeMQTT = jsonOutputDevices["deviceTypeMQTT"];
        char* deviceTypeMQTT = nullptr;
        strcpy(deviceTypeMQTT, outputDevice["deviceTypeMQTT"]);
        int idMQTT = outputDevice["MQTTDeviceID"]; // 29, 30
        int pinExtenderNumber = outputDevice["pinExtenderNumber"];
        int pinNumber = outputDevice["pinNumber"]; // 3, 4
        int relayTriggerMode = outputDevice["relayTriggerMode"];

        communicationClient = new MQTTCommunication();
        if (pinExtenderNumber == -1)
        {
            basicOutputDevices[index] = new BasicOutputDevice(new LocalPin(pinNumber), relayTriggerMode, deviceTypeMQTT, idMQTT, communicationClient);
        } else
        {
            mcpInterruptMatrix[pinExtenderNumber][pinNumber] = false;
            basicOutputDevices[index] = new BasicOutputDevice(new ExpanderPin(pinNumber, &mcp[pinExtenderNumber]), relayTriggerMode, deviceTypeMQTT, idMQTT, communicationClient);
        }
        index++;
    }


    ////////////////////////////////////////////////////////
    // ASSIGN BASIC INPUT DEVICES DEVICE OPTIONS
    ////////////////////////////////////////////////////////
    JsonArray inputDeviceJson = doc["inputDevices"]["basicInputDevices"]["devices"].as<JsonArray>();
    digitalSensorCount = inputDeviceJson.size();
    digitalSensors = new AbstractDigitalSensor*[digitalSensorCount];
    index = 0;
    for (JsonObject inputDevice : inputDeviceJson) {
        char* deviceTypeMQTT = nullptr;
        strcpy(deviceTypeMQTT, inputDevice["deviceTypeMQTT"]);
        int idMQTT = inputDevice["MQTTDeviceID"];
        int pinExtenderNumber = inputDevice["pinExtenderNumber"];
        int pinNumber = inputDevice["pinNumber"];

        // TODO: add switch assignment options to json (brain no worky)
        if (pinExtenderNumber == -1)
        {
            auto* pinObject = new LocalPin(pinNumber);
            gpio_set_irq_enabled_with_callback(pinNumber, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true, &sharedInterruptCallback);
            pinObject->pinMode(INPUT_PULLUP);
            digitalSensors[index] = new BasicMomentarySwitchSensor(pinObject, deviceTypeMQTT, idMQTT, [ObjectPtr = basicOutputDevices[index]] { ObjectPtr->toggle(); }, communicationClient);
            localPinInterruptCallbacks[pinNumber] = std::move([ObjectPtr = digitalSensors[index]] { ObjectPtr->dealWithInterrupt(); });
            interruptsToCheck[pinNumber] = true;
        } else
        {
            mcpInterruptMatrix[pinExtenderNumber][pinNumber] = true;
            digitalSensors[index] = new BasicMomentarySwitchSensor(new ExpanderPin(pinNumber, &mcp[pinExtenderNumber]), deviceTypeMQTT, idMQTT, [ObjectPtr = basicOutputDevices[index]] { ObjectPtr->toggle(); }, communicationClient);
            mcpInterruptCallbacks[pinExtenderNumber][pinNumber] = std::move([ObjectPtr = digitalSensors[index]] { ObjectPtr->dealWithInterrupt(); });
        }

        index++;
    }

    ////////////////////////////////////////////////////////
    // ASSIGN MCP PIN EXTENDER DEVICE OPTIONS
    ////////////////////////////////////////////////////////
    JsonArray MCPDeviceJson = doc["MCP23017PinExtenders"]["devices"].as<JsonArray>();
    mcpDeviceCount = MCPDeviceJson.size();
    mcpInterruptHandlers = new MCPInterruptHandler*[mcpDeviceCount];
    index = 0;
    for (JsonObject mcpDevice : MCPDeviceJson) {
        int I2CAddress = mcpDevice["I2CAddress"];
        int pinNumber = mcpDevice["pinNumber"];

        mcpInterruptPins[index] = new LocalPin(pinNumber);
        mcpInterruptPins[index]->pinMode(INPUT_PULLUP);
        gpio_set_irq_enabled(pinNumber, GPIO_IRQ_EDGE_FALL, true);

        mcpInterruptHandlers[index] = new MCPInterruptHandler(I2CAddress, &mcp[index], mcpInterruptPins[index], mcpInterruptCallbacks[index]);

        localPinInterruptCallbacks[pinNumber] = std::move([ObjectPtr = mcpInterruptHandlers[index]] { ObjectPtr->callback(); });
        interruptsToCheck[pinNumber] = true;

        index++;
    }

    file.close();
}

void setup() {
    pinMode(ethernetCSPin, OUTPUT);       // ensure ethernet CS is an output
    pinMode(ethernetResetPin, OUTPUT);
    pinMode(SDCSPin, OUTPUT);             // optional: ensure SD CS also an output
    digitalWrite(ethernetCSPin, HIGH);    // deselect ethernet device
    digitalWrite(ethernetResetPin, HIGH); // inactive (not holding in reset)
    digitalWrite(SDCSPin, HIGH);          // deselect SD until begin()

    // Initialize serial
    Serial.begin(57600);
    SPI.begin();
    while (!Serial) {}

    // Initialize SD library
    while (!SD.begin(SDCSPin)) {
        Serial.println("Failed to initialize SD library");
        delay(1000);
    }

    Wire.begin();

    loadConfiguration(configFilename);

    SD.end();

    Ethernet.init(ethernetCSPin);

    ipAddress = IPAddress(ipAddr);
    ipServer = IPAddress(ipServ);

    // reset ethernet shield
    pinMode(ethernetResetPin, OUTPUT);
    digitalWrite(ethernetResetPin, LOW);
    delay(100);           // Hold reset low for a short time
    digitalWrite(ethernetResetPin, HIGH);
    delay(500);

    client.setServer(ipServer, 1883);
    client.setCallback(MQTTCallback);

    Ethernet.begin(mac, ipAddress);

    if (Ethernet.hardwareStatus() == EthernetNoHardware) {
        Serial.println("Ethernet shield was not found.  Sorry, can't run without hardware. :(");
        while (true) {
            delay(1); // do nothing, no point running without Ethernet hardware
        }
    }
    if (Ethernet.linkStatus() == LinkOFF) {
        Serial.println("Ethernet cable is not connected.");
    }

    previousMillisMQTTReconnect = millis();

    // Allow the hardware to sort itself out
    delay(1000);

    for (int i = 0; i < mcpDeviceCount; i++)
    {
        mcpInterruptHandlers[i]->initialize(mcpInterruptMatrix[i]);
    }
    for (int i = 0; i < basicOutputDeviceCount; i++)
    {
        basicOutputDevices[i]->initialize();
    }
    for (int i = 0; i < digitalSensorCount; i++)
    {
        digitalSensors[i]->initialize();
    }

    Serial.println("Setup done");
    delay(1500);
}

void loop() {
    if (!client.connected()) {
        reconnectNetwork();
    }

    if (triggeredLocalInterrupts)
    {
        Serial.println("Local interrupt triggered");
        // TODO: might need noInterrupts(); and interrupts(); afterwards
        uint32_t status = triggeredLocalInterrupts;
        triggeredLocalInterrupts = 0;

        for (int pin = 0; pin < 29; pin++)
        {
            if (status & (1 << pin))
            {
                if (interruptsToCheck[pin])
                {
                    Serial.println("pinCallback called");
                    localPinInterruptCallbacks[pin]();
                    Serial.println("pinCallback done");
                }
            }
        }
    }
    for (size_t i = 0; i < mcpDeviceCount; i++)
    {
        mcpInterruptHandlers[i]->update();
    }
    for (size_t i = 0; i < digitalSensorCount; i++)
    {
        digitalSensors[i]->update();
    }

    client.loop();
}