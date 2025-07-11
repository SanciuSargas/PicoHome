/**
 * @file LocalPin.h
 * @brief Declaration of LocalPin: specifies functions for interacting with on-chip GPIO.
 */

#ifndef LOCALPIN_H
#define LOCALPIN_H
#include "../AbstractPin/AbstractPin.h"

class LocalPin : public AbstractPin
{
    public:
        LocalPin();
        void setPinMode(int pin, int mode) override;
        void digitalWrite(int pin, int value) override;
        int digitalRead(int pin) override;
        void analogWrite(int pin, int value) override;
        int analogRead(int pin) override;
};
#endif //LOCALPIN_H
