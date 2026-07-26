#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "picohome/control/source_event.h"

// Remote binding sources: a device on *another* controller. This is what makes
// cross-panel wiring work — the corridor 30PJ/31PJ switches sit on the closet
// panel while their relays are on the utility one, and a PIR in a stairwell can
// light a lamp whose relay lives in a different building.
//
// Two flavours, because the two kinds of signal genuinely differ:
//   * an action (a button event) is one exact payload that means "this
//     happened"; there is nothing to go back to;
//   * a sensor carries a state, so it needs both edges to drive an off-delay.
//
// Either way the pairs are interned once at boot and referenced by index, so
// matching an inbound message costs no allocation.

namespace picohome {

class RemoteSourceTable {
public:
    struct Match {
        uint16_t sourceIndex = 0;
        EventKind kind = EventKind::Gesture;
        int32_t value = 0;
    };

    // A button event from another controller: one payload, one meaning.
    uint16_t internAction(std::string topic, std::string payload);

    // A sensor on another controller: the same source, reported by two payloads.
    uint16_t internSensor(std::string topic, std::string activePayload,
                          std::string inactivePayload);

    // What an inbound (topic, payload) means, or nullptr if it is not ours.
    const Match* find(std::string_view topic, std::string_view payload) const;

    // Distinct topics, for building the MQTT subscription list.
    const std::vector<std::string>& topics() const { return topics_; }

    // Number of distinct sources (a sensor counts once, not once per edge).
    std::size_t size() const { return sourceCount_; }

private:
    struct Row {
        std::string topic;
        std::string payload;
        Match match;
    };

    void addRow(const std::string& topic, std::string payload, const Match& match);

    std::vector<Row> rows_;
    std::vector<std::string> topics_;
    uint16_t sourceCount_ = 0;
};

}  // namespace picohome
