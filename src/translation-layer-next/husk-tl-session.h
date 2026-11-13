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
bool tl_guest_session_closing(void);
void tl_guest_session_wake_sleepers(void);
bool tl_guest_session_wait_close(int64_t ns);
void tl_guest_session_log_live(void);
unsigned tl_guest_session_threads(void);
bool tl_guest_session_finish(void);
void tl_guest_thread_register(uintptr_t thread, bool tracked);
/* After the counter drains, joins the retained host pthreads, including TLS
   destructors. Guest detach is emulated until this final join. */
bool tl_guest_session_reap(unsigned timeout_ms);

#ifdef TL_SESSION_IMPLEMENTATION
#include <stdatomic.h>
/* Low bits: 0 boot/no app, 1 accepting threads, 2 draining. Other bits count
   reservations. One CAS orders creation against closing, without a race in
   which the drain sees zero just before a new pthread becomes visible. */
static atomic_uint tl_session_word;
#if defined(PTHREAD_MUTEX_INITIALIZER) && defined(PTHREAD_COND_INITIALIZER)
#include <time.h>
static pthread_mutex_t tl_session_sleep_mu = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t tl_session_sleep_cv = PTHREAD_COND_INITIALIZER;
#endif

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
    tl_guest_session_wake_sleepers();
}
bool tl_guest_session_closing(void)
{
    return (atomic_load(&tl_session_word) & 3) == 2;
}
void tl_guest_session_wake_sleepers(void)
{
#if defined(PTHREAD_MUTEX_INITIALIZER) && defined(PTHREAD_COND_INITIALIZER)
    pthread_mutex_lock(&tl_session_sleep_mu);
    pthread_cond_broadcast(&tl_session_sleep_cv);
    pthread_mutex_unlock(&tl_session_sleep_mu);
#endif
}
bool tl_guest_session_wait_close(int64_t ns)
{
    if (tl_guest_session_closing()) return true;
    if (ns <= 0) return false;
#if defined(PTHREAD_MUTEX_INITIALIZER) && defined(PTHREAD_COND_INITIALIZER)
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    int64_t start_ns = (int64_t)now.tv_sec * 1000000000ll + now.tv_nsec;
    int64_t end_ns = start_ns + ns;
    if (end_ns < start_ns) end_ns = INT64_MAX;
    pthread_mutex_lock(&tl_session_sleep_mu);
    while (!tl_guest_session_closing()) {
        clock_gettime(CLOCK_MONOTONIC, &now);
        int64_t cur_ns = (int64_t)now.tv_sec * 1000000000ll + now.tv_nsec;
        int64_t left = end_ns - cur_ns;
        if (left <= 0) break;
#ifdef __APPLE__
        struct timespec rel = { (time_t)(left / 1000000000ll), (long)(left % 1000000000ll) };
        pthread_cond_timedwait_relative_np(&tl_session_sleep_cv, &tl_session_sleep_mu, &rel);
#else
        struct timespec rt;
        clock_gettime(CLOCK_REALTIME, &rt);
        int64_t rt_ns = (int64_t)rt.tv_sec * 1000000000ll + rt.tv_nsec + left;
        struct timespec abs_ts = { (time_t)(rt_ns / 1000000000ll), (long)(rt_ns % 1000000000ll) };
        pthread_cond_timedwait(&tl_session_sleep_cv, &tl_session_sleep_mu, &abs_ts);
#endif
    }
    bool closing = tl_guest_session_closing();
    pthread_mutex_unlock(&tl_session_sleep_mu);
    return closing;
#else
    return tl_guest_session_closing();
#endif
}
unsigned tl_guest_session_threads(void) { return atomic_load(&tl_session_word) >> 2; }
bool tl_guest_session_finish(void)
{
    unsigned expected = 2;
    return atomic_compare_exchange_strong(&tl_session_word, &expected, 0) || expected == 0;
}
#endif
#endif
