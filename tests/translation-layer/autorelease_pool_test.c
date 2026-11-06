/* SPDX-License-Identifier: GPL-2.0-or-later */
#define TL_AUTORELEASE_TEST
#include "husk-tl-autorelease.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
static unsigned depth, pushes, pops;
void *objc_autoreleasePoolPush(void) { pushes++; return (void *)(uintptr_t)++depth; }
void objc_autoreleasePoolPop(void *pool) { assert((uintptr_t)pool==depth); depth--;pops++; }
static void early_return(void) { TL_AUTORELEASE_SCOPE(pool); assert(depth==2); }
int main(void)
{
    {
        TL_AUTORELEASE_SCOPE(thread_pool);
        assert(depth==1);
        for (int i=0;i<100000;i++) {
            TL_AUTORELEASE_SCOPE(frame_pool);
            assert(depth==2);
            if (i%2) continue;
        }
        assert(depth==1&&pushes==100001&&pops==100000);
        early_return();assert(depth==1);
    }
    assert(depth==0&&pushes==pops);
    puts("PASS: production C autorelease scope balances nested frames, continue and early return over 100000 frames");
    return 0;
}
