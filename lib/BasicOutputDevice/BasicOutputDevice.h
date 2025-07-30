/**
 * @file BasicOutputDevice.h
 * @brief Declaration of BasicOutputDevice: class for basic (on/off) output devices such as lights, valves, etc.
 */

#ifndef BASICOUTPUTDEVICE_H
#define BASICOUTPUTDEVICE_H
#include <Arduino.h>

#include "CommunicationInterface.h"
#include "../AbstractPin/AbstractPin.h"
#include "../Subscriber/Subscriber.h"
#include "../BasicOutputInterface/BasicOutputInterface.h"

class BasicOutputDevice : public BasicOutputInterface, public Subscriber
{
public:
    BasicOutputDevice() = default;
    BasicOutputDevice(AbstractPin* pinObject, bool highTrigger, char* chanelPathForMQTT, CommunicationInterface* communicationClient);
    void initialize() override;
    void turnOn() override;
    void turnOff() override;
    void toggle() override;
    int getState() override;
    void callback() override;
private:
    AbstractPin* pinObject;
    bool highTrigger;
    CommunicationInterface* communicationClient;
    char* chanelPathForMQTT = nullptr;
    int currentState;
};

#endif //BASICOUTPUTDEVICE_H
