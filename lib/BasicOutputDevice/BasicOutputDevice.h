/**
 * @file BasicOutputDevice.h
 * @brief Declaration of BasicOutputDevice: class for basic (on/off) output devices such as lights, valves, etc.
 */

#ifndef BASICOUTPUTDEVICE_H
#define BASICOUTPUTDEVICE_H
#include <Arduino.h>

#include <CommunicationInterface.h>
#include <AbstractPin.h>
#include <Subscriber.h>
#include <BasicOutputInterface.h>

class BasicOutputDevice : public BasicOutputInterface, public Subscriber
{
public:
    BasicOutputDevice() = default;
    BasicOutputDevice(AbstractPin* pinObject, bool highTrigger, char* chanelPathForMQTT, uint16_t MQTTDeviceID,
                      CommunicationInterface* communicationClient);
    void initialize() override;
    uint16_t getMQTTDeviceId() override;
    void turnOn() override;
    void turnOff() override;
    void toggle() override;
    int getState() override;
    void callback() override;
    void dealWithMQTTCallback(int payload) override;
private:
    AbstractPin* pinObject = nullptr;
    bool highTrigger = false;
    CommunicationInterface* communicationClient = nullptr;
    // PubSubClient mqttClient;
    char* chanelPathForMQTT = nullptr;
    uint16_t MQTTDeviceID = 0;
    int currentState = 0;
};

#endif //BASICOUTPUTDEVICE_H
