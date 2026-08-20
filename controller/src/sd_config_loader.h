#pragma once

#include <string>

namespace picohome::pico_hal {

struct SdReadResult {
    bool ok = false;
    std::string content;  // file contents when ok
    std::string error;    // human-readable reason when !ok
    // The card was readable, the file simply is not on it. Lets an optional
    // file (secrets.json) be missing without hiding a genuinely broken card.
    bool notFound = false;
};

// Reads a whole file from the SD card: keeps the W5100 off the shared SPI bus,
// mounts the filesystem, reads, unmounts. Boot-time use only (blocking).
SdReadResult readFileFromSd(const char* path);

}  // namespace picohome::pico_hal