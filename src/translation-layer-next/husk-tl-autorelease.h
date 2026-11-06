/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef HUSK_TL_AUTORELEASE_H
#define HUSK_TL_AUTORELEASE_H

/* C render threads do not get UIKit's per-event autorelease pool. These are the
 * same Objective-C runtime entry points emitted for @autoreleasepool by Clang.
 * cleanup also balances early returns/continue; callers must stay on one thread. */
#if defined(__APPLE__) || defined(TL_AUTORELEASE_TEST)
#ifdef __cplusplus
extern "C" {
#endif
extern void *objc_autoreleasePoolPush(void);
extern void objc_autoreleasePoolPop(void *pool);
#ifdef __cplusplus
}
#endif
static inline void tl_autorelease_cleanup(void **pool) { objc_autoreleasePoolPop(*pool); }
#define TL_AUTORELEASE_SCOPE(name) \
    void *name __attribute__((cleanup(tl_autorelease_cleanup))) = objc_autoreleasePoolPush()
#else
#define TL_AUTORELEASE_SCOPE(name) ((void)0)
#endif
#endif
