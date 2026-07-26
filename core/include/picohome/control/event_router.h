#pragma once

#include <vector>

#include "picohome/control/source_event.h"

namespace picohome {

// Observer side of the control model.
class IEventObserver {
public:
    virtual ~IEventObserver() = default;
    virtual void onSourceEvent(const SourceEvent& event) = 0;
};

// Subject side: sources emit, subscribed bindings get notified. Neither side
// knows the other — they only share the SourceKey namespace, which is what
// makes fan-out (one switch driving many bindings) and fan-in (many switches
// feeding one binding) free.
//
// Subscriptions are built once at boot from the config and never change, so
// dispatch is an allocation-free walk of a flat vector. Events are rare (a
// human pressing a switch), so the linear scan is irrelevant next to the
// simplicity it buys.
class EventRouter {
public:
    void subscribe(const SourceKey& key, IEventObserver* observer);
    void emit(const SourceEvent& event);

    std::size_t subscriptionCount() const { return entries_.size(); }

private:
    struct Entry {
        SourceKey key;
        IEventObserver* observer;
    };
    std::vector<Entry> entries_;
};

}  // namespace picohome
