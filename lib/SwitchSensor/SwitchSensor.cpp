/**
 * @file SwitchSensor.cpp
 * @brief Implementation of SwitchSensor.
 */


#include "SwitchSensor.h"

SwitchSensor::SwitchSensor(SwitchStrategyInterface* switchStrat)
{
    this->switchStrat = switchStrat;
}
