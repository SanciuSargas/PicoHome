/**
 * @brief Declaration of AbstractDigitalSensor: abstract class for digital sensors such as switches, PIR, etc.
 */

#ifndef ABSTRACTDIGITALSENSOR_H
#define ABSTRACTDIGITALSENSOR_H
#include "../CommunicationInterface/CommunicationInterface.h"

class AbstractDigitalSensor // : public Publisher
{
public:
    virtual void initialize() = 0;
    virtual void update() = 0;
    
    /**
     * @brief Handles interrupts dedicated to this device.
     */
    virtual void dealWithInterrupt() = 0;

    virtual uint16_t getMQTTDeviceID() = 0;

    virtual ~AbstractDigitalSensor() = default;
};
#endif //ABSTRACTDIGITALSENSOR_H
