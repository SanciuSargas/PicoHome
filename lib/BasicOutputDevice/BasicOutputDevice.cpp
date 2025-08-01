/**
 * @file BasicOutputDevice.cpp
 * @brief Implementation of BasicOutputDevice internals.
 */


#include "BasicOutputDevice.h"

BasicOutputDevice::BasicOutputDevice(AbstractPin* pinObject, bool highTrigger, char* chanelPathForMQTT, CommunicationInterface* communicationClient)
{
    this->pinObject = pinObject;
    this->highTrigger = highTrigger;
    this->currentState = !highTrigger;
    strcpy(this->chanelPathForMQTT, chanelPathForMQTT);
    this->communicationClient = communicationClient;
}

void BasicOutputDevice::initialize()
{
    pinObject->pinMode(OUTPUT);
    pinObject->digitalWrite(!highTrigger);
    communicationClient->publish(chanelPathForMQTT, reinterpret_cast<uint8_t*>(&currentState));
}
void BasicOutputDevice::turnOn()
{
    pinObject->digitalWrite(highTrigger);
    currentState = highTrigger;
    communicationClient->publish(chanelPathForMQTT, reinterpret_cast<uint8_t*>(&currentState));
}
void BasicOutputDevice::turnOff()
{
    pinObject->digitalWrite(!highTrigger);
    currentState = !highTrigger;
    communicationClient->publish(chanelPathForMQTT, reinterpret_cast<uint8_t*>(&currentState));
}
void BasicOutputDevice::toggle()
{
    Serial.println("toggle");
    if(currentState == highTrigger)
    {
        pinObject->digitalWrite(!highTrigger);
        currentState = !highTrigger;
    } else
    {
        pinObject->digitalWrite(highTrigger);
        currentState = highTrigger;
    }
    // communicationClient->publish(chanelPathForMQTT, reinterpret_cast<uint8_t*>(&currentState));
}
int BasicOutputDevice::getState()
{
    return currentState;
}

void BasicOutputDevice::callback()
{
    // TODO: still have to think about this.
}



