#include <doctest/doctest.h>

#include <memory>
#include <string>

#include "mocks/fakes.h"
#include "picohome/config/config.h"
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

    explicit Bench(bool bootOpen = true, uint32_t tiltMs = 1000, bool homeAtBoot = false,
                   uint32_t downMs = 0) {
        Cover::Timing timing;
        timing.travelUpMs = 10000;
        timing.travelDownMs = downMs == 0 ? 10000 : downMs;
        timing.tiltMs = tiltMs;
        timing.stepMs = 500;
        timing.reverseDelayMs = 500;
        timing.bootOpen = bootOpen;
        timing.homeAtBoot = homeAtBoot;
        cover = std::make_unique<Cover>(DeviceType::Blinds, 500, "kitchen", up, down, timing);
        cover->initialize();
        cover->update(now);  // first update just anchors the clock
    }

    void run(uint32_t ms) {
        for (uint32_t i = 0; i < ms; ++i) {
            cover->update(++now);
            // Checked after every single millisecond rather than at the end of
            // a movement: energising both windings at once is the one failure
            // that damages the motor, so no window of any width is acceptable.
            if (up.value && down.value) bothEnergised = true;
        }
    }

    bool bothEnergised = false;
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

TEST_CASE("the motor rests before reversing even when it was stopped first") {
    // The dangerous sequence: a blind that has just stopped is asked to go the
    // other way. Nothing is "moving" any more, but the relay that was live a
    // millisecond ago is still physically opening.
    Bench bench;
    bench.cover->moveAlongDirection();
    bench.run(1000);
    REQUIRE(bench.down.value);

    bench.cover->stop();
    CHECK_FALSE(bench.down.value);

    bench.cover->toggleDirection();
    bench.cover->moveAlongDirection();
    CHECK_FALSE(bench.up.value);  // must not start straight away
    bench.run(400);
    CHECK_FALSE(bench.up.value);
    bench.run(200);
    CHECK(bench.up.value);        // only after the rest period
    CHECK_FALSE(bench.bothEnergised);
}

TEST_CASE("reaching a limit also counts as recently live") {
    // Hitting the bottom stops the motor and flips the direction, so the very
    // next gesture drives the other way — the rest period must still apply.
    Bench bench;
    bench.cover->moveToEnd();
    while (bench.cover->isMoving() && bench.now < 30000) bench.run(1);
    REQUIRE(bench.cover->position() == 0);
    REQUIRE(bench.cover->direction() == Cover::Direction::Up);

    // Acting on the same millisecond the motor dropped out.
    bench.cover->moveAlongDirection();
    CHECK_FALSE(bench.up.value);
    bench.run(400);
    CHECK_FALSE(bench.up.value);
    bench.run(200);
    CHECK(bench.up.value);
    CHECK_FALSE(bench.bothEnergised);
}

TEST_CASE("a cover left idle starts without waiting") {
    // The rest period protects the motor; it must not become latency on a
    // blind that has been sitting still.
    Bench bench;
    bench.run(5000);
    bench.cover->moveAlongDirection();
    CHECK(bench.down.value);  // immediately
}

