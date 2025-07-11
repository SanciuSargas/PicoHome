/**
 * @file main.cpp
 * @brief Main program for homo automation microcontrollers.
 */

#include <Arduino.h>

#include "../lib/BasicOutputDevice/BasicOutputDevice.h"
#include "../lib/BasicOutputInterface/BasicOutputInterface.h"
#include "../lib/AbstractDigitalSensor/AbstractDigitalSensor.h"
#include "../lib/SwitchStrategyInterface/SwitchStrategyInterface.h"
#include "../lib/BasicToggleSwitchStrategy/BasicToggleSwitchStrategy.h"
#include "../lib/LocalPin/LocalPin.h"
#include "../lib/AbstractPin/AbstractPin.h"
#include "../lib/SwitchSensor/SwitchSensor.h"


BasicOutputInterface* light1;
AbstractDigitalSensor* switch1;
SwitchStrategyInterface* switchStrategy;
AbstractPin* pin1;

void setup() {
// write your initialization code here
    // TODO: construct devices from json file
    switchStrategy = new BasicToggleSwitchStrategy();
    pin1 = new LocalPin();
    light1 = new BasicOutputDevice(pin1);
    switch1 = new SwitchSensor(switchStrategy);
}

void loop() {
// write your code here
}