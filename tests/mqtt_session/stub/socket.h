// Host stand-in for ioLibrary's Ethernet/socket.h. The real API reuses POSIX
// names (socket, connect, send, recv, close), so the POSIX headers are parsed
// first and ioLibrary's names are then renamed onto the emulation.
// disconnect keeps its name: POSIX has none, and Paho's Network struct has a
// member of that name that a macro would rename too.
#pragma once

#include <stdint.h>
#include <sys/socket.h>
#include <unistd.h>

#define SOCK_OK 1
#define SOCK_ERROR 0

#define socket wiz_socket
#define connect wiz_connect
#define close wiz_close
#define send wiz_send
#define recv wiz_recv

#ifdef __cplusplus
extern "C" {
#endif

int8_t wiz_socket(uint8_t sn, uint8_t protocol, uint16_t port, uint8_t flag);
int8_t wiz_connect(uint8_t sn, uint8_t* addr, uint16_t port);
int8_t disconnect(uint8_t sn);
int8_t wiz_close(uint8_t sn);
int32_t wiz_send(uint8_t sn, uint8_t* buf, uint16_t len);
int32_t wiz_recv(uint8_t sn, uint8_t* buf, uint16_t len);

#ifdef __cplusplus
}
#endif
