/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Real NDK/JNI/EGL adapter code, deterministic mock EGL driver. No APK or GUI. */
#include "husk-tl-jni.h"
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <unistd.h>

/* Include this translation unit to inject only the driver's entry points and
 * pause a readback deterministically while another thread destroys its surface. */
#include "../../src/translation-layer-next/husk-tl-egl.c"

void *tl_nwindow_get(void);
void tl_nwindow_configure(int, int, void *);
static jobj *size_array, *expected_texture;
static atomic_int posts, live_surfaces, failed_creates, failed_destroys;
static pthread_mutex_t swap_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t swap_condition = PTHREAD_COND_INITIALIZER;
static bool block_readback, entered_readback, continue_readback;

void tl_log_line(const char *format, ...)
{
    (void)format; /* Thousands of valid create/release cycles are deliberately quiet. */
}
static void size_method(tl_jcall *call) { call->ret.l = size_array; }
static void post_method(tl_jcall *call)
{
    assert(call->self == expected_texture);
    assert(call->args[0].j && call->args[1].i == 2 && call->args[2].i == 3);
    atomic_fetch_add(&posts, 1);
}
static const tl_jhle fixture_methods[] = {
    {"husk/test/WindowTexture", "huskSize", "()[I", size_method},
    {"husk/test/WindowTexture", "huskPostRgba", "(JIIZ)V", post_method}, {NULL}
};
static void *ndk(const char *name)
{
    for (const tl_bionic_entry *entry = tl_tab_ndk; entry->name; entry++)
        if (!strcmp(entry->name, name)) return entry->addr;
    assert(!"missing NDK function"); return NULL;
}
static EGLSurface driver_create(EGLDisplay d, EGLConfig config, const EGLint *attributes)
{
    (void)d; (void)config; (void)attributes;
    if (atomic_exchange(&failed_creates, 0)) return NULL;
    void *surface = malloc(1); assert(surface);
    atomic_fetch_add(&live_surfaces, 1);
    return surface;
}
static EGLBoolean driver_destroy(EGLDisplay d, EGLSurface surface)
{
    (void)d;
    if (atomic_exchange(&failed_destroys, 0)) return EGL_FALSE;
    if (!surface) return EGL_FALSE;
    free(surface);
    atomic_fetch_sub(&live_surfaces, 1);
    return EGL_TRUE;
}
static EGLBoolean driver_query(EGLDisplay d, EGLSurface surface, EGLint key, EGLint *value)
{
    (void)d; assert(surface);
    *value = key == EGL_WIDTH ? 2 : 3;
    return EGL_TRUE;
}
static EGLint driver_error(void) { return 0x3000; }
static void gl_integer(unsigned key, int *value) { (void)key; *value = 0; }
static void gl_binding(unsigned key, unsigned value) { (void)key; (void)value; }
static void gl_pack(unsigned key, int value) { (void)key; (void)value; }
static void gl_read(int x, int y, int width, int height, unsigned format, unsigned type, void *pixels)
{
    (void)x; (void)y; (void)format; (void)type;
    pthread_mutex_lock(&swap_lock);
    if (block_readback) {
        entered_readback = true;
        pthread_cond_broadcast(&swap_condition);
        while (!continue_readback) pthread_cond_wait(&swap_condition, &swap_lock);
    }
    pthread_mutex_unlock(&swap_lock);
    memset(pixels, 0x55, (size_t)width * height * 4);
}
static void *swap_worker(void *surface)
{
    assert(w_eglSwapBuffers((void *)2, surface));
    return NULL;
}

