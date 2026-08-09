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

// Inverse of toPayload(); false if the text names no gesture.
bool gestureFromPayload(std::string_view payload, Gesture& out);

// Hold gestures require a switch that springs back — see SwitchKind.
inline bool isHoldGesture(Gesture gesture) {
    return gesture == Gesture::HoldStart || gesture == Gesture::HoldLong ||
           gesture == Gesture::HoldEnd;
}

// Both switch types are used around the house, often interchangeably on plain
// lights, so which one is fitted is a per-switch config field:
//
//  - Momentary (spring-return pushbutton): the full grammar. A press counts on
//    release, so multi-press and hold are both available.
//  - Toggle (latching rocker that stays where you put it): every change of
//    position is one user action and emits press_1 — in either direction, since
//    the position no longer tracks the light once MQTT or a second switch of a
//    P-group can drive it. Holding is physically impossible, so a toggle switch
//    never produces hold gestures, and anything needing them (covers, dimmers)
//    wants a momentary switch.
//
// This mirrors the old Arduino MomentarySwitch/ToggleSwitch pair, where a
// toggle's "press" was likewise any state change.
enum class SwitchKind : uint8_t { Momentary, Toggle };

inline bool producesHoldGestures(SwitchKind kind) { return kind == SwitchKind::Momentary; }

// How much of the grammar a given switch is asked to decode. This is separate
// from SwitchKind because it is about intent, not wiring, and both kinds
// support both modes.
//
// The full grammar cannot act on a press until it knows no second press is
// coming, so every single press pays the multi-press window as latency —
// ~330 ms on a momentary switch, ~230 ms on a rocker. A plain light switch
// never uses press_2, press_3 or hold, so that wait buys it nothing and is
// simply felt as lag.
//
//  - Instant: act on the actuating edge itself (the press, or either flip of a
//    rocker) and emit press_1 straight away. Debounce is the only delay left.
//    Nothing else is ever emitted.
//  - Full: the whole grammar — multi-press and, on momentary switches, hold.
//    Needed by covers and dimmers, and by anything using press_2/press_3
//    locally or from Home Assistant.
enum class GestureMode : uint8_t { Instant, Full };

// Whether a switch so configured can ever produce a given gesture. Used at boot
// to refuse a binding that would sit there waiting for something impossible.
inline bool producesGesture(SwitchKind kind, GestureMode mode, Gesture gesture) {
    if (mode == GestureMode::Instant) return gesture == Gesture::Press1;
    if (isHoldGesture(gesture)) return producesHoldGestures(kind);
    return true;
}

class IGestureListener {
public:
    virtual ~IGestureListener() = default;
    virtual void onGesture(Gesture gesture) = 0;
};

// Turns a raw (bouncy) switch signal into gestures. Pure logic driven by pin
// state + monotonic time — no hardware dependencies, host-testable.
//
// Current simplification for momentary switches: a hold is only detected when
// it is the first press of a sequence (press-press-hold is not a gesture yet).
class GestureDetector {
public:
    struct Timing {
        uint32_t debounceMs = 30;
        uint32_t multiPressGapMs = 300;
        uint32_t holdMs = 400;      // momentary only
        uint32_t holdLongMs = 1000; // momentary only, covers
    };

    // A toggle switch gets a shorter multi-press window (values carried over
    // from the old Arduino switches, which were tuned on these very rockers):
    // flipping a rocker back and forth is quicker than double-clicking a
    // pushbutton, and every single flip pays the window as latency before it
    // acts, so it wants to be as short as it can be.
    static Timing timingFor(SwitchKind kind);

    explicit GestureDetector(IGestureListener& listener);
    GestureDetector(IGestureListener& listener, SwitchKind kind,
                    GestureMode mode = GestureMode::Full);
    GestureDetector(IGestureListener& listener, SwitchKind kind, GestureMode mode, Timing timing);

    SwitchKind kind() const { return kind_; }
    GestureMode mode() const { return mode_; }

    // Adopt the switch's current position without reporting it. Called at boot
    // so a toggle switch resting in the "on" position does not look like a flip
    // and switch every bound light the moment the controller powers up.
    void prime(bool rawState);

    // Call every loop iteration with the current raw pin state.
    void update(bool rawPressed, uint32_t nowMs);

private:
    void onStablePress(uint32_t nowMs);
    void onStableRelease(uint32_t nowMs);
    void countPress(uint32_t nowMs);
    void emitPressCount();

    IGestureListener& listener_;
    SwitchKind kind_;
    GestureMode mode_;
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
