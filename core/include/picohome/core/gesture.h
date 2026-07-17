#pragma once

#include <cstdint>
#include <string_view>

namespace picohome {

// The PicoHome switch gesture grammar.
enum class Gesture : uint8_t {
    Press1,
    Press2,
    Press3,
    HoldStart,
    HoldLong,  // emitted while still holding, holdLongMs after the press
    HoldEnd,
};

// MQTT event payload for a gesture: "press_1", "hold_start", ...
std::string_view toPayload(Gesture gesture);

class IGestureListener {
public:
    virtual ~IGestureListener() = default;
    virtual void onGesture(Gesture gesture) = 0;
};

// Turns a raw (bouncy) press/release signal into gestures. Pure logic driven by
// pin state + monotonic time — no hardware dependencies, host-testable.
//
// Current simplification: a hold is only detected when it is the first press of
// a sequence (press-press-hold combinations are not a gesture yet).
class GestureDetector {
public:
    struct Timing {
        uint32_t debounceMs = 30;
        uint32_t multiPressGapMs = 350;
        uint32_t holdMs = 500;
        uint32_t holdLongMs = 3000;
    };

    explicit GestureDetector(IGestureListener& listener);
    GestureDetector(IGestureListener& listener, Timing timing);

    // Call every loop iteration with the current raw pin state.
    void update(bool rawPressed, uint32_t nowMs);

private:
    void onStablePress(uint32_t nowMs);
    void onStableRelease(uint32_t nowMs);
    void emitPressCount();

    IGestureListener& listener_;
    Timing timing_;

    bool rawLast_ = false;
    uint32_t rawChangeMs_ = 0;
    bool stable_ = false;

    bool holding_ = false;
    bool holdLongSent_ = false;
    uint8_t pressCount_ = 0;
    uint32_t pressStartMs_ = 0;
    uint32_t releaseMs_ = 0;
};

}  // namespace picohome