// Host stand-in for pico-sdk's pico/time.h: only what controller/src/net uses,
// running on the emulation's device clock (see w5100_emulation.h).
#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uint64_t absolute_time_t;

absolute_time_t get_absolute_time(void);
static inline uint32_t to_ms_since_boot(absolute_time_t t) { return (uint32_t)(t / 1000); }
void sleep_us(uint64_t us);
void sleep_ms(uint32_t ms);

#ifdef __cplusplus
}
#endif
