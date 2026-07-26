#pragma once

// Compile-time board wiring for the building controller (YD-RP2040 + W5100
// Arduino shield). These cannot come from the SD config because they are
// needed *to reach* the SD card; the SPI wiring is identical in all three
// buildings — only the JSON on the card differs.
//
// Plain #defines so both C (hw_config.c) and C++ can use them.
// Values follow the wiring proven with PicoHomeOld.

// SPI0 bus shared by the W5100 shield's Ethernet chip and SD slot.
#define PICOHOME_SPI_INSTANCE spi0
#define PICOHOME_SPI_SCK_GPIO 18
#define PICOHOME_SPI_MOSI_GPIO 19
#define PICOHOME_SPI_MISO_GPIO 16

// Chip selects + reset. The W5100 is known to hog MISO when it is not held
// in reset/deselected — strict CS discipline is mandatory on this bus.
#define PICOHOME_SD_CS_GPIO 17
#define PICOHOME_W5100_CS_GPIO 10
#define PICOHOME_W5100_RESET_GPIO 14  // active low

// 1 MHz is the speed the shield is verified working at (2026-07-25) and is kept
// deliberately: the config file is read once at boot and is ~1 KB, so even at
// this clock the read costs single-digit milliseconds. Nothing to gain by
// pushing jumper wires faster.
#define PICOHOME_SD_SPI_BAUD (1 * 1000 * 1000)

// The W5100 tolerates a faster clock than the SD card does on these jumpers;
// core 1 raises the bus to this once the config has been read.
#define PICOHOME_W5100_SPI_BAUD (4 * 1000 * 1000)

#define PICOHOME_HEARTBEAT_LED_GPIO 25  // YD-RP2040 user LED (WS2812 on GPIO23 later)

#define PICOHOME_CONFIG_PATH "/config.json"
