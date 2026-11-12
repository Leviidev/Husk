/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "husk-tl-gl-capabilities.h"
#include "husk-tl-frame-sync.h"
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>

/* Exercise the production context registry, observer and make-current
 * wrapper. No GPU or ES-major queries that could alter guest GL errors. */
typedef void *EGLDisplay, *EGLSurface, *EGLContext;
typedef int EGLint;
typedef unsigned EGLBoolean;
#define EGL_FALSE 0u
static int g_tex_lock;
static atomic_bool g_compositor_es3;
static EGLContext current;
static bool bind_ok = true;
static const char *raw_version;
static unsigned raw_queries, log_lines, finishes, fences, flushes, drops;
static struct { bool ready; } E = { true };
static void pthread_mutex_lock(int *m) { assert(!*m); *m = 1; }
static void pthread_mutex_unlock(int *m) { assert(*m); *m = 0; }
static uintptr_t pthread_self(void) { return 1; }
static int pthread_getname_np(uintptr_t thread, char *name, size_t size)
{ (void)thread; if (size) name[0] = 0; return 0; }
static const char *tl_ld_symbol_at(const void *address, const char **lib, const void **base)
{ (void)address; *lib = NULL; *base = NULL; return NULL; }
static void tl_log_line(const char *fmt, ...) { (void)fmt; assert(!g_tex_lock); log_lines++; }
static EGLBoolean bind_context(EGLDisplay display, EGLSurface draw, EGLSurface read, EGLContext context)
{ (void)display; (void)draw; (void)read; if (bind_ok) current = context; return bind_ok; }
static EGLContext get_current(void) { return current; }
static EGLint get_error(void) { return 0x3001; }
static const unsigned char *get_string(unsigned token)
{ assert(current && token == 0x1f02 && !g_tex_lock); raw_queries++; return (const unsigned char *)raw_version; }
static void *fence(unsigned condition, unsigned flags)
{ assert(condition == 0x9117 && !flags); fences++; return (void *)(uintptr_t)0x1234; }
static void wait_sync(void *sync, unsigned flags, uint64_t timeout) { (void)sync; (void)flags; (void)timeout; }
static void drop(void *sync) { assert(sync); drops++; }
static void flush(void) { flushes++; }
static void finish(void) { finishes++; }
static EGLBoolean (*a_eglMakeCurrent)(EGLDisplay, EGLSurface, EGLSurface, EGLContext) = bind_context;
static EGLContext (*a_eglGetCurrentContext)(void) = get_current;
static EGLint (*a_eglGetError)(void) = get_error;
static const unsigned char *(*a_glGetString)(unsigned) = get_string;
static void *(*a_glFenceSync)(unsigned, unsigned) = fence;
static void (*a_glWaitSync)(void *, unsigned, uint64_t) = wait_sync;
static void (*a_glDeleteSync)(void *) = drop;
static void (*a_glFlush)(void) = flush;

#include "egl_context_caps_under_test.h"

static void *make_fence(void) { return a_glFenceSync(0x9117, 0); }
static void frame_wait(void *sync) { a_glWaitSync(sync, 0, UINT64_MAX); }
static const tl_frame_sync_ops ops = { make_fence, frame_wait, drop, flush, finish };
static EGLContext ctx(unsigned n) { return (void *)(uintptr_t)n; }

