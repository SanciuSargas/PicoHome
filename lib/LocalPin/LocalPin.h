/**
 * @file LocalPin.h
 * @brief Declaration of LocalPin: specifies functions for interacting with on-chip GPIO.
 */

#ifndef LOCALPIN_H
#define LOCALPIN_H
#include <Arduino.h>
#include <AbstractPin.h>

class LocalPin : public AbstractPin
{
public:
    LocalPin(int pinNumber);
    void pinMode(int mode) override;
    void attachInterrupt(voidFuncPtr callback, int mode) override;
    void digitalWrite(int value) override;
    int digitalRead() override;
    void analogWrite(int value) override;
    int analogRead() override;
private:
    int pinNumber;
};
#endif //LOCALPIN_H
