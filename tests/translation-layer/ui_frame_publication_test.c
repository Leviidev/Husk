/* SPDX-License-Identifier: GPL-2.0-or-later */
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TL_SESSION_IMPLEMENTATION
#include "husk-tl-session.h"

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

/* Include the production publisher and borrowed-frame consumer below. The
 * lock stand-in checks ownership on every exit, and can deterministically
 * publish just before a consumer acquires the lock to reproduce the old
 * fresh-before-lock race without a timing-dependent threaded test. */
/* POSIX stdlib headers may already declare pthread_mutex_t. Rename the
 * production spelling only in this extraction test instead of redeclaring it. */
typedef int test_mutex;
#define pthread_mutex_t test_mutex
#define pthread_mutex_lock test_mutex_lock
#define pthread_mutex_unlock test_mutex_unlock
#define PTHREAD_MUTEX_INITIALIZER 0
static void (*before_lock)(void);
static void pthread_mutex_lock(pthread_mutex_t *mutex)
{
    if (before_lock) {
        void (*callback)(void) = before_lock;
        before_lock = NULL;
        callback();
    }
    assert(!*mutex);
    *mutex = 1;
}
static void pthread_mutex_unlock(pthread_mutex_t *mutex)
{
    assert(*mutex);
    *mutex = 0;
}

typedef struct { int w, h; size_t stride; uint8_t *px; } gbitmap;
static gbitmap *bitmap;
#define H(type, index) ((type *)bitmap)
#define NAT(name) static bool name(void *self, void *ret)
#include "ui_frame_publication_under_test.h"
#undef NAT
#undef H

static unsigned wakes;
void tl_javaapp_wake_gl(void)
{
    /* A wake can immediately let the GL thread attempt its frame borrow. */
    assert(!S.lock);
    wakes++;
}

static void present(gbitmap *next)
{
    bitmap = next;
    assert(G_present(NULL, NULL));
    assert(!S.lock);
}

static uint8_t replacement_pixels[16] = { 0x55, 0x66, 0x77, 0xff };
static gbitmap replacement = { 2, 2, 8, replacement_pixels };
static void replace_before_consumer_lock(void) { present(&replacement); }

static void consume(bool visible, int width, int height, const uint8_t *expected)
{
    const uint8_t *pixels = NULL;
    int w = 0, h = 0;
    bool any = !visible;
    assert(tl_ui_frame_take(&pixels, &w, &h, &any));
    assert(S.lock && any == visible && w == width && h == height);
    if (visible) assert(!memcmp(pixels, expected, (size_t)w * h * 4));
    tl_ui_frame_done();
    assert(!S.lock && tl_ui_has_frame() == visible && !tl_ui_frame_pending());
    assert(!tl_ui_frame_take(&pixels, &w, &h, &any) && !S.lock);
}

