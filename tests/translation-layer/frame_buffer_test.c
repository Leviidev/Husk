/* SPDX-License-Identifier: GPL-2.0-or-later */
#include <assert.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fail_allocation, allocations;
static void *test_realloc(void *p, size_t bytes)
{
    allocations++;
    return fail_allocation ? NULL : realloc(p, bytes);
}
#define realloc test_realloc
#include "husk-tl-frame-buffer.h"
#undef realloc

/* Host lock model verifies every exit releases the production function's critical section. */
static void pthread_mutex_lock(int *held) { assert(!*held); *held = 1; }
static void pthread_mutex_unlock(int *held) { assert(*held); *held = 0; }
static void tl_log_line(const char *format, ...) { (void)format; }
static struct { struct { int width, height; } cfg; } A = { { 320, 180 } };
static struct {
    int lk;
    uint32_t tex; uint8_t *px; int pw, ph; int x, y, w, h; uint64_t seq; bool on;
    size_t px_capacity;
} E2;
#include "frame_copy_under_test.h"

int main(void)
{
    uint8_t small[16]; memset(small, 0x34, sizeof(small));
    uint8_t large[64]; memset(large, 0x56, sizeof(large));
    fail_allocation = 1;
    tl_javaapp_app_pixels(small, 2, 2);
    assert(!E2.on && !E2.px && !E2.px_capacity && !E2.seq && !E2.lk);
    fail_allocation = 0;
    tl_javaapp_app_pixels(small, 2, 2);
    assert(E2.on && E2.pw == 2 && E2.ph == 2 && E2.px_capacity == 16);
    assert(!memcmp(E2.px, small, 16) && !E2.lk && E2.seq == 1);
    uint8_t *previous = E2.px;
    fail_allocation = 1;
    tl_javaapp_app_pixels(large, 4, 4);
    assert(E2.px == previous && E2.px_capacity == 16 && !memcmp(E2.px, small, 16));
    assert(E2.on && E2.pw == 2 && E2.ph == 2 && E2.seq == 1 && !E2.lk);
    tl_javaapp_app_pixels(large, INT_MAX, INT_MAX);
    tl_javaapp_app_pixels(large, -1, 4);
    tl_javaapp_app_pixels(large, 4, 0);
    assert(E2.seq == 1 && !E2.lk && E2.px == previous);
    fail_allocation = 0;
    /* Shared-texture metadata may change independently of the CPU allocation. */
    E2.pw = E2.ph = 4; E2.tex = 42;
    tl_javaapp_app_pixels(large, 4, 4);
    assert(E2.px_capacity == 64 && !memcmp(E2.px, large, 64) && E2.tex == 0);
    assert(E2.w == A.cfg.width && E2.h == A.cfg.height && !E2.lk);
    int before = allocations;
    for (int i = 0; i < 50000; i++) {
        tl_javaapp_app_pixels(small, 2, 2);
        tl_javaapp_app_pixels(large, 4, 4);
    }
    assert(allocations == before && E2.px_capacity == 64 && !E2.lk);
    uint64_t sequence = E2.seq;
    tl_javaapp_app_pixels(NULL, 0, 0);
    assert(!E2.on && E2.seq == sequence + 1 && !E2.lk);
    free(E2.px);
    puts("PASS: production compositor retains frames on allocation failure, tracks capacity independently, reuses buffers across 100000 frames");
    return 0;
}
