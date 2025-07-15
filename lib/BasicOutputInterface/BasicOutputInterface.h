/**
 * @file BasicOutputInterface.h
 * @brief Declaration of BasicOutputInterface: interface for basic (on/off) output devices such as lights, valves, etc.
 */

#ifndef BASICOUTPUTINTERFACE_H
#define BASICOUTPUTINTERFACE_H

class BasicOutputInterface
{
public:
    BasicOutputInterface() = default;

    /**
     * @brief Handles any code that needs to run at startup of the device.
     */
    virtual void initialize() = 0;

    /**
     * @brief Switches the device to the on state.
     */
    virtual void turnOn() = 0;

    /**
     * @brief Switches the device to the off state.
     */
    virtual void turnOff() = 0;

    /**
     * @brief Toggles the device from on to off state and vice versa.
     */
    virtual void toggle() = 0;

    /**
     * @brief Gets current device state.
     *
     * @return Returns current device state.
     */
    virtual int getState() = 0;
    virtual ~BasicOutputInterface() = default;
};

#endif //BASICOUTPUTINTERFACE_H
