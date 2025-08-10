/**
 * @file MCPInterruptHandler.h
 * @brief Declaration of MCPInterruptHandler: class to deal with MCP23017 interrupts.
 */

#ifndef MCPINTERRUPTHANDLER_H
#define MCPINTERRUPTHANDLER_H

#include "AbstractPin.h"
#include "Adafruit_MCP23X17.h"
#include <array>

using Callback = std::function<void()>;

class MCPInterruptHandler
{
public:
    MCPInterruptHandler(int i2cAddress, Adafruit_MCP23X17* mcp, AbstractPin* interruptPin, std::array<Callback, 16> callbackFunctions);
    void initialize(bool interruptMask[16]);
    void update();
    void callback();
private:
    void setupInterrupts(bool interruptMask[16]);
    void dealWithInterrupts();
    Adafruit_MCP23X17* mcp;
    AbstractPin* interruptPin;
    int i2cAddress;
    volatile bool interruptTriggered = false;
    uint16_t triggeredInterruptMask = 0;
    std::array<Callback, 16> callbackFunctions;
    bool *interruptMask = nullptr;
};

#endif //MCPINTERRUPTHANDLER_H
