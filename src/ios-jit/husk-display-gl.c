/*
 * Husk: the GL display path.
 *
 * The software path (husk-display.c) takes a CPU framebuffer from QEMU and
 * copies it out. That is why Android ran at 2 frames per second: with no GPU
 * behind virtio-gpu, every pixel was rasterised in software by a CPU that was
 * itself being emulated, so the cost of drawing was paid twice.
 *
 * This path removes the inner cost. virtio-gpu-gl hands virglrenderer the
 * guest's GL commands, virglrenderer executes them against a real context --
 * ANGLE on top of Metal -- and what arrives here is a texture the phone's GPU
 * has already drawn. All that is left is to blit it to the screen.
 *
 * QEMU supplies most of the machinery once CONFIG_OPENGL is on: egl-context.c
 * implements the three context callbacks verbatim, shader.c does the blit, and
 * qemu_egl_init_dpy_cocoa() brings up EGL through ANGLE. This file is the
 * wiring, not the engine.
 */
#include "qemu/osdep.h"
#include "qemu/error-report.h"
#include "ui/console.h"
#include "ui/egl-helpers.h"
#include "ui/egl-context.h"
#include "ui/shader.h"
#include "system/system.h"

#include <dlfcn.h>

#include "husk-display-gl.h"

static DisplayGLCtx husk_gl_ctx;
static QemuGLShader *husk_gls;
static EGLSurface    husk_surface = EGL_NO_SURFACE;
static EGLContext    husk_context = EGL_NO_CONTEXT;

static egl_fb husk_guest_fb;   /* what the guest drew */
static egl_fb husk_window_fb;  /* the layer we present to */

static int  husk_win_w, husk_win_h;
static bool husk_have_scanout;
static bool husk_flip;
static uint64_t husk_gl_frames;

static const DisplayGLCtxOps husk_gl_ctx_ops = {
    .dpy_gl_ctx_create        = qemu_egl_create_context,
    .dpy_gl_ctx_destroy       = qemu_egl_destroy_context,
    .dpy_gl_ctx_make_current  = qemu_egl_make_context_current,
};

static void husk_gl_scanout_disable(DisplayChangeListener *dcl)
{
    husk_have_scanout = false;
    egl_fb_destroy(&husk_guest_fb);
}

static void husk_gl_scanout_texture(DisplayChangeListener *dcl,
                                    uint32_t backing_id,
                                    bool backing_y_0_top,
                                    uint32_t backing_width,
                                    uint32_t backing_height,
                                    uint32_t x, uint32_t y,
                                    uint32_t w, uint32_t h,
                                    ScanoutTextureNative native)
{
    /*
     * backing_y_0_top says the guest's origin is top-left while GL's is
     * bottom-left, so it decides whether the blit flips. Getting it wrong
     * renders the whole of Android upside down, which is a confusing way to
     * discover a one-line mistake.
     */
    husk_flip = !backing_y_0_top;
    egl_fb_setup_for_tex(&husk_guest_fb, backing_width, backing_height,
                         backing_id, false);
    husk_have_scanout = true;
}

static void husk_gl_update(DisplayChangeListener *dcl,
                           uint32_t x, uint32_t y, uint32_t w, uint32_t h)
{
    if (!husk_have_scanout || husk_surface == EGL_NO_SURFACE) {
        return;
    }

    eglMakeCurrent(qemu_egl_display, husk_surface, husk_surface, husk_context);
    egl_fb_setup_default(&husk_window_fb, husk_win_w, husk_win_h);
    egl_texture_blit(husk_gls, &husk_window_fb, &husk_guest_fb, husk_flip);
    eglSwapBuffers(qemu_egl_display, husk_surface);
    husk_gl_frames++;
}

static void husk_gl_refresh(DisplayChangeListener *dcl)
{
    graphic_hw_update(dcl->con);
}

static const DisplayChangeListenerOps husk_gl_dcl_ops = {
    .dpy_name                = "husk-gl",
    .dpy_refresh             = husk_gl_refresh,
    .dpy_gl_scanout_disable  = husk_gl_scanout_disable,
    .dpy_gl_scanout_texture  = husk_gl_scanout_texture,
    .dpy_gl_update           = husk_gl_update,
};

