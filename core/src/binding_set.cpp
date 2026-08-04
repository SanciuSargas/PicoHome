#include "picohome/control/binding_set.h"

namespace picohome {

namespace {

std::string describe(const config::BindingDef& def) {
    return def.name.empty() ? std::string("<unnamed binding>") : ("binding '" + def.name + "'");
}

std::string describe(const config::DeviceRef& ref) {
    return std::string(toSegment(ref.type)) + "/" + std::to_string(ref.id);
}

bool resolveTargets(const config::BindingDef& def, DeviceRegistry& registry,
                    std::vector<Device*>& out, std::string& error) {
    for (const config::DeviceRef& ref : def.targets) {
        Device* device = registry.find(ref.type, ref.id);
        if (device == nullptr) {
            error = describe(def) + ": target " + describe(ref) + " does not exist";
            return false;
        }
        out.push_back(device);
    }
    return true;
}

bool collectBinary(const config::BindingDef& def, const std::vector<Device*>& devices,
                   std::vector<IBinaryActuator*>& out, std::string& error) {
    for (Device* device : devices) {
        IBinaryActuator* actuator = device->asBinary();
        if (actuator == nullptr) {
            error = describe(def) + ": target " + std::string(toSegment(device->type())) + "/" +
                    std::to_string(device->id()) + " is not an on/off device";
            return false;
        }
        out.push_back(actuator);
    }
    return true;
}

bool attachSources(const config::BindingDef& def, const config::App& config, Binding& binding,
                   EventRouter& router, RemoteSourceTable& remoteSources, std::string& error) {
    for (const config::SourceDef& source : def.sources) {
        SourceFilter filter;
        switch (source.kind) {
            case config::SourceType::Button: {
                const config::ButtonDef* button = nullptr;
                for (const config::ButtonDef& candidate : config.buttons) {
                    if (candidate.id == source.buttonId) {
                        button = &candidate;
                        break;
                    }
                }
                if (button == nullptr) {
                    error = describe(def) + ": source button " + std::to_string(source.buttonId) +
                            " does not exist";
                    return false;
                }
                // The cover grammar is built on holding the switch, so a
                // latching rocker can only ever reach half of it.
                if (def.rule == config::RuleKind::Cover && !producesHoldGestures(button->kind)) {
                    error = describe(def) + ": button " + std::to_string(source.buttonId) +
                            " is a toggle switch, but a cover needs a momentary one to be held";
                    return false;
                }
                // A latching rocker cannot be held, so a binding waiting for a
                // hold from one would never fire. Catch it at boot rather than
                // leaving someone pressing a switch that does nothing.
                if (isHoldGesture(source.gesture) && !producesHoldGestures(button->kind)) {
                    error = describe(def) + ": button " + std::to_string(source.buttonId) +
                            " is a toggle switch and cannot produce " +
                            std::string(toPayload(source.gesture)) +
                            " — a momentary switch is needed for hold gestures";
                    return false;
                }
                filter.key = SourceKey::button(source.buttonId);
                filter.gesture = source.gesture;
                // The cover rule reads the whole grammar — press, double press,
                // hold, long hold, release — so it must not be handed a single
                // filtered gesture the way toggle bindings are.
                filter.anyGesture = def.rule == config::RuleKind::Cover;
                break;
            }
            case config::SourceType::Sensor: {
                bool exists = false;
                for (const config::SensorDef& sensor : config.sensors) {
                    if (sensor.type == source.sensor.type && sensor.id == source.sensor.id) {
                        exists = true;
                        break;
                    }
                }
                if (!exists) {
                    error = describe(def) + ": source sensor " + describe(source.sensor) +
                            " does not exist";
                    return false;
                }
                filter.key = SourceKey::sensor(source.sensor.type, source.sensor.id);
                filter.anyGesture = true;  // sensor edges, no gesture to match
                break;
            }
            case config::SourceType::Remote: {
                // A device on another controller. Interning here is also what
                // produces the extra MQTT subscriptions the session needs.
                // Motion wants a sensor's two edges; everything else wants one
                // exact button event.
                const uint16_t index =
                    def.rule == config::RuleKind::Motion
                        ? remoteSources.internSensor(source.topic, source.payloadActive,
                                                     source.payloadInactive)
                        : remoteSources.internAction(source.topic, source.payload);
                filter.key = SourceKey::remote(index);
                filter.anyGesture = true;  // the payload match *is* the filter
                break;
            }
        }
        binding.addSource(filter);
        router.subscribe(filter.key, &binding);
    }
    return true;
}

}  // namespace

bool BindingSet::build(const config::App& config, DeviceRegistry& registry, EventRouter& router,
                       RemoteSourceTable& remoteSources, std::string& error) {
    bindings_.clear();
    bindings_.reserve(config.bindings.size());

    for (const config::BindingDef& def : config.bindings) {
        std::vector<Device*> targets;
        if (!resolveTargets(def, registry, targets, error)) return false;

        std::unique_ptr<Binding> binding;
        switch (def.rule) {
            case config::RuleKind::Toggle: {
                std::vector<IBinaryActuator*> binary;
                if (!collectBinary(def, targets, binary, error)) return false;
                binding = std::make_unique<ToggleRule>(def.name, std::move(binary));
                break;
            }
            case config::RuleKind::Motion: {
                std::vector<MotionRule::Target> motionTargets;
                for (Device* device : targets) {
                    MotionRule::Target target;
                    target.actuator = device->asBinary();
                    if (target.actuator == nullptr) {
                        error = describe(def) + ": target " +
                                std::string(toSegment(device->type())) + "/" +
                                std::to_string(device->id()) + " is not an on/off device";
                        return false;
                    }
                    target.gate = device->asMotionGated();
                    motionTargets.push_back(target);
                }
                binding = std::make_unique<MotionRule>(def.name, std::move(motionTargets),
                                                       def.offDelayMs);
                break;
            }
            case config::RuleKind::Cover: {
                std::vector<IPositionActuator*> covers;
                for (Device* device : targets) {
                    IPositionActuator* cover = device->asPosition();
                    if (cover == nullptr) {
                        error = describe(def) + ": target " +
                                std::string(toSegment(device->type())) + "/" +
                                std::to_string(device->id()) + " cannot be positioned";
                        return false;
                    }
                    covers.push_back(cover);
                }
                binding = std::make_unique<CoverRule>(def.name, std::move(covers));
                break;
            }
        }

        if (!attachSources(def, config, *binding, router, remoteSources, error)) return false;
        binding->finalize();
        bindings_.push_back(std::move(binding));
    }
    return true;
}

void BindingSet::updateAll(uint32_t nowMs) {
    for (auto& binding : bindings_) binding->update(nowMs);
}

}  // namespace picohome
