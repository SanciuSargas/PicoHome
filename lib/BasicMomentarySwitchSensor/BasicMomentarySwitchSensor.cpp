/**
 * @file BasicMomentarySwitchSensor.cpp
 * @brief Implementation of BasicMomentarySwitchSensor.
 */


#include "BasicMomentarySwitchSensor.h"

BasicMomentarySwitchSensor::BasicMomentarySwitchSensor(AbstractPin* dataPinObject, char* chanelPathForMQTT, Callback singlePressCallback)
{
    this->dataPinObject = dataPinObject;
    this->chanelPathForMQTT = chanelPathForMQTT;
    this->commInterface = commInterface;
    _singlePressCallback = std::move(singlePressCallback);
    // this->singlePressCallback = singlePressCallback;
}

void BasicMomentarySwitchSensor::initialize()
{
    dataPinObject->pinMode(INPUT_PULLUP);
    currentState = dataPinObject->digitalRead();
    lastReading = currentState;
}

void BasicMomentarySwitchSensor::flush()
{

}

void BasicMomentarySwitchSensor::update()
{
    getDebauncedReading();
    checkIfPressCompleted();

    if (pressCompleted) {
        // commInterface->publish(chanelPathForMQTT, reinterpret_cast<uint8_t*>(&currentState));
        if  (_singlePressCallback) {
            _singlePressCallback();
        }
    }
}

void BasicMomentarySwitchSensor::checkIfPressCompleted()
{
    if (stateChanged && currentState == HIGH) {
        pressCompleted = true;
    } else {
        pressCompleted = false;
    }
}

void BasicMomentarySwitchSensor::getDebauncedReading()
{
    byte newReading = dataPinObject->digitalRead();

    if (newReading != lastReading) {
        lastDebounceTime = millis();
    }

    if (millis() - lastDebounceTime > debounceDelay) {
        stateChanged = (currentState != newReading);
        currentState = newReading;
    }

    lastReading = newReading;
}