int main(void)
{
    assert(tl_gl_es_version_major("OpenGL ES 3.0 (ANGLE 2.1)") == 3);
    assert(tl_gl_es_version_major("OpenGL ES 2.0") == 2);
    assert(tl_gl_es_version_major("OpenGL ES-CM 1.1") == 1);
    assert(tl_gl_es_version_major(NULL) == 0 && tl_gl_es_version_major("") == 0);
    assert(tl_gl_es_version_major("OpenGL ES") == 0 && tl_gl_es_version_major("OpenGL ES ") == 0);
    assert(tl_gl_es_version_major("OpenGL ES 3") == 0 && tl_gl_es_version_major("4.1 desktop") == 0);
    assert(tl_gl_es_version_major("OpenGL ES compatibility fake") == 0);

    comp_ctx_add(ctx(1), 2);
    raw_version = "OpenGL ES 3.0 (ANGLE 2.1.28778 git hash: e3fdc27d77e3)";
    assert(w_eglMakeCurrent(NULL, NULL, NULL, ctx(1)));
    assert(g_comp_ctx[0].ver == 2 && comp_ctx_ver(ctx(1)) == 3 && comp_ctx_can_sync(ctx(1)));
    assert(raw_queries == 1 && log_lines == 1);
    raw_version = "OpenGL ES 2.0";
    assert(w_eglMakeCurrent(NULL, NULL, NULL, ctx(1)));
    assert(raw_queries == 1 && comp_ctx_can_sync(ctx(1))); /* Observe once per context lifetime. */

    comp_ctx_add(ctx(2), 2);
    assert(w_eglMakeCurrent(NULL, NULL, NULL, ctx(2)));
    assert(comp_ctx_ver(ctx(2)) == 2 && !comp_ctx_can_sync(ctx(2)));
    unsigned before = raw_queries;
    comp_ctx_observe_current(ctx(1)); /* Foreign/non-current context must not borrow this one's version. */
    assert(raw_queries == before);
    assert(w_eglMakeCurrent(NULL, NULL, NULL, ctx(99)) && raw_queries == before); /* Unregistered context. */

    comp_ctx_add(ctx(3), 3);
    raw_version = NULL;
    assert(w_eglMakeCurrent(NULL, NULL, NULL, ctx(3)) && !comp_ctx_can_sync(ctx(3)));
    assert(comp_ctx_ver(ctx(3)) == 3); /* Retain requested configuration; unknown sync remains conservative. */
    comp_ctx_add(ctx(4), 2);
    before = raw_queries; bind_ok = false; raw_version = "OpenGL ES 3.0";
    assert(!w_eglMakeCurrent(NULL, NULL, NULL, ctx(4)) && raw_queries == before && !comp_ctx_can_sync(ctx(4)));
    bind_ok = true;
    assert(w_eglMakeCurrent(NULL, NULL, NULL, ctx(4)) && comp_ctx_can_sync(ctx(4)));
    a_glWaitSync = NULL; assert(!comp_ctx_can_sync(ctx(4))); a_glWaitSync = wait_sync;

    /* The UI registers after its initial current binding. Both real current
     * contexts need verified capabilities before the existing async path. */
    tl_egl_compositor_context(ctx(4), false);
    assert(atomic_load(&g_compositor_es3) && comp_ctx_can_sync(ctx(4)));
    tl_frame_sync frame = {0};
    tl_frame_sync_after_write(&frame, &ops, comp_ctx_can_sync(ctx(1)) && atomic_load(&g_compositor_es3));
    assert(frame.asynchronous && fences == 1 && flushes == 1 && finishes == 0);
    tl_frame_sync_reset(&frame, &ops); assert(drops == 1);
    tl_frame_sync_after_write(&frame, &ops, comp_ctx_can_sync(ctx(2)) && atomic_load(&g_compositor_es3));
    assert(!frame.asynchronous && finishes == 1);

    assert(w_eglMakeCurrent(NULL, NULL, NULL, NULL));
    before = raw_queries;
    tl_egl_compositor_context(ctx(3), true); /* No current UI: never infer capability from requested3. */
    assert(!atomic_load(&g_compositor_es3) && raw_queries == before);
    comp_ctx_remove(ctx(1)); assert(!comp_ctx_can_sync(ctx(1)) && !comp_ctx_ver(ctx(1)));
    comp_ctx_add(ctx(1), 2); raw_version = "OpenGL ES 2.0";
    assert(w_eglMakeCurrent(NULL, NULL, NULL, ctx(1)));
    assert(comp_ctx_ver(ctx(1)) == 2 && !comp_ctx_can_sync(ctx(1))); /* Reused/lost context has no stale ES3 fact. */
    puts("PASS: production current-context observation uses raw actual ES capabilities, enables requested2/actual3 GPU fences, preserves true2/unknown/missing-function fallback and resets across failed bindings and handle reuse");
    return 0;
}
