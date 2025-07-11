/**
 * @file SDCardConfig.h
 * @brief Declaration of SDCardConfig: class for reading config from SD card.
 */

#ifndef SDCARDCONFIG_H
#define SDCARDCONFIG_H
#include "../ConfigInterface/ConfigInterface.h"

class SDCardConfig : ConfigInterface
{
    public:
        SDCardConfig();
        ~SDCardConfig();
};

#endif //SDCARDCONFIG_H
