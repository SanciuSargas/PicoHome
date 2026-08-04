#include <doctest/doctest.h>

#include <memory>
#include <string>

#include "mocks/fakes.h"
#include "picohome/devices/cover.h"

using namespace picohome;
using test::FakeDigitalOutput;

namespace {

// A blind on the bench, driven a millisecond at a time so the timing model can
// be inspected exactly. travel 10 s, slat sweep 1 s — the same shape as the
// real 82 s / 1.5 s blinds, small enough to read.
struct Bench {
    FakeDigitalOutput up;
    FakeDigitalOutput down;
    std::unique_ptr<Cover> cover;
    uint32_t now = 0;

    explicit Bench(bool bootOpen = true, uint32_t tiltMs = 1000) {
        Cover::Timing timing;
        timing.travelMs = 10000;
        timing.tiltMs = tiltMs;
        timing.stepMs = 500;
        timing.reverseDelayMs = 500;
        timing.bootOpen = bootOpen;
        cover = std::make_unique<Cover>(DeviceType::Blinds, 500, "kitchen", up, down, timing);
        cover->initialize();
        cover->update(now);  // first update just anchors the clock
    }

    void run(uint32_t ms) {
        for (uint32_t i = 0; i < ms; ++i) cover->update(++now);
    }
};

}  // namespace

TEST_CASE("a blind assumed open starts pointing down, with its slats packed") {
    Bench bench;
    CHECK(bench.cover->position() == 100);
    CHECK(bench.cover->tilt() == 0);
    CHECK(bench.cover->direction() == Cover::Direction::Down);
    CHECK_FALSE(bench.cover->isMoving());
    CHECK_FALSE(bench.up.value);
    CHECK_FALSE(bench.down.value);
}

TEST_CASE("going down from fully open lowers immediately: the slats are already closed") {
    Bench bench;
    bench.cover->moveAlongDirection();
    CHECK(bench.down.value);
    CHECK_FALSE(bench.up.value);

    bench.run(2000);
    CHECK(bench.cover->tilt() == 0);
    CHECK(bench.cover->position() == 80);  // 2 s of a 10 s travel
}

TEST_CASE("going up rotates the slats open before the blind rises") {
    Bench bench(/*bootOpen=*/false);
    REQUIRE(bench.cover->position() == 0);
    REQUIRE(bench.cover->direction() == Cover::Direction::Up);

    bench.cover->moveAlongDirection();
    bench.run(500);  // half of the 1 s slat sweep
    CHECK(bench.cover->tilt() == 50);
    CHECK(bench.cover->position() == 0);  // still on the floor

    bench.run(500);  // slats now fully open
    CHECK(bench.cover->tilt() == 100);
    CHECK(bench.cover->position() == 0);

    bench.run(1000);  // only now does it climb
    CHECK(bench.cover->position() == 10);
}

TEST_CASE("coming back down closes the slats before lowering") {
    Bench bench(/*bootOpen=*/false);
    bench.cover->moveAlongDirection();
    bench.run(3000);  // slats open, then 2 s of travel
    REQUIRE(bench.cover->tilt() == 100);
    REQUIRE(bench.cover->position() == 20);
    bench.cover->stop();

    bench.cover->toggleDirection();
    bench.cover->moveAlongDirection();
    bench.run(600);  // the reverse rest, then slat movement
    CHECK(bench.cover->position() == 20);  // height untouched so far
    CHECK(bench.cover->tilt() < 100);
}

TEST_CASE("a short press nudges the slats rather than the height") {
    Bench bench(/*bootOpen=*/false);
    bench.cover->stepAlongDirection();  // direction is up
    bench.run(600);

    CHECK(bench.cover->tilt() == 50);      // 500 ms of a 1 s sweep
    CHECK(bench.cover->position() == 0);
    CHECK_FALSE(bench.cover->isMoving());  // and it stopped by itself
    CHECK_FALSE(bench.up.value);
}

TEST_CASE("press while moving stops; press while idle steps") {
    Bench bench;
    bench.cover->moveAlongDirection();
    bench.run(2000);
    REQUIRE(bench.cover->isMoving());

    bench.cover->stop();
    CHECK_FALSE(bench.cover->isMoving());
    CHECK_FALSE(bench.down.value);
    const uint8_t held = bench.cover->position();

    bench.run(3000);
    CHECK(bench.cover->position() == held);  // really stopped, not drifting
}

