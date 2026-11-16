/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef HUSK_TL_SESSION_H
#define HUSK_TL_SESSION_H
#include <stdbool.h>
#include <stdint.h>

/* A session may be forgotten only after every guest-created thread has left.
   Reservations happen BEFORE pthread_create, including detached threads. */
bool tl_guest_session_begin(void);
unsigned tl_guest_session_epoch(void);
bool tl_guest_thread_reserve(bool *tracked);
void tl_guest_thread_release(void *tracked);
void tl_guest_session_close(void);
void tl_guest_session_abort(void);
bool tl_guest_session_closing(void);
bool tl_guest_session_force_stopping(void);
bool tl_guest_session_wake_once(void);
void tl_guest_session_wake_sleepers(void);
bool tl_guest_session_wait_close(int64_t ns);
void tl_guest_session_log_live(void);
unsigned tl_guest_session_threads(void);
bool tl_guest_session_finish(void);
void tl_guest_thread_register(uintptr_t thread, bool tracked);
void tl_guest_thread_set_retirable(bool on);
bool tl_guest_thread_is_retirable(void);
bool tl_guest_thread_is_retiring(void);
void tl_guest_register_wake_hook(void (*fn)(void));
void tl_guest_register_retire_hook(void (*fn)(void));
void tl_guest_thread_retire(void);
/* After the counter drains, joins the retained host pthreads, including TLS
   destructors. Guest detach is emulated until this final join. */
bool tl_guest_session_reap(unsigned timeout_ms);

#ifdef TL_SESSION_IMPLEMENTATION
#include <stdatomic.h>
/* Low bits: 0 boot/no app, 1 accepting threads, 2 cooperative closing,
   3 force-retiring lingering guest threads. Other bits count reservations.
   One CAS orders creation against closing, without a race in which the drain
   sees zero just before a new pthread becomes visible. */
static atomic_uint tl_session_word;
static atomic_uint tl_session_epoch;
static atomic_uint tl_session_close_seq;
static _Thread_local bool tl_guest_retirable;
static _Thread_local bool tl_guest_retiring;
static _Thread_local unsigned tl_guest_woken_epoch;
static _Thread_local unsigned tl_guest_woken_seq;
static void (*tl_session_wake_hooks[16])(void);
static int tl_session_nwake_hooks;
static void (*tl_session_retire_hooks[16])(void);
static int tl_session_nretire_hooks;
#if defined(PTHREAD_MUTEX_INITIALIZER) && defined(PTHREAD_COND_INITIALIZER)
#include <time.h>
static pthread_mutex_t tl_session_sleep_mu = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t tl_session_sleep_cv = PTHREAD_COND_INITIALIZER;
#endif

static void tl_session_run_wake_hooks(void)
{
    int n = __atomic_load_n(&tl_session_nwake_hooks, __ATOMIC_ACQUIRE);
    for (int i = 0; i < n; i++) {
        void (*fn)(void) = __atomic_load_n(&tl_session_wake_hooks[i], __ATOMIC_ACQUIRE);
        if (fn) fn();
    }
}

void tl_guest_register_wake_hook(void (*fn)(void))
{
    if (!fn) return;
#if defined(PTHREAD_MUTEX_INITIALIZER) && defined(PTHREAD_COND_INITIALIZER)
    pthread_mutex_lock(&tl_session_sleep_mu);
    for (int i = 0; i < tl_session_nwake_hooks; i++) {
        if (tl_session_wake_hooks[i] == fn) { pthread_mutex_unlock(&tl_session_sleep_mu); return; }
    }
    if (tl_session_nwake_hooks < 16) {
        __atomic_store_n(&tl_session_wake_hooks[tl_session_nwake_hooks], fn, __ATOMIC_RELEASE);
        __atomic_add_fetch(&tl_session_nwake_hooks, 1, __ATOMIC_RELEASE);
    }
    pthread_mutex_unlock(&tl_session_sleep_mu);
#else
    (void)fn;
#endif
}

void tl_guest_register_retire_hook(void (*fn)(void))
{
    if (!fn) return;
#if defined(PTHREAD_MUTEX_INITIALIZER) && defined(PTHREAD_COND_INITIALIZER)
    pthread_mutex_lock(&tl_session_sleep_mu);
    for (int i = 0; i < tl_session_nretire_hooks; i++) {
        if (tl_session_retire_hooks[i] == fn) { pthread_mutex_unlock(&tl_session_sleep_mu); return; }
    }
    if (tl_session_nretire_hooks < 16) {
        __atomic_store_n(&tl_session_retire_hooks[tl_session_nretire_hooks], fn, __ATOMIC_RELEASE);
        __atomic_add_fetch(&tl_session_nretire_hooks, 1, __ATOMIC_RELEASE);
    }
    pthread_mutex_unlock(&tl_session_sleep_mu);
#else
    (void)fn;
#endif
}

unsigned tl_guest_session_epoch(void) { return atomic_load_explicit(&tl_session_epoch, memory_order_acquire); }
void tl_guest_thread_set_retirable(bool on) { tl_guest_retirable = on; }
bool tl_guest_thread_is_retirable(void) { return tl_guest_retirable; }
bool tl_guest_thread_is_retiring(void) { return tl_guest_retiring; }

