#pragma once

#include <map>
#include <string>
#include <vector>

#include "picohome/core/device.h"
#include "picohome/core/gesture.h"
#include "picohome/hal/hal.h"

namespace picohome::test {

struct FakeDigitalInput final : hal::IDigitalInput {
    bool value = false;
    bool read() override { return value; }
};

struct FakeDigitalOutput final : hal::IDigitalOutput {
    bool value = false;
    int writes = 0;
    void write(bool active) override {
        value = active;
        ++writes;
    }
};

struct RecordingPublisher final : IPublisher {
    struct Message {
        DeviceType type;
        uint16_t id;
        std::string room;
        std::string command;
        std::string payload;
        bool retained;
    };
    std::vector<Message> messages;

    void publish(const Device& device, std::string_view command, std::string_view payload,
                 bool retained) override {
        messages.push_back({device.type(), device.id(), std::string(device.room()),
                            std::string(command), std::string(payload), retained});
    }
};

struct GestureRecorder final : IGestureListener {
    std::vector<Gesture> gestures;
    void onGesture(Gesture gesture) override { gestures.push_back(gesture); }
};

}  // namespace picohome::test