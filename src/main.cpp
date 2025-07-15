/**
 * @file main.cpp
 * @brief Main program for homo automation microcontrollers.
 */

#include <Arduino.h>
#include <functional>

#include "../lib/BasicOutputDevice/BasicOutputDevice.h"
#include "../lib/BasicOutputInterface/BasicOutputInterface.h"
#include "../lib/AbstractDigitalSensor/AbstractDigitalSensor.h"
#include "../lib/LocalPin/LocalPin.h"
#include "../lib/AbstractPin/AbstractPin.h"
#include "../lib/BasicMomentarySwitchSensor/BasicMomentarySwitchSensor.h"
#include "../lib/MQTTCommunication/MQTTCommunication.h"
#include "../lib/CommunicationInterface/CommunicationInterface.h"


BasicOutputDevice* light;
AbstractDigitalSensor* lightSwitch;
AbstractPin* outputPin;
AbstractPin* inputPin;
// CommunicationInterface* communicationClient;

void setup() {
    Serial.begin(9600);
    while (!Serial)
    {
        delay(1);
    }

    // TODO: construct devices from json file
    outputPin = new LocalPin(5);
    inputPin = new LocalPin(4);
    CommunicationInterface* communicationClient = new MQTTCommunication();
    char path[5] = "1234";
    light = new BasicOutputDevice(outputPin, 1, path, communicationClient);
    lightSwitch = new BasicMomentarySwitchSensor(inputPin, path, [ObjectPtr = light] { ObjectPtr->toggle(); });
    light->initialize();
    lightSwitch->initialize();
    Serial.println("Setup done");
}

void loop() {
    lightSwitch->update();
}