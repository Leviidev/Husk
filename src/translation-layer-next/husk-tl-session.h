/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef HUSK_TL_SESSION_H
#define HUSK_TL_SESSION_H
#include <stdbool.h>
#include <stdint.h>

/* A session may be forgotten only after every guest-created thread has left.
   Reservations happen BEFORE pthread_create, including detached threads. */
bool tl_guest_session_begin(void);
bool tl_guest_thread_reserve(bool *tracked);
void tl_guest_thread_release(void *tracked);
void tl_guest_session_close(void);
unsigned tl_guest_session_threads(void);
bool tl_guest_session_finish(void);
void tl_guest_thread_register(uintptr_t thread, bool tracked);
/* After the counter drains, joins the retained host pthreads, including TLS
   destructors. Guest detach is emulated until this final join. */
bool tl_guest_session_reap(unsigned timeout_ms);

#ifdef TL_SESSION_IMPLEMENTATION
#include <stdint.h>
#include <stdatomic.h>
/* Low bits: 0 boot/no app, 1 accepting threads, 2 draining. Other bits count
   reservations. One CAS orders creation against closing, without a race in
   which the drain sees zero just before a new pthread becomes visible. */
static atomic_uint tl_session_word;

bool tl_guest_session_begin(void)
{
    unsigned expected = 0;
    return atomic_compare_exchange_strong(&tl_session_word, &expected, 1);
}
bool tl_guest_thread_reserve(bool *tracked)
{
    unsigned word = atomic_load(&tl_session_word);
    *tracked = false;
    for (;;) {
        if ((word & 3) == 0) return true;  /* runtime boot threads */
        if ((word & 3) == 2 || word > UINT32_MAX - 4) return false;
        if (atomic_compare_exchange_weak(&tl_session_word, &word, word + 4)) {
            *tracked = true;
            return true;
        }
    }
}
void tl_guest_thread_release(void *tracked)
{
    if (tracked) atomic_fetch_sub(&tl_session_word, 4);
}
void tl_guest_session_close(void)
{
    unsigned word = atomic_load(&tl_session_word);
    while ((word & 3) == 1 &&
           !atomic_compare_exchange_weak(&tl_session_word, &word, (word & ~3u) | 2)) {}
}
unsigned tl_guest_session_threads(void) { return atomic_load(&tl_session_word) >> 2; }
bool tl_guest_session_finish(void)
{
    unsigned expected = 2;
    return atomic_compare_exchange_strong(&tl_session_word, &expected, 0) || expected == 0;
}
#endif
#endif
