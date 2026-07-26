#pragma once

#include <cstdint>

#include "picohome/config/config.h"

// W5100 bring-up on the shared SPI0 bus. Runs on core 1 only, and only after
// core 0 has finished reading the SD card at boot — after that the Ethernet
// chip is the sole user of the bus, so no arbitration is needed at runtime.

namespace picohome::net {

// Resets the chip, registers the SPI callbacks and applies the static network
// configuration. Returns false if the chip does not answer.
bool w5100Init(const config::Network& network);

}  // namespace picohome::net
