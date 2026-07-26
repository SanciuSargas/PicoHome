#include "net/w5100_link.h"

#include <cstdio>
#include <cstring>

#include "hardware/gpio.h"
#include "hardware/spi.h"
#include "pico/time.h"

extern "C" {
#include "socket.h"
#include "wizchip_conf.h"
}

#include "board_config.h"

namespace picohome::net {

namespace {

void csSelect() {
    gpio_put(PICOHOME_W5100_CS_GPIO, false);
    __asm volatile("nop \n nop \n nop");
}

void csDeselect() {
    __asm volatile("nop \n nop \n nop");
    gpio_put(PICOHOME_W5100_CS_GPIO, true);
}

uint8_t spiReadByte() {
    uint8_t value = 0;
    spi_read_blocking(PICOHOME_SPI_INSTANCE, 0xFF, &value, 1);
    return value;
}

void spiWriteByte(uint8_t value) { spi_write_blocking(PICOHOME_SPI_INSTANCE, &value, 1); }

// The ioLibrary calls these around every register access. Only core 1 talks to
// the chip, so there is nothing to lock against — but the hooks must exist.
void crisEnter() {}
void crisExit() {}

// Parses "192.168.1.201" into four octets.
bool parseIpv4(const std::string& text, uint8_t out[4]) {
    unsigned a = 0, b = 0, c = 0, d = 0;
    if (std::sscanf(text.c_str(), "%u.%u.%u.%u", &a, &b, &c, &d) != 4) return false;
    if (a > 255 || b > 255 || c > 255 || d > 255) return false;
    out[0] = static_cast<uint8_t>(a);
    out[1] = static_cast<uint8_t>(b);
    out[2] = static_cast<uint8_t>(c);
    out[3] = static_cast<uint8_t>(d);
    return true;
}

}  // namespace

bool w5100Init(const config::Network& network) {
    // The SD loader left both chip selects high and the W5100 out of reset.
    // Give the chip a clean reset pulse of its own before configuring it.
    gpio_init(PICOHOME_W5100_CS_GPIO);
    gpio_set_dir(PICOHOME_W5100_CS_GPIO, GPIO_OUT);
    gpio_put(PICOHOME_W5100_CS_GPIO, true);

    gpio_init(PICOHOME_SD_CS_GPIO);
    gpio_set_dir(PICOHOME_SD_CS_GPIO, GPIO_OUT);
    gpio_put(PICOHOME_SD_CS_GPIO, true);  // keep the card off the bus

    gpio_init(PICOHOME_W5100_RESET_GPIO);
    gpio_set_dir(PICOHOME_W5100_RESET_GPIO, GPIO_OUT);
    gpio_put(PICOHOME_W5100_RESET_GPIO, false);
    sleep_ms(10);
    gpio_put(PICOHOME_W5100_RESET_GPIO, true);
    sleep_ms(200);  // datasheet: PLL lock after reset

    spi_set_baudrate(PICOHOME_SPI_INSTANCE, PICOHOME_W5100_SPI_BAUD);

    reg_wizchip_cris_cbfunc(crisEnter, crisExit);
    reg_wizchip_cs_cbfunc(csSelect, csDeselect);
    reg_wizchip_spi_cbfunc(spiReadByte, spiWriteByte);

    // W5100 has four sockets sharing 8 KB each way: 2 KB per socket.
    uint8_t txSizes[4] = {2, 2, 2, 2};
    uint8_t rxSizes[4] = {2, 2, 2, 2};
    if (wizchip_init(txSizes, rxSizes) != 0) {
        std::printf("net: wizchip_init failed (check wiring/power)\n");
        return false;
    }

    wiz_NetInfo info;
    std::memset(&info, 0, sizeof(info));
    std::memcpy(info.mac, network.mac, sizeof(info.mac));
    if (!parseIpv4(network.ip, info.ip) || !parseIpv4(network.netmask, info.sn) ||
        !parseIpv4(network.gateway, info.gw)) {
        std::printf("net: bad ip/netmask/gateway in config\n");
        return false;
    }
    info.dhcp = NETINFO_STATIC;
    wizchip_setnetinfo(&info);

    // Read it back: a chip that is not really there returns zeros or 0xFF.
    wiz_NetInfo check;
    wizchip_getnetinfo(&check);
    if (std::memcmp(check.ip, info.ip, 4) != 0) {
        std::printf("net: W5100 not responding (readback mismatch)\n");
        return false;
    }

    std::printf("net: W5100 up at %u.%u.%u.%u\n", info.ip[0], info.ip[1], info.ip[2], info.ip[3]);
    return true;
}

}  // namespace picohome::net
