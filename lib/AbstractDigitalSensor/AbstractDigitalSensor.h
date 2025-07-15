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
    virtual ~AbstractDigitalSensor() = default;
};
#endif //ABSTRACTDIGITALSENSOR_H
