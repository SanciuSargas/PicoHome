/**
 * @file SwitchSensor.h
 * @brief Declaration of SwitchSensor: class for all switches with different strategies for each switch type.
 */

#ifndef SWITCHSENSOR_H
#define SWITCHSENSOR_H
#include "../SwitchStrategyInterface/SwitchStrategyInterface.h"
#include "../AbstractDigitalSensor/AbstractDigitalSensor.h"

class SwitchSensor : public AbstractDigitalSensor
{
    public:
        SwitchSensor(SwitchStrategyInterface* switchStrat);
        void initialize() override;
        void update() override;
        void subscribe(Subscriber* subscriber) override;
        void unsubscribe(Subscriber* subscriber) override;
        void notifySubscribers() override;
        void setStrategy(SwitchStrategyInterface* switchStrat);
    private:
        SwitchStrategyInterface* switchStrat;
};
#endif //SWITCHSENSOR_H