int main(void)
{
    uint8_t transparent_pixels[16] = { 0 };
    uint8_t transparent_rgb_pixels[16] = { 255, 255, 255, 0, 127, 63, 31, 0 };
    uint8_t small_pixels[16] = { 1, 2, 3, 0xff };
    uint8_t large_pixels[64] = { 4, 5, 6, 0xff };
    gbitmap transparent = { 2, 2, 8, transparent_pixels };
    gbitmap transparent_rgb = { 2, 2, 8, transparent_rgb_pixels };
    gbitmap small = { 2, 2, 8, small_pixels };
    gbitmap large = { 4, 4, 16, large_pixels };

    /* Empty SurfaceView traversals and explicit null have identical semantics.
     * They cannot force host swaps while a native game waits for its next frame. */
    for (int i = 0; i < 60; i++) {
        present(&transparent);
        present(&transparent_rgb);
        present(NULL);
    }
    assert(!wakes && !tl_ui_frame_pending() && !tl_ui_has_frame() && !allocations);

    present(&small);
    assert(wakes == 1 && tl_ui_frame_pending() && !tl_ui_has_frame());
    consume(true, 2, 2, small_pixels);
    present(NULL);
    assert(wakes == 2 && tl_ui_frame_pending() && tl_ui_has_frame());
    for (int i = 0; i < 60; i++) present(&transparent_rgb);
    assert(wakes == 2 && tl_ui_has_frame());
    consume(false, 2, 2, NULL);
    present(NULL);
    assert(wakes == 2 && !tl_ui_frame_pending());

    /* Do not deduplicate visible views: every animation/UI update still wins,
     * and a consumer sees only the newest complete dimensions/pixel snapshot. */
    present(&small);
    present(&small);
    assert(wakes == 4 && !tl_ui_has_frame());
    consume(true, 2, 2, small_pixels);
    present(&small);
    before_lock = replace_before_consumer_lock;
    consume(true, 2, 2, replacement_pixels);
    assert(wakes == 6 && !before_lock);

    /* A failed resize retains the previous frame rather than clearing or
     * publishing dimensions that exceed the old allocation. */
    uint8_t *previous = S.px;
    fail_allocation = true;
    present(&large);
    assert(wakes == 6 && S.px == previous && S.w == 2 && S.h == 2);
    assert(!tl_ui_frame_pending() && tl_ui_has_frame());
    assert(!memcmp(S.px, replacement_pixels, sizeof(replacement_pixels)));
    fail_allocation = false;
    present(&large);
    consume(true, 4, 4, large_pixels);
    unsigned before = allocations;
    present(&small);
    consume(true, 2, 2, small_pixels);
    present(&large);
    consume(true, 4, 4, large_pixels);
    assert(allocations == before);

    /* A clear replacing an unconsumed visible publication must survive; a
     * later visible frame replacing a queued clear must also remain visible. */
    present(&small);
    present(NULL);
    assert(tl_ui_has_frame());
    consume(false, 2, 2, NULL);
    present(&small);
    consume(true, 2, 2, small_pixels);
    present(NULL);
    present(&large);
    consume(true, 4, 4, large_pixels);

    /* Exercise the actual startup gate as well as the reset implementation:
     * live/reserved old workers must keep their frame until the session drains. */
    assert(javaapp_begin_session());
    assert(!S.px && !S.capacity && !S.w && !S.h && !S.pending_any);
    assert(!tl_ui_has_frame() && !tl_ui_frame_pending() && !S.lock);
    present(&small);
    consume(true, 2, 2, small_pixels);
    present(&large);
    uint8_t *closing_pixels = S.px;
    unsigned before_reset_wakes = wakes;
    bool tracked;
    assert(tl_guest_thread_reserve(&tracked) && tracked);
    assert(!javaapp_begin_session());
    assert(S.px == closing_pixels && S.w == 4 && S.h == 4);
    assert(tl_ui_has_frame() && tl_ui_frame_pending() && !S.lock);
    tl_guest_session_close();
    assert(!tl_guest_session_finish() && !javaapp_begin_session());
    assert(S.px == closing_pixels && tl_ui_has_frame() && tl_ui_frame_pending());
    tl_guest_thread_release((void *)1);
    assert(tl_guest_session_finish() && javaapp_begin_session());
    assert(!S.px && !S.capacity && !S.w && !S.h && !S.pending_any);
    assert(!tl_ui_has_frame() && !tl_ui_frame_pending() && !S.lock);
    assert(wakes == before_reset_wakes);
    const uint8_t *no_pixels;
    int no_width, no_height;
    bool no_visible;
    assert(!tl_ui_frame_take(&no_pixels, &no_width, &no_height, &no_visible));
    present(&transparent_rgb);
    assert(!tl_ui_frame_pending() && wakes == before_reset_wakes);
    present(&small);
    consume(true, 2, 2, small_pixels);
    tl_guest_session_close();
    assert(tl_guest_session_finish() && javaapp_begin_session());
    assert(!S.px && !tl_ui_has_frame() && !tl_ui_frame_pending());
    tl_guest_session_close();
    assert(tl_guest_session_finish());
    puts("PASS: production UI frames publish coherent snapshots, clear once, suppress empty wakes, retain frames on resize failure and reset only after the old session drains");
    return 0;
}