TEST_CASE("reversing never energises both relays and rests the motor first") {
    Bench bench;
    bench.cover->moveAlongDirection();
    bench.run(1000);
    REQUIRE(bench.down.value);

    bench.cover->toggleDirection();
    CHECK_FALSE(bench.down.value);  // dropped out immediately
    CHECK_FALSE(bench.up.value);    // and the other one is not on yet

    bench.run(400);  // still inside the 500 ms rest
    CHECK_FALSE(bench.up.value);
    CHECK_FALSE(bench.down.value);

    bench.run(200);  // rest over
    CHECK(bench.up.value);
    CHECK_FALSE(bench.down.value);
}

TEST_CASE("hold moves while held and stops on release") {
    Bench bench;
    bench.cover->moveAlongDirection();
    bench.run(1500);
    const uint8_t reached = bench.cover->position();
    CHECK(reached < 100);

    bench.cover->releaseHold();
    CHECK_FALSE(bench.cover->isMoving());
    bench.run(2000);
    CHECK(bench.cover->position() == reached);
}

TEST_CASE("a long hold commits to the end, so letting go does not stop it") {
    Bench bench;
    bench.cover->moveToEnd();
    bench.run(1000);
    bench.cover->releaseHold();  // the switch was let go

    CHECK(bench.cover->isMoving());
    bench.run(20000);
    CHECK(bench.cover->position() == 0);
    CHECK_FALSE(bench.cover->isMoving());
}

TEST_CASE("reaching a limit re-calibrates and turns the blind around") {
    Bench bench;
    bench.cover->moveToEnd();
    bench.run(20000);

    CHECK(bench.cover->position() == 0);
    CHECK(bench.cover->tilt() == 0);
    CHECK_FALSE(bench.down.value);  // motor released at the limit
    // The only useful thing left is to go back up, so that is where the next
    // gesture points.
    CHECK(bench.cover->direction() == Cover::Direction::Up);
}

TEST_CASE("set_position drives the right way and stops on arrival") {
    Bench bench;
    CHECK(bench.cover->handleCommand("set_position", "40"));
    CHECK(bench.down.value);  // from 100 towards 40 means downwards

    bench.run(20000);
    CHECK(bench.cover->position() == 40);
    CHECK_FALSE(bench.cover->isMoving());

    CHECK(bench.cover->handleCommand("set_position", "90"));
    bench.run(20000);
    CHECK(bench.cover->position() == 90);
}

TEST_CASE("set_tilt moves only the slats") {
    Bench bench(/*bootOpen=*/false);
    CHECK(bench.cover->handleCommand("set_tilt", "100"));
    bench.run(2000);

    CHECK(bench.cover->tilt() == 100);
    CHECK(bench.cover->position() == 0);  // never started climbing
    CHECK_FALSE(bench.cover->isMoving());
}

TEST_CASE("stop and malformed commands") {
    Bench bench;
    bench.cover->moveAlongDirection();
    bench.run(500);
    CHECK(bench.cover->handleCommand("stop", ""));
    CHECK_FALSE(bench.cover->isMoving());

    CHECK_FALSE(bench.cover->handleCommand("set_position", "120"));
    CHECK_FALSE(bench.cover->handleCommand("set_position", "half"));
    CHECK_FALSE(bench.cover->handleCommand("set_binary", "1"));
}

TEST_CASE("a curtain-style cover with no tilt moves height only") {
    Bench bench(/*bootOpen=*/false, /*tiltMs=*/0);
    bench.cover->moveAlongDirection();
    bench.run(1000);

    CHECK(bench.cover->position() == 10);  // no slat sweep to work through first
    CHECK(bench.cover->tilt() == 0);

    bench.cover->stop();
    bench.cover->setTilt(50);  // and tilt commands are simply inert
    CHECK_FALSE(bench.cover->isMoving());
}

TEST_CASE("position and tilt are published retained") {
    Bench bench;
    test::RecordingPublisher publisher;
    bench.cover->attachPublisher(&publisher);
    bench.cover->publishState();

    REQUIRE(publisher.messages.size() == 2);
    CHECK(publisher.messages[0].command == "state");
    CHECK(publisher.messages[0].payload == "100");
    CHECK(publisher.messages[0].retained);
    CHECK(publisher.messages[1].command == "tilt");
    CHECK(publisher.messages[1].retained);
}

TEST_CASE("position is republished while travelling, so the slider keeps up") {
    Bench bench;
    test::RecordingPublisher publisher;
    bench.cover->attachPublisher(&publisher);

    bench.cover->moveAlongDirection();
    bench.run(3500);
    // Roughly one update per second of travel, not one per loop iteration.
    CHECK(publisher.messages.size() >= 4);
    CHECK(publisher.messages.size() <= 16);
}
