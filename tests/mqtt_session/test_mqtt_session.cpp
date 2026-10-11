// Integration test of the controller's MQTT session, on the host: the
// firmware's own controller/src/net (net_core.cpp, mqtt_port.cpp) and the
// vendored Paho client, unmodified, against a real mosquitto. Only the hardware
// is emulated (w5100_emulation.cpp). Core 0 is a real Relay behind the real
// CommandRouter, so a command counts only once it has reached a device.
//
// Each scenario guards against one way a session used to die without the
// controller noticing (CLAUDE.md, M5). Core 1 never returns, so a scenario
// needs a process of its own and ctest starts this binary once per scenario:
//
//   mqtt_session_test <scenario> <mosquitto executable>
//
// Exits 77 — reported by ctest as skipped — when mosquitto is not installed.

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <signal.h>
#include <sys/prctl.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include <memory>
#include <random>
#include <string>
#include <thread>

extern "C" {
#include "MQTTPacket.h"
}

#include "mocks/fakes.h"
#include "net/net_core.h"
#include "picohome/config/config.h"
#include "picohome/control/event_router.h"
#include "picohome/core/device_registry.h"
#include "picohome/devices/relay.h"
#include "picohome/net/command_router.h"
#include "picohome/net/publisher.h"
#include "picohome/net/remote_sources.h"
#include "w5100_emulation.h"

using namespace picohome;

