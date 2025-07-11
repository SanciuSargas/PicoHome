/**
 * @file PositionControlInterface.h
 * @brief Declaration of PositionControlInterface: interface for devices with position control.
 */

#ifndef POSITIONCONTROLINTERFACE_H
#define POSITIONCONTROLINTERFACE_H

class PositionControlInterface
{
    public:
        PositionControlInterface();
        virtual void initialize() = 0;
        virtual void update() = 0;
        virtual void stop() = 0;
        virtual void setPosition(int position) = 0;
        virtual int getPosition() = 0;
        virtual ~PositionControlInterface() = default;
};
#endif //POSITIONCONTROLINTERFACE_H
