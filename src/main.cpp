/**
 * @file main.cpp
 * @brief Main program for homo automation microcontrollers.
 */

#include <Arduino.h>
#include <functional>
#include <SD.h>
#include <ArduinoJson.h>

#include <BasicOutputInterface.h>
#include <BasicOutputDevice.h>
#include <BasicMomentarySwitchSensor.h>
#include <LocalPin.h>
#include <MQTTCommunication.h>

// MQTT details
char* mqttServer;
char* mqttClientID;

const char* configFilename = "/config.txt";

const char* controlerNameMQTT;
size_t outputDeviceCount = 0;
size_t inputDeviceCount = 0;

BasicOutputInterface** lights = nullptr;
AbstractDigitalSensor** lightSwitches = nullptr;
CommunicationInterface* communicationClient;

// Loads the configuration from a file
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

    controlerNameMQTT = doc["controlerName"];

    JsonArray outputDeviceJson = doc["outputDevices"]["basicOutputDevices"].as<JsonArray>();
    outputDeviceCount = outputDeviceJson.size();

    lights = new BasicOutputInterface*[outputDeviceCount];

    size_t index = 0;
    for (JsonObject outputDevice : outputDeviceJson) {
        // char* deviceTypeMQTT = jsonOutputDevices["deviceTypeMQTT"];
        char* deviceTypeMQTT = nullptr;
        strcpy(deviceTypeMQTT, outputDevice["deviceTypeMQTT"]);
        int idMQTT = outputDevice["idMQTT"]; // 29, 30
        int pinNumber = outputDevice["pinNumber"]; // 3, 4
        int relayTrigerMode = outputDevice["relayTrigerMode"];

        communicationClient = new MQTTCommunication();
        lights[index] = new BasicOutputDevice(new LocalPin(pinNumber), relayTrigerMode, deviceTypeMQTT, communicationClient);
        index++;
    }

    JsonArray inputDeviceJson = doc["inputDevices"]["basicInputDevices"]["devices"].as<JsonArray>();
    inputDeviceCount = outputDeviceJson.size();

    lightSwitches = new AbstractDigitalSensor*[inputDeviceCount];

    index = 0;
    for (JsonObject inputDevice : inputDeviceJson) {
        char* deviceTypeMQTT = nullptr;
        strcpy(deviceTypeMQTT, inputDevice["deviceTypeMQTT"]);
        int idMQTT = inputDevice["idMQTT"];
        int pinNumber = inputDevice["pinNumber"];

        // TODO: add switch assignment functions to json (brain no worky)
        lightSwitches[index] = new BasicMomentarySwitchSensor(new LocalPin(pinNumber), deviceTypeMQTT, [ObjectPtr = lights[index]] { ObjectPtr->toggle(); }, communicationClient);
        index++;
    }
    file.close();
}

void setup() {
    // Initialize serial
    Serial.begin(9600);
    while (!Serial) continue;

    // Initialize SD library
    while (!SD.begin()) {
        Serial.println("Failed to initialize SD library");
        delay(1000);
    }

    Serial.println("Loading configuration...");
    loadConfiguration(configFilename);

    // TODO: add PubSubClient
    for (int i = 0; i < outputDeviceCount; i++)
    {
        lights[i]->initialize();
    }
    for (int i = 0; i < inputDeviceCount; i++)
    {
        lightSwitches[i]->initialize();
    }
    Serial.println("Setup done");
}

void loop() {
    for (int i = 0; i < inputDeviceCount; i++)
    {
        lightSwitches[i]->update();
    }
}