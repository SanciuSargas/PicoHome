// Host stand-in for pico-sdk's inter-core queue: same calls, a locked deque.
#pragma once

#include <stdbool.h>

typedef struct queue_impl queue_impl;
typedef struct {
    queue_impl* impl;
} queue_t;

#ifdef __cplusplus
extern "C" {
#endif

void queue_init(queue_t* q, unsigned element_size, unsigned element_count);
bool queue_try_add(queue_t* q, const void* data);
bool queue_try_remove(queue_t* q, void* data);

#ifdef __cplusplus
}
#endif
