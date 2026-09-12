#ifndef HUSK_BALLOON_H
#define HUSK_BALLOON_H

#include <stdint.h>

/*
 * Ask the guest to shrink to (or grow back to) target_bytes of usable RAM.
 * Callable from any thread. The request is asynchronous: the guest takes time
 * to hand pages back, so treat this as steering, not as an allocation.
 */
void husk_balloon_set_bytes(int64_t target_bytes);

#endif
