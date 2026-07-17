#pragma once

#include <cstdint>

// Hardware abstraction interfaces. Devices talk only to these — never to
// pico-sdk directly — so all device logic builds and tests on the host.
// Electrical polarity (active-low relays, pull-ups, inverted sensors) is
// resolved inside the pin implementations, configured from the SD config;
// device code always sees logical values (true = active).

namespace picohome::hal {

class IDigitalInput {
public:
    virtual ~IDigitalInput() = default;
    virtual bool read() = 0;
};

class IDigitalOutput {
public:
    virtual ~IDigitalOutput() = default;
    virtual void write(bool active) = 0;
};

class IAnalogInput {
public:
    virtual ~IAnalogInput() = default;
    virtual uint16_t readRaw() = 0;
};

class IPwmOutput {
public:
    virtual ~IPwmOutput() = default;
    virtual void setDutyPermille(uint16_t permille) = 0;  // 0..1000
};

class IClock {
public:
    virtual ~IClock() = default;
    virtual uint32_t nowMs() = 0;
};

}  // namespace picohome::hal