bool tl_guest_session_begin(void)
{
    unsigned expected = 0;
    if (!atomic_compare_exchange_strong(&tl_session_word, &expected, 1)) return false;
    atomic_fetch_add_explicit(&tl_session_epoch, 1, memory_order_acq_rel);
    return true;
}
bool tl_guest_thread_reserve(bool *tracked)
{
    unsigned word = atomic_load(&tl_session_word);
    *tracked = false;
    for (;;) {
        if ((word & 3) == 0) return true;  /* runtime boot threads */
        if ((word & 3) >= 2 || word > UINT32_MAX - 4) return false;
        if (atomic_compare_exchange_weak(&tl_session_word, &word, word + 4)) {
            *tracked = true;
            return true;
        }
    }
}
void tl_guest_thread_release(void *tracked)
{
    if (tracked) {
        unsigned prev = atomic_fetch_sub(&tl_session_word, 4);
        unsigned after = prev - 4;
        if ((after & ~3u) == 0 && (after & 3u) >= 2) tl_guest_session_wake_sleepers();
    }
}
void tl_guest_session_close(void)
{
    unsigned word = atomic_load(&tl_session_word);
    bool changed = false;
    while ((word & 3) == 1) {
        if (atomic_compare_exchange_weak(&tl_session_word, &word, (word & ~3u) | 2)) {
            changed = true;
            break;
        }
    }
    if (changed) atomic_fetch_add_explicit(&tl_session_close_seq, 1, memory_order_acq_rel);
    tl_guest_session_wake_sleepers();
    tl_session_run_wake_hooks();
}
void tl_guest_session_abort(void)
{
    unsigned word = atomic_load(&tl_session_word);
    bool changed = false;
    while ((word & 3) == 1 || (word & 3) == 2) {
        if (atomic_compare_exchange_weak(&tl_session_word, &word, (word & ~3u) | 3)) {
            changed = true;
            break;
        }
    }
    if (changed) atomic_fetch_add_explicit(&tl_session_close_seq, 1, memory_order_acq_rel);
    tl_guest_session_wake_sleepers();
    tl_session_run_wake_hooks();
}
bool tl_guest_session_closing(void)
{
    return (atomic_load(&tl_session_word) & 3) >= 2;
}
bool tl_guest_session_force_stopping(void)
{
    return (atomic_load(&tl_session_word) & 3) == 3;
}
bool tl_guest_session_wake_once(void)
{
    if (!tl_guest_session_closing()) return false;
    unsigned ep = atomic_load_explicit(&tl_session_epoch, memory_order_acquire);
    unsigned seq = atomic_load_explicit(&tl_session_close_seq, memory_order_acquire);
    if (tl_guest_woken_epoch == ep && tl_guest_woken_seq == seq) return false;
    tl_guest_woken_epoch = ep;
    tl_guest_woken_seq = seq;
    return true;
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
    if (tl_guest_session_force_stopping()) return true;
    if (tl_guest_session_closing() && tl_guest_session_wake_once()) return true;
    if (ns <= 0) return tl_guest_session_closing();
#if defined(PTHREAD_MUTEX_INITIALIZER) && defined(PTHREAD_COND_INITIALIZER)
    int64_t wait_ns = (tl_guest_session_closing() && ns > 20000000ll) ? 20000000ll : ns;
    bool started_closing = tl_guest_session_closing();
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    int64_t start_ns = (int64_t)now.tv_sec * 1000000000ll + now.tv_nsec;
    int64_t end_ns = start_ns + wait_ns;
    if (end_ns < start_ns) end_ns = INT64_MAX;
    pthread_mutex_lock(&tl_session_sleep_mu);
    for (;;) {
        if (tl_guest_session_force_stopping()) break;
        if (!started_closing && tl_guest_session_closing()) break;
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
    if (closing) (void)tl_guest_session_wake_once();
    pthread_mutex_unlock(&tl_session_sleep_mu);
    return closing;
#else
    return tl_guest_session_closing();
#endif
}
void tl_guest_thread_retire(void)
{
    if (!tl_guest_retirable || tl_guest_retiring) return;
    tl_guest_retiring = true;
    int n = __atomic_load_n(&tl_session_nretire_hooks, __ATOMIC_ACQUIRE);
    for (int i = 0; i < n; i++) {
        void (*fn)(void) = __atomic_load_n(&tl_session_retire_hooks[i], __ATOMIC_ACQUIRE);
        if (fn) fn();
    }
#if defined(PTHREAD_MUTEX_INITIALIZER) && defined(PTHREAD_COND_INITIALIZER)
#if defined(_WIN32) && defined(WINPTHREAD_API)
    /* mingw-w64 winpthreads' pthread_exit() longjmps without running pthread_cleanup_push frames. */
    _pthread_cleanup **clean = pthread_getclean();
    while (clean && *clean) {
        _pthread_cleanup *node = *clean;
        *clean = node->next;
        if (node->func) node->func(node->arg);
    }
#endif
    pthread_exit(NULL);
#endif
}
unsigned tl_guest_session_threads(void) { return atomic_load(&tl_session_word) >> 2; }
bool tl_guest_session_finish(void)
{
    unsigned w = atomic_load(&tl_session_word);
    if ((w & ~3u) != 0) return false;
    if (w == 0) return true;
    return atomic_compare_exchange_strong(&tl_session_word, &w, 0) || atomic_load(&tl_session_word) == 0;
}
#endif
#endif
