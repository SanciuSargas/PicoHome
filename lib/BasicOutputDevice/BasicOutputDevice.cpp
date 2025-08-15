/**
 * @file BasicOutputDevice.cpp
 * @brief Implementation of BasicOutputDevice internals.
 */


#include "BasicOutputDevice.h"


BasicOutputDevice::BasicOutputDevice(AbstractPin* pinObject, bool highTrigger, char* chanelPathForMQTT,
                                     uint16_t MQTTDeviceID, CommunicationInterface* communicationClient)
{
    this->pinObject = pinObject;
    this->highTrigger = highTrigger;
    this->currentState = !highTrigger;
    strcpy(this->chanelPathForMQTT, chanelPathForMQTT);
    this->MQTTDeviceID = MQTTDeviceID;
    // this->mqttClient = mqttClient;
    this->communicationClient = communicationClient;
}

void BasicOutputDevice::initialize()
{
    pinObject->pinMode(OUTPUT);
    pinObject->digitalWrite(!highTrigger);
    // mqttClient->publish(chanelPathForMQTT, reinterpret_cast<const char*>(currentState));
}
uint16_t BasicOutputDevice::getMQTTDeviceId()
{
    return MQTTDeviceID;
}

void BasicOutputDevice::turnOn()
{
    pinObject->digitalWrite(highTrigger);
    currentState = highTrigger;
    // mqttClient->publish(chanelPathForMQTT, reinterpret_cast<uint8_t*>(&currentState));
}
void BasicOutputDevice::turnOff()
{
    pinObject->digitalWrite(!highTrigger);
    currentState = !highTrigger;
    // mqttClient->publish(chanelPathForMQTT, reinterpret_cast<uint8_t*>(&currentState));
}
void BasicOutputDevice::toggle()
{
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
    // probably don't need this
}

void BasicOutputDevice::dealWithMQTTCallback(int payload)
{
    if (payload == 1)
    {
        turnOn();
    } else if (payload == 0)
    {
        turnOff();
    } else if (payload == 2)
    {
        toggle();
    } else
    {
        Serial.println("unknown payload");
    }
}




