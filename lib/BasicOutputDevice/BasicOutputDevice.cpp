/**
 * @file BasicOutputDevice.cpp
 * @brief Implementation of BasicOutputDevice internals.
 */


#include "BasicOutputDevice.h"

BasicOutputDevice::BasicOutputDevice(AbstractPin* pinNumber) : BasicOutputInterface(), Subscriber()
{
    this->pinNumber = pinNumber;
}

void BasicOutputDevice::initialize()
{

}
void BasicOutputDevice::turnOn()
{

}
void BasicOutputDevice::turnOff()
{

}
void BasicOutputDevice::toggle()
{

}
void BasicOutputDevice::getState()
{

}

void BasicOutputDevice::callback()
{

}



