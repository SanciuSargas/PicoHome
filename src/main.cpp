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

// MQTT details
char* mqttServer;
char* mqttClientID;

const char* configFilename = "/config.txt";

// Details from the config.txt file
const char* controllerNameMQTT;
size_t outputDeviceCount = 0;
size_t inputDeviceCount = 0;

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

BasicOutputInterface** lights = nullptr;
AbstractDigitalSensor** lightSwitches = nullptr;
CommunicationInterface* communicationClient;

void sharedCallback(uint gpio, uint32_t events) {
    triggeredLocalInterrupts |= (1u << gpio);
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

    /******************************************************/
    // MQTT DETAILS
    /******************************************************/
    controllerNameMQTT = doc["controllerName"];

    /******************************************************/
    // ASSIGN OUTPUT DEVICE OPTIONS
    /******************************************************/
    JsonArray outputDeviceJson = doc["outputDevices"]["basicOutputDevices"]["devices"].as<JsonArray>();
    outputDeviceCount = outputDeviceJson.size();
    lights = new BasicOutputInterface*[outputDeviceCount];
    size_t index = 0;
    for (JsonObject outputDevice : outputDeviceJson) {
        // char* deviceTypeMQTT = jsonOutputDevices["deviceTypeMQTT"];
        char* deviceTypeMQTT = nullptr;
        strcpy(deviceTypeMQTT, outputDevice["deviceTypeMQTT"]);
        int idMQTT = outputDevice["idMQTT"]; // 29, 30
        int pinExtenderNumber = outputDevice["pinExtenderNumber"];
        int pinNumber = outputDevice["pinNumber"]; // 3, 4
        int relayTriggerMode = outputDevice["relayTriggerMode"];

        communicationClient = new MQTTCommunication();
        if (pinExtenderNumber == -1)
        {
            lights[index] = new BasicOutputDevice(new LocalPin(pinNumber), relayTriggerMode, deviceTypeMQTT, communicationClient);
        } else
        {
            mcpInterruptMatrix[pinExtenderNumber][pinNumber] = false;
            lights[index] = new BasicOutputDevice(new ExpanderPin(pinNumber, &mcp[pinExtenderNumber]), relayTriggerMode, deviceTypeMQTT, communicationClient);
        }
        index++;
    }


    // TODO: move back into the for loop
    gpio_set_irq_enabled_with_callback(6, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true, &sharedCallback);
    /******************************************************/
    // ASSIGN BASIC INPUT DEVICES DEVICE OPTIONS
    /******************************************************/
    JsonArray inputDeviceJson = doc["inputDevices"]["basicInputDevices"]["devices"].as<JsonArray>();
    inputDeviceCount = inputDeviceJson.size();
    lightSwitches = new AbstractDigitalSensor*[inputDeviceCount];
    index = 0;
    for (JsonObject inputDevice : inputDeviceJson) {
        char* deviceTypeMQTT = nullptr;
        strcpy(deviceTypeMQTT, inputDevice["deviceTypeMQTT"]);
        int idMQTT = inputDevice["idMQTT"];
        int pinExtenderNumber = inputDevice["pinExtenderNumber"];
        int pinNumber = inputDevice["pinNumber"];

        // TODO: add switch assignment options to json (brain no worky)
        if (pinExtenderNumber == -1)
        {
            LocalPin* pinObject = new LocalPin(pinNumber);
            // pinObject->attachInterrupt(sharedCallback, CHANGE);
            pinObject->pinMode(INPUT_PULLUP);
            lightSwitches[index] = new BasicMomentarySwitchSensor(pinObject, deviceTypeMQTT, [ObjectPtr = lights[index]] { ObjectPtr->toggle(); }, communicationClient);
            localPinInterruptCallbacks[pinNumber] = std::move([ObjectPtr = lightSwitches[index]] { ObjectPtr->dealWithInterrupt(); });
            interruptsToCheck[pinNumber] = true;
        } else
        {
            mcpInterruptMatrix[pinExtenderNumber][pinNumber] = true;
            lightSwitches[index] = new BasicMomentarySwitchSensor(new ExpanderPin(pinNumber, &mcp[pinExtenderNumber]), deviceTypeMQTT, [ObjectPtr = lights[index]] { ObjectPtr->toggle(); }, communicationClient);
            mcpInterruptCallbacks[pinExtenderNumber][pinNumber] = std::move([ObjectPtr = lightSwitches[index]] { ObjectPtr->dealWithInterrupt(); });
        }

        index++;
    }

    /******************************************************/
    // ASSIGN MCP PIN EXTENDER DEVICE OPTIONS
    /******************************************************/
    JsonArray MCPDeviceJson = doc["MCP23017PinExtenders"]["devices"].as<JsonArray>();
    mcpDeviceCount = MCPDeviceJson.size();
    mcpInterruptHandlers = new MCPInterruptHandler*[mcpDeviceCount];
    index = 0;
    for (JsonObject mcpDevice : MCPDeviceJson) {
        int I2CAddress = mcpDevice["I2CAddress"];
        int pinNumber = mcpDevice["pinNumber"];

        mcpInterruptPins[index] = new LocalPin(pinNumber);
        // mcpInterruptPins[index]->attachInterrupt(sharedCallback, FALLING);
        mcpInterruptPins[index]->pinMode(INPUT_PULLUP);
        gpio_set_irq_enabled(pinNumber, GPIO_IRQ_EDGE_FALL, true);

        // TODO: add callback functions
        mcpInterruptHandlers[index] = new MCPInterruptHandler(I2CAddress, &mcp[index], mcpInterruptPins[index], mcpInterruptCallbacks[index]);

        localPinInterruptCallbacks[pinNumber] = std::move([ObjectPtr = mcpInterruptHandlers[index]] { ObjectPtr->callback(); });
        interruptsToCheck[pinNumber] = true;

        index++;
    }

    file.close();
}

void setup() {
    // Initialize serial
    Serial.begin(9600);
    Wire.begin();

    while (!Serial) {}

    // Initialize SD library
    while (!SD.begin()) {
        Serial.println("Failed to initialize SD library");
        delay(1000);
    }

    loadConfiguration(configFilename);

    // TODO: add PubSubClient

    for (int i = 0; i < mcpDeviceCount; i++)
    {
        mcpInterruptHandlers[i]->initialize(mcpInterruptMatrix[i]);
    }
    for (int i = 0; i < outputDeviceCount; i++)
    {
        lights[i]->initialize();
    }
    for (int i = 0; i < inputDeviceCount; i++)
    {
        lightSwitches[i]->initialize();
    }
    Serial.println("Setup done");
    delay(1000);
}

void loop() {
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
    for (size_t i = 0; i < inputDeviceCount; i++)
    {
        lightSwitches[i]->update();
    }
}