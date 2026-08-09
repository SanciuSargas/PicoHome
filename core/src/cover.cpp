#include "picohome/devices/cover.h"

#include <cstdio>

#include "picohome/core/commands.h"

namespace picohome {

namespace {

// While travelling, position is republished at this rate so Home Assistant's
// cover slider follows an 82-second blind instead of appearing frozen.
constexpr uint32_t kTravelPublishMs = 1000;

uint32_t scaleToPercent(uint32_t value, uint32_t total) {
    if (total == 0) return 0;
    return (value * 100 + total / 2) / total;
}

uint32_t percentToMs(uint8_t percent, uint32_t total) {
    if (percent > 100) percent = 100;
    return (total * percent) / 100;
}

bool parsePercent(std::string_view payload, uint8_t& out) {
    if (payload.empty() || payload.size() > 3) return false;
    uint32_t value = 0;
    for (const char c : payload) {
        if (c < '0' || c > '9') return false;
        value = value * 10 + static_cast<uint32_t>(c - '0');
    }
    if (value > 100) return false;
    out = static_cast<uint8_t>(value);
    return true;
}

}  // namespace

Cover::Cover(DeviceType type, uint16_t id, std::string room, hal::IDigitalOutput& up,
             hal::IDigitalOutput& down, Timing timing)
    : Device(type, id, std::move(room)), up_(up), down_(down), timing_(timing) {}

void Cover::initialize() {
    up_.write(false);
    down_.write(false);
    moving_ = false;
    reversing_ = false;
    target_ = Target::None;

    // Nothing can be asked about where the blind is. Either it is assumed (and
    // the first run to a limit re-synchronises it), or homing starts from
    // "fully closed" so that driving to the top is a guaranteed full traverse.
    // The slats are packed at either limit, hence tilt 0; the direction points
    // at the useful next move.
    const bool assumeOpen = timing_.bootOpen && !timing_.homeAtBoot;
    positionMs_ = assumeOpen ? timing_.travelUpMs : 0;
    tiltMs_ = 0;
    direction_ = assumeOpen ? Direction::Down : Direction::Up;
}

void Cover::update(uint32_t nowMs) {
    if (!started_) {
        started_ = true;
        nowMs_ = nowMs;
        if (timing_.homeAtBoot) {
            // The estimate says "closed", so this is a full traverse whatever
            // the blind was really doing — it ends held against the top stop.
            setPosition(100);
        }
        return;
    }
    const uint32_t elapsed = nowMs - nowMs_;
    nowMs_ = nowMs;

    if (reversing_) {
        // Both relays are off; the motor is resting before it is driven again.
        if (!rested(nowMs)) return;
        energize(pendingDirection_);
        return;
    }

    if (!moving_) return;

    integrate(elapsed);

    // There is nothing left to do only when *both* axes are exhausted in this
    // direction. The slats move before the height, so a blind sitting at the
    // bottom still has a slat sweep left to close, and one at the top still has
    // one to open — checking the height alone would refuse to move them.
    const bool atLimit =
        direction_ == Direction::Up
            ? (positionMs_ >= timing_.travelUpMs && tiltMs_ >= timing_.tiltMs)
            : (positionMs_ == 0 && tiltMs_ == 0);
    if (atLimit || targetReached()) {
        stop();
        return;
    }
    publishIfDue(false);
}

uint32_t Cover::downTravelToScale(uint32_t realMs) {
    if (realMs == 0) return 0;
    if (timing_.travelDownMs == timing_.travelUpMs) return realMs;  // symmetric: 1:1

    // Going down covers the same distance in less time, so each real
    // millisecond is worth more of the scale. The remainder is carried rather
    // than truncated — dropping it would lose ~14 % of the travel on a blind
    // that falls 82 s / climbs 71 s, which is the whole point of measuring both.
    const uint64_t scaled =
        static_cast<uint64_t>(realMs) * timing_.travelUpMs + downCarry_;
    downCarry_ = static_cast<uint32_t>(scaled % timing_.travelDownMs);
    return static_cast<uint32_t>(scaled / timing_.travelDownMs);
}

void Cover::integrate(uint32_t elapsedMs) {
    if (elapsedMs == 0) return;

    if (direction_ == Direction::Up) {
        // Slats open before the blind rises.
        const uint32_t tiltRoom = timing_.tiltMs - tiltMs_;
        const uint32_t toTilt = elapsedMs < tiltRoom ? elapsedMs : tiltRoom;
        tiltMs_ += toTilt;
        const uint32_t rest = elapsedMs - toTilt;
        positionMs_ =
            positionMs_ + rest >= timing_.travelUpMs ? timing_.travelUpMs : positionMs_ + rest;
    } else {
        // Slats close before the blind lowers.
        const uint32_t toTilt = elapsedMs < tiltMs_ ? elapsedMs : tiltMs_;
        tiltMs_ -= toTilt;
        const uint32_t rest = downTravelToScale(elapsedMs - toTilt);
        positionMs_ = rest >= positionMs_ ? 0 : positionMs_ - rest;
    }
}

bool Cover::targetReached() const {
    switch (target_) {
        case Target::Position:
            return direction_ == Direction::Up ? positionMs_ >= targetPositionMs_
                                               : positionMs_ <= targetPositionMs_;
        case Target::Tilt:
            return direction_ == Direction::Up ? tiltMs_ >= targetTiltMs_ : tiltMs_ <= targetTiltMs_;
        case Target::Duration:
            return static_cast<int32_t>(nowMs_ - targetEndMs_) >= 0;
        case Target::None:
            return false;
    }
    return false;
}

void Cover::releaseRelays() {
    up_.write(false);
    down_.write(false);
    // The moment the windings dropped out is what the rest period is measured
    // from — not the moment a command arrived.
    lastOffMs_ = nowMs_;
}

bool Cover::rested(uint32_t nowMs) const {
    return static_cast<int32_t>(nowMs - lastOffMs_) >= static_cast<int32_t>(timing_.reverseDelayMs);
}

void Cover::energize(Direction direction) {
    everEnergised_ = true;
    // Drop the opposite winding *before* raising this one. On a GPIO backend
    // these are two separate register writes, so the order is the difference
    // between "never both on" and "both on for a microsecond".
    if (direction == Direction::Up) {
        down_.write(false);
        up_.write(true);
    } else {
        up_.write(false);
        down_.write(true);
    }
    direction_ = direction;
    moving_ = true;
    reversing_ = false;
}

void Cover::requestMove(Direction direction) {
    if (moving_ && direction_ == direction) return;  // already going that way

    if (moving_) {
        releaseRelays();
        moving_ = false;
    }

    // Wait out the motor rest whenever the windings were live recently — which
    // includes having just been stopped by a press, a target, or a limit, not
    // only a reversal while running. A relay takes milliseconds to open, so
    // energising the other one straight away can have both closed at once.
    pendingDirection_ = direction;
    if (!everEnergised_ || rested(nowMs_)) {
        energize(direction);  // nothing to rest from at power-up
    } else {
        reversing_ = true;
    }
}

void Cover::stop() {
    releaseRelays();
    moving_ = false;
    reversing_ = false;
    target_ = Target::None;
    committedToEnd_ = false;

    // Reaching a limit both re-calibrates the estimate and means the only
    // useful thing left to do is go the other way — the behaviour the old
    // Arduino shade code had, and what makes a one-button switch workable.
    if (positionMs_ >= timing_.travelUpMs) {
        positionMs_ = timing_.travelUpMs;
        direction_ = Direction::Down;
    } else if (positionMs_ == 0) {
        direction_ = Direction::Up;
    }
    publishIfDue(true);
}

void Cover::stepAlongDirection() {
    target_ = Target::Duration;
    targetEndMs_ = nowMs_ + timing_.stepMs;
    requestMove(direction_);
}

void Cover::toggleDirection() {
    const Direction flipped = direction_ == Direction::Up ? Direction::Down : Direction::Up;
    if (isMoving()) {
        target_ = Target::None;
        committedToEnd_ = false;
        requestMove(flipped);  // reverses through the motor rest
        return;
    }
    direction_ = flipped;  // idle: just aim the next gesture
}

void Cover::moveAlongDirection() {
    target_ = Target::None;  // dead-man: runs until released
    committedToEnd_ = false;
    requestMove(direction_);
}

void Cover::moveToEnd() {
    target_ = Target::Position;
    targetPositionMs_ = direction_ == Direction::Up ? timing_.travelUpMs : 0;
    committedToEnd_ = true;
    requestMove(direction_);
}

void Cover::releaseHold() {
    // A long hold has already committed to running to the limit, so letting go
    // must not cancel it; an ordinary hold is dead-man control.
    if (committedToEnd_) return;
    stop();
}

void Cover::setPosition(uint8_t percent) {
    const uint32_t target = percentToMs(percent, timing_.travelUpMs);
    if (target == positionMs_) return;
    target_ = Target::Position;
    targetPositionMs_ = target;
    committedToEnd_ = false;
    requestMove(target > positionMs_ ? Direction::Up : Direction::Down);
}

void Cover::setTilt(uint8_t percent) {
    if (timing_.tiltMs == 0) return;  // curtain-style cover, nothing to tilt
    const uint32_t target = percentToMs(percent, timing_.tiltMs);
    if (target == tiltMs_) return;
    target_ = Target::Tilt;
    targetTiltMs_ = target;
    committedToEnd_ = false;
    // Opening the slats is the start of an up movement, closing them of a down one.
    requestMove(target > tiltMs_ ? Direction::Up : Direction::Down);
}

uint8_t Cover::position() const {
    return static_cast<uint8_t>(scaleToPercent(positionMs_, timing_.travelUpMs));
}

uint8_t Cover::tilt() const {
    return static_cast<uint8_t>(scaleToPercent(tiltMs_, timing_.tiltMs));
}

bool Cover::handleCommand(std::string_view command, std::string_view payload) {
    if (command == commands::kSetPosition) {
        uint8_t percent = 0;
        if (!parsePercent(payload, percent)) return false;
        setPosition(percent);
        return true;
    }
    if (command == commands::kSetTilt) {
        uint8_t percent = 0;
        if (!parsePercent(payload, percent)) return false;
        setTilt(percent);
        return true;
    }
    if (command == commands::kStop) {
        stop();
        return true;
    }
    return false;
}

void Cover::publishIfDue(bool force) {
    if (!force && nowMs_ - lastPublishMs_ < kTravelPublishMs) return;
    lastPublishMs_ = nowMs_;
    publishState();
}

void Cover::publishState() {
    char payload[4];
    std::snprintf(payload, sizeof(payload), "%u", static_cast<unsigned>(position()));
    publish(commands::kState, payload, /*retained=*/true);
    if (timing_.tiltMs > 0) {
        std::snprintf(payload, sizeof(payload), "%u", static_cast<unsigned>(tilt()));
        publish(commands::kTilt, payload, /*retained=*/true);
    }
}

}  // namespace picohome
