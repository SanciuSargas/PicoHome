#include "picohome/drivers/mcp23017.h"

namespace picohome {

namespace {

// Register map with IOCON.BANK = 0 (the power-on default), where the A and B
// registers are adjacent and a 16-bit access covers both ports at once.
constexpr uint8_t kIodirA = 0x00;
constexpr uint8_t kIpolA = 0x02;
constexpr uint8_t kGpintenA = 0x04;
constexpr uint8_t kDefvalA = 0x06;
constexpr uint8_t kIntconA = 0x08;
constexpr uint8_t kIocon = 0x0A;
constexpr uint8_t kGppuA = 0x0C;
constexpr uint8_t kGpioA = 0x12;
constexpr uint8_t kOlatA = 0x14;

// IOCON: MIRROR so either port's change shows up on both INT pins — one wire
// back to the Pico covers all 16 pins. INTPOL = 0 leaves INT active-low.
constexpr uint8_t kIoconMirror = 1u << 6;

}  // namespace

Mcp23017::Mcp23017(hal::II2cBus& bus, uint8_t address) : bus_(bus), address_(address) {}

void Mcp23017::claimInput(uint8_t pin, bool pullUp) {
    if (pin > 15) return;
    inputMask_ |= static_cast<uint16_t>(1u << pin);
    if (pullUp) pullUpMask_ |= static_cast<uint16_t>(1u << pin);
}

void Mcp23017::claimOutput(uint8_t pin) {
    if (pin > 15) return;
    inputMask_ &= static_cast<uint16_t>(~(1u << pin));
}

bool Mcp23017::initialize(uint32_t nowMs) {
    bool ok = bus_.writeRegister(address_, kIocon, kIoconMirror);
    ok = bus_.writeRegister16(address_, kIodirA, inputMask_) && ok;
    ok = bus_.writeRegister16(address_, kGppuA, pullUpMask_) && ok;
    ok = bus_.writeRegister16(address_, kIpolA, 0) && ok;  // no inversion: polarity is the pin's job
    // Compare against the previous value rather than DEFVAL, so any change
    // raises the interrupt.
    ok = bus_.writeRegister16(address_, kIntconA, 0) && ok;
    ok = bus_.writeRegister16(address_, kDefvalA, 0) && ok;
    ok = bus_.writeRegister16(address_, kGpintenA, inputMask_) && ok;
    ok = bus_.writeRegister16(address_, kOlatA, outputShadow_) && ok;

    if (!ok) {
        ++errors_;
        return false;
    }
    outputsDirty_ = false;
    // The first reading settles the shadow, so the control loop starts with a
    // clean slate rather than an interrupt that is already spent.
    interrupted_ = false;
    lastReadMs_ = nowMs;
    return readInputs();
}

void Mcp23017::refreshInputs(uint32_t nowMs) {
    const bool stale = nowMs - lastReadMs_ >= kSafetyRefreshMs;
    if (!interrupted_ && !stale) return;
    interrupted_ = false;
    lastReadMs_ = nowMs;
    readInputs();
}

bool Mcp23017::readInputs() {
    uint16_t value = 0;
    // Reading GPIO also clears the chip's interrupt, which is why this is the
    // only place that needs to touch it.
    if (!bus_.readRegister16(address_, kGpioA, value)) {
        ++errors_;
        return false;
    }
    inputShadow_ = value;
    return true;
}

void Mcp23017::setOutputLevel(uint8_t pin, bool level) {
    if (pin > 15) return;
    const uint16_t bit = static_cast<uint16_t>(1u << pin);
    const uint16_t updated =
        level ? static_cast<uint16_t>(outputShadow_ | bit)
              : static_cast<uint16_t>(outputShadow_ & ~bit);
    if (updated == outputShadow_) return;
    outputShadow_ = updated;
    outputsDirty_ = true;
}

void Mcp23017::flushOutputs() {
    if (!outputsDirty_) return;
    if (!bus_.writeRegister16(address_, kOlatA, outputShadow_)) {
        ++errors_;
        return;  // keep the dirty flag: try again next iteration
    }
    outputsDirty_ = false;
}

McpDigitalInput::McpDigitalInput(Mcp23017& expander, uint8_t pin, bool activeHigh, bool pullUp)
    : expander_(expander), pin_(pin), activeHigh_(activeHigh) {
    expander_.claimInput(pin, pullUp);
}

bool McpDigitalInput::read() { return expander_.inputLevel(pin_) == activeHigh_; }

McpDigitalOutput::McpDigitalOutput(Mcp23017& expander, uint8_t pin, bool activeHigh)
    : expander_(expander), pin_(pin), activeHigh_(activeHigh) {
    expander_.claimOutput(pin);
    expander_.setOutputLevel(pin_, !activeHigh_);  // start de-energised
}

void McpDigitalOutput::write(bool active) { expander_.setOutputLevel(pin_, active == activeHigh_); }

}  // namespace picohome