#include <assert.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "husk-tl-frame-sync.h"
#include "husk-tl-host-device.h"

typedef void *EGLDisplay;
typedef void *EGLSurface;
typedef int EGLBoolean;
typedef int EGLint;

#define EGL_TRUE 1
#define EGL_FALSE 0
#define EGL_WIDTH 0x3057
#define EGL_HEIGHT 0x3056
#define MAX_COMP_SURFACES 4
#define GLT_RING 4

#ifndef CLOCK_MONOTONIC
#define CLOCK_MONOTONIC 1
#endif

typedef struct {
    EGLSurface s;
    void *win;
    bool retired;
    int cur;
    unsigned tex[3];
    int tw[3], th[3];
    tl_frame_sync sync[3];
} comp_surf_entry;

static comp_surf_entry g_comp_surf[MAX_COMP_SURFACES];
static int g_frame_handoff;
static int g_tex_lock;
static EGLSurface g_published_comp_surface;
static unsigned g_published_comp_tex;
static unsigned g_compositor_pinned_tex;
static bool g_handoff_released_for_draw;
static atomic_bool g_compositor_es3 = true;
static _Atomic int64_t g_capture_pace_last_ns;
static struct {
    char frame_dir[4];
    int frame_every;
    _Atomic unsigned long presented;
} E = { "", 1, 0 };
static _Atomic unsigned long g_glt_n, g_glt_draws, g_glt_uploads;

/* Test instrumentation */
static int g_handoff_held = 0;
static int g_handoff_lock_attempts = 0;
static int g_tex_lock_held = 0;
static unsigned g_next_tex_id = 100;
static int g_delete_calls = 0;
static unsigned g_deleted_tex[16];
static int g_delete_count = 0;
static unsigned g_last_published_tex = 0;
static int g_publish_calls = 0;
static uint64_t g_publish_seq = 1;
static int g_pace_wait_calls = 0;
static int g_cpu_fallback = 0;
static void *g_current_ctx = (void *)0xC0;
static int g_screen_captured = 0;
static int g_thermal_state = 0;
static int64_t g_fake_now_ns = 1000000000ll;
static int64_t g_last_sleep_ns = 0;
static int g_sleep_calls = 0;
static int g_present_calls = 0;
static int g_event_seq = 0;
static int g_last_present_seq = 0;
static int g_last_sleep_seq = 0;
static uintptr_t g_fence_seq = 1;
static int g_wait_calls = 0;
static int g_drop_calls = 0;
static int g_flush_calls = 0;

static void test_mutex_lock(int *m)
{
    if (m == &g_frame_handoff) {
        g_handoff_lock_attempts++;
        assert(g_handoff_held == 0);
        g_handoff_held = 1;
    } else if (m == &g_tex_lock) {
        assert(g_tex_lock_held == 0);
        g_tex_lock_held = 1;
    }
}

static void test_mutex_unlock(int *m)
{
    if (m == &g_frame_handoff) {
        assert(g_handoff_held == 1);
        g_handoff_held = 0;
    } else if (m == &g_tex_lock) {
        assert(g_tex_lock_held == 1);
        g_tex_lock_held = 0;
    }
}

static int test_clock_gettime(int clk, struct timespec *ts)
{
    (void)clk;
    ts->tv_sec = (time_t)(g_fake_now_ns / 1000000000ll);
    ts->tv_nsec = (long)(g_fake_now_ns % 1000000000ll);
    return 0;
}

static int test_nanosleep(const struct timespec *req, struct timespec *rem)
{
    (void)rem;
    int64_t ns = (int64_t)req->tv_sec * 1000000000ll + (int64_t)req->tv_nsec;
    g_last_sleep_ns = ns;
    g_sleep_calls++;
    g_last_sleep_seq = ++g_event_seq;
    g_fake_now_ns += ns;
    return 0;
}

#define pthread_mutex_lock(m) test_mutex_lock(m)
#define pthread_mutex_unlock(m) test_mutex_unlock(m)
#define clock_gettime(c, ts) test_clock_gettime((c), (ts))
#define nanosleep(req, rem) test_nanosleep((req), (rem))