static DisplayChangeListener husk_gl_dcl = {
    .ops = &husk_gl_dcl_ops,
};

uint64_t husk_display_gl_frames(void)
{
    return husk_gl_frames;
}

/*
 * Split in two because of when QEMU needs the answer.
 *
 * virtio-gpu-gl refuses to realize unless display_opengl is already set:
 *
 *   "-device virtio-gpu-gl-pci: The display backend does not have OpenGL
 *    support enabled"
 *
 * and devices are created inside qemu_init(), long before a
 * DisplayChangeListener can be registered. QEMU's own answer is a display
 * backend with an early_init hook, but every one of those is gated on X11,
 * Win32 or GBM -- egl_init() has no Darwin branch at all, so -display
 * egl-headless cannot help here.
 *
 * What actually has to exist that early is only the EGL display and the flag.
 * The surface needs a CAMetalLayer, and that cannot exist yet because the
 * layer comes from UIKit after layout. So this runs before qemu_init(), and
 * husk_display_gl_init() finishes the job afterwards.
 */
bool husk_display_gl_early(void)
{
    /*
     * This sets a flag and does nothing else, deliberately.
     *
     * An earlier version brought EGL up here too and died before returning:
     *
     *   Assertion failed: (mutex->initialized), qemu_mutex_lock_impl, line 95
     *
     * Nothing in QEMU is safe to call before qemu_init(); its locks, logging
     * and RCU machinery do not exist yet. Even error_report() is a trap.
     *
     * Fortunately none of it is needed. virtio_gpu_gl_device_realize() tests
     * exactly one thing -- display_opengl -- and never touches a context, so
     * the flag is the entire requirement at this point in startup. The EGL
     * display, context, surface and listener are all built afterwards in
     * husk_display_gl_init(), which still runs long before the guest issues
     * its first GL command: Android takes tens of seconds to reach graphics.
     *
     * fprintf, not info_report, for the same reason.
     */
    display_opengl = 1;
    fprintf(stderr, "[husk-gl] display_opengl = 1 (before device creation)\n");
    return true;
}

