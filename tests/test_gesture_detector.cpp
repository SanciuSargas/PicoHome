#include <doctest/doctest.h>

#include "mocks/fakes.h"
#include "picohome/core/gesture.h"

using picohome::Gesture;
using picohome::GestureDetector;
using picohome::test::GestureRecorder;

namespace {

// Drives the detector with a scripted press pattern, advancing 1 ms per update.
struct Driver {
    GestureRecorder recorder;
    GestureDetector detector{recorder};
    uint32_t now = 0;

    void step(bool pressed, uint32_t durationMs) {
        for (uint32_t i = 0; i < durationMs; ++i) detector.update(pressed, ++now);
    }
};

}  // namespace

TEST_CASE("single short press emits press_1 after the multi-press gap") {
    Driver d;
    d.step(true, 100);
    d.step(false, 450);  // debounce (30) + gap (350) with margin
    REQUIRE(d.recorder.gestures.size() == 1);
    CHECK(d.recorder.gestures[0] == Gesture::Press1);
}

TEST_CASE("double press emits press_2") {
    Driver d;
    d.step(true, 100);
    d.step(false, 100);
    d.step(true, 100);
    d.step(false, 450);
    REQUIRE(d.recorder.gestures.size() == 1);
    CHECK(d.recorder.gestures[0] == Gesture::Press2);
}

TEST_CASE("triple press emits press_3 immediately on the third release") {
    Driver d;
    for (int i = 0; i < 3; ++i) {
        d.step(true, 100);
        d.step(false, 100);
    }
    REQUIRE(d.recorder.gestures.size() == 1);
    CHECK(d.recorder.gestures[0] == Gesture::Press3);
}

TEST_CASE("hold emits hold_start then hold_end") {
    Driver d;
    d.step(true, 700);  // stable at ~30 ms, hold threshold 500 ms later
    d.step(false, 50);
    REQUIRE(d.recorder.gestures.size() == 2);
    CHECK(d.recorder.gestures[0] == Gesture::HoldStart);
    CHECK(d.recorder.gestures[1] == Gesture::HoldEnd);
}

TEST_CASE("long hold adds hold_long while still holding") {
    Driver d;
    d.step(true, 3600);
    REQUIRE(d.recorder.gestures.size() == 2);
    CHECK(d.recorder.gestures[0] == Gesture::HoldStart);
    CHECK(d.recorder.gestures[1] == Gesture::HoldLong);
    d.step(false, 50);
    REQUIRE(d.recorder.gestures.size() == 3);
    CHECK(d.recorder.gestures[2] == Gesture::HoldEnd);
}

TEST_CASE("contact bounce is filtered out") {
    Driver d;
    for (int i = 0; i < 4; ++i) {
        d.step(true, 5);
        d.step(false, 5);
    }
    d.step(false, 500);
    CHECK(d.recorder.gestures.empty());
}

TEST_CASE("press shorter than debounce is ignored") {
    Driver d;
    d.step(true, 10);
    d.step(false, 500);
    CHECK(d.recorder.gestures.empty());
}

// --- instant mode ---------------------------------------------------------
//
// The full grammar cannot act until it knows no second press is coming, so a
// plain light switch pays ~300 ms of latency for a feature it never uses.
// Instant mode drops the grammar and acts on the edge instead.

namespace {

struct InstantDriver {
    GestureRecorder recorder;
    GestureDetector detector{recorder, picohome::SwitchKind::Momentary,
                             picohome::GestureMode::Instant};
    uint32_t now = 0;

    void step(bool pressed, uint32_t durationMs) {
        for (uint32_t i = 0; i < durationMs; ++i) detector.update(pressed, ++now);
    }
};

}  // namespace

TEST_CASE("instant momentary: press_1 lands while the button is still down") {
    InstantDriver d;
    d.step(false, 10);
    d.step(true, 40);  // just past debounce, finger still on the button

    REQUIRE(d.recorder.gestures.size() == 1);
    CHECK(d.recorder.gestures[0] == Gesture::Press1);
}

TEST_CASE("instant momentary: releasing reports nothing more") {
    InstantDriver d;
    d.step(false, 10);
    d.step(true, 100);
    d.step(false, 500);
    CHECK(d.recorder.gestures.size() == 1);
}

TEST_CASE("instant momentary: holding produces no hold gestures at all") {
    InstantDriver d;
    d.step(false, 10);
    d.step(true, 10000);
    CHECK(d.recorder.gestures.size() == 1);
    CHECK(d.recorder.gestures[0] == Gesture::Press1);
}

