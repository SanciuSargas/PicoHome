/**
 * @file AbstractDigitalSensor.h
 * @brief Declaration of AbstractDigitalSensor: abstract class for digital sensors such as switches, PIR, etc.
 */

#ifndef ABSTRACTDIGITALSENSOR_H
#define ABSTRACTDIGITALSENSOR_H
#include "../CommunicationInterface/CommunicationInterface.h"
#include "../Publisher/Publisher.h"

class AbstractDigitalSensor : public Publisher
{
    protected:
        virtual void initialize() = 0;
        virtual void update() = 0;
    private:
        CommunicationInterface* commInterface;
        AbstractPin* dataPin;
};
#endif //ABSTRACTDIGITALSENSOR_H
