/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef HUSK_TL_GUEST_POOL_H
#define HUSK_TL_GUEST_POOL_H

/* Guest C/Java threads have no UIKit event-loop pool. Keep a pool around the
   guest entry point, and drain it AFTER each completed EGL swap (including
   swaps redirected to the compositor). Host render threads own their scopes
   separately and never enter this pool. */
void tl_guest_pool_begin(void);
void tl_guest_pool_end(void *unused);
void tl_guest_pool_frame(void);

#ifdef TL_GUEST_POOL_IMPLEMENTATION
#if defined(__APPLE__) || defined(TL_AUTORELEASE_TEST)
#include "husk-tl-autorelease.h"
static _Thread_local void *tl_guest_pool;
void tl_guest_pool_begin(void)
{
    if (!tl_guest_pool) tl_guest_pool = objc_autoreleasePoolPush();
}
void tl_guest_pool_end(void *unused)
{
    (void)unused;
    if (tl_guest_pool) {
        objc_autoreleasePoolPop(tl_guest_pool);
        tl_guest_pool = 0;
    }
}
void tl_guest_pool_frame(void)
{
    if (tl_guest_pool) {
        objc_autoreleasePoolPop(tl_guest_pool);
        tl_guest_pool = objc_autoreleasePoolPush();
    }
}
#else
void tl_guest_pool_begin(void) {}
void tl_guest_pool_end(void *unused) { (void)unused; }
void tl_guest_pool_frame(void) {}
#endif
#endif
#endif
