// Host stand-in for pico-sdk's pico/multicore.h: core 1 becomes a thread.
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void multicore_launch_core1(void (*entry)(void));

#ifdef __cplusplus
}
#endif
