// Hardware description for the no-OS-FatFS SD library: one SD card on the
// W5100 Arduino shield's slot, sharing SPI0 with the Ethernet chip.
// Pin assignments live in board_config.h.

#include "hw_config.h"

#include "board_config.h"

static spi_t spi = {
    .hw_inst = PICOHOME_SPI_INSTANCE,
    .sck_gpio = PICOHOME_SPI_SCK_GPIO,
    .mosi_gpio = PICOHOME_SPI_MOSI_GPIO,
    .miso_gpio = PICOHOME_SPI_MISO_GPIO,
    .baud_rate = PICOHOME_SD_SPI_BAUD,
};

static sd_spi_if_t spi_if = {
    .spi = &spi,
    .ss_gpio = PICOHOME_SD_CS_GPIO,
};

static sd_card_t sd_card = {
    .type = SD_IF_SPI,
    .spi_if_p = &spi_if,
};

size_t sd_get_num(void) { return 1; }

sd_card_t *sd_get_by_num(size_t num) { return num == 0 ? &sd_card : NULL; }