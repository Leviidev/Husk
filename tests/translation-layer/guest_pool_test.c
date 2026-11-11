/* SPDX-License-Identifier: GPL-2.0-or-later */
#define TL_AUTORELEASE_TEST
#define TL_GUEST_POOL_IMPLEMENTATION
#include "husk-tl-guest-pool.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
static unsigned depth, pushes, pops, pending[16];
void *objc_autoreleasePoolPush(void) { pushes++; return (void *)(uintptr_t)++depth; }
void objc_autoreleasePoolPop(void *pool)
{
    assert((uintptr_t)pool == depth);
    pending[depth] = 0;
    depth--; pops++;
}
int main(void)
{
    /* A host render thread has its own scopes: the guest frame hook is inert. */
    {
        TL_AUTORELEASE_SCOPE(host);
        tl_guest_pool_frame();
        assert(depth == 1 && pops == 0);
    }
    for (int session = 0; session < 3; session++) {
        tl_guest_pool_begin();
        tl_guest_pool_begin(); /* idempotent */
        for (int frame = 0; frame < 100000; frame++) {
            assert(depth == 1);
            pending[depth] += 250; /* objects made before entering eglSwapBuffers */
            {
                TL_AUTORELEASE_SCOPE(driver);
                pending[depth] += 10;
            }
            tl_guest_pool_frame();
            assert(depth == 1 && pending[depth] == 0);
        }
        tl_guest_pool_end(NULL);
        tl_guest_pool_end(NULL);
        assert(depth == 0 && pushes == pops);
    }
    puts("PASS: guest render pools drain pre-swap temporary objects across 300000 frames, without popping host scopes");
    return 0;
}
