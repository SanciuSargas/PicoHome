#pragma once

// The test's handles on the emulated hardware that controller/src/net runs on
// in this test (w5100_emulation.cpp).

namespace picohome::emulation {

// Device time runs this many times faster than real time. Everything the
// firmware does — sleeps, timeouts, the 30 s keepalive — runs on device time,
// so a scenario that has to wait out keepalives finishes in seconds. Set it
// before net::launch(); 1 (real time) by default.
void setTimeScale(unsigned factor);

double deviceSeconds();

// How often the firmware has given up on a broker connection that was still
// open — as opposed to noticing that the broker closed it.
unsigned hangUps();

// Device seconds since an MQTT packet last went either way.
double secondsSinceLastPacket();

}  // namespace picohome::emulation
