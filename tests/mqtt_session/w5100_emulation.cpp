// Host emulation of the hardware controller/src/net runs on. The pico-sdk calls
// it makes become a scaled clock, thread sleeps, a thread for core 1 and a
// locked deque per queue. The one W5100 TCP socket the MQTT session uses is a
// real TCP connection to the test broker, reported through the chip's status
// register (Sn_SR) the way the W5100 reports it:
//   connected                         -> SOCK_ESTABLISHED
//   broker sent FIN (closed its side) -> SOCK_CLOSE_WAIT, unread data still readable
//   broker sent RST, or an error      -> SOCK_CLOSED
// Every register access also costs what it does over SPI at 4 MHz — spun, as
// the pico-sdk SPI calls spin — because one of the bugs this test guards
// against is a race against exactly that time.
//
// stub/socket.h is deliberately not included: it renames close/send/recv,
// which this file needs as the POSIX calls.

#include "w5100_emulation.h"

#include <netinet/in.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

#include <atomic>
#include <chrono>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <deque>
#include <mutex>
#include <thread>
#include <vector>

#include "net/w5100_link.h"
#include "pico/multicore.h"
#include "pico/time.h"
#include "pico/util/queue.h"
#include "wizchip_conf.h"

namespace {

using Clock = std::chrono::steady_clock;
const Clock::time_point kStart = Clock::now();
std::atomic<unsigned> gTimeScale{1};

}  // namespace

// ---- pico-sdk ---------------------------------------------------------------

extern "C" absolute_time_t get_absolute_time(void) {
    const auto real = std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - kStart);
    return static_cast<absolute_time_t>(real.count()) * gTimeScale;
}

extern "C" void sleep_us(uint64_t us) {
    std::this_thread::sleep_for(std::chrono::microseconds(us / gTimeScale));
}

extern "C" void sleep_ms(uint32_t ms) { sleep_us(uint64_t{ms} * 1000); }

extern "C" void multicore_launch_core1(void (*entry)(void)) { std::thread(entry).detach(); }

struct queue_impl {
    std::mutex mutex;
    std::deque<std::vector<unsigned char>> items;
    unsigned elementSize = 0;
    unsigned capacity = 0;
};

extern "C" void queue_init(queue_t* q, unsigned element_size, unsigned element_count) {
    q->impl = new queue_impl;  // a queue lives for the whole run, as on the chip
    q->impl->elementSize = element_size;
    q->impl->capacity = element_count;
}

extern "C" bool queue_try_add(queue_t* q, const void* data) {
    std::lock_guard<std::mutex> lock(q->impl->mutex);
    if (q->impl->items.size() >= q->impl->capacity) return false;
    const auto* bytes = static_cast<const unsigned char*>(data);
    q->impl->items.emplace_back(bytes, bytes + q->impl->elementSize);
    return true;
}

extern "C" bool queue_try_remove(queue_t* q, void* data) {
    std::lock_guard<std::mutex> lock(q->impl->mutex);
    if (q->impl->items.empty()) return false;
    std::memcpy(data, q->impl->items.front().data(), q->impl->elementSize);
    q->impl->items.pop_front();
    return true;
}

// ---- W5100 socket 0 ---------------------------------------------------------
// Only core 1 touches the socket, so its state needs no lock; what the test
// thread reads is atomic.

namespace {

constexpr int8_t kSockOk = 1;                // SOCK_OK
constexpr int8_t kErrClosed = -4;            // SOCKERR_SOCKCLOSED
constexpr int32_t kErrStatus = -7;           // SOCKERR_SOCKSTATUS
constexpr uint16_t kSocketBufferSize = 2048;  // w5100Init gives each socket 2 KB

// ioLibrary moves every register byte and every buffer byte as a 4-byte SPI
// frame with its own chip select, ~14 us at 4 MHz, and wraps each recv()/send()
// in about a dozen register accesses.
constexpr unsigned kFrameUs = 14;
constexpr unsigned kTransferOverheadFrames = 12;

void spiCost(unsigned frames) {
    const auto until =
        Clock::now() + std::chrono::nanoseconds(uint64_t{frames} * kFrameUs * 1000 / gTimeScale);
    while (Clock::now() < until) {
    }
}

int gFd = -1;
uint8_t gState = SOCK_CLOSED;
uint8_t gLoggedState = 0xFF;
std::atomic<unsigned> gHangUps{0};
std::atomic<uint64_t> gLastPacketUs{0};

void logLine(const char* format, ...) {
    char line[160];
    va_list args;
    va_start(args, format);
    std::vsnprintf(line, sizeof(line), format, args);
    va_end(args);
    std::printf("[%8.3f] %s\n", picohome::emulation::deviceSeconds(), line);
}

const char* stateName(uint8_t state) {
    switch (state) {
        case SOCK_CLOSED: return "SOCK_CLOSED";
        case SOCK_INIT: return "SOCK_INIT";
        case SOCK_ESTABLISHED: return "SOCK_ESTABLISHED";
        case SOCK_CLOSE_WAIT: return "SOCK_CLOSE_WAIT";
        default: return "?";
    }
}

const char* packetName(uint8_t header) {
    static const char* const kNames[16] = {
        "reserved",  "CONNECT",  "CONNACK",     "PUBLISH",
        "PUBACK",    "PUBREC",   "PUBREL",      "PUBCOMP",
        "SUBSCRIBE", "SUBACK",   "UNSUBSCRIBE", "UNSUBACK",
        "PINGREQ",   "PINGRESP", "DISCONNECT",  "reserved",
    };
    return kNames[header >> 4];
}

void dropConnection() {
    if (gFd >= 0) ::close(gFd);
    gFd = -1;
    gState = SOCK_CLOSED;
}

// Brings gState up to what the chip would report now.
void refresh() {
    if (gState == SOCK_ESTABLISHED || gState == SOCK_CLOSE_WAIT) {
        pollfd p{gFd, POLLIN | POLLRDHUP, 0};
        ::poll(&p, 1, 0);
        if (p.revents & POLLERR) {
            dropConnection();  // RST: the chip discards the connection outright
        } else if (p.revents & (POLLRDHUP | POLLHUP)) {
            gState = SOCK_CLOSE_WAIT;
        }
    }
    if (gState != gLoggedState) {
        logLine("emu: socket 0 -> %s", stateName(gState));
        gLoggedState = gState;
    }
}

void notePacket() { gLastPacketUs = get_absolute_time(); }

// Follows packet boundaries in the received byte stream — Paho reads the fixed
// header a byte at a time — so each packet is logged once, by type.
struct RxTracker {
    enum Phase { kHeader, kLength, kBody };
    Phase phase = kHeader;
    uint8_t header = 0;
    uint32_t remaining = 0;
    uint32_t multiplier = 1;

