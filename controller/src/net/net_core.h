#pragma once

#include <string>

#include "picohome/config/config.h"
#include "picohome/net/messages.h"
#include "picohome/net/publisher.h"
#include "picohome/net/remote_sources.h"

// Core 1: Ethernet, MQTT and everything else that is allowed to block.
// It exchanges work with core 0 only through two lock-free queues, so a dead
// broker or an unplugged cable can never delay a wall switch.

namespace picohome::net {

// Call on core 0 before launch(); safe to use even if the network never comes up.
void initQueues();

// Sink for QueuePublisher (core 0 side of the outbound queue).
IPublishSink& publishSink();

// Drain one inbound message on core 0. Returns false when the queue is empty.
bool popInbound(InboundMessage& message);

// Starts core 1. `config` and `remoteSources` must outlive the program (they
// are read from core 1). `bootError` is published retained on connect when the
// controller came up with a broken config but a usable network section.
void launch(const config::App& config, const RemoteSourceTable& remoteSources,
            const std::string& bootError);

}  // namespace picohome::net
