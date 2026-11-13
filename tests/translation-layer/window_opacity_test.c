/* SPDX-License-Identifier: GPL-2.0-or-later */
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "husk-tl-visual.h"

/* Compile the real NDK classification and compositor draw functions with a
 * fake driver. Buffer format alone must never make a TextureView opaque. */
typedef struct { void *view, *texture; } tl_nwindow;
static int view_format;
static bool field_error, pending;
typedef union { int i; } jvalue;
static jvalue tl_jni_get_field(void *view, const char *name, const char *sig)
{
    assert(view && !strcmp(name, "huskCompositionFormat") && !strcmp(sig, "I"));
    pending = field_error;
    return (jvalue){ .i = view_format };
}
static bool tl_jni_pending(void) { return pending; }
static void tl_jni_clear(void) { pending = false; }

typedef struct { uint32_t prog, tex, vbo; int tw, th, opaque_uniform, texel_uniform, enhance_uniform; } overlay;
static int tl_host_visual_quality(void) { return TL_VISUAL_PRO_MAX; }
static bool tl_host_display_wide_color(void) { return true; }
static struct {
    int lk;
    bool on, opaque;
    uint32_t tex;
    uint64_t seq;
    int x, y, w, h, pw, ph;
    uint8_t *px;
} E2;
static unsigned draws, uploads, begins, ends, uniform_calls, enhance_calls;
static int last_opaque = -1;
static float last_inv_x, last_inv_y, last_sharp, last_contrast, last_vibrance;
static uint32_t program;
static int64_t now_ns(void) { static int64_t now; return now += 1000; }
static void pthread_mutex_lock(int *mutex) { assert(!*mutex); *mutex = 1; }
static void pthread_mutex_unlock(int *mutex) { assert(*mutex); *mutex = 0; }
static void tl_egl_compositor_frame_begin(void) { begins++; }
static void tl_egl_compositor_frame_wait(uint32_t tex) { (void)tex; assert(!E2.lk); }
static void tl_egl_compositor_frame_end(uint32_t tex) { (void)tex; ends++; }
static void ov_upload(overlay *o, const uint8_t *px, int w, int h)
{ assert(E2.lk && px && w == 2 && h == 2); o->tw = w; o->th = h; uploads++; }
static void viewport(int32_t x, int32_t y, int32_t w, int32_t h) { (void)x; (void)y; assert(w > 0 && h > 0); }
static void use_program(uint32_t p) { program = p; }
static void one_u32(uint32_t value) { (void)value; }
static void two_u32(uint32_t target, uint32_t value) { (void)target; (void)value; }
static void attrib(uint32_t a, int32_t b, uint32_t c, uint8_t d, int32_t e, const void *f)
{ (void)a; (void)b; (void)c; (void)d; (void)e; (void)f; }
static void uniform(int32_t location, int32_t value)
{ assert(location == (program == 1 ? 9 : 10)); last_opaque = value; uniform_calls++; }
static void uniform2f(int32_t location, float x, float y)
{ assert(location == 11 && program == 1); last_inv_x = x; last_inv_y = y; }
static void uniform4f(int32_t location, float x, float y, float z, float w)
{
    assert(location == 12 && program == 1 && w == 0.0f);
    last_sharp = x; last_contrast = y; last_vibrance = z; enhance_calls++;
}
static void draw(uint32_t mode, int32_t first, int32_t count)
{ assert(mode == 5 && !first && count == 4 && last_opaque >= 0); draws++; }
static void *gl_fn(const char *name)
{
    if (!strcmp(name, "glViewport")) return viewport;
    if (!strcmp(name, "glUseProgram")) return use_program;
    if (!strcmp(name, "glUniform1i")) return uniform;
    if (!strcmp(name, "glUniform2f")) return uniform2f;
    if (!strcmp(name, "glUniform4f")) return uniform4f;
    if (!strcmp(name, "glDisable") || !strcmp(name, "glEnable") || !strcmp(name, "glActiveTexture")
        || !strcmp(name, "glEnableVertexAttribArray")) return one_u32;
    if (!strcmp(name, "glBindTexture") || !strcmp(name, "glBindBuffer") || !strcmp(name, "glBlendFunc")) return two_u32;
    if (!strcmp(name, "glVertexAttribPointer")) return attrib;
    if (!strcmp(name, "glDrawArrays")) return draw;
    assert(!"unexpected GL call"); return NULL;
}

#include "window_opacity_under_test.h"

int main(void)
{
    tl_nwindow w = { .view = (void *)(uintptr_t)1 };
    int opaque_formats[] = { -1, 2, 3, 4 };
    for (unsigned i = 0; i < sizeof(opaque_formats)/sizeof(*opaque_formats); i++) {
        view_format = opaque_formats[i]; assert(tl_nwindow_is_opaque(&w));
    }
    int alpha_formats[] = { -3, -2, 0, 1, 99 };
    for (unsigned i = 0; i < sizeof(alpha_formats)/sizeof(*alpha_formats); i++) {
        view_format = alpha_formats[i]; assert(!tl_nwindow_is_opaque(&w));
    }
    view_format = -1; w.texture = (void *)(uintptr_t)2;
    assert(!tl_nwindow_is_opaque(&w)); w.texture = NULL;
    field_error = true; assert(!tl_nwindow_is_opaque(&w) && !pending); field_error = false;
    w.view = NULL; assert(!tl_nwindow_is_opaque(&w) && !tl_nwindow_is_opaque(NULL));

    overlay native = { .prog = 1, .tex = 11, .vbo = 12, .tw = 2, .th = 2, .opaque_uniform = 9, .texel_uniform = 11, .enhance_uniform = 12 };
    overlay cpu = { .tex = 13, .opaque_uniform = -1, .texel_uniform = -1, .enhance_uniform = -1 };
    overlay ui = { .prog = 2, .tex = 21, .vbo = 22, .tw = 2, .th = 2, .opaque_uniform = 10, .texel_uniform = -1, .enhance_uniform = -1 };
    E2.on = true; E2.tex = 70; E2.pw = 1568; E2.ph = 724; E2.w = 2796; E2.h = 1290; E2.opaque = true;
    E2.seq = 42;
    uint64_t handoff_ns;
    assert(app_frame_draw(&native, &cpu, 2796, 1290, &handoff_ns) == 42 && handoff_ns == 1000);
    assert(last_opaque == 1 && draws == 1 && !uploads && enhance_calls == 1);
    assert(last_inv_x > 0.0f && last_inv_y > 0.0f && last_sharp > 0.58f && last_contrast > 0.0f && last_vibrance > 0.0f);
    ov_draw(&ui, 2796, 1290);
    assert(last_opaque == 0 && program == 2 && draws == 2 && enhance_calls == 1);
    E2.opaque = false;
    assert(app_frame_draw(&native, &cpu, 1864, 860, &handoff_ns) == 42);
    assert(last_opaque == 0 && draws == 3);
    uint8_t pixels[16] = { 0 };
    E2.tex = 0; E2.px = pixels; E2.pw = E2.ph = 2; E2.opaque = true;
    E2.seq = 43;
    assert(app_frame_draw(&native, &cpu, 1864, 860, &handoff_ns) == 43);
    assert(last_opaque == 1 && uploads == 1 && draws == 4);
    E2.on = false;
    assert(app_frame_draw(&native, &cpu, 1864, 860, &handoff_ns) == 0);
    assert(draws == 4 && uniform_calls == 4 && begins == ends && !E2.lk);
    puts("PASS: SurfaceView opacity follows its composition format, is preserved for GPU/CPU frames, and never forces Java UI or TextureView alpha");
    return 0;
}