int main(void)
{
    tl_jni_init();
    tl_jni_register_hle(fixture_methods);
    size_array = tl_jni_new_prim_array('I', 2);
    ((int32_t *)size_array->arr.data)[0] = 2;
    ((int32_t *)size_array->arr.data)[1] = 3;
    jobj *texture = tl_jni_new_object(tl_jni_class("husk/test/WindowTexture"));
    jobj *surface = tl_jni_new_object(tl_jni_class("husk/test/WindowSurface"));
    expected_texture = texture;
    tl_jni_set_field(surface, "huskTexture", "Landroid/graphics/SurfaceTexture;", (jvalue){.l = texture});
    void *(*from_surface)(void *, void *) = ndk("ANativeWindow_fromSurface");
    void (*acquire)(void *) = ndk("ANativeWindow_acquire");
    void (*release)(void *) = ndk("ANativeWindow_release");
    int (*lock)(void *, void *, void *) = ndk("ANativeWindow_lock");
    int (*unlock_post)(void *) = ndk("ANativeWindow_unlockAndPost");

    tl_nwindow_configure(720, 1280, NULL);
    for (int n = 0; n < 10000; n++) {
        void *window = from_surface(NULL, surface); assert(window);
        assert(texture->refs == 2 && size_array->refs == 1);
        acquire(window); acquire(window);
        release(window); release(window);
        assert(texture->refs == 2);
        struct { int32_t width, height, stride, format; void *bits; uint32_t reserved[6]; } buffer;
        assert(!lock(window, &buffer, NULL));
        assert(buffer.width == 2 && buffer.height == 3 && buffer.bits);
        assert(!unlock_post(window));
        release(window);
        assert(texture->refs == 1 && size_array->refs == 1);
        void *main_window = from_surface(NULL, NULL);
        assert(main_window == tl_nwindow_get());
        release(main_window); release(main_window);
        main_window = tl_nwindow_get(); acquire(main_window); release(main_window); release(main_window);
        assert(tl_nwindow_width(main_window) == 720); /* Static base remains alive. */
    }
    assert(atomic_load(&posts) == 10000);
    puts("PASS: 10000 NDK acquire/release + software buffer cycles return texture/size JNI refs to baseline; static host window survives");

    a_eglCreatePbufferSurface = driver_create;
    a_eglDestroySurface = driver_destroy;
    a_eglQuerySurface = driver_query;
    a_eglGetError = driver_error;
    a_glGetIntegerv = gl_integer;
    a_glBindFramebuffer = gl_binding;
    a_glPixelStorei = gl_pack;
    a_glReadPixels = gl_read;
    E.ready = true;
    EGLDisplay display = (void *)2;
    void *window = from_surface(NULL, surface);
    atomic_store(&failed_creates, 1);
    assert(!w_eglCreateWindowSurface(display, NULL, window, NULL));
    assert(!atomic_load(&live_surfaces) && texture->refs == 2);
    EGLSurface surfaces[MAX_TEX_SURFACES];
    for (int n = 0; n < MAX_TEX_SURFACES; n++) {
        surfaces[n] = w_eglCreateWindowSurface(display, NULL, window, NULL);
        assert(surfaces[n]);
    }
    assert(!w_eglCreateWindowSurface(display, NULL, window, NULL));
    assert(w_eglGetError() == 0x3003 && w_eglGetError() == 0x3000);
    assert(atomic_load(&live_surfaces) == MAX_TEX_SURFACES); /* Overflow pbuffer was destroyed. */
    release(window); /* Legal: EGL independently retains it after this call. */
    assert(texture->refs == 2);
    assert(w_eglSwapBuffers(display, surfaces[0]));
    atomic_store(&failed_destroys, 1);
    assert(!w_eglDestroySurface(display, surfaces[0]));
    assert(texture->refs == 2 && atomic_load(&live_surfaces) == MAX_TEX_SURFACES);
    assert(w_eglSwapBuffers(display, surfaces[0])); /* Failed destroy retained registration. */
    for (int n = 0; n < MAX_TEX_SURFACES; n++) assert(w_eglDestroySurface(display, surfaces[n]));
    assert(texture->refs == 1 && !atomic_load(&live_surfaces));
    puts("PASS: caller release after EGL create can still swap; create failure/table overflow/destroy failure preserve exact ownership; final destroy releases texture");

    window = from_surface(NULL, surface);
    EGLSurface concurrent = w_eglCreateWindowSurface(display, NULL, window, NULL); assert(concurrent);
    release(window);
    block_readback = true;
    pthread_t thread; assert(!pthread_create(&thread, NULL, swap_worker, concurrent));
    pthread_mutex_lock(&swap_lock);
    while (!entered_readback) pthread_cond_wait(&swap_condition, &swap_lock);
    pthread_mutex_unlock(&swap_lock);
    assert(w_eglDestroySurface(display, concurrent));
    assert(texture->refs == 2); /* The in-flight lookup, independently of the table. */
    pthread_mutex_lock(&swap_lock);
    continue_readback = true;
    pthread_cond_broadcast(&swap_condition);
    pthread_mutex_unlock(&swap_lock);
    assert(!pthread_join(thread, NULL));
    assert(texture->refs == 1 && !atomic_load(&live_surfaces));
    assert(atomic_load(&posts) == 10003);
    tl_jni_unref(surface); tl_jni_unref(texture); tl_jni_unref(size_array);
    puts("PASS: concurrent EGL swap lookup retains window through successful destroy, then releases its last native/JNI reference");
    return 0;
}