TEST_CASE("both relays are never energised together, whatever the sequence") {
    Bench bench;
    // A deliberately abusive mix of gestures and commands, including reversals
    // at speed, stops mid-travel and commands arriving during the motor rest.
    for (int round = 0; round < 3; ++round) {
        bench.cover->moveAlongDirection();
        bench.run(300);
        bench.cover->toggleDirection();
        bench.run(50);
        bench.cover->toggleDirection();  // change of mind during the rest
        bench.run(100);
        bench.cover->stop();
        bench.cover->moveAlongDirection();
        bench.run(20);
        bench.cover->setPosition(100);
        bench.run(200);
        bench.cover->setPosition(0);
        bench.run(200);
        bench.cover->setTilt(100);
        bench.run(100);
        bench.cover->moveToEnd();
        bench.run(400);
        bench.cover->stepAlongDirection();
        bench.run(30);
        bench.cover->releaseHold();
        bench.run(10);
    }
    bench.run(30000);  // and let everything run itself out

    CHECK_FALSE(bench.bothEnergised);
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

TEST_CASE("the slats still move when the blind is already at the bottom") {
    // Reported from the bench: set_tilt 50 opened the slats, set_tilt 0 did
    // nothing at all. Closing them is a *downward* movement, and the height was
    // already at its limit — but the slats were not.
    Bench bench(/*bootOpen=*/false);
    REQUIRE(bench.cover->position() == 0);
    REQUIRE(bench.cover->tilt() == 0);

    bench.cover->setTilt(50);
    bench.run(1000);
    CHECK(bench.cover->tilt() == 50);
    CHECK(bench.cover->position() == 0);  // never left the floor

    bench.run(600);  // motor rest
    bench.cover->setTilt(0);
    CHECK(bench.down.value);  // it must actually drive this time
    bench.run(1000);
    CHECK(bench.cover->tilt() == 0);
    CHECK(bench.cover->position() == 0);
    CHECK_FALSE(bench.cover->isMoving());
    CHECK_FALSE(bench.bothEnergised);
}

TEST_CASE("the slats still move when the blind is already at the top") {
    // The mirror case: at the top the slats are open, and closing them is a
    // downward movement that must not be refused either.
    Bench bench(/*bootOpen=*/true);
    bench.cover->setTilt(100);
    bench.run(2000);
    REQUIRE(bench.cover->position() == 100);
    REQUIRE(bench.cover->tilt() == 100);

    bench.run(600);
    bench.cover->setTilt(0);
    bench.run(1200);
    CHECK(bench.cover->tilt() == 0);
    CHECK(bench.cover->position() == 100);  // still up, just shaded
}

TEST_CASE("holding down at the bottom closes the slats, then stops for good") {
    Bench bench(/*bootOpen=*/false);
    bench.cover->setTilt(100);
    bench.run(1500);
    REQUIRE(bench.cover->tilt() == 100);
    bench.run(600);

    bench.cover->toggleDirection();  // now pointing down
    bench.cover->moveAlongDirection();
    bench.run(2000);

    CHECK(bench.cover->tilt() == 0);
    CHECK(bench.cover->position() == 0);
    CHECK_FALSE(bench.cover->isMoving());  // both axes exhausted: really done
    CHECK_FALSE(bench.down.value);
}

TEST_CASE("a cover with no tilt still stops dead at the height limit") {
    Bench bench(/*bootOpen=*/false, /*tiltMs=*/0);
    bench.cover->moveToEnd();  // upward, to the top
    bench.run(11000);
    REQUIRE(bench.cover->position() == 100);
    CHECK_FALSE(bench.cover->isMoving());

    // Reaching the top already turned it around, so this drives back down.
    REQUIRE(bench.cover->direction() == Cover::Direction::Down);
    bench.run(600);
    bench.cover->moveAlongDirection();
    bench.run(11000);
    CHECK(bench.cover->position() == 0);
    CHECK_FALSE(bench.cover->isMoving());
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

TEST_CASE("a blind that falls faster than it climbs is timed per direction") {
    // Climbs in 10 s, falls in 5 s — an exaggerated version of the real 82 s
    // up / 71 s down, so the arithmetic is easy to read.
    Bench bench(/*bootOpen=*/true, /*tiltMs=*/0, /*homeAtBoot=*/false, /*downMs=*/5000);
    REQUIRE(bench.cover->position() == 100);

    // Down: half the travel in half of *five* seconds.
    bench.cover->moveAlongDirection();
    bench.run(2500);
    CHECK(bench.cover->position() == 50);

    bench.cover->stop();
    bench.run(600);  // motor rest
    bench.cover->toggleDirection();
    bench.cover->moveAlongDirection();

    // Up: the same half of the travel now takes half of *ten* seconds.
    bench.run(2500);
    CHECK(bench.cover->position() == 75);
    bench.run(2500);
    CHECK(bench.cover->position() == 100);
}

TEST_CASE("a full descent takes exactly the down travel, with no rounding drift") {
    // The conversion between the two scales carries its remainder; truncating
    // it would lose ~14 % of the travel on a real blind.
    Bench bench(/*bootOpen=*/true, /*tiltMs=*/0, /*homeAtBoot=*/false, /*downMs=*/7080);
    bench.cover->moveToEnd();

    bench.run(7000);
    CHECK(bench.cover->position() > 0);  // not there quite yet
    CHECK(bench.down.value);

    bench.run(200);
    CHECK(bench.cover->position() == 0);
    CHECK_FALSE(bench.cover->isMoving());
}

TEST_CASE("set_position accounts for the direction it has to travel") {
    Bench bench(/*bootOpen=*/true, /*tiltMs=*/0, /*homeAtBoot=*/false, /*downMs=*/5000);

    bench.cover->setPosition(20);  // 80 % of the range, downwards: 4 s
    bench.run(3900);
    CHECK(bench.cover->isMoving());
    bench.run(200);
    CHECK_FALSE(bench.cover->isMoving());
    CHECK(bench.cover->position() == 20);

    bench.run(600);                 // motor rest
    bench.cover->setPosition(100);  // 80 % of the range, upwards: 8 s
    bench.run(7900);
    CHECK(bench.cover->isMoving());
    bench.run(200);
    CHECK(bench.cover->position() == 100);
}

TEST_CASE("homing at boot drives to the top for a full traverse") {
    // A blind's real position at power-up is unknown. Homing starts the
    // estimate at "closed" and asks for the top, so the run lasts a whole
    // travel — long enough to reach the stop from anywhere — and the estimate
    // is measured at the end rather than assumed.
    Bench bench(/*bootOpen=*/true, /*tiltMs=*/1000, /*homeAtBoot=*/true);

    CHECK(bench.up.value);  // moving up from the first iteration
    CHECK_FALSE(bench.down.value);
    CHECK(bench.cover->position() == 0);  // the estimate it starts from

    // Still running well past where a wrong guess would have stopped it.
    bench.run(9000);
    CHECK(bench.up.value);

    // The full traverse is the slat sweep plus the height: 1 s + 10 s here.
    bench.run(2100);
    CHECK_FALSE(bench.up.value);
    CHECK_FALSE(bench.cover->isMoving());
    CHECK(bench.cover->position() == 100);
    CHECK(bench.cover->direction() == Cover::Direction::Down);  // next gesture lowers
    CHECK_FALSE(bench.bothEnergised);
}

TEST_CASE("homing overrides the boot_position guess, and is off by default") {
    // Without homing the blind takes the assumption and stays put...
    Bench idle(/*bootOpen=*/true);
    idle.run(500);
    CHECK_FALSE(idle.up.value);
    CHECK_FALSE(idle.down.value);
    CHECK(idle.cover->position() == 100);

    // ...with it, boot_position no longer matters: homing always starts from
    // "closed" so that the drive to the top is a guaranteed full travel.
    Bench homing(/*bootOpen=*/true, /*tiltMs=*/1000, /*homeAtBoot=*/true);
    CHECK(homing.cover->position() == 0);
    CHECK(homing.up.value);
}

TEST_CASE("a homing run can be interrupted by the wall switch") {
    // It is a normal movement, so the usual grammar still applies — nobody
    // should be stuck watching a blind they did not ask to move.
    Bench bench(/*bootOpen=*/true, /*tiltMs=*/1000, /*homeAtBoot=*/true);
    bench.run(2000);
    REQUIRE(bench.up.value);

    bench.cover->stop();
    CHECK_FALSE(bench.up.value);
    bench.run(5000);
    CHECK_FALSE(bench.cover->isMoving());
}

TEST_CASE("cover relays take active_high like any other pin, and may differ per direction") {
    // Nothing about a cover pin is special: "up" and "down" go through the same
    // parser as a light's pin, so a low-trigger relay board is configured the
    // same way — and the two directions need not match, since they can even be
    // on different backends.
    config::App app;
    std::string error;
    REQUIRE_MESSAGE(config::parse(R"({
      "schema_version": 2, "building": "t",
      "devices": {
        "expanders": [{"index": 0, "address": "0x20", "int_pin": 7}],
        "actuators": [{"type": "blinds", "id": 500, "room": "kitchen",
                       "backend": {"kind": "cover",
                                   "up":   {"pin": 6, "active_high": false},
                                   "down": {"expander": 0, "pin": 3}},
                       "options": {"travel_ms": 10000, "tilt_ms": 1500}}]},
      "bindings": []})",
                                  app, error),
                    error);

    REQUIRE(app.actuators.size() == 1);
    CHECK(app.actuators[0].backend == config::BackendKind::Cover);
    CHECK(app.actuators[0].pin.pin == 6);
    CHECK_FALSE(app.actuators[0].pin.activeHigh);   // low-trigger board
    CHECK(app.actuators[0].pinDown.expander == 0);  // and on the expander
    CHECK(app.actuators[0].pinDown.activeHigh);     // defaults to high-trigger
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
