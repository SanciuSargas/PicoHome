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