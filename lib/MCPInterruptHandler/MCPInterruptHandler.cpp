/**
 * @file MCPInterruptHandler.cpp
 * @brief Implementation of MCPInterruptHandler.
 */

#include "MCPInterruptHandler.h"

MCPInterruptHandler::MCPInterruptHandler(int i2cAddress, Adafruit_MCP23X17* mcp, AbstractPin* interruptPin, std::array<Callback, 16> callbackFunctions)
{
    this->i2cAddress = i2cAddress;
    this->mcp = mcp;
    this->interruptPin = interruptPin;
    this->callbackFunctions = callbackFunctions;
}

void MCPInterruptHandler::initialize(bool interruptMask[16])
{
    // TODO: add check for size
    this->interruptMask = interruptMask;

    // mcp->begin_I2C(i2cAddress);
    if (!mcp->begin_I2C()) {
        Serial.println("Failed to initialize MCP23017");
        while (1);
    }
    setupInterrupts(interruptMask);
}

void MCPInterruptHandler::update()
{
    if (interruptTriggered)
    {
        dealWithInterrupts();
    }
}

void MCPInterruptHandler::callback()
{
    interruptTriggered = true;
    // TODO: make an OR operation on previous values
    triggeredInterruptMask = mcp->getTriggeredInterruptMask();
}

void MCPInterruptHandler::setupInterrupts(bool interruptMask[16])
{
    // Arguments:
    //   mirror=true: INTA/INTB share the same signal
    //   openDrain=false: Use push-pull output
    //   polarity=LOW: Active-low interrupt signal
    mcp->setupInterrupts(true, false, LOW);

    for (int i = 0; i < 16; i++)
    {
        if (interruptMask[i])
        {
            mcp->pinMode(i, INPUT_PULLUP);
            mcp->setupInterruptPin(i, CHANGE);
        }
    }
    Serial.println();
}

void MCPInterruptHandler::dealWithInterrupts()
{
    interruptTriggered = false;

    // TODO: check if this is needed (in case of an interrupt while in for loop)
    uint16_t copyOfTriggeredInterruptMask = triggeredInterruptMask;
    triggeredInterruptMask = 0;
    mcp->clearInterrupts();
    Serial.println(copyOfTriggeredInterruptMask, BIN);

    for (uint16_t pin = 0; pin < 16; pin++)
    {
        if (copyOfTriggeredInterruptMask & (1 << pin))
        {
            callbackFunctions[pin]();
        }
    }
}



