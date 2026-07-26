#pragma once

#include <cstdint>

#include "picohome/hal/hal.h"

// MCP23017 16-bit I2C port expander.
//
// An I2C transaction costs ~100 µs, so a pin object must never touch the bus:
// a panel with 40 switches would spend milliseconds per loop iteration reading
// pins one at a time, and core 0 is not allowed to spend that. Instead the
// driver keeps a shadow copy of both ports —
//
//   * inputs are re-read only when the chip's INT line says something changed
//     (plus a slow safety refresh, so a missed edge cannot strand the shadow),
//   * outputs are written into the shadow latch and flushed once per iteration,
//     so eight relays changing together cost one bus write, not eight.
//
// Pin objects then read and write bits in RAM, which is what lets a device
// behind an expander behave exactly like one on a local GPIO.

namespace picohome {

class Mcp23017 {
public:
    // How long the shadow may go unrefreshed when no interrupt arrives. This
    // is not polling: it is the safety net for a lost edge.
    static constexpr uint32_t kSafetyRefreshMs = 250;

    Mcp23017(hal::II2cBus& bus, uint8_t address);

    // Claimed by the pin objects while devices are built, before initialize().
    void claimInput(uint8_t pin, bool pullUp);
    void claimOutput(uint8_t pin);

    // Writes the direction/pull-up/interrupt configuration and takes the first
    // reading, which also anchors the safety-refresh clock. Returns false if
    // the chip does not answer.
    bool initialize(uint32_t nowMs);

    // Start of a control-loop iteration: refresh inputs if anything changed.
    void refreshInputs(uint32_t nowMs);
    // End of a control-loop iteration: write the outputs that moved.
    void flushOutputs();

    // Called from the GPIO interrupt handler — must stay this small.
    void onInterrupt() { interrupted_ = true; }

    bool inputLevel(uint8_t pin) const { return (inputShadow_ >> pin) & 1u; }
    void setOutputLevel(uint8_t pin, bool level);

    uint8_t address() const { return address_; }
    // Bus errors seen since boot; published as a diagnostic later.
    uint32_t errorCount() const { return errors_; }

private:
    bool readInputs();

    hal::II2cBus& bus_;
    uint8_t address_;

    uint16_t inputMask_ = 0;   // 1 = pin is an input
    uint16_t pullUpMask_ = 0;
    uint16_t inputShadow_ = 0;
    uint16_t outputShadow_ = 0;

    volatile bool interrupted_ = true;  // force the first read
    bool outputsDirty_ = false;
    uint32_t lastReadMs_ = 0;
    uint32_t errors_ = 0;
};

// The pin objects devices actually hold. Identical interfaces to the local
// GPIO ones, so nothing above the HAL knows the difference.
class McpDigitalInput final : public hal::IDigitalInput {
public:
    McpDigitalInput(Mcp23017& expander, uint8_t pin, bool activeHigh, bool pullUp);
    bool read() override;

private:
    Mcp23017& expander_;
    uint8_t pin_;
    bool activeHigh_;
};

class McpDigitalOutput final : public hal::IDigitalOutput {
public:
    McpDigitalOutput(Mcp23017& expander, uint8_t pin, bool activeHigh);
    void write(bool active) override;

private:
    Mcp23017& expander_;
    uint8_t pin_;
    bool activeHigh_;
};

}  // namespace picohome