bool husk_display_gl_init(void *native_layer, int width, int height)
{
    QemuConsole *con;
    EGLBoolean ok;

    /*
     * Step-by-step, to stderr, because this sequence has crashed three times in
     * places that were only distinguishable after the fact. fprintf rather than
     * info_report so a crash mid-step still leaves the last line on the log.
     */
#define HUSK_GL_STEP(msg) fprintf(stderr, "[husk-gl] step: " msg "\n")

    husk_win_w = width;
    husk_win_h = height;

    HUSK_GL_STEP("qemu_egl_init_dpy_cocoa");
    if (qemu_egl_init_dpy_cocoa(DISPLAY_GL_MODE_ES) < 0) {
        fprintf(stderr, "[husk-gl] qemu_egl_init_dpy_cocoa failed\n");
        return false;
    }

    HUSK_GL_STEP("qemu_egl_init_ctx");
    husk_context = qemu_egl_init_ctx();
    if (husk_context == EGL_NO_CONTEXT) {
        fprintf(stderr, "[husk-gl] eglCreateContext failed: 0x%x\n", eglGetError());
        return false;
    }

    HUSK_GL_STEP("qemu_egl_init_surface (CAMetalLayer)");
    husk_surface = qemu_egl_init_surface(husk_context,
                                         (EGLNativeWindowType)native_layer);
    if (husk_surface == EGL_NO_SURFACE) {
        fprintf(stderr, "[husk-gl] eglCreateWindowSurface failed: 0x%x\n", eglGetError());
        return false;
    }

    /*
     * Checked, unlike before. If this fails, everything after it runs with no
     * current context -- and the first GL call then dereferences null inside
     * ANGLE, which is a segfault with no explanation attached.
     */
    HUSK_GL_STEP("eglMakeCurrent");
    ok = eglMakeCurrent(qemu_egl_display, husk_surface, husk_surface, husk_context);
    if (ok != EGL_TRUE) {
        fprintf(stderr, "[husk-gl] eglMakeCurrent failed: 0x%x\n", eglGetError());
        return false;
    }

    /*
     * glGetString came back NULL with a context that eglMakeCurrent said was
     * current, which is contradictory enough to be worth separating properly.
     * Three questions, answered independently:
     *
     *   1. Does EGL agree the context is current? eglGetCurrentContext and
     *      eglQueryString need no GL context, so they isolate EGL from GL.
     *   2. Does ANGLE hand out core GLES entry points via eglGetProcAddress?
     *      The EGL spec says it need not, and epoxy's own comments say drivers
     *      disagree about this.
     *   3. What does ANGLE's own entry point say when called directly, with
     *      epoxy's dispatch taken out of the path entirely?
     *
     * If (3) returns a vendor string while epoxy's glGetString returns NULL,
     * the fault is in epoxy's dispatch. If (3) is NULL too, the context really
     * is not current and EGL is lying.
     */
    {
        EGLContext cur_ctx = eglGetCurrentContext();
        EGLSurface cur_draw = eglGetCurrentSurface(EGL_DRAW);
        const char *egl_vendor = eglQueryString(qemu_egl_display, EGL_VENDOR);
        const char *egl_version = eglQueryString(qemu_egl_display, EGL_VERSION);
        void *direct = dlsym(RTLD_DEFAULT, "GL_GetString");
        const GLubyte *(*gl_get_string)(GLenum) = direct;

        fprintf(stderr, "[husk-gl] egl: ctx=%p (created %p) draw=%p (surface %p)\n",
                cur_ctx, husk_context, cur_draw, husk_surface);
        fprintf(stderr, "[husk-gl] egl: vendor=%s version=%s\n",
                egl_vendor ? egl_vendor : "(null)",
                egl_version ? egl_version : "(null)");
        fprintf(stderr, "[husk-gl] eglGetProcAddress(glGetString)=%p "
                        "eglGetProcAddress(glCreateShader)=%p\n",
                (void *)eglGetProcAddress("glGetString"),
                (void *)eglGetProcAddress("glCreateShader"));
        fprintf(stderr, "[husk-gl] dlsym(GL_GetString)=%p\n", direct);
        if (gl_get_string) {
            const GLubyte *v = gl_get_string(GL_VENDOR);
            const GLubyte *r = gl_get_string(GL_RENDERER);
            fprintf(stderr, "[husk-gl] ANGLE direct: vendor=%s renderer=%s\n",
                    v ? (const char *)v : "(null)",
                    r ? (const char *)r : "(null)");
        }
    }

    fprintf(stderr, "[husk-gl] epoxy: GL_VENDOR=%s GL_RENDERER=%s GL_VERSION=%s\n",
            (const char *)glGetString(GL_VENDOR),
            (const char *)glGetString(GL_RENDERER),
            (const char *)glGetString(GL_VERSION));

    /*
     * Do not walk into qemu_gl_init_shader() without a working GL dispatch --
     * that is where the segfault landed last time, calling through a pointer
     * that resolution had already failed to produce.
     */
    if (!glGetString(GL_VENDOR)) {
        fprintf(stderr, "[husk-gl] GL dispatch is not usable; refusing to "
                        "compile shaders. Falling back to the software display.\n");
        return false;
    }

    HUSK_GL_STEP("qemu_gl_init_shader");
    husk_gls = qemu_gl_init_shader();
    if (!husk_gls) {
        fprintf(stderr, "[husk-gl] shader init failed\n");
        return false;
    }

    HUSK_GL_STEP("register listener");
    husk_gl_ctx.ops = &husk_gl_ctx_ops;
    con = qemu_console_lookup_by_index(0);
    if (!con) {
        fprintf(stderr, "[husk-gl] no console 0\n");
        return false;
    }
    husk_gl_dcl.con = con;
    qemu_console_set_display_gl_ctx(con, &husk_gl_ctx);
    register_displaychangelistener(&husk_gl_dcl);

    fprintf(stderr, "[husk-gl] GL display up: %dx%d\n", width, height);
    return true;
#undef HUSK_GL_STEP
}
