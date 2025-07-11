/**
 * @file BasicOutputDevice.h
 * @brief Declaration of BasicOutputDevice: class for basic (on/off) output devices such as lights, valves, etc.
 */

#ifndef BASICOUTPUTDEVICE_H
#define BASICOUTPUTDEVICE_H
#include "../AbstractPin/AbstractPin.h"
#include "../Subscriber/Subscriber.h"
#include "../BasicOutputInterface/BasicOutputInterface.h"

class BasicOutputDevice : public BasicOutputInterface, public Subscriber
{
    public:
        BasicOutputDevice(AbstractPin* pinNumber);
        void initialize() override;
        void turnOn() override;
        void turnOff() override;
        void toggle() override;
        void getState() override;
        void callback() override;
private:
        AbstractPin* pinNumber;
};

#endif //BASICOUTPUTDEVICE_H
