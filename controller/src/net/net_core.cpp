#include "net/net_core.h"

#include <cstdio>
#include <cstring>
#include <string_view>
#include <vector>

#include "pico/multicore.h"
#include "pico/time.h"
#include "pico/util/queue.h"

extern "C" {
#include "MQTTClient.h"
#include "socket.h"
#include "wizchip_conf.h"
}

#include "net/w5100_link.h"
#include "picohome/core/commands.h"
#include "picohome/net/topic.h"

namespace picohome::net {

namespace {

// Sized so a burst of state changes survives a slow broker without blocking
// core 0. Dropping is safe: every retained state is refreshed on reconnect.
constexpr int kOutboundDepth = 32;
constexpr int kInboundDepth = 16;
constexpr int kMqttSocket = 0;

queue_t gOutbound;
queue_t gInbound;

const config::App* gConfig = nullptr;
const RemoteSourceTable* gRemoteSources = nullptr;
std::string gBootError;

// MQTT working buffers. Static because nothing may allocate after boot.
unsigned char gSendBuffer[512];
unsigned char gReadBuffer[1024];

Network gNetwork;
MQTTClient gClient;

std::string gClientId;
std::string gAliveTopic;
std::string gErrorTopic;
std::vector<std::string> gSubscriptions;  // must outlive MQTTSubscribe: it keeps the pointers

class QueueSink final : public IPublishSink {
public:
    bool push(const PublishMessage& message) override { return queue_try_add(&gOutbound, &message); }
};

QueueSink gSink;

std::string_view viewOf(MQTTString& string) {
    if (string.cstring != nullptr) return string.cstring;
    return {string.lenstring.data, static_cast<std::size_t>(string.lenstring.len)};
}

void copyInto(char* destination, std::size_t capacity, std::string_view text) {
    const std::size_t length = text.size() < capacity - 1 ? text.size() : capacity - 1;
    std::memcpy(destination, text.data(), length);
    destination[length] = '\0';
}

// Every inbound publish lands here. Core 1 parses, core 0 acts.
void onMessage(MessageData* data) {
    const std::string_view topic = viewOf(*data->topicName);
    const std::string_view payload{static_cast<const char*>(data->message->payload),
                                   static_cast<std::size_t>(data->message->payloadlen)};

    // A remote binding source is matched on the exact (topic, payload) pair
    // the config asked for — a button event, or one edge of a remote sensor.
    if (const RemoteSourceTable::Match* match = gRemoteSources->find(topic, payload)) {
        const InboundMessage message =
            InboundMessage::remoteEvent(match->sourceIndex, match->kind, match->value);
        queue_try_add(&gInbound, &message);
        return;
    }

    ParsedTopic parsed;
    if (!parseTopic(topic, parsed) || parsed.building != gConfig->building ||
        !commands::isCommandVerb(parsed.command)) {
        // Say so rather than dropping it in silence: a command that reaches the
        // broker but never reaches a device is otherwise invisible.
        std::printf("net: ignoring '%.*s'\n", static_cast<int>(topic.size()), topic.data());
        return;
    }

    InboundMessage message;
    message.kind = InboundKind::DeviceCommand;
    message.deviceType = parsed.type;
    message.deviceId = parsed.id;
    copyInto(message.command, sizeof(message.command), parsed.command);
    copyInto(message.payload, sizeof(message.payload), payload);
    queue_try_add(&gInbound, &message);
}

bool socketAlive() {
    const uint8_t status = getSn_SR(kMqttSocket);
    return status == SOCK_ESTABLISHED || status == SOCK_CLOSE_WAIT;
}

// MQTTConnect returns the broker's CONNACK code, or FAILURE when no CONNACK
// came back at all. Naming them matters: a refused login retries on the same
// backoff as an unplugged cable and is indistinguishable from one otherwise,
// and there is no way to report it over MQTT — we are not connected.
constexpr int kConnackBadCredentials = 4;
constexpr int kConnackNotAuthorized = 5;

const char* connackReason(int code) {
    switch (code) {
        case 1: return "broker refused MQTT 3.1.1";
        case 2: return "client id rejected";
        case 3: return "broker unavailable";
        case kConnackBadCredentials: return "bad username or password";
        case kConnackNotAuthorized: return "not authorized (the user may not connect)";
        default: return "no CONNACK — broker unreachable, or it closed the connection";
    }
}

bool publishNow(const char* topic, const char* payload, bool retained, int qos) {
    MQTTMessage message;
    std::memset(&message, 0, sizeof(message));
    message.qos = static_cast<enum QoS>(qos);
    message.retained = retained ? 1 : 0;
    message.payload = const_cast<char*>(payload);
    message.payloadlen = std::strlen(payload);
    return MQTTPublish(&gClient, topic, &message) == SUCCESSS;
}

// Anything still queued from before a reconnect describes a world that has
// moved on; publishAllStates() will send the current truth instead.
void flushOutbound() {
    PublishMessage message;
    while (queue_try_remove(&gOutbound, &message)) {
    }
}

bool connectSession(const config::Network& network) {
    uint8_t brokerIp[4] = {};
    unsigned a = 0, b = 0, c = 0, d = 0;
    if (std::sscanf(network.broker.c_str(), "%u.%u.%u.%u", &a, &b, &c, &d) != 4) {
        std::printf("net: broker address '%s' is not an IPv4 address\n", network.broker.c_str());
        return false;
    }
    brokerIp[0] = static_cast<uint8_t>(a);
    brokerIp[1] = static_cast<uint8_t>(b);
    brokerIp[2] = static_cast<uint8_t>(c);
    brokerIp[3] = static_cast<uint8_t>(d);

    NewNetwork(&gNetwork, kMqttSocket);
    if (ConnectNetwork(&gNetwork, brokerIp, network.brokerPort) != SOCK_OK) return false;

    MQTTClientInit(&gClient, &gNetwork, 5000, gSendBuffer, sizeof(gSendBuffer), gReadBuffer,
                   sizeof(gReadBuffer));
    gClient.defaultMessageHandler = onMessage;

    MQTTPacket_connectData data = MQTTPacket_connectData_initializer;
    data.MQTTVersion = 4;  // 3.1.1
    data.clientID.cstring = const_cast<char*>(gClientId.c_str());
    data.keepAliveInterval = 30;
    data.cleansession = 1;
    // The broker announces our death for us — the only reliable way, since a
    // controller that crashes or loses power cannot send anything itself.
    data.willFlag = 1;
    data.will.topicName.cstring = const_cast<char*>(gAliveTopic.c_str());
    data.will.message.cstring = const_cast<char*>("offline");
    data.will.retained = 1;
    data.will.qos = QOS1;

    // Broker login, when the config carries one. Leaving these pointers null is
    // what makes the serializer omit the flags entirely, so an anonymous broker
    // receives exactly the packet it always did. An *empty* string would not do
    // that — a non-null pointer sets the flag and sends a zero-length user
    // name, which brokers reject. The strings live in the boot config, so they
    // outlive the packet without core 1 copying (or allocating) anything.
    if (!network.username.empty()) {
        data.username.cstring = const_cast<char*>(network.username.c_str());
    }
    if (!network.password.empty()) {
        data.password.cstring = const_cast<char*>(network.password.c_str());
    }

    const int rc = MQTTConnect(&gClient, &data);
    if (rc != SUCCESSS) {
        // Never log the password, and log the username only so a typo in it is
        // visible: this goes to a serial console, but it is still the one place
        // credentials could leak into a log file.
        std::printf("net: MQTT connect refused (%d): %s\n", rc, connackReason(rc));
        if (rc == kConnackBadCredentials || rc == kConnackNotAuthorized) {
            std::printf("net: check network.username / network.password in config.json "
                        "(username sent: %s)\n",
                        network.username.empty() ? "<none — the config carries no credentials>"
                                                 : network.username.c_str());
        }
        gNetwork.disconnect(&gNetwork);
        return false;
    }

    publishNow(gAliveTopic.c_str(), "online", /*retained=*/true, QOS1);
    publishNow(gErrorTopic.c_str(), gBootError.c_str(), /*retained=*/true, QOS1);

    // Subscribe *before* asking core 0 for its states: the retained
    // set_night_mode commands then arrive first, so the controller never
    // advertises a gate value it is about to change.
    for (const std::string& filter : gSubscriptions) {
        if (MQTTSubscribe(&gClient, filter.c_str(), QOS1, nullptr) != SUCCESSS) {
            std::printf("net: subscribe failed for %s\n", filter.c_str());
            gNetwork.disconnect(&gNetwork);
            return false;
        }
    }
    MQTTYield(&gClient, 300);  // let retained commands land

    const InboundMessage refresh = InboundMessage::publishAllStates();
    queue_try_add(&gInbound, &refresh);

    std::printf("net: MQTT connected as '%s', user %s (%u subscriptions)\n", gClientId.c_str(),
                network.username.empty() ? "<anonymous>" : network.username.c_str(),
                static_cast<unsigned>(gSubscriptions.size()));
    return true;
}

void sessionLoop() {
    while (true) {
        if (MQTTYield(&gClient, 10) == FAILURE || !socketAlive()) return;

        // Bounded per iteration so inbound traffic and keepalives still get a turn.
        PublishMessage message;
        for (int budget = 8; budget > 0 && queue_try_remove(&gOutbound, &message); --budget) {
            if (!publishNow(message.topic, message.payload, message.retained, message.qos)) {
                std::printf("net: publish failed, dropping session\n");
                return;
            }
        }
    }
}

[[noreturn]] void core1Main() {
    const config::Network& network = gConfig->network;

    while (!w5100Init(network)) {
        std::printf("net: Ethernet init failed, retrying in 5 s\n");
        sleep_ms(5000);
    }

    uint32_t backoffMs = 1000;
    while (true) {
        flushOutbound();
        if (connectSession(network)) {
            backoffMs = 1000;
            sessionLoop();
            std::printf("net: MQTT session lost\n");
            gNetwork.disconnect(&gNetwork);
        }
        sleep_ms(backoffMs);
        if (backoffMs < 15000) backoffMs *= 2;
    }
}

}  // namespace

void initQueues() {
    queue_init(&gOutbound, sizeof(PublishMessage), kOutboundDepth);
    queue_init(&gInbound, sizeof(InboundMessage), kInboundDepth);
}

IPublishSink& publishSink() { return gSink; }

bool popInbound(InboundMessage& message) { return queue_try_remove(&gInbound, &message); }

void launch(const config::App& config, const RemoteSourceTable& remoteSources,
            const std::string& bootError) {
    gConfig = &config;
    gRemoteSources = &remoteSources;
    gBootError = bootError;

    // Everything that allocates is done here, on core 0, before core 1 exists:
    // the heap is only safe while a single core is using it, and after boot
    // nothing in this firmware allocates at all.
    gClientId = config.building + "_controller";
    char topic[kMaxTopicLen];
    buildTopic(topic, sizeof(topic), config.building, "controller", DeviceType::Status, 0,
               commands::kAlive);
    gAliveTopic = topic;
    buildTopic(topic, sizeof(topic), config.building, "controller", DeviceType::Status, 0,
               commands::kError);
    gErrorTopic = topic;
    gSubscriptions = buildSubscriptions(config.building, remoteSources.topics());

    multicore_launch_core1(core1Main);
}

}  // namespace picohome::net