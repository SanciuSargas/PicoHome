/**
 * @file ExpanderPin.h
 * @brief Declaration of ExpanderPin: specifies functions for interacting with i2c GPIO expander mcp23017.
 */

#ifndef EXPANDERPIN_H
#define EXPANDERPIN_H
#include "Adafruit_MCP23X17.h"
#include "../AbstractPin/AbstractPin.h"

class ExpanderPin : public AbstractPin
{
public:
    ExpanderPin(int pinNumber, Adafruit_MCP23X17* mcp);
    void attachInterrupt(voidFuncPtr callback, int mode) override;
    void pinMode(int mode) override;
    void digitalWrite(int value) override;
    int digitalRead() override;
    void analogWrite(int value) override;
    int analogRead() override;
private:
    int pinNumber;
    Adafruit_MCP23X17* mcp;
};
#endif //EXPANDERPIN_H
