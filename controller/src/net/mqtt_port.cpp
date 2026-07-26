// Platform port for the Paho MQTT client shipped inside ioLibrary_Driver.
//
// The library ships its own mqtt_interface.c, which is deliberately NOT
// compiled here, for two reasons:
//   * its Timer relies on a 1 kHz tick interrupt, which would land on core 0
//     and add jitter to the control loop for no reason — pico-sdk already has
//     a free-running microsecond clock;
//   * its read returns whatever the socket happens to hold, while Paho expects
//     a read that keeps waiting until it has the requested number of bytes or
//     the timeout expires. A packet split across two TCP segments would
//     otherwise be reported as a protocol error and drop the session.
//
// Only the vendored header is used, so MQTTClient.c links against these.

#include "pico/time.h"

extern "C" {
#include "mqtt_interface.h"
#include "socket.h"
#include "wizchip_conf.h"
}

namespace {

uint32_t nowMs() { return to_ms_since_boot(get_absolute_time()); }

// Wrap-safe comparison: the difference of two uint32_t millisecond stamps is
// correct across the ~49.7-day rollover as long as it is read as signed.
bool reached(uint32_t deadline) { return static_cast<int32_t>(nowMs() - deadline) >= 0; }

uint16_t gLocalPort = 20000;

}  // namespace

extern "C" {

// Kept only because the vendored header declares it; nothing calls it now.
void MilliTimer_Handler(void) {}

void TimerInit(Timer* timer) { timer->end_time = nowMs(); }

char TimerIsExpired(Timer* timer) {
    return reached(static_cast<uint32_t>(timer->end_time)) ? 1 : 0;
}

void TimerCountdownMS(Timer* timer, unsigned int timeout) {
    timer->end_time = nowMs() + timeout;
}

void TimerCountdown(Timer* timer, unsigned int timeout) {
    TimerCountdownMS(timer, timeout * 1000);
}

int TimerLeftMS(Timer* timer) {
    const int32_t left = static_cast<int32_t>(static_cast<uint32_t>(timer->end_time) - nowMs());
    return left < 0 ? 0 : left;
}

int w5x00_read(Network* n, unsigned char* buffer, int len, long timeoutMs) {
    const uint32_t deadline = nowMs() + static_cast<uint32_t>(timeoutMs < 0 ? 0 : timeoutMs);
    int received = 0;
    while (received < len) {
        const uint8_t status = getSn_SR(n->my_socket);
        if (status != SOCK_ESTABLISHED && status != SOCK_CLOSE_WAIT) return -1;

        const uint16_t available = getSn_RX_RSR(n->my_socket);
        if (available > 0) {
            int wanted = len - received;
            if (available < wanted) wanted = available;
            const int32_t got = recv(n->my_socket, buffer + received, wanted);
            if (got <= 0) return -1;
            received += got;
            continue;
        }
        if (reached(deadline)) break;  // caller decides whether that is fatal
        sleep_us(200);
    }
    return received;
}

int w5x00_write(Network* n, unsigned char* buffer, int len, long) {
    if (getSn_SR(n->my_socket) != SOCK_ESTABLISHED) return -1;
    const int32_t sent = send(n->my_socket, buffer, len);
    return sent < 0 ? -1 : sent;
}

void w5x00_disconnect(Network* n) {
    disconnect(n->my_socket);
    close(n->my_socket);
}

void NewNetwork(Network* n, int sn) {
    n->my_socket = sn;
    n->mqttread = w5x00_read;
    n->mqttwrite = w5x00_write;
    n->disconnect = w5x00_disconnect;
}

int ConnectNetwork(Network* n, uint8_t* ip, uint16_t port) {
    close(n->my_socket);
    // Walk the source port so a reconnect never collides with the previous
    // connection still sitting in TIME_WAIT on the broker's side.
    if (++gLocalPort < 20000) gLocalPort = 20000;
    if (socket(n->my_socket, Sn_MR_TCP, gLocalPort, 0) != n->my_socket) return SOCK_ERROR;
    if (connect(n->my_socket, ip, port) != SOCK_OK) {
        close(n->my_socket);
        return SOCK_ERROR;
    }
    return SOCK_OK;
}

}  // extern "C"
