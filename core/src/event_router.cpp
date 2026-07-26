#include "picohome/control/event_router.h"

namespace picohome {

void EventRouter::subscribe(const SourceKey& key, IEventObserver* observer) {
    if (observer == nullptr) return;
    entries_.push_back({key, observer});
}

void EventRouter::emit(const SourceEvent& event) {
    for (const auto& entry : entries_) {
        if (entry.key == event.key) entry.observer->onSourceEvent(event);
    }
}

}  // namespace picohome
