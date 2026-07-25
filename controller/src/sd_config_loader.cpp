#include "sd_config_loader.h"

#include "f_util.h"
#include "ff.h"
#include "hardware/gpio.h"
#include "pico/time.h"

#include "board_config.h"

namespace picohome::pico_hal {

namespace {

// Prepares the shared SPI0 bus for the boot-time SD transaction.
//
// Counter-intuitively the W5100 must be left *out* of reset: holding RESET low
// (to tri-state it off MISO) makes f_mount fail with FR_NOT_READY on this
// hardware. The sequence proven on this shield with PicoHomeOld is simply
// "both chip selects high, W5100 reset inactive-high, then talk to the card".
// M5 (Ethernet) takes over the reset lifecycle after boot.
void prepareSpiBusForSd() {
    gpio_init(PICOHOME_W5100_RESET_GPIO);
    gpio_set_dir(PICOHOME_W5100_RESET_GPIO, GPIO_OUT);
    gpio_put(PICOHOME_W5100_RESET_GPIO, true);  // inactive — chip runs, merely deselected

    gpio_init(PICOHOME_W5100_CS_GPIO);
    gpio_set_dir(PICOHOME_W5100_CS_GPIO, GPIO_OUT);
    gpio_put(PICOHOME_W5100_CS_GPIO, true);  // deselected

    // Deselect the card too until the FatFS driver claims the pin.
    gpio_init(PICOHOME_SD_CS_GPIO);
    gpio_set_dir(PICOHOME_SD_CS_GPIO, GPIO_OUT);
    gpio_put(PICOHOME_SD_CS_GPIO, true);

    sleep_ms(100);  // let the shield settle after power-up before using the bus
}

std::string fresultError(const char* operation, FRESULT fr) {
    std::string message = operation;
    message += ": ";
    message += FRESULT_str(fr);
    return message;
}

}  // namespace

SdReadResult readFileFromSd(const char* path) {
    SdReadResult result;

    prepareSpiBusForSd();

    // Cards on this shield regularly need a couple of attempts to come up
    // (PicoHomeOld retried in a loop); only a persistent failure is an error.
    constexpr int kMountAttempts = 5;
    FATFS fs;
    FRESULT fr = FR_NOT_READY;
    for (int attempt = 0; attempt < kMountAttempts; ++attempt) {
        fr = f_mount(&fs, "", 1);
        if (fr == FR_OK) break;
        sleep_ms(250);
    }
    if (fr != FR_OK) {
        result.error = fresultError("f_mount", fr);
        if (fr == FR_NOT_READY) {
            // Most common cause once wiring is proven: the driver only supports
            // block-addressed cards, so SDSC (<=2 GB) is rejected after a
            // successful card init.
            result.error += " (card must be SDHC, 4-32 GB, FAT32)";
        }
        return result;
    }

    FIL file;
    fr = f_open(&file, path, FA_READ);
    if (fr != FR_OK) {
        result.error = fresultError("f_open", fr);
        f_unmount("");
        return result;
    }

    result.content.resize(f_size(&file));
    UINT bytesRead = 0;
    fr = f_read(&file, result.content.data(), result.content.size(), &bytesRead);
    f_close(&file);
    f_unmount("");

    if (fr != FR_OK) {
        result.error = fresultError("f_read", fr);
        result.content.clear();
        return result;
    }
    if (bytesRead != result.content.size()) {
        result.error = "f_read: short read";
        result.content.clear();
        return result;
    }

    result.ok = true;
    return result;
}

}  // namespace picohome::pico_hal