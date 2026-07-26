#include "picohome/net/remote_sources.h"

#include <algorithm>

#include "picohome/core/gesture.h"

namespace picohome {

void RemoteSourceTable::addRow(const std::string& topic, std::string payload, const Match& match) {
    if (std::find(topics_.begin(), topics_.end(), topic) == topics_.end()) {
        topics_.push_back(topic);
    }
    rows_.push_back({topic, std::move(payload), match});
}

uint16_t RemoteSourceTable::internAction(std::string topic, std::string payload) {
    for (const Row& row : rows_) {
        if (row.topic == topic && row.payload == payload) return row.match.sourceIndex;
    }
    const Match match{sourceCount_, EventKind::Gesture, static_cast<int32_t>(Gesture::Press1)};
    addRow(topic, std::move(payload), match);
    return sourceCount_++;
}

uint16_t RemoteSourceTable::internSensor(std::string topic, std::string activePayload,
                                         std::string inactivePayload) {
    const uint16_t index = sourceCount_++;
    addRow(topic, std::move(activePayload), {index, EventKind::SensorEdge, 1});
    addRow(topic, std::move(inactivePayload), {index, EventKind::SensorEdge, 0});
    return index;
}

const RemoteSourceTable::Match* RemoteSourceTable::find(std::string_view topic,
                                                        std::string_view payload) const {
    for (const Row& row : rows_) {
        if (row.topic == topic && row.payload == payload) return &row.match;
    }
    return nullptr;
}

}  // namespace picohome
