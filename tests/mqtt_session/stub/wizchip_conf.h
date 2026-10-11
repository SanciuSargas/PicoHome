// Host stand-in for ioLibrary's wizchip_conf.h / W5100/w5100.h: the socket
// status values and the two socket registers the MQTT session reads.
#pragma once

#include <stdint.h>

#define Sn_MR_TCP 0x01

#define SOCK_CLOSED 0x00
#define SOCK_INIT 0x13
#define SOCK_ESTABLISHED 0x17
#define SOCK_CLOSE_WAIT 0x1C

#ifdef __cplusplus
extern "C" {
#endif

uint8_t getSn_SR(uint8_t sn);
uint16_t getSn_RX_RSR(uint8_t sn);

#ifdef __cplusplus
}
#endif
