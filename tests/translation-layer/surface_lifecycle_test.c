/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "husk-tl-frame-sync.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdatomic.h>

/* Run production swap/destroy paths against two overlapping Android surfaces.
 * Driver calls are inert here: the regression concerns which surface owns
 * the frame publication, not GL's rendering or fence implementation. */
typedef void *EGLDisplay, *EGLSurface;
typedef int EGLint;
typedef unsigned EGLBoolean;
#define EGL_WIDTH 0x3057
#define EGL_HEIGHT 0x3056
#define MAX_TEX_SURFACES 16
#define MAX_COMP_SURFACES 8
typedef struct {
    EGLSurface s;
    void *win;
    unsigned tex[3];
    int tw[3], th[3], cur;
    tl_frame_sync sync[3];
    bool retired;
} comp_surf_entry;
static comp_surf_entry g_comp_surf[MAX_COMP_SURFACES];
static struct { EGLSurface s; void *win; } g_tex_surf[MAX_TEX_SURFACES];
static int g_frame_handoff, g_tex_lock;
static EGLSurface g_published_comp_surface;
static atomic_bool g_compositor_es3;
static struct { char frame_dir[1]; int frame_every; } E;
static unsigned publications, clears, releases, cleanups, paced, driver_destroys;
static unsigned displayed;
static int context_version;
static bool fail_cpu_publication;
static bool displayed_opaque;

static void pthread_mutex_lock(int *m) { assert(!*m); *m = 1; }
static void pthread_mutex_unlock(int *m) { assert(*m); *m = 0; }
static int comp_ctx_ver(void *context) { return context ? context_version : 0; }
static void *current_context(void) { return (void *)(uintptr_t)9; }
static void *(*a_eglGetCurrentContext)(void) = current_context;
static void frame_clean_retired(void) { assert(g_frame_handoff); cleanups++; }
static void tl_nwindow_release(void *win) { assert(win); releases++; }
static int tl_nwindow_width(void *win) { assert(win); return 1568; }
static int tl_nwindow_height(void *win) { assert(win); return 724; }
static bool tl_nwindow_is_opaque(void *win) { return win == (void *)(uintptr_t)11; }
static bool tl_nwindow_view_bounds(void *win, int *x, int *y, int *w, int *h)
{
    assert(win); *x = *y = 0; *w = 1864; *h = 860; return true;
}
static EGLBoolean destroy(EGLDisplay d, EGLSurface s)
{
    (void)d; assert(s && !g_frame_handoff); driver_destroys++; return 1;
}
static EGLBoolean (*a_eglDestroySurface)(EGLDisplay, EGLSurface) = destroy;
static EGLBoolean query(EGLDisplay d, EGLSurface s, EGLint at, EGLint *value)
{
    (void)d; assert(s); *value = at == EGL_WIDTH ? 1568 : 724; return 1;
}
static EGLBoolean (*a_eglQuerySurface)(EGLDisplay, EGLSurface, EGLint, EGLint *) = query;
static uint64_t publish(unsigned tex, int pw, int ph, int x, int y, int w, int h)
{
    (void)x; (void)y; (void)w; (void)h;
    assert(g_frame_handoff && !g_tex_lock);
    if (tex) { assert(pw == 1568 && ph == 724); publications++; }
    else clears++;
    displayed = tex;
    return publications;
}
static uint64_t (*tl_javaapp_app_tex_publish)(unsigned, int, int, int, int, int, int) = publish;
static uint64_t publish_opacity(unsigned tex, int pw, int ph, int x, int y, int w, int h, bool opaque)
{
    displayed_opaque = opaque;
    return publish(tex, pw, ph, x, y, w, h);
}
static uint64_t (*tl_javaapp_app_tex_publish_with_opacity)(unsigned, int, int, int, int, int, int, bool) = publish_opacity;
static void (*tl_javaapp_app_tex)(unsigned, int, int, int, int, int, int);
static void pace(uint64_t seq) { (void)seq; assert(!g_frame_handoff); paced++; }
static void (*tl_javaapp_app_frame_pace)(uint64_t) = pace;
static void pixels_rect(const void *px, int pw, int ph, int x, int y, int w, int h)
{
    (void)x; (void)y; (void)w; (void)h;
    assert(g_frame_handoff && !g_tex_lock && pw == 1568 && ph == 724 && px);
    displayed = 1000; publications++;
}
static void (*tl_javaapp_app_pixels_rect)(const void *, int, int, int, int, int, int) = pixels_rect;
static bool pixels_rect_publish(const void *px, int pw, int ph, int x, int y, int w, int h)
{
    assert(g_frame_handoff);
    if (fail_cpu_publication) return false;
    pixels_rect(px, pw, ph, x, y, w, h);
    return true;
}
static bool (*tl_javaapp_app_pixels_rect_publish)(const void *, int, int, int, int, int, int) = pixels_rect_publish;
static bool pixels_opacity(const void *px, int pw, int ph, int x, int y, int w, int h, bool opaque)
{
    if (!pixels_rect_publish(px, pw, ph, x, y, w, h)) return false;
    displayed_opaque = opaque;
    return true;
}
static bool (*tl_javaapp_app_pixels_rect_publish_with_opacity)(const void *, int, int, int, int, int, int, bool);
static void pixels(const void *px, int w, int h)
{
    assert(g_frame_handoff);
    if (px) { assert(w == 1568 && h == 724); displayed = 1000; publications++; }
    else { displayed = 0; clears++; }
}
static void (*tl_javaapp_app_pixels)(const void *, int, int) = pixels;
static const uint8_t *read_back_rect(EGLDisplay d, EGLSurface s, int w, int h, EGLint *pw, EGLint *ph)
{
    static const uint8_t pixel;
    (void)d; assert(s && g_frame_handoff); *pw = w; *ph = h; return &pixel;
}
static void save_frame(EGLDisplay d, EGLSurface s, unsigned long n) { (void)d; (void)s; (void)n; }
static void tl_log_line(const char *fmt, ...) { (void)fmt; }
static void finish(void) { assert(g_frame_handoff); }
static const tl_frame_sync_ops k_frame_sync = { NULL, NULL, NULL, NULL, finish };
static void *(*a_glFenceSync)(unsigned, unsigned);
static void (*a_glWaitSync)(void *, unsigned, uint64_t);
static void (*a_glDeleteSync)(void *);
static void (*a_glFlush)(void);
static void (*a_glFinish)(void) = finish;
static void get_int(unsigned at, int *value) { *value = at == 0x84E0 ? 0x84C0 : 0; }
static void (*a_glGetIntegerv)(unsigned, int *) = get_int;
static void bind(unsigned target, unsigned value) { (void)target; (void)value; }
static void (*a_glBindFramebuffer)(unsigned, unsigned) = bind;
static void (*a_glBindTexture)(unsigned, unsigned) = bind;
static void active(unsigned unit) { (void)unit; }
static void (*a_glActiveTexture)(unsigned) = active;
static void gen(int count, unsigned *tex) { static unsigned next; assert(count == 1); *tex = ++next; }
static void (*a_glGenTextures)(int, unsigned *) = gen;
static void parameter(unsigned t, unsigned p, int value) { (void)t; (void)p; (void)value; }
static void (*a_glTexParameteri)(unsigned, unsigned, int) = parameter;
static void image(unsigned t, int l, unsigned f, int x, int y, int w, int h, int b)
{ (void)t; (void)l; (void)f; (void)x; (void)y; (void)w; (void)h; (void)b; }
static void (*a_glCopyTexImage2D)(unsigned, int, unsigned, int, int, int, int, int) = image;
static void subimage(unsigned t, int l, int xo, int yo, int x, int y, int w, int h)
{ (void)t; (void)l; (void)xo; (void)yo; (void)x; (void)y; (void)w; (void)h; }
static void (*a_glCopyTexSubImage2D)(unsigned, int, int, int, int, int, int, int) = subimage;

#include "surface_lifecycle_under_test.h"

static void scenario(bool gpu, int cpu_mode)
{
    memset(g_comp_surf, 0, sizeof(g_comp_surf));
    g_published_comp_surface = NULL;
    publications = clears = releases = cleanups = paced = driver_destroys = displayed = 0;
    context_version = gpu ? 3 : 0;
    tl_javaapp_app_pixels_rect_publish_with_opacity = cpu_mode == 3 ? pixels_opacity : NULL;
    tl_javaapp_app_pixels_rect_publish = cpu_mode == 2 ? pixels_rect_publish : NULL;
    tl_javaapp_app_pixels_rect = cpu_mode == 1 ? pixels_rect : NULL;
    EGLSurface first = (void *)(uintptr_t)1, replacement = (void *)(uintptr_t)2;
    g_comp_surf[0].s = first; g_comp_surf[0].win = (void *)(uintptr_t)11;
    g_comp_surf[1].s = replacement; g_comp_surf[1].win = (void *)(uintptr_t)12;
    assert(comp_swap(NULL, first, 10) && displayed);
    if (gpu || cpu_mode == 3) assert(displayed_opaque);
    assert(g_published_comp_surface == first);
    assert(comp_swap(NULL, replacement, 11) && displayed);
    if (gpu || cpu_mode == 3) assert(!displayed_opaque);
    unsigned replacement_frame = displayed;
    assert(g_published_comp_surface == replacement && publications == 2);
    /* Android may tear down the old SurfaceView after replacement is drawn. */
    assert(w_eglDestroySurface(NULL, first));
    assert(displayed == replacement_frame && !clears && releases == 1);
    assert(g_published_comp_surface == replacement && g_comp_surf[0].retired);
    assert(comp_swap(NULL, replacement, 12) && publications == 3);
    assert(w_eglDestroySurface(NULL, replacement));
    assert(!displayed && clears == 1 && !g_published_comp_surface);
    assert(releases == 2 && driver_destroys == 2);
    assert(!g_frame_handoff && !g_tex_lock && paced == (gpu ? 3u : 0u));
    /* Unpublished surfaces must never clear another frame. */
    g_comp_surf[2].s = first; g_comp_surf[2].win = (void *)(uintptr_t)11;
    displayed = 99;
    assert(w_eglDestroySurface(NULL, first) && displayed == 99 && clears == 1);
}
static void failed_cpu_replacement(void)
{
    memset(g_comp_surf, 0, sizeof(g_comp_surf));
    g_published_comp_surface = NULL;
    publications = clears = releases = cleanups = paced = driver_destroys = displayed = 0;
    EGLSurface first = (void *)(uintptr_t)1, replacement = (void *)(uintptr_t)2;
    g_comp_surf[0].s = first; g_comp_surf[0].win = (void *)(uintptr_t)11;
    g_comp_surf[1].s = replacement; g_comp_surf[1].win = (void *)(uintptr_t)12;
    context_version = 3;
    assert(comp_swap(NULL, first, 10));
    unsigned retained = displayed;
    context_version = 0;
    tl_javaapp_app_pixels_rect_publish_with_opacity = pixels_opacity;
    tl_javaapp_app_pixels_rect_publish = pixels_rect_publish;
    fail_cpu_publication = true;
    assert(comp_swap(NULL, replacement, 11));
    fail_cpu_publication = false;
    /* A failed allocation keeps the old GPU frame and its old owner. */
    assert(publications == 1 && displayed == retained && g_published_comp_surface == first);
    assert(displayed_opaque);
    assert(w_eglDestroySurface(NULL, replacement) && displayed == retained && !clears);
    assert(w_eglDestroySurface(NULL, first) && !displayed && clears == 1 && !g_published_comp_surface);
}
int main(void)
{
    scenario(true, 2);
    scenario(false, 3);
    scenario(false, 2);
    scenario(false, 1);
    scenario(false, 0);
    failed_cpu_replacement();
    puts("PASS: GPU/CPU frames survive old-surface teardown; failed CPU allocation retains the old owner; current-surface destruction clears once without pacing under handoff");
    return 0;
}
