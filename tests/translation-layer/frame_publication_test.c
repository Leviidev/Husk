/* SPDX-License-Identifier: GPL-2.0-or-later */
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdatomic.h>
#include <stdio.h>
#include <time.h>

/* Actual production publication/pacing functions are included below. A host
 * mutex/condition stand-in records any attempted wait while EGL still owns
 * the texture handoff, without needing a GL driver or a timing-sensitive
 * thread race to reproduce the old destruction/publication problem. */
static struct {
    int lk, cv;
    uint32_t tex;
    int pw, ph, x, y, w, h;
    uint64_t seq, drawn_seq;
    bool on;
} E2;
static struct { atomic_bool ended, paused; struct { int width, height; } cfg; } A;
static bool handoff_held;
static unsigned broadcasts, waits;

static void pthread_mutex_lock(int *mutex) { assert(!*mutex); *mutex = 1; }
static void pthread_mutex_unlock(int *mutex) { assert(*mutex); *mutex = 0; }
static void pthread_cond_broadcast(int *condition) { (void)condition; assert(E2.lk); broadcasts++; }
static int pthread_cond_timedwait(int *condition, int *mutex, const struct timespec *deadline)
{
    (void)condition; (void)deadline;
    assert(*mutex && !handoff_held);
    waits++;
    return 1;
}
static int test_clock_gettime(int clock, struct timespec *value)
{
    (void)clock;
    value->tv_sec = 1; value->tv_nsec = 999000000;
    return 0;
}
#ifndef CLOCK_REALTIME
#define CLOCK_REALTIME 0
#endif
#define clock_gettime test_clock_gettime
#include "frame_publication_under_test.h"
#undef clock_gettime

int main(void)
{
    A.cfg.width = 1864; A.cfg.height = 860;
    E2.seq = 4; E2.drawn_seq = 0;
    handoff_held = true;
    uint64_t previous = tl_javaapp_app_tex_publish(71, 1568, 724, 19, 23, 900, 400);
    assert(previous == 4 && E2.seq == 5 && E2.on && E2.tex == 71);
    assert(E2.pw == 1568 && E2.ph == 724 && E2.x == 19 && E2.y == 23);
    assert(E2.w == 900 && E2.h == 400 && !E2.lk && !waits && broadcasts == 1);
    handoff_held = false;
    tl_javaapp_app_frame_pace(previous);
    assert(waits == 1 && !E2.lk);

    atomic_store(&A.paused, true);
    tl_javaapp_app_frame_pace(99);
    assert(waits == 1);
    atomic_store(&A.paused, false); atomic_store(&A.ended, true);
    tl_javaapp_app_frame_pace(99);
    assert(waits == 1);
    atomic_store(&A.ended, false); E2.drawn_seq = E2.seq;
    tl_javaapp_app_frame_pace(previous);
    assert(waits == 1);

    handoff_held = true;
    previous = tl_javaapp_app_tex_publish(72, 400, 300, 0, 0, 0, 0);
    assert(E2.w == 1864 && E2.h == 860 && previous == 5 && E2.seq == 6);
    /* Surface destruction/reset clears publication before ownership unlock;
     * it cannot block waiting for that same compositor to consume a frame. */
    assert(tl_javaapp_app_tex_publish(0, 0, 0, 0, 0, 0, 0) == 0);
    assert(!E2.on && !E2.tex && E2.seq == 7 && !E2.lk && waits == 1);
    handoff_held = false;
    tl_javaapp_app_frame_pace(0);
    assert(waits == 1);
    puts("PASS: production texture publication is nonblocking under handoff, pairs geometry with texture, clears on close and paces only after ownership unlock");
    return 0;
}
