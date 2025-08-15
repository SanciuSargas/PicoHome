/**
 * @file BasicMomentarySwitchSensor.cpp
 * @brief Implementation of BasicMomentarySwitchSensor.
 */


#include "BasicMomentarySwitchSensor.h"

BasicMomentarySwitchSensor::BasicMomentarySwitchSensor(AbstractPin* dataPinObject, char* chanelPathForMQTT, uint16_t MQTTDeviceID, Callback singlePressCallback, CommunicationInterface* commInterface)
{
    this->dataPinObject = dataPinObject;
    this->chanelPathForMQTT = chanelPathForMQTT;
    this->MQTTDeviceID = MQTTDeviceID;
    this->commInterface = commInterface;
    _singlePressCallback = std::move(singlePressCallback);
}

void BasicMomentarySwitchSensor::initialize()
{
    dataPinObject->pinMode(INPUT_PULLUP);
    currentState = dataPinObject->digitalRead();
    lastReading = currentState;
}

void BasicMomentarySwitchSensor::update()
{
    getDebouncedReading();
    checkIfPressCompleted();

    if (pressCompleted) {
        Serial.println("press completed");
        // commInterface->publish(chanelPathForMQTT, reinterpret_cast<uint8_t*>(&currentState));
        if  (_singlePressCallback) {
            _singlePressCallback();
        }
    }
}

void BasicMomentarySwitchSensor::dealWithInterrupt()
{
    interruptTriggered = true;
}

uint16_t BasicMomentarySwitchSensor::getMQTTDeviceID()
{
    return MQTTDeviceID;
}


void BasicMomentarySwitchSensor::checkIfPressCompleted()
{
    if (stateChanged && currentState == HIGH) {
        pressCompleted = true;
    } else {
        pressCompleted = false;
    }
}

void BasicMomentarySwitchSensor::flush()
{

}


void BasicMomentarySwitchSensor::getDebouncedReading()
{
    byte newReading = lastReading;

    if (interruptTriggered)
    {
        interruptTriggered = false;

        newReading = dataPinObject->digitalRead();

        if (newReading != lastReading) {
            lastDebounceTime = millis();
        }
    }

    if (millis() - lastDebounceTime > debounceDelay) {
        stateChanged = (currentState != newReading);
        currentState = newReading;
    }

    lastReading = newReading;
}


