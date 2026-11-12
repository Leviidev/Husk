/* SPDX-License-Identifier: GPL-2.0-or-later */
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static bool fail_allocation;
static unsigned allocations;
static void *test_realloc(void *pixels, size_t bytes)
{
    allocations++;
    return fail_allocation ? NULL : realloc(pixels, bytes);
}
#define realloc test_realloc
#include "husk-tl-frame-buffer.h"
#undef realloc

/* Actual production publication/pacing functions are included below. A host
 * mutex/condition stand-in records any attempted wait while EGL still owns
 * the texture handoff, without needing a GL driver or a timing-sensitive
 * thread race to reproduce the old destruction/publication problem. */
static struct {
    int lk, cv;
    uint32_t tex;
    int pw, ph, x, y, w, h;
    uint64_t seq, drawn_seq;
    bool on, opaque;
    uint8_t *px;
    size_t px_capacity;
} E2;
static struct { atomic_bool ended, paused; struct { int width, height; } cfg; } A;
static bool handoff_held;
static unsigned broadcasts, waits;

static void tl_log_line(const char *format, ...) { (void)format; }
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

    /* CPU fallback must report whether publication actually succeeded so EGL
     * does not transfer surface ownership away from a retained GPU/CPU frame. */
    uint8_t small[16]; memset(small, 0x34, sizeof(small));
    uint8_t large[64]; memset(large, 0x56, sizeof(large));
    handoff_held = true;
    tl_javaapp_app_tex_publish_with_opacity(90, 800, 600, 3, 4, 700, 500, true);
    assert(E2.opaque);
    tl_javaapp_app_tex_publish(91, 800, 600, 3, 4, 700, 500);
    assert(!E2.opaque); /* Old API explicitly preserves texture alpha. */
    tl_javaapp_app_tex_publish_with_opacity(91, 800, 600, 3, 4, 700, 500, true);
    uint64_t sequence = E2.seq;
    unsigned notifications = broadcasts;
    fail_allocation = true;
    assert(!tl_javaapp_app_pixels_rect_publish(small, 2, 2, 9, 10, 200, 100));
    assert(E2.seq == sequence && broadcasts == notifications && !E2.lk);
    assert(E2.on && E2.tex == 91 && E2.pw == 800 && E2.ph == 600);
    assert(E2.opaque); /* Failed CPU publication retains matching old opacity. */
    assert(E2.x == 3 && E2.y == 4 && E2.w == 700 && E2.h == 500 && !E2.px);
    fail_allocation = false;
    assert(tl_javaapp_app_pixels_rect_publish(small, 2, 2, 9, 10, 200, 100));
    assert(E2.seq == sequence + 1 && broadcasts == notifications + 1 && !E2.lk);
    assert(E2.on && !E2.tex && E2.pw == 2 && E2.ph == 2);
    assert(!E2.opaque);
    assert(E2.x == 9 && E2.y == 10 && E2.w == 200 && E2.h == 100);
    assert(!memcmp(E2.px, small, sizeof(small)) && waits == 1);

    uint8_t *previous_pixels = E2.px;
    sequence = E2.seq; notifications = broadcasts;
    fail_allocation = true;
    assert(!tl_javaapp_app_pixels_rect_publish(large, 4, 4, 20, 30, 400, 300));
    assert(E2.seq == sequence && broadcasts == notifications && E2.px == previous_pixels);
    assert(E2.on && !E2.tex && E2.pw == 2 && E2.ph == 2 && !E2.lk);
    assert(E2.x == 9 && E2.y == 10 && E2.w == 200 && E2.h == 100);
    assert(!memcmp(E2.px, small, sizeof(small)));
    /* The legacy void entry point retains exactly the same failure behavior. */
    tl_javaapp_app_pixels_rect(large, 4, 4, 20, 30, 400, 300);
    assert(E2.seq == sequence && broadcasts == notifications && !E2.lk);
    fail_allocation = false;
    assert(tl_javaapp_app_pixels_rect_publish_with_opacity(large, 4, 4, 20, 30, 0, 0, true));
    assert(E2.opaque);
    assert(E2.w == A.cfg.width && E2.h == A.cfg.height && !memcmp(E2.px, large, sizeof(large)));
    unsigned before = allocations;
    tl_javaapp_app_pixels_rect(small, 2, 2, 0, 0, 100, 50);
    assert(allocations == before && E2.pw == 2 && E2.ph == 2 && !E2.lk);
    assert(!E2.opaque);
    sequence = E2.seq; notifications = broadcasts;
    assert(tl_javaapp_app_pixels_rect_publish(NULL, 0, 0, 0, 0, 0, 0));
    assert(!E2.on && E2.seq == sequence + 1 && broadcasts == notifications + 1 && !E2.lk && waits == 1);
    handoff_held = false;
    free(E2.px);
    puts("PASS: production GPU/CPU publication is nonblocking under handoff, preserves geometry and prior frames on CPU allocation failure, reports success/clear, and paces only after ownership unlock");
    return 0;
}
