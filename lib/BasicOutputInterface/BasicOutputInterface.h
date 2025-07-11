/**
 * @file BasicOutputInterface.h
 * @brief Declaration of BasicOutputInterface: interface for basic (on/off) output devices such as lights, valves, etc.
 */

#ifndef BASICOUTPUTINTERFACE_H
#define BASICOUTPUTINTERFACE_H

class BasicOutputInterface
{
    public:
        BasicOutputInterface();
        virtual void initialize() = 0;
        virtual void turnOn() = 0;
        virtual void turnOff() = 0;
        virtual void toggle() = 0;
        virtual void getState() = 0;
        virtual ~BasicOutputInterface();
};

#endif //BASICOUTPUTINTERFACE_H
