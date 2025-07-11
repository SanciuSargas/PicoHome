/**
 * @file DimmableInterface.h
 * @brief Declaration of DimmableInterface: interface for dimmable devices.
 */

#ifndef DIMMIBLEINTERFACE_H
#define DIMMIBLEINTERFACE_H

class DimmableInterface
{
    public:
        DimmableInterface();
        virtual void initialize() = 0;
        virtual void increaseBrightness() = 0;
        virtual void decreaseBrightness() = 0;
        virtual int getBrightness() = 0;
        virtual void setBrightness(int value) = 0;
        virtual ~DimmableInterface() = default;
};

#endif //DIMMIBLEINTERFACE_H
