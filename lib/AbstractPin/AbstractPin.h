/**
 * @file AbstractPin.h
 * @brief Declaration of AbstractPin: hardware abstraction layer for microcontroller pins.
 *
 * Hardware abstraction layer for microcontroller pins.
 * This way it can be tested or replaced by i2c expander pins.
 */

#ifndef ABSTRACTPIN_H
#define ABSTRACTPIN_H

class AbstractPin
{
    public:
        AbstractPin();
        virtual void setPinMode(int pin, int mode) = 0;
        virtual void digitalWrite(int pin, int value) = 0;
        virtual int digitalRead(int pin) = 0;
        virtual void analogWrite(int pin, int value) = 0;
        virtual int analogRead(int pin) = 0;
        virtual ~AbstractPin() = default;
};
#endif //ABSTRACTPIN_H
