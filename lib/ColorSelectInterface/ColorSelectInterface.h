/**
 * @file ColorSelectInterface.h
 * @brief Declaration of ColorSelectInterface: interface for devices with color outputs such as RGB lights.
 */

#ifndef COLORSELECTINTERFACE_H
#define COLORSELECTINTERFACE_H

class ColorSelectInterface
{
    public:
        ColorSelectInterface();
        virtual void initialize() = 0;
        virtual void setRed() = 0;
        virtual void setGreen() = 0;
        virtual void setBlue() = 0;
        virtual int getRed() = 0;
        virtual int getGreen() = 0;
        virtual int getBlue() = 0;
        virtual ~ColorSelectInterface();
};
#endif //COLORSELECTINTERFACE_H
