#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "picohome/control/event_router.h"
#include "picohome/control/source_event.h"
#include "picohome/core/capabilities.h"

namespace picohome {

// Which events from a subscribed source this binding acts on.
struct SourceFilter {
    SourceKey key;
    bool anyGesture = false;
    Gesture gesture = Gesture::Press1;

    bool matches(const SourceEvent& event) const;
};

// A binding is an observer of N sources driving M targets. Rules subclass this
// and add whatever state their behaviour needs; update() runs their timers.
//
// The rule set is deliberately small: these are the behaviours that must keep
// working with the broker down. Everything else is Home Assistant's job.
class Binding : public IEventObserver {
public:
    explicit Binding(std::string name);
    ~Binding() override = default;

    void addSource(const SourceFilter& filter) { sources_.push_back(filter); }
    const std::vector<SourceFilter>& sources() const { return sources_; }

    std::string_view name() const { return name_; }

    void onSourceEvent(const SourceEvent& event) final;

    // Called by the builder once all sources are attached, so rules can size
    // their per-source state at boot instead of allocating on first event.
    virtual void finalize() {}

    // Timers; default does nothing (most rules are purely event-driven).
    virtual void update(uint32_t nowMs) {}

protected:
    // Called only for events that passed this binding's source filters.
    // `sourceIndex` is the position in sources() that matched, which rules with
    // per-source state (motion) use to track which sensor spoke.
    virtual void handle(const SourceEvent& event, std::size_t sourceIndex) = 0;

private:
    std::string name_;
    std::vector<SourceFilter> sources_;
};

// Wall switch (local or on another controller) -> lights, outlets, dampers.
class ToggleRule : public Binding {
public:
    ToggleRule(std::string name, std::vector<IBinaryActuator*> targets);

protected:
    void handle(const SourceEvent& event, std::size_t sourceIndex) override;

private:
    std::vector<IBinaryActuator*> targets_;
};

// Any number of PIR/presence sensors -> any number of lights.
//
// On while at least one source is active; when the last one goes inactive the
// off-delay starts, and new activity retriggers it — which is what makes PIR
// pulses and continuous presence behave sensibly together.
//
// Each target is only switched on while its own night_mode is set, so Home
// Assistant decides *whether* motion lighting applies while the controller owns
// *how* it behaves. A light whose night_mode is off is left completely alone.
class MotionRule : public Binding {
public:
    struct Target {
        IBinaryActuator* actuator = nullptr;
        IMotionGated* gate = nullptr;  // null = never gated (not a light)
    };

    MotionRule(std::string name, std::vector<Target> targets, uint32_t offDelayMs);

    void finalize() override;
    void update(uint32_t nowMs) override;

protected:
    void handle(const SourceEvent& event, std::size_t sourceIndex) override;

private:
    void switchOn();
    void switchOff();

    std::vector<Target> targets_;
    std::vector<bool> turnedOn_;      // targets this rule switched on, sized in finalize()
    std::vector<bool> sourceActive_;  // sized in finalize()
    uint32_t offDelayMs_;
    // The off-delay is armed on the first update() after the last source went
    // quiet, because events carry no timestamp of their own.
    uint32_t countdownStartMs_ = 0;
    bool countdownRunning_ = false;
    bool countdownArmed_ = false;
    bool on_ = false;
};

// Covers. This is the one rule that consumes the whole gesture grammar rather
// than a single filtered gesture, because one wall switch has to express five
// intentions for a two-direction motor. The mapping is the one the old Arduino
// shade code established, and it is why cover bindings require a momentary
// switch: a latching rocker cannot produce a hold.
class CoverRule : public Binding {
public:
    CoverRule(std::string name, std::vector<IPositionActuator*> targets);

protected:
    void handle(const SourceEvent& event, std::size_t sourceIndex) override;

private:
    std::vector<IPositionActuator*> targets_;
};

}  // namespace picohome
