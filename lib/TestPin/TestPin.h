/**
 * @file TestPin.h
 * @brief Declaration of TestPin: specifies functions for mock pin for testing.
 */

#ifndef TESTPIN_H
#define TESTPIN_H
#include "../AbstractPin/AbstractPin.h"

class TestPin : AbstractPin
{
    public:
        void setPinMode(int pin, int mode) override;
        void digitalWrite(int pin, int value) override;
        int digitalRead(int pin) override;
        void analogWrite(int pin, int value) override;
        int analogRead(int pin) override;
};
#endif //TESTPIN_H
