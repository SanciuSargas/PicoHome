#pragma once

#include <cstdint>

#include "picohome/control/event_router.h"
#include "picohome/core/device_registry.h"
#include "picohome/net/messages.h"

namespace picohome {

// Applies inbound MQTT traffic to the local system. Runs on core 0, because
// only core 0 may touch devices; core 1 has already parsed the topic.
//
// An MQTT command and a wall switch are equal citizens here: both end up
// calling the same device, and a remote event enters the same EventRouter a
// local button would.
class CommandRouter {
public:
    CommandRouter(DeviceRegistry& registry, EventRouter& events);

    void handle(const InboundMessage& message);

    // Commands addressed to devices this controller does not have. Non-zero is
    // normal on a shared broker; a spike usually means a config/topic mismatch.
    uint32_t unroutedCount() const { return unrouted_; }

private:
    DeviceRegistry& registry_;
    EventRouter& events_;
    uint32_t unrouted_ = 0;
};

}  // namespace picohome