static int stub_host_screen_captured(void) { return g_screen_captured; }
static int stub_host_thermal_state(void) { return g_thermal_state; }
static int (*tl_host_screen_captured)(void) = stub_host_screen_captured;
static int (*tl_host_thermal_state)(void) = stub_host_thermal_state;

static void *stub_eglGetCurrentContext(void) { return g_current_ctx; }
static EGLBoolean stub_eglQuerySurface(EGLDisplay d, EGLSurface s, EGLint attr, EGLint *val)
{
    (void)d; (void)s;
    *val = (attr == EGL_WIDTH) ? 1280 : 720;
    return EGL_TRUE;
}
static EGLBoolean stub_eglSwapBuffers(EGLDisplay d, EGLSurface s)
{
    (void)d; (void)s;
    g_present_calls++;
    g_last_present_seq = ++g_event_seq;
    return EGL_TRUE;
}
static void stub_glGetIntegerv(unsigned p, int *v) { (void)p; *v = 0; }
static void stub_glBindFramebuffer(unsigned t, unsigned fb) { (void)t; (void)fb; }
static void stub_glActiveTexture(unsigned u) { (void)u; }
static void stub_glBindTexture(unsigned t, unsigned tex) { (void)t; (void)tex; }
static void stub_glGenTextures(int n, unsigned *out)
{
    for (int i = 0; i < n; i++) out[i] = g_next_tex_id++;
}
static void stub_glTexParameteri(unsigned t, unsigned p, int v) { (void)t; (void)p; (void)v; }
static void stub_glCopyTexImage2D(unsigned t, int l, unsigned fmt, int x, int y, int w, int h, int b)
{
    (void)t; (void)l; (void)fmt; (void)x; (void)y; (void)w; (void)h; (void)b;
}
static void stub_glCopyTexSubImage2D(unsigned t, int l, int xo, int yo, int x, int y, int w, int h)
{
    (void)t; (void)l; (void)xo; (void)yo; (void)x; (void)y; (void)w; (void)h;
}
static void stub_glDeleteTextures(int n, const unsigned *tex)
{
    g_delete_calls++;
    for (int i = 0; i < n; i++) {
        if (tex[i] && g_delete_count < 16) g_deleted_tex[g_delete_count++] = tex[i];
    }
}
static void stub_glFlush(void) { g_flush_calls++; }
static void stub_glFinish(void) {}

static void *(*a_eglGetCurrentContext)(void) = stub_eglGetCurrentContext;
static EGLBoolean (*a_eglQuerySurface)(EGLDisplay, EGLSurface, EGLint, EGLint *) = stub_eglQuerySurface;
static EGLBoolean (*a_eglSwapBuffers)(EGLDisplay, EGLSurface) = stub_eglSwapBuffers;
static void (*a_glGetIntegerv)(unsigned, int *) = stub_glGetIntegerv;
static void (*a_glBindFramebuffer)(unsigned, unsigned) = stub_glBindFramebuffer;
static void (*a_glActiveTexture)(unsigned) = stub_glActiveTexture;
static void (*a_glBindTexture)(unsigned, unsigned) = stub_glBindTexture;
static void (*a_glGenTextures)(int, unsigned *) = stub_glGenTextures;
static void (*a_glTexParameteri)(unsigned, unsigned, int) = stub_glTexParameteri;
static void (*a_glCopyTexImage2D)(unsigned, int, unsigned, int, int, int, int, int) = stub_glCopyTexImage2D;
static void (*a_glCopyTexSubImage2D)(unsigned, int, int, int, int, int, int, int) = stub_glCopyTexSubImage2D;
static void (*a_glDeleteTextures)(int, const unsigned *) = stub_glDeleteTextures;
static void (*a_glFlush)(void) = stub_glFlush;
static void (*a_glFinish)(void) = stub_glFinish;

static int comp_ctx_ver(void *ctx) { return ctx ? 3 : 0; }
static bool comp_ctx_can_sync(void *ctx) { return ctx != NULL; }
static void *tex_window_of(EGLSurface s) { (void)s; return NULL; }
static void tex_swap(EGLDisplay d, EGLSurface s, void *w) { (void)d; (void)s; (void)w; }
static int tl_nwindow_width(void *w) { (void)w; return 1280; }
static int tl_nwindow_height(void *w) { (void)w; return 720; }
static bool tl_nwindow_view_bounds(void *w, int *x, int *y, int *vw, int *vh)
{
    (void)w; *x = 0; *y = 0; *vw = 1280; *vh = 720; return true;
}
static bool tl_nwindow_is_opaque(void *w) { (void)w; return true; }
static void tl_log_line(const char *fmt, ...) { (void)fmt; }
static void tl_egl_recent_calls(char *b, size_t n, int k) { (void)b; (void)n; (void)k; }
static void tl_egl_gl_histogram(char *b, size_t n) { (void)b; (void)n; }
static uint64_t frame_perf_token(void) { return 1; }
static uint64_t frame_perf_now(void) { return (uint64_t)g_fake_now_ns; }
static void frame_perf_producer(uint64_t t, uint64_t a, uint64_t b, uint64_t c, uint64_t d)
{
    (void)t; (void)a; (void)b; (void)c; (void)d;
}
static void save_frame(EGLDisplay d, EGLSurface s, unsigned long n) { (void)d; (void)s; (void)n; }
static const uint8_t *read_back_rect(EGLDisplay d, EGLSurface s, int cw, int ch, EGLint *rw, EGLint *rh)
{
    (void)d; (void)s; (void)cw; (void)ch;
    static const uint8_t px[4] = { 1, 2, 3, 4 };
    *rw = 1; *rh = 1;
    return px;
}

static uint64_t stub_tex_publish_with_opacity(unsigned tex, int cw, int ch, int vx, int vy, int vw, int vh, bool opaque)
{
    (void)cw; (void)ch; (void)vx; (void)vy; (void)vw; (void)vh; (void)opaque;
    g_last_published_tex = tex;
    g_publish_calls++;
    return g_publish_seq++;
}
static uint64_t stub_tex_publish(unsigned tex, int cw, int ch, int vx, int vy, int vw, int vh)
{
    return stub_tex_publish_with_opacity(tex, cw, ch, vx, vy, vw, vh, true);
}
static void stub_frame_pace(uint64_t prev_seq)
{
    (void)prev_seq;
    assert(g_handoff_held == 0);
    g_pace_wait_calls++;
}
static bool stub_pixels_publish_with_opacity(const void *p, int w, int h, int vx, int vy, int vw, int vh, bool opaque)
{
    (void)p; (void)w; (void)h; (void)vx; (void)vy; (void)vw; (void)vh; (void)opaque;
    return true;
}

static uint64_t (*tl_javaapp_app_tex_publish_with_opacity)(unsigned, int, int, int, int, int, int, bool) = stub_tex_publish_with_opacity;
static uint64_t (*tl_javaapp_app_tex_publish)(unsigned, int, int, int, int, int, int) = stub_tex_publish;
static void (*tl_javaapp_app_frame_pace)(uint64_t) = stub_frame_pace;
static bool (*tl_javaapp_app_pixels_rect_publish_with_opacity)(const void *, int, int, int, int, int, int, bool) = stub_pixels_publish_with_opacity;
static bool (*tl_javaapp_app_pixels_rect_publish)(const void *, int, int, int, int, int, int) = NULL;
static void (*tl_javaapp_app_pixels_rect)(const void *, int, int, int, int, int, int) = NULL;
static void (*tl_javaapp_app_pixels)(const void *, int, int) = NULL;

static void *frame_fence(void) { return (void *)(++g_fence_seq); }
static void frame_wait(void *sync) { (void)sync; g_wait_calls++; }
static void frame_drop(void *sync) { (void)sync; g_drop_calls++; }
static void frame_flush(void) { a_glFlush(); }
static void frame_finish(void) { a_glFinish(); }
static const tl_frame_sync_ops k_frame_sync = { frame_fence, frame_wait, frame_drop, frame_flush, frame_finish };

#include "compositor_handoff_under_test.h"

static void reset_test_state(void)
{
    memset(g_comp_surf, 0, sizeof(g_comp_surf));
    g_frame_handoff = 0;
    g_tex_lock = 0;
    g_published_comp_surface = NULL;
    g_published_comp_tex = 0;
    g_compositor_pinned_tex = 0;
    g_handoff_released_for_draw = false;
    atomic_store(&g_compositor_es3, true);
    atomic_store(&g_capture_pace_last_ns, 0);
    g_handoff_held = 0;
    g_handoff_lock_attempts = 0;
    g_tex_lock_held = 0;
    g_next_tex_id = 100;
    g_delete_calls = 0;
    memset(g_deleted_tex, 0, sizeof(g_deleted_tex));
    g_delete_count = 0;
    g_last_published_tex = 0;
    g_publish_calls = 0;
    g_pace_wait_calls = 0;
    g_cpu_fallback = 0;
    g_current_ctx = (void *)0xC0;
    g_screen_captured = 0;
    g_thermal_state = 0;
    g_fake_now_ns = 1000000000ll;
    g_last_sleep_ns = 0;
    g_sleep_calls = 0;
    g_present_calls = 0;
    g_event_seq = 0;
    g_last_present_seq = 0;
    g_last_sleep_seq = 0;
    g_wait_calls = 0;
    g_drop_calls = 0;
    g_flush_calls = 0;
    tl_javaapp_app_tex_publish_with_opacity = stub_tex_publish_with_opacity;
    tl_javaapp_app_tex_publish = stub_tex_publish;
    tl_javaapp_app_frame_pace = stub_frame_pace;
}

int main(void)
{
    /* 1. gl_run releases g_frame_handoff during draw/nextDrawable while pinning
     * the sampled texture, and TruTien's comp_swap advances without blocking or
     * clobbering either the pinned texture or the newest published texture. */
    reset_test_state();
    EGLDisplay dpy = (EGLDisplay)0xD0;
    EGLSurface game_surf = (EGLSurface)0xA1;
    void *win = (void *)0x11;
    g_comp_surf[0].s = game_surf;
    g_comp_surf[0].win = win;

    assert(swap_buffers(dpy, game_surf) == EGL_TRUE);
    unsigned t0 = g_last_published_tex;
    assert(t0 == 100);
    assert(g_published_comp_tex == t0);
    assert(g_handoff_held == 0);
    assert(g_pace_wait_calls == 1);

    /* gl_run begins frame and waits/pins t0; handoff MUST now be unlocked! */
    tl_egl_compositor_frame_begin();
    assert(g_handoff_held == 1);
    tl_egl_compositor_frame_wait(t0);
    assert(g_handoff_held == 0);
    assert(g_handoff_released_for_draw == true);
    assert(g_compositor_pinned_tex == t0);
    assert(g_wait_calls == 1);

    /* While gl_run is stalled in glDrawArrays / CAMetalLayer nextDrawable,
     * producer swaps twice -> must pick slot 1 (101) and slot 2 (102), never t0! */
    assert(swap_buffers(dpy, game_surf) == EGL_TRUE);
    unsigned t1 = g_last_published_tex;
    assert(t1 == 101 && t1 != t0);
    assert(g_published_comp_tex == t1);

    assert(swap_buffers(dpy, game_surf) == EGL_TRUE);
    unsigned t2 = g_last_published_tex;
    assert(t2 == 102 && t2 != t0 && t2 != t1);
    assert(g_published_comp_tex == t2);

    /* Even if producer produces a 4th frame while t0 is STILL pinned, it reuses
     * slot 1 (t1=101) rather than touching pinned t0 or newest t2! */
    assert(swap_buffers(dpy, game_surf) == EGL_TRUE);
    assert(g_last_published_tex == t1);
    assert(g_comp_surf[0].tex[0] == t0);
    assert(g_comp_surf[0].tex[1] == t1);
    assert(g_comp_surf[0].tex[2] == t2);

    /* gl_run finishes drawing t0 -> re-acquires handoff, drops read sync, unpins. */
    tl_egl_compositor_frame_end(t0);
    assert(g_handoff_held == 0);
    assert(g_handoff_released_for_draw == false);
    assert(g_compositor_pinned_tex == 0);
    assert(g_comp_surf[0].sync[0].sampled != NULL);

    /* 2. Retiring a surface while one of its textures is pinned on GPU defers
     * texture deletion until tl_egl_compositor_frame_end unpins it. */
    tl_egl_compositor_frame_begin();
    tl_egl_compositor_frame_wait(t2);
    assert(g_compositor_pinned_tex == t2);
    assert(g_handoff_held == 0);

    g_comp_surf[0].retired = true;
    g_comp_surf[0].s = NULL;
    frame_clean_retired();
    assert(g_delete_calls == 0);
    assert(g_comp_surf[0].retired == true);

    tl_egl_compositor_frame_end(t2);
    assert(g_compositor_pinned_tex == 0);
    assert(g_delete_calls >= 1);
    assert(g_delete_count == 3);
    assert(g_comp_surf[0].retired == false);

    /* 3. Host compositor window (not in g_comp_surf) returns from comp_swap
     * without locking g_frame_handoff, and sleeps for capture pacing STRICTLY
     * AFTER a_eglSwapBuffers presents/releases the CAMetalDrawable. */
    reset_test_state();
    EGLSurface host_surf = (EGLSurface)0x1;
    g_screen_captured = 1;
    g_thermal_state = 1; /* .fair while captured should now target 60 Hz (~16.67ms), not 45 Hz */
    int64_t expected_60hz_ns = 1000000000ll / 60;

    assert(swap_buffers(dpy, host_surf) == EGL_TRUE);
    assert(g_handoff_lock_attempts == 0);
    assert(g_present_calls == 1);
    assert(g_sleep_calls == 0); /* first captured swap records timestamp, no prior wait */

    /* Second captured swap after only 4 ms must present FIRST, then sleep ~12.67 ms. */
    g_fake_now_ns += 4000000ll;
    assert(swap_buffers(dpy, host_surf) == EGL_TRUE);
    assert(g_handoff_lock_attempts == 0);
    assert(g_present_calls == 2);
    assert(g_sleep_calls == 1);
    assert(g_last_present_seq < g_last_sleep_seq);
    assert(g_last_sleep_ns == expected_60hz_ns - 4000000ll);

    /* .critical thermal while captured drops to 30 Hz (33.33 ms interval). */
    g_thermal_state = 3;
    g_fake_now_ns += 5000000ll;
    assert(swap_buffers(dpy, host_surf) == EGL_TRUE);
    assert(g_sleep_calls == 2);
    assert(g_last_present_seq < g_last_sleep_seq);
    assert(g_last_sleep_ns == (1000000000ll / 30) - 5000000ll);

    /* Resetting session capture timestamp avoids spurious sleep on next frame. */
    atomic_store(&g_capture_pace_last_ns, 0);
    assert(swap_buffers(dpy, host_surf) == EGL_TRUE);
    assert(g_sleep_calls == 2);

    /* When screen capture ends, no pacing sleep runs. */
    g_screen_captured = 0;
    g_fake_now_ns += 1000000ll;
    assert(swap_buffers(dpy, host_surf) == EGL_TRUE);
    assert(g_sleep_calls == 2);
    assert(g_handoff_lock_attempts == 0);

    /* 4. CPU fallback publish clears g_published_comp_tex so no slot remains reserved. */
    reset_test_state();
    g_comp_surf[0].s = game_surf;
    g_comp_surf[0].win = win;
    assert(swap_buffers(dpy, game_surf) == EGL_TRUE);
    assert(g_published_comp_tex == 100);
    tl_javaapp_app_tex_publish_with_opacity = NULL;
    tl_javaapp_app_tex_publish = NULL;
    assert(swap_buffers(dpy, game_surf) == EGL_TRUE);
    assert(g_published_comp_tex == 0);

    puts("PASS: compositor handoff releases mutex before draw/present, protects pinned/published ring slots, defers pinned retirement, and paces after swap at 60/30 Hz");
    return 0;
}
