#include "picohome/core/gesture.h"

namespace picohome {

std::string_view toPayload(Gesture gesture) {
    switch (gesture) {
        case Gesture::Press1: return "press_1";
        case Gesture::Press2: return "press_2";
        case Gesture::Press3: return "press_3";
        case Gesture::HoldStart: return "hold_start";
        case Gesture::HoldLong: return "hold_long";
        case Gesture::HoldEnd: return "hold_end";
    }
    return "unknown";
}

bool gestureFromPayload(std::string_view payload, Gesture& out) {
    constexpr Gesture kAll[] = {Gesture::Press1,    Gesture::Press2,   Gesture::Press3,
                                Gesture::HoldStart, Gesture::HoldLong, Gesture::HoldEnd};
    for (const Gesture gesture : kAll) {
        if (toPayload(gesture) == payload) {
            out = gesture;
            return true;
        }
    }
    return false;
}

GestureDetector::Timing GestureDetector::timingFor(SwitchKind kind) {
    Timing timing;
    if (kind == SwitchKind::Toggle) timing.multiPressGapMs = 200;
    return timing;
}

GestureDetector::GestureDetector(IGestureListener& listener)
    : GestureDetector(listener, SwitchKind::Momentary) {}

GestureDetector::GestureDetector(IGestureListener& listener, SwitchKind kind)
    : GestureDetector(listener, kind, timingFor(kind)) {}

GestureDetector::GestureDetector(IGestureListener& listener, SwitchKind kind, Timing timing)
    : listener_(listener), kind_(kind), timing_(timing) {}

void GestureDetector::prime(bool rawState) {
    rawLast_ = rawState;
    stable_ = rawState;
    rawChangeMs_ = 0;
    holding_ = false;
    holdLongSent_ = false;
    pressCount_ = 0;
}

void GestureDetector::update(bool rawPressed, uint32_t nowMs) {
    if (rawPressed != rawLast_) {
        rawLast_ = rawPressed;
        rawChangeMs_ = nowMs;
    }
    if (rawPressed != stable_ && nowMs - rawChangeMs_ >= timing_.debounceMs) {
        stable_ = rawPressed;
        if (kind_ == SwitchKind::Toggle) {
            // Either direction is one flip of the rocker, so both count.
            countPress(nowMs);
        } else if (stable_) {
            onStablePress(nowMs);
        } else {
            onStableRelease(nowMs);
        }
    }

    if (kind_ == SwitchKind::Momentary && stable_) {
        const uint32_t heldFor = nowMs - pressStartMs_;
        if (!holding_ && pressCount_ == 0 && heldFor >= timing_.holdMs) {
            holding_ = true;
            listener_.onGesture(Gesture::HoldStart);
        }
        if (holding_ && !holdLongSent_ && heldFor >= timing_.holdLongMs) {
            holdLongSent_ = true;
            listener_.onGesture(Gesture::HoldLong);
        }
        return;
    }

    // A momentary switch only counts presses out while released, so that a
    // second press starting inside the gap is not cut short. A toggle switch
    // rests in both positions, so its window always runs.
    if (pressCount_ > 0 && nowMs - releaseMs_ >= timing_.multiPressGapMs) emitPressCount();
}

void GestureDetector::onStablePress(uint32_t nowMs) { pressStartMs_ = nowMs; }

void GestureDetector::onStableRelease(uint32_t nowMs) {
    if (holding_) {
        holding_ = false;
        holdLongSent_ = false;
        listener_.onGesture(Gesture::HoldEnd);
        return;
    }
    countPress(nowMs);
}

void GestureDetector::countPress(uint32_t nowMs) {
    ++pressCount_;
    releaseMs_ = nowMs;
    if (pressCount_ >= 3) emitPressCount();
}

void GestureDetector::emitPressCount() {
    const Gesture gesture = pressCount_ == 1   ? Gesture::Press1
                            : pressCount_ == 2 ? Gesture::Press2
                                               : Gesture::Press3;
    pressCount_ = 0;
    listener_.onGesture(gesture);
}

}  // namespace picohome
