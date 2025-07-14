/**
 * @file LocalPin.cpp
 * @brief Implementation of LocalPin.
 */

#include "LocalPin.h"

LocalPin::LocalPin(int pinNumber) : AbstractPin()
{
    // TODO: check if pinNumber is in range
    this->pinNumber = pinNumber;
}

void LocalPin::pinMode(int mode)
{
    ArduinoPinMode arduinoMode = static_cast<ArduinoPinMode>(mode);
    ::pinMode(pinNumber, arduinoMode);
}
void LocalPin::digitalWrite(int value)
{
    // TODO: check if pin mode is OUTPUT
    PinStatus arduinoValue = static_cast<PinStatus>(value);
    ::digitalWrite(pinNumber, arduinoValue);
}
int LocalPin::digitalRead()
{
    // TODO: check if pin mode is INPUT
    return ::digitalRead(pinNumber);
}
void LocalPin::analogWrite(int value)
{
    // TODO: check if pin mode is OUTPUT
    ::analogWrite(pinNumber, value);
}
int LocalPin::analogRead()
{
    // TODO: check if pin mode is INPUT
    return ::analogRead(pinNumber);
}
