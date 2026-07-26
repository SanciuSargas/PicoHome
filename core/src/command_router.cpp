#include "picohome/net/command_router.h"

#include "picohome/core/gesture.h"

namespace picohome {

CommandRouter::CommandRouter(DeviceRegistry& registry, EventRouter& events)
    : registry_(registry), events_(events) {}

void CommandRouter::handle(const InboundMessage& message) {
    switch (message.kind) {
        case InboundKind::PublishAllStates:
            registry_.publishAllStates();
            return;

        case InboundKind::RemoteEvent: {
            // Core 1 already matched the payload, so all that is left is to
            // inject the event a local button or sensor would have produced —
            // from here on nothing can tell the difference.
            SourceEvent event;
            event.key = SourceKey::remote(message.remoteIndex);
            event.kind = message.remoteKind;
            event.value = message.remoteValue;
            events_.emit(event);
            return;
        }

        case InboundKind::DeviceCommand: {
            Device* device = registry_.find(message.deviceType, message.deviceId);
            if (device == nullptr || !device->handleCommand(message.command, message.payload)) {
                ++unrouted_;
            }
            return;
        }
    }
}

}  // namespace picohome