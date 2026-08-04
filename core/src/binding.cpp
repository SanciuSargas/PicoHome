#include "picohome/control/binding.h"

namespace picohome {

bool SourceFilter::matches(const SourceEvent& event) const {
    if (!(key == event.key)) return false;
    if (event.kind != EventKind::Gesture) return true;  // sensor edges carry no gesture
    return anyGesture || event.gesture() == gesture;
}

Binding::Binding(std::string name) : name_(std::move(name)) {}

void Binding::onSourceEvent(const SourceEvent& event) {
    for (std::size_t i = 0; i < sources_.size(); ++i) {
        if (sources_[i].matches(event)) {
            handle(event, i);
            return;  // one binding reacts once per event, even with duplicate sources
        }
    }
}

// --- toggle ---------------------------------------------------------------

ToggleRule::ToggleRule(std::string name, std::vector<IBinaryActuator*> targets)
    : Binding(std::move(name)), targets_(std::move(targets)) {}

void ToggleRule::handle(const SourceEvent&, std::size_t) {
    for (IBinaryActuator* target : targets_) target->toggle();
}

// --- motion ---------------------------------------------------------------

MotionRule::MotionRule(std::string name, std::vector<Target> targets, uint32_t offDelayMs)
    : Binding(std::move(name)), targets_(std::move(targets)), offDelayMs_(offDelayMs) {}

void MotionRule::finalize() {
    sourceActive_.assign(sources().size(), false);
    turnedOn_.assign(targets_.size(), false);
}

void MotionRule::handle(const SourceEvent& event, std::size_t sourceIndex) {
    // A PIR reports motion as a pulse and a presence sensor as a level; both
    // arrive here as edges, and either one going active means "occupied".
    const bool active = event.kind == EventKind::SensorEdge ? event.active() : true;
    if (sourceIndex < sourceActive_.size()) sourceActive_[sourceIndex] = active;

    if (active) {
        countdownRunning_ = false;
        countdownArmed_ = false;
        if (!on_) switchOn();
        return;
    }

    for (const bool stillActive : sourceActive_) {
        if (stillActive) return;  // someone is still present
    }
    if (on_) {  // last source went quiet: start the off-delay
        countdownRunning_ = true;
        countdownArmed_ = false;
    }
}

void MotionRule::update(uint32_t nowMs) {
    if (!countdownRunning_) return;
    if (!countdownArmed_) {
        countdownArmed_ = true;
        countdownStartMs_ = nowMs;
        return;
    }
    if (nowMs - countdownStartMs_ >= offDelayMs_) {
        switchOff();
        countdownRunning_ = false;
        countdownArmed_ = false;
    }
}

void MotionRule::switchOn() {
    on_ = true;
    for (std::size_t i = 0; i < targets_.size(); ++i) {
        // Daytime (night_mode off): leave this light entirely alone — including
        // not turning it off later, since motion never touched it.
        if (targets_[i].gate != nullptr && !targets_[i].gate->nightMode()) continue;
        turnedOn_[i] = true;
        targets_[i].actuator->setBinary(true);
    }
}

// --- cover ----------------------------------------------------------------

CoverRule::CoverRule(std::string name, std::vector<IPositionActuator*> targets)
    : Binding(std::move(name)), targets_(std::move(targets)) {}

void CoverRule::handle(const SourceEvent& event, std::size_t) {
    if (event.kind != EventKind::Gesture) return;

    for (IPositionActuator* target : targets_) {
        switch (event.gesture()) {
            case Gesture::Press1:
                // The press that everyone reaches for first: halt a moving
                // blind, or nudge the slats when it is already still.
                if (target->isMoving()) {
                    target->stop();
                } else {
                    target->stepAlongDirection();
                }
                break;
            case Gesture::Press2:
                target->toggleDirection();
                break;
            case Gesture::HoldStart:
                target->moveAlongDirection();
                break;
            case Gesture::HoldLong:
                target->moveToEnd();
                break;
            case Gesture::HoldEnd:
                target->releaseHold();
                break;
            case Gesture::Press3:
                break;  // unused for covers
        }
    }
}

void MotionRule::switchOff() {
    on_ = false;
    for (std::size_t i = 0; i < targets_.size(); ++i) {
        if (!turnedOn_[i]) continue;
        turnedOn_[i] = false;
        targets_[i].actuator->setBinary(false);
    }
}

}  // namespace picohome
