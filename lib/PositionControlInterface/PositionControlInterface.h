/**
 * @file PositionControlInterface.h
 * @brief Declaration of PositionControlInterface: interface for devices with position control.
 */

#ifndef POSITIONCONTROLINTERFACE_H
#define POSITIONCONTROLINTERFACE_H

#include <Arduino.h>

class PositionControlInterface
{
    public:
        PositionControlInterface();
        virtual void initialize() = 0;
        virtual uint16_t getMQTTDeviceID() = 0;
        virtual void update() = 0;
        virtual void stop() = 0;
        virtual void setPosition(int position) = 0;
        virtual void setPosition(int position, int angle) = 0;
        virtual int getPosition() = 0;
        virtual int getAngle() = 0;
        virtual ~PositionControlInterface() = default;
};
#endif //POSITIONCONTROLINTERFACE_H
