/**
 * @file AbstractPin.h
 * @brief Declaration of AbstractPin: hardware abstraction layer for microcontroller pins.
 *
 * Hardware abstraction layer for microcontroller pins.
 * This way it can be tested or replaced by i2c expander pins.
 */

#ifndef ABSTRACTPIN_H
#define ABSTRACTPIN_H

#include <Arduino.h>

class AbstractPin
{
public:
    AbstractPin() = default;

    /**
     * @brief Sets pin mode for the object.
     *
     * @param mode pin mode either INPUT or OUTPUT.
     */
    virtual void pinMode(int mode) = 0;


    /**
     * @brief Attach an interrupt to the pin.
     *
     * @param callback function to call once interrupt is triggered
     * @param mode trigger type for the interrupt
     */
    virtual void attachInterrupt(voidFuncPtr callback, int mode) = 0;

    /**
     * @brief Sets the pin to HIGH or LOW.
     *
     * @param value value to set the pin to either 0 or 1.
     */
    virtual void digitalWrite(int value) = 0;

    /**
     * @brief Reads the digital value of the pin.
     *
     * @return reading from the pin.
     */
    virtual int digitalRead() = 0;

    /**
     * @brief Writes a value for an analog functionality (or PWM) of the pin.
     *
     * @param value value to set pin to.
     */
    virtual void analogWrite(int value) = 0;

    /**
     * @brief Analog reading of the pin.
     *
     * @return pin reading.
     */
    virtual int analogRead() = 0;
    virtual ~AbstractPin() = default;
};
#endif //ABSTRACTPIN_H