    void feed(const uint8_t* data, std::size_t size) {
        for (std::size_t i = 0; i < size; ++i) {
            const uint8_t byte = data[i];
            switch (phase) {
                case kHeader:
                    header = byte;
                    remaining = 0;
                    multiplier = 1;
                    phase = kLength;
                    break;
                case kLength:
                    remaining += (byte & 0x7Fu) * multiplier;
                    multiplier *= 128;
                    if ((byte & 0x80u) == 0) {
                        logLine("emu: <- %s", packetName(header));
                        notePacket();
                        phase = remaining > 0 ? kBody : kHeader;
                    }
                    break;
                case kBody:
                    if (--remaining == 0) phase = kHeader;
                    break;
            }
        }
    }
};

RxTracker gRx;

}  // namespace

extern "C" uint8_t getSn_SR(uint8_t) {
    spiCost(1);
    refresh();
    return gState;
}

extern "C" uint16_t getSn_RX_RSR(uint8_t) {
    refresh();
    int available = 0;
    if (gFd >= 0) ::ioctl(gFd, FIONREAD, &available);
    spiCost(available > 0 ? 4 : 2);  // ioLibrary reads it twice when it is non-zero
    return static_cast<uint16_t>(available > kSocketBufferSize ? kSocketBufferSize : available);
}

extern "C" int8_t wiz_socket(uint8_t sn, uint8_t, uint16_t, uint8_t) {
    dropConnection();
    gFd = ::socket(AF_INET, SOCK_STREAM, 0);
    gState = SOCK_INIT;
    gRx = RxTracker{};
    refresh();
    return static_cast<int8_t>(sn);
}

extern "C" int8_t wiz_connect(uint8_t, uint8_t* address, uint16_t port) {
    sockaddr_in broker{};
    broker.sin_family = AF_INET;
    broker.sin_port = htons(port);
    std::memcpy(&broker.sin_addr, address, 4);
    if (::connect(gFd, reinterpret_cast<const sockaddr*>(&broker), sizeof(broker)) != 0) {
        dropConnection();
        refresh();
        return kErrClosed;
    }
    gState = SOCK_ESTABLISHED;
    refresh();
    return kSockOk;
}

extern "C" int32_t wiz_send(uint8_t, uint8_t* buffer, uint16_t length) {
    refresh();
    if (gState != SOCK_ESTABLISHED && gState != SOCK_CLOSE_WAIT) return kErrStatus;
    spiCost(kTransferOverheadFrames + length);
    const ssize_t sent = ::send(gFd, buffer, length, MSG_NOSIGNAL);
    if (sent < 0) {
        dropConnection();
        return kErrStatus;
    }
    // Paho writes whole packets, so the first byte is always a fixed header.
    if (length > 0) logLine("emu: -> %s", packetName(buffer[0]));
    notePacket();
    return static_cast<int32_t>(sent);
}

extern "C" int32_t wiz_recv(uint8_t, uint8_t* buffer, uint16_t length) {
    const ssize_t got = ::recv(gFd, buffer, length, 0);
    if (got <= 0) {
        dropConnection();
        return kErrStatus;
    }
    spiCost(kTransferOverheadFrames + static_cast<unsigned>(got));
    gRx.feed(buffer, static_cast<std::size_t>(got));
    return static_cast<int32_t>(got);
}

extern "C" int8_t disconnect(uint8_t) {
    refresh();
    if (gState == SOCK_ESTABLISHED) ++gHangUps;
    if (gFd >= 0) ::shutdown(gFd, SHUT_WR);  // the chip sends FIN
    return kSockOk;
}

extern "C" int8_t wiz_close(uint8_t) {
    dropConnection();
    refresh();
    return kSockOk;
}

namespace picohome::net {

// The real one resets the chip and programs its addresses; here there is
// nothing to bring up.
bool w5100Init(const config::Network&) {
    logLine("emu: W5100 up");
    return true;
}

}  // namespace picohome::net

namespace picohome::emulation {

void setTimeScale(unsigned factor) { gTimeScale = factor > 0 ? factor : 1; }

double deviceSeconds() { return static_cast<double>(get_absolute_time()) / 1e6; }

unsigned hangUps() { return gHangUps; }

double secondsSinceLastPacket() {
    return static_cast<double>(get_absolute_time() - gLastPacketUs) / 1e6;
}

}  // namespace picohome::emulation