TEST_CASE("instant momentary: two quick presses are two press_1, not a press_2") {
    InstantDriver d;
    d.step(false, 10);
    d.step(true, 40);
    d.step(false, 40);
    d.step(true, 40);

    REQUIRE(d.recorder.gestures.size() == 2);
    CHECK(d.recorder.gestures[0] == Gesture::Press1);
    CHECK(d.recorder.gestures[1] == Gesture::Press1);
}

TEST_CASE("instant momentary: bounce is still filtered") {
    InstantDriver d;
    d.step(false, 10);
    for (int i = 0; i < 6; ++i) {
        d.step(true, 5);
        d.step(false, 5);
    }
    CHECK(d.recorder.gestures.empty());
}

TEST_CASE("instant mode reports only press_1, whatever the switch kind") {
    using picohome::GestureMode;
    using picohome::SwitchKind;
    for (const SwitchKind kind : {SwitchKind::Momentary, SwitchKind::Toggle}) {
        CHECK(producesGesture(kind, GestureMode::Instant, Gesture::Press1));
        CHECK_FALSE(producesGesture(kind, GestureMode::Instant, Gesture::Press2));
        CHECK_FALSE(producesGesture(kind, GestureMode::Instant, Gesture::HoldStart));
    }
    // With the grammar on, only the physical limitation remains.
    CHECK(producesGesture(SwitchKind::Momentary, GestureMode::Full, Gesture::HoldStart));
    CHECK(producesGesture(SwitchKind::Toggle, GestureMode::Full, Gesture::Press3));
    CHECK_FALSE(producesGesture(SwitchKind::Toggle, GestureMode::Full, Gesture::HoldStart));
}

// --- toggle (latching rocker) switches ------------------------------------
//
// A toggle switch stays where it is put, so its position stops tracking the
// light as soon as MQTT or a second switch of a P-group can drive it. What the
// user *did* is flip it, so every change of position is one press_1 — in either
// direction — exactly as the old Arduino ToggleSwitch behaved.

namespace {

struct ToggleDriver {
    GestureRecorder recorder;
    GestureDetector detector{recorder, picohome::SwitchKind::Toggle};
    uint32_t now = 0;

    void step(bool closed, uint32_t durationMs) {
        for (uint32_t i = 0; i < durationMs; ++i) detector.update(closed, ++now);
    }
};

}  // namespace

TEST_CASE("toggle switch: flipping on emits press_1") {
    ToggleDriver d;
    d.step(false, 50);
    d.step(true, 300);  // past the 200 ms toggle window
    REQUIRE(d.recorder.gestures.size() == 1);
    CHECK(d.recorder.gestures[0] == Gesture::Press1);
}

TEST_CASE("toggle switch: flipping back off also emits press_1") {
    ToggleDriver d;
    d.step(false, 50);
    d.step(true, 300);
    d.step(false, 300);
    REQUIRE(d.recorder.gestures.size() == 2);
    CHECK(d.recorder.gestures[0] == Gesture::Press1);
    CHECK(d.recorder.gestures[1] == Gesture::Press1);
}

TEST_CASE("toggle switch: flip and flip back quickly is press_2") {
    ToggleDriver d;
    d.step(false, 50);
    d.step(true, 60);   // both flips fall inside the 200 ms window
    d.step(false, 300);
    REQUIRE(d.recorder.gestures.size() == 1);
    CHECK(d.recorder.gestures[0] == Gesture::Press2);
}

TEST_CASE("toggle switch: three quick flips are press_3") {
    ToggleDriver d;
    d.step(false, 50);
    d.step(true, 60);
    d.step(false, 60);
    d.step(true, 60);
    REQUIRE(d.recorder.gestures.size() == 1);
    CHECK(d.recorder.gestures[0] == Gesture::Press3);
}

TEST_CASE("toggle switch: holding a position produces no hold gestures") {
    ToggleDriver d;
    d.step(false, 50);
    d.step(true, 10000);  // a rocker simply stays there; nothing more to report
    REQUIRE(d.recorder.gestures.size() == 1);
    CHECK(d.recorder.gestures[0] == Gesture::Press1);
}

TEST_CASE("toggle switch: its window is shorter than a momentary switch's") {
    CHECK(GestureDetector::timingFor(picohome::SwitchKind::Toggle).multiPressGapMs <
          GestureDetector::timingFor(picohome::SwitchKind::Momentary).multiPressGapMs);
}

TEST_CASE("priming adopts the boot position without reporting a flip") {
    // A toggle switch resting in the "on" position at power-up must not switch
    // every light bound to it.
    ToggleDriver d;
    d.detector.prime(true);
    d.step(true, 500);
    CHECK(d.recorder.gestures.empty());

    d.step(false, 300);  // a real flip afterwards still counts
    REQUIRE(d.recorder.gestures.size() == 1);
    CHECK(d.recorder.gestures[0] == Gesture::Press1);
}