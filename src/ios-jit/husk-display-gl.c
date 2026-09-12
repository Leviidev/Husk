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

bool husk_display_gl_init(void *native_layer, int width, int height)
{
    QemuConsole *con;

    husk_win_w = width;
    husk_win_h = height;

    /*
     * DISPLAY_GL_MODE_ES, not core. ANGLE speaks GLES, and so does everything
     * the guest will send through virglrenderer.
     */
    if (qemu_egl_init_dpy_cocoa(DISPLAY_GL_MODE_ES) < 0) {
        error_report("[husk-gl] qemu_egl_init_dpy_cocoa failed");
        return false;
    }

    husk_context = qemu_egl_init_ctx();
    if (husk_context == EGL_NO_CONTEXT) {
        error_report("[husk-gl] could not create the EGL context");
        return false;
    }

    husk_surface = qemu_egl_init_surface(husk_context,
                                         (EGLNativeWindowType)native_layer);
    if (husk_surface == EGL_NO_SURFACE) {
        error_report("[husk-gl] could not create a surface for the layer");
        return false;
    }

    eglMakeCurrent(qemu_egl_display, husk_surface, husk_surface, husk_context);
    husk_gls = qemu_gl_init_shader();

    husk_gl_ctx.ops = &husk_gl_ctx_ops;

    con = qemu_console_lookup_by_index(0);
    if (!con) {
        error_report("[husk-gl] no console 0");
        return false;
    }
    husk_gl_dcl.con = con;
    qemu_console_set_display_gl_ctx(con, &husk_gl_ctx);
    register_displaychangelistener(&husk_gl_dcl);

    info_report("[husk-gl] GL display up: %dx%d, renderer=%s",
                width, height, (const char *)glGetString(GL_RENDERER));
    return true;
}