namespace {

constexpr char kBuilding[] = "testbench";
constexpr char kCommandTopic[] = "testbench/kitchen/light/29/set_binary";
constexpr int kSkipped = 77;

void say(const char* text) { std::printf("[%8.3f] TEST: %s\n", emulation::deviceSeconds(), text); }

// Ends the scenario, reporting why, when `condition` does not hold.
#define EXPECT(condition, ...)                                                        \
    do {                                                                              \
        if (!(condition)) {                                                           \
            std::printf("[%8.3f] FAILED: ", emulation::deviceSeconds());              \
            std::printf(__VA_ARGS__);                                                 \
            std::printf("\n           (%s, %s:%d)\n", #condition, __FILE__, __LINE__); \
            return false;                                                             \
        }                                                                             \
    } while (false)

// Polls until `condition` holds, for at most `seconds` of device time.
bool waitFor(const std::function<bool()>& condition, double seconds) {
    const double deadline = emulation::deviceSeconds() + seconds;
    while (!condition()) {
        if (emulation::deviceSeconds() > deadline) return false;
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    return true;
}

sockaddr_in loopback(uint16_t port) {
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    return address;
}

// A private mosquitto on a free loopback port, logging to a file of its own.
class Broker {
public:
    Broker(std::string executable, const std::filesystem::path& directory)
        : executable_(std::move(executable)),
          config_((directory / "mosquitto.conf").string()),
          log_((directory / "broker.log").string()),
          port_(freePort()) {
        std::ofstream(config_) << "listener " << port_ << " 127.0.0.1\n"
                               << "allow_anonymous true\n"
                               << "persistence false\n"
                               << "log_dest stdout\n";
    }
    ~Broker() { stop(); }

    uint16_t port() const { return port_; }

    bool start() {
        pid_ = ::fork();
        if (pid_ == 0) {
            ::prctl(PR_SET_PDEATHSIG, SIGKILL);  // never outlive the test
            const int log = ::open(log_.c_str(), O_WRONLY | O_CREAT | O_APPEND, 0644);
            ::dup2(log, STDOUT_FILENO);
            ::dup2(log, STDERR_FILENO);
            ::execl(executable_.c_str(), executable_.c_str(), "-c", config_.c_str(), nullptr);
            ::_exit(127);
        }
        return pid_ > 0 && waitUntilListening();
    }

    void stop() {
        if (pid_ <= 0) return;
        ::kill(pid_, SIGCONT);  // a frozen process cannot act on SIGTERM
        ::kill(pid_, SIGTERM);
        ::waitpid(pid_, nullptr, 0);
        pid_ = -1;
    }

    bool restart() {
        stop();
        return start();
    }
    void freeze() { ::kill(pid_, SIGSTOP); }
    void thaw() { ::kill(pid_, SIGCONT); }

private:
    static uint16_t freePort() {
        const int fd = ::socket(AF_INET, SOCK_STREAM, 0);
        sockaddr_in address = loopback(0);
        socklen_t size = sizeof(address);
        ::bind(fd, reinterpret_cast<sockaddr*>(&address), size);
        ::getsockname(fd, reinterpret_cast<sockaddr*>(&address), &size);
        ::close(fd);
        return ntohs(address.sin_port);
    }

    bool waitUntilListening() {
        for (int attempt = 0; attempt < 500; ++attempt) {
            if (::waitpid(pid_, nullptr, WNOHANG) == pid_) {
                pid_ = -1;  // it exited: bad config, or the port was taken
                return false;
            }
            const int fd = ::socket(AF_INET, SOCK_STREAM, 0);
            const sockaddr_in address = loopback(port_);
            const bool up =
                ::connect(fd, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) == 0;
            ::close(fd);
            if (up) return true;
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        return false;
    }

    std::string executable_;
    std::string config_;
    std::string log_;
    uint16_t port_;
    pid_t pid_ = -1;
};

bool sendAll(int fd, const unsigned char* packet, int length) {
    return length > 0 && ::send(fd, packet, length, MSG_NOSIGNAL) == length;
}

// CONNACK and PUBACK are both exactly four bytes.
bool receiveAck(int fd, int type) {
    unsigned char ack[4];
    return ::recv(fd, ack, sizeof(ack), MSG_WAITALL) == sizeof(ack) && ack[0] >> 4 == type &&
           (type != CONNACK || ack[3] == 0);
}

// Home Assistant's side: one QoS 1 command on a connection of its own, the way
// mosquitto_pub sends one. Returns once the broker has acknowledged it.
bool publishCommand(uint16_t port, const char* payload) {
    const int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    const timeval timeout{2, 0};
    ::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    const sockaddr_in broker = loopback(port);
    unsigned char packet[128];

    MQTTPacket_connectData connect = MQTTPacket_connectData_initializer;
    connect.MQTTVersion = 4;
    connect.clientID.cstring = const_cast<char*>("home_assistant");
    MQTTString topic = MQTTString_initializer;
    topic.cstring = const_cast<char*>(kCommandTopic);

    const bool ok =
        ::connect(fd, reinterpret_cast<const sockaddr*>(&broker), sizeof(broker)) == 0 &&
        sendAll(fd, packet, MQTTSerialize_connect(packet, sizeof(packet), &connect)) &&
        receiveAck(fd, CONNACK) &&
        sendAll(fd, packet,
                MQTTSerialize_publish(packet, sizeof(packet), 0, 1, 0, 1, topic,
                                      reinterpret_cast<unsigned char*>(const_cast<char*>(payload)),
                                      static_cast<int>(std::strlen(payload)))) &&
        receiveAck(fd, PUBACK);
    if (ok) sendAll(fd, packet, MQTTSerialize_disconnect(packet, sizeof(packet)));
    ::close(fd);
    return ok;
}

// What core 0 has seen. Core 1 asks for a full state refresh once a session is
// subscribed, so a completed session setup counts exactly once.
std::atomic<unsigned> gDelivered{0};
std::atomic<unsigned> gSessions{0};

// Core 0 as main.cpp runs it, cut down to one light.
void core0() {
    test::FakeDigitalOutput pin;
    DeviceRegistry registry;
    EventRouter router;
    registry.add(std::make_unique<Relay>(DeviceType::Light, 29, "kitchen", pin));
    QueuePublisher publisher(net::publishSink(), kBuilding);
    registry.attachPublisher(&publisher);
    registry.initializeAll();
    CommandRouter commands(registry, router);

    while (true) {
        InboundMessage inbound;
        while (net::popInbound(inbound)) {
            const uint32_t unrouted = commands.unroutedCount();
            commands.handle(inbound);
            if (inbound.kind == InboundKind::PublishAllStates) ++gSessions;
            if (inbound.kind == InboundKind::DeviceCommand &&
                commands.unroutedCount() == unrouted) {
                std::printf("[%8.3f] core0: %s '%s'\n", emulation::deviceSeconds(),
                            inbound.command, inbound.payload);
                ++gDelivered;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

// Publishes a command as Home Assistant would and waits for core 0 to act on it.
bool commandGetsThrough(const Broker& broker, const char* payload) {
    const unsigned before = gDelivered;
    return publishCommand(broker.port(), payload) &&
           waitFor([before] { return gDelivered > before; }, 5.0);
}

// Waits until no packet has crossed the link for a second, so nothing is in
// flight. A broker fault that hits a publish still waiting for its PUBACK is
// noticed through that publish failing, which would hide what a scenario is
// testing — a controller that is idle, as it is most of the time.
bool linkGoesQuiet() {
    return waitFor([] { return emulation::secondsSinceLastPacket() > 1.0; }, 10.0);
}

// The broker restarts — an add-on update, a host reboot. It closes every
// connection with a FIN, leaving the W5100 socket in CLOSE_WAIT: nothing will
// ever arrive on it again, and nothing is published unless someone happens to
// press a switch. The controller has to notice from the socket alone.
bool brokerRestart(Broker& broker) {
    EXPECT(commandGetsThrough(broker, "1"), "a command did not get through on a fresh session");
    EXPECT(linkGoesQuiet(), "the MQTT link never went quiet");
    say("broker restarts");
    EXPECT(broker.restart(), "mosquitto did not come back up");
    EXPECT(waitFor([] { return gSessions >= 2; }, 10.0),
           "the controller never reconnected: it is still on the connection the broker closed");
    EXPECT(commandGetsThrough(broker, "0"), "a command after the restart did not get through");
    return true;
}

// Commands arriving at random moments. Paho acknowledges a QoS 1 message on the
// timer it spent waiting for it, and reading one over SPI takes over a
// millisecond, so a command that arrived late in a short poll window used to
// find that timer expired: no PUBACK, a dropped session, and every command
// sent during the reconnect lost.
bool qos1Burst(Broker& broker) {
    constexpr unsigned kCommands = 60;
    // Gaps at microsecond resolution: whole-millisecond gaps can phase-lock to
    // the controller's polling and land every command at the same point of it.
    std::mt19937 random(2026);
    std::uniform_int_distribution<int> gapUs(20000, 120000);
    for (unsigned i = 0; i < kCommands; ++i) {
        std::this_thread::sleep_for(std::chrono::microseconds(gapUs(random)));
        EXPECT(publishCommand(broker.port(), i % 2 == 0 ? "1" : "0"),
               "the broker did not accept command %u", i);
    }
    // Not "until all are delivered": a session dropped on the last command has
    // delivered it already, and only shows once it reconnects — after the 1 s
    // backoff and the reconnect itself.
    std::this_thread::sleep_for(std::chrono::seconds(3));
    const unsigned sessions = gSessions;
    const unsigned delivered = gDelivered;
    EXPECT(sessions == 1, "the controller dropped its session %u time(s) during the burst",
           sessions - 1);
    EXPECT(delivered == kCommands, "%u of %u commands never reached core 0",
           kCommands - delivered, kCommands);
    return true;
}

// The broker freezes (SIGSTOP): TCP stays up — the kernel still acknowledges
// every segment — but no MQTT packet is answered. The controller pings after
// 30 s of quiet and has to give up one keepalive interval later.
bool brokerFrozen(Broker& broker) {
    EXPECT(linkGoesQuiet(), "the MQTT link never went quiet");
    const unsigned hangUps = emulation::hangUps();
    say("broker frozen: TCP stays open, nothing is answered");
    broker.freeze();
    EXPECT(waitFor([hangUps] { return emulation::hangUps() > hangUps; }, 75.0),
           "the controller kept its session to a broker that answered nothing for 75 s");
    say("broker resumes");
    broker.thaw();
    EXPECT(waitFor([] { return gSessions >= 2; }, 30.0),
           "the controller did not reconnect after the broker resumed");
    EXPECT(commandGetsThrough(broker, "1"), "a command after the broker resumed did not get through");
    return true;
}

struct Scenario {
    const char* name;
    unsigned timeScale;
    bool (*run)(Broker&);
};

constexpr Scenario kScenarios[] = {
    {"broker_restart", 1, brokerRestart},
    {"qos1_burst", 1, qos1Burst},
    // Device time at 10x: the keepalive arithmetic alone takes a minute of it.
    {"broker_frozen", 10, brokerFrozen},
};

// Read by core 1 for the whole run.
config::App gConfig;
RemoteSourceTable gRemoteSources;

bool run(const Scenario& scenario, Broker& broker) {
    EXPECT(broker.start(), "mosquitto did not start (its log is below)");
    gConfig.building = kBuilding;
    gConfig.network.broker = "127.0.0.1";
    gConfig.network.brokerPort = broker.port();

    net::initQueues();
    std::thread(core0).detach();
    net::launch(gConfig, gRemoteSources, /*bootError=*/"");
    EXPECT(waitFor([] { return gSessions >= 1; }, 10.0),
           "the controller never completed its first MQTT session");
    return scenario.run(broker);
}

}  // namespace

int main(int argc, char** argv) {
    std::setvbuf(stdout, nullptr, _IOLBF, 0);

    const Scenario* scenario = nullptr;
    for (const Scenario& candidate : kScenarios) {
        if (argc == 3 && std::strcmp(argv[1], candidate.name) == 0) scenario = &candidate;
    }
    if (scenario == nullptr) {
        std::fprintf(stderr,
                     "usage: %s <broker_restart|qos1_burst|broker_frozen> <mosquitto executable>\n",
                     argv[0]);
        return 2;
    }
    if (::access(argv[2], X_OK) != 0) {
        std::printf("mosquitto not found ('%s'): skipped\n", argv[2]);
        return kSkipped;
    }

    emulation::setTimeScale(scenario->timeScale);
    const std::filesystem::path directory =
        std::filesystem::temp_directory_path() /
        ("picohome_" + std::string(scenario->name) + "_" + std::to_string(::getpid()));
    std::filesystem::create_directories(directory);

    bool passed = false;
    {
        Broker broker(argv[2], directory);
        passed = run(*scenario, broker);
    }  // stops the broker

    if (passed) {
        std::filesystem::remove_all(directory);
        std::printf("PASSED %s\n", scenario->name);
    } else {
        std::ifstream log(directory / "broker.log");
        const std::string text{std::istreambuf_iterator<char>(log), std::istreambuf_iterator<char>()};
        std::printf("--- mosquitto log (kept in %s) ---\n%s", directory.c_str(), text.c_str());
    }
    std::fflush(stdout);
    // Core 0 and core 1 never return: leave without running destructors under them.
    std::_Exit(passed ? 0 : 1);
}
