#include "picohome/net/publisher.h"

#include <cstring>

#include "picohome/core/commands.h"
#include "picohome/net/topic.h"

namespace picohome {

namespace {

void copyInto(char* destination, std::size_t capacity, std::string_view text) {
    const std::size_t length = text.size() < capacity - 1 ? text.size() : capacity - 1;
    std::memcpy(destination, text.data(), length);
    destination[length] = '\0';
}

}  // namespace

QueuePublisher::QueuePublisher(IPublishSink& sink, std::string building)
    : sink_(sink), building_(std::move(building)) {}

void QueuePublisher::publish(const Device& device, std::string_view command,
                             std::string_view payload, bool retained) {
    PublishMessage message;
    if (!buildTopic(message.topic, sizeof(message.topic), building_, device.room(), device.type(),
                    device.id(), command)) {
        ++dropped_;
        return;
    }
    copyInto(message.payload, sizeof(message.payload), payload);
    message.retained = retained;
    // A lost measurement is replaced by the next one; a lost state or command
    // is not, so those are worth the extra round trip.
    message.qos = command == commands::kMeasurement ? 0 : 1;

    if (!sink_.push(message)) ++dropped_;
}

}  // namespace picohome