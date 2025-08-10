/**
 * @file BasicMomentarySwitchSensor.h
 * @brief Declaration of BasicMomentarySwitchSensor: class for momentary switches.
 */

#ifndef MOMENTARYSWITCHSENSOR_H
#define MOMENTARYSWITCHSENSOR_H
#include "AbstractPin.h"
#include "../AbstractDigitalSensor/AbstractDigitalSensor.h"

using Callback = std::function<void()>;

// TODO: instead of publishers/subscribers have void* functions to call on each action
class BasicMomentarySwitchSensor : public AbstractDigitalSensor
{
public:
    BasicMomentarySwitchSensor(AbstractPin* dataPinObject, char* chanelPathForMQTT, Callback singlePressCallback, CommunicationInterface* commInterface);
    void initialize() override;
    void update() override;
    void dealWithInterrupt() override;
    void checkIfPressCompleted();
    void flush();
    void getDebauncedReading();
    // void subscribe(Subscriber* subscriber) override;
    // void unsubscribe(Subscriber* subscriber) override;
    // void notifySubscribers() override;
private:
    volatile bool interruptTriggered = false;
    Callback _singlePressCallback;
    // void (*singlePressCallback)() = nullptr; // TODO: add [maxNumberOfFunctions]
    AbstractPin* dataPinObject;
    char* chanelPathForMQTT = nullptr;
    CommunicationInterface* commInterface = nullptr;
    int currentState = 0;
    int stateChanged = false;
    int pressCompleted = false;
    int lastReading = 0;
    unsigned long debounceDelay = 0;
    unsigned long lastDebounceTime = 0;
};
#endif //MOMENTARYSWITCHSENSOR_H
