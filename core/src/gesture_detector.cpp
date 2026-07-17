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

GestureDetector::GestureDetector(IGestureListener& listener)
    : GestureDetector(listener, Timing{}) {}

GestureDetector::GestureDetector(IGestureListener& listener, Timing timing)
    : listener_(listener), timing_(timing) {}

void GestureDetector::update(bool rawPressed, uint32_t nowMs) {
    if (rawPressed != rawLast_) {
        rawLast_ = rawPressed;
        rawChangeMs_ = nowMs;
    }
    if (rawPressed != stable_ && nowMs - rawChangeMs_ >= timing_.debounceMs) {
        stable_ = rawPressed;
        if (stable_) {
            onStablePress(nowMs);
        } else {
            onStableRelease(nowMs);
        }
    }

    if (stable_) {
        const uint32_t heldFor = nowMs - pressStartMs_;
        if (!holding_ && pressCount_ == 0 && heldFor >= timing_.holdMs) {
            holding_ = true;
            listener_.onGesture(Gesture::HoldStart);
        }
        if (holding_ && !holdLongSent_ && heldFor >= timing_.holdLongMs) {
            holdLongSent_ = true;
            listener_.onGesture(Gesture::HoldLong);
        }
    } else if (pressCount_ > 0 && nowMs - releaseMs_ >= timing_.multiPressGapMs) {
        emitPressCount();
    }
}

void GestureDetector::onStablePress(uint32_t nowMs) { pressStartMs_ = nowMs; }

void GestureDetector::onStableRelease(uint32_t nowMs) {
    if (holding_) {
        holding_ = false;
        holdLongSent_ = false;
        listener_.onGesture(Gesture::HoldEnd);
        return;
    }
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