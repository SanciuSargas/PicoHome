/**
 * @file ExpanderPin.cpp
 * @brief Implementation of ExpanderPin.
 */

#include "ExpanderPin.h"

ExpanderPin::ExpanderPin(int pinNumber, Adafruit_MCP23X17* mcp)
{
    this->pinNumber = pinNumber;
    this->mcp = mcp;
}

void ExpanderPin::attachInterrupt(voidFuncPtr callback, int mode)
{
    // TODO: add a warning that interrupts for expanderPin is handled through MCPInterruptHandler
    Serial.println("Interrupts for expanderPin are handled through MCPInterruptHandler");
}

void ExpanderPin::pinMode(int mode)
{
    ArduinoPinMode arduinoMode = static_cast<ArduinoPinMode>(mode);
    mcp->pinMode(pinNumber, arduinoMode);
}

void ExpanderPin::digitalWrite(int value)
{
    PinStatus arduinoValue = static_cast<PinStatus>(value);
    mcp->digitalWrite(pinNumber, arduinoValue);
}

int ExpanderPin::digitalRead()
{
    return mcp->digitalRead(pinNumber);
}


// i2c expender doesn't have ADC capability
void ExpanderPin::analogWrite(int value)
{
    if (value >= 1) value = 1;
    mcp->digitalWrite(pinNumber, value);
}

// i2c expender doesn't have ADC capability
int ExpanderPin::analogRead()
{
    if (mcp->digitalRead(pinNumber) == 1) return 4095;
    else return 0;
}