#pragma once

#include <string>

#include "picohome/core/device.h"
#include "picohome/net/messages.h"

namespace picohome {

// Where finished messages go. On the target this is the inter-core queue; in
// tests it is a vector, which is what makes the whole publish path host-testable.
class IPublishSink {
public:
    virtual ~IPublishSink() = default;
    // Returns false if the sink is full and the message was dropped.
    virtual bool push(const PublishMessage& message) = 0;
};

// The IPublisher devices see. Builds the topic from the device's identity and
// hands the message to core 1; it never blocks and never fails loudly, because
// a device changing state must not care whether the network is alive.
class QueuePublisher : public IPublisher {
public:
    QueuePublisher(IPublishSink& sink, std::string building);

    void publish(const Device& device, std::string_view command, std::string_view payload,
                 bool retained) override;

    // Messages lost because the queue was full (broker down, or a burst).
    // Harmless by design: every retained state is refreshed on reconnect.
    uint32_t droppedCount() const { return dropped_; }

private:
    IPublishSink& sink_;
    std::string building_;
    uint32_t dropped_ = 0;
};

}  // namespace picohome