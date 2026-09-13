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

/* The current scanout's Metal texture, when virgl offers one, and the app-side
 * hook that can draw it. See husk_display_gl_set_metal_presenter(). */
static void *husk_metal_tex;
static int   husk_metal_w, husk_metal_h;
static husk_metal_present_fn husk_metal_present;

void husk_display_gl_set_metal_presenter(husk_metal_present_fn fn)
{
    husk_metal_present = fn;
    fprintf(stderr, "[husk-gl] metal presenter %s\n",
            fn ? "registered" : "cleared");
}

/*
 * Defined below, once husk_gl_dcl_ops exists to compare against.
 *
 * This one is not optional, whatever the struct's shape suggests.
 * console_compatible_with() calls it the moment a console has a GL context:
 *
 *   if (console_has_gl(con) &&
 *       !con->gl->ops->dpy_gl_ctx_is_compatible_dcl(con->gl, dcl))
 *
 * -- with no NULL check on the member. Attaching a context whose ops table
 * leaves it unset therefore does not fail a compatibility test, it jumps to
 * address zero inside register_displaychangelistener(). Every other op in this
 * table is guarded at its call site; this one is not.
 */
static bool husk_gl_ctx_is_compatible_dcl(DisplayGLCtx *dgc,
                                          DisplayChangeListener *dcl);

static const DisplayGLCtxOps husk_gl_ctx_ops = {
    .dpy_gl_ctx_is_compatible_dcl = husk_gl_ctx_is_compatible_dcl,
    .dpy_gl_ctx_create        = qemu_egl_create_context,
    .dpy_gl_ctx_destroy       = qemu_egl_destroy_context,
    .dpy_gl_ctx_make_current  = qemu_egl_make_context_current,
};

static void husk_gl_scanout_disable(DisplayChangeListener *dcl)
{
    husk_have_scanout = false;
    husk_metal_tex = NULL;
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
     * Passed through, not negated.
     *
     * The reasoning that produced "!backing_y_0_top" was sound and the answer
     * was still wrong: QEMU has two blit paths and they take opposite senses of
     * this flag. gtk-egl.c calls egl_texture_blit() with y0_top directly and
     * egl_fb_blit() with !y0_top. We use the texture path, so it is the
     * un-negated one -- and getting it backwards renders Android upside down,
     * which is exactly what it did.
     */
    /*
     * Say which handle QEMU is offering, not just the GL one.
     *
     * This fork of QEMU carries a ScanoutTextureNative alongside the GL texture
     * id, and when virglrenderer is running on Metal it can hand over the
     * underlying MTLTexture. UTM's own cocoa display prefers that handle and
     * treats the GL id as the fallback -- which is a strong hint that on this
     * stack the GL id is not always the thing holding the guest's pixels. If
     * these logs show a METAL handle, the GL texture we have been blitting from
     * may be an empty sibling of the texture virgl actually renders into.
     */
    /* Only when the shape or the handle kind changes. Android re-sets the
     * scanout constantly -- 655 times in one session -- and each of these is a
     * formatted write to stderr while the BQL is held. */
    static uint32_t last_w, last_h; static int last_native = -1;
    if (backing_width != last_w || backing_height != last_h
        || (int)native.type != last_native) {
        last_w = backing_width; last_h = backing_height;
        last_native = (int)native.type;
    fprintf(stderr, "[husk-gl] scanout_texture: id=%u %ux%u y0top=%d "
                    "native=%s handle=%p\n",
            backing_id, backing_width, backing_height, backing_y_0_top ? 1 : 0,
            native.type == SCANOUT_TEXTURE_NATIVE_TYPE_METAL ? "METAL" :
            native.type == SCANOUT_TEXTURE_NATIVE_TYPE_D3D   ? "D3D"   : "none",
            native.handle);
    }
    husk_flip = backing_y_0_top;
    husk_metal_tex = native.type == SCANOUT_TEXTURE_NATIVE_TYPE_METAL
                   ? native.handle : NULL;
    husk_metal_w = backing_width;
    husk_metal_h = backing_height;
    egl_fb_setup_for_tex(&husk_guest_fb, backing_width, backing_height,
                         backing_id, false);

    /*
     * Give the texture a filter it can actually be sampled with.
     *
     * The blit above no longer samples it, so this is not what makes the
     * picture appear -- but a render target arriving with the default
     * GL_NEAREST_MIPMAP_LINEAR and no mip chain is incomplete, and anything
     * that ever does sample it gets black for its trouble. Costs nothing, and
     * removes the trap rather than stepping around it.
     */
    glBindTexture(GL_TEXTURE_2D, backing_id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);

    husk_have_scanout = true;
    {
        GLenum st;
        GLint prev = 0;
        glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prev);
        glBindFramebuffer(GL_FRAMEBUFFER, husk_guest_fb.framebuffer);
        st = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)prev);
        if (st != GL_FRAMEBUFFER_COMPLETE) {
            fprintf(stderr, "[husk-gl] scanout fb NOT COMPLETE: 0x%x\n", st);
        }
    }
}

/*
 * Print a framebuffer as a small picture.
 *
 * Three sampled pixels were not enough. They established that SOME pixel was
 * non-black and nothing else -- not whether Android was drawing a screen or a
 * void, and not whether the blit was landing. A coarse luminance map answers
 * both at a glance, and it costs one read-back per row rather than per cell.
 *
 * GL's origin is bottom-left, so rows are read from the bottom up and printed
 * top-down; what appears in the log is the right way up.
 */
#define HUSK_TW 40
#define HUSK_TH 18

static void husk_gl_thumbnail(const char *what, GLuint fb, int w, int h)
{
    static const char ramp[] = " .:-=+*#%@";
    uint8_t *row;
    GLint prev = 0;

    if (w <= 0 || h <= 0) {
        fprintf(stderr, "[husk-gl] %s: %dx%d, nothing to read\n", what, w, h);
        return;
    }

    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prev);
    glBindFramebuffer(GL_FRAMEBUFFER, fb);
    row = g_malloc0(4 * (size_t)w);

    fprintf(stderr, "[husk-gl] %s (%dx%d) at frame %llu:\n",
            what, w, h, (unsigned long long)husk_gl_frames);
    for (int gy = 0; gy < HUSK_TH; gy++) {
        char line[HUSK_TW + 1];
        int y = (int)(((double)gy + 0.5) * h / HUSK_TH);
        unsigned long long sum = 0;

        glReadPixels(0, h - 1 - y, w, 1, GL_RGBA, GL_UNSIGNED_BYTE, row);
        for (int gx = 0; gx < HUSK_TW; gx++) {
            int x = (int)(((double)gx + 0.5) * w / HUSK_TW);
            const uint8_t *px = row + 4 * x;
            int lum = (px[0] * 30 + px[1] * 59 + px[2] * 11) / 100;
            sum += lum;
            line[gx] = ramp[lum * 9 / 255];
        }
        line[HUSK_TW] = 0;
        fprintf(stderr, "[husk-gl]   |%s| avg=%02llx\n",
                line, sum / HUSK_TW);
    }

    g_free(row);
    glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)prev);
}

/*
 * Both ends of the blit, so the two can be told apart.
 *
 * If the guest picture is blank, Android is not drawing and no amount of iOS
 * compositing work will help. If the guest picture has content and the window
 * does not, the blit is at fault. If both have content and the screen is still
 * black, the pixels are reaching the layer and the layer is not reaching the
 * screen -- three distinct bugs that until now all looked the same from here.
 */
static void husk_gl_sample(void)
{
    husk_gl_thumbnail("what the guest drew", husk_guest_fb.framebuffer,
                      husk_guest_fb.width, husk_guest_fb.height);
    husk_gl_thumbnail("what we present", 0, husk_win_w, husk_win_h);
}

static void husk_gl_update(DisplayChangeListener *dcl,
                           uint32_t x, uint32_t y, uint32_t w, uint32_t h)
{
    bool sample;

    if (!husk_have_scanout || husk_surface == EGL_NO_SURFACE) {
        return;
    }

    /*
     * Metal first, when virgl gave us a texture.
     *
     * Everything below this point goes through the GL texture id, and on this
     * stack that id is not ours to read -- the blit fails with
     * GL_INVALID_FRAMEBUFFER_OPERATION every single frame and writes nothing.
     * The GL path stays for scanouts that arrive with no native handle, which
     * is what the firmware console is, and which did render correctly.
     */
    if (husk_metal_tex && husk_metal_present) {
        husk_metal_present(husk_metal_tex, husk_flip ? 1 : 0,
                           husk_metal_w, husk_metal_h);
        husk_gl_frames++;
        return;
    }

    eglMakeCurrent(qemu_egl_display, husk_surface, husk_surface, husk_context);
    egl_fb_setup_default(&husk_window_fb, husk_win_w, husk_win_h);

    /*
     * egl_fb_blit, not egl_texture_blit -- which is what QEMU itself uses.
     *
     * The two look interchangeable and are not. egl_texture_blit() samples the
     * scanout as a GL_TEXTURE_2D through a shader, and sampling requires the
     * texture to be COMPLETE. egl_fb_setup_for_tex() only attaches it to a
     * framebuffer; it never sets GL_TEXTURE_MIN_FILTER, so the texture keeps
     * the default GL_NEAREST_MIPMAP_LINEAR with no mipmap levels behind it. In
     * GLES that is an incomplete texture, and sampling one is defined to return
     * opaque black. Not an error, not a warning -- black.
     *
     * That is why the boot console appeared and Android did not. UEFI's
     * scanouts are 2D resources virglrenderer creates and filters itself;
     * Android's are 3D render targets from the guest's own Mesa driver, which
     * sets sampler state per draw and leaves the texture object at its default.
     * One was complete by accident, the other never could be.
     *
     * glBlitFramebuffer reads through the framebuffer attachment instead, where
     * completeness does not apply -- only level 0 has to exist, which it does.
     * gd_egl_scanout_flush() reaches for egl_texture_blit() only when it has a
     * cursor to blend on top, and that path is desktop-GL anyway: the function
     * opens with glEnable(GL_TEXTURE_2D), which GLES 3.0 does not have.
     *
     * The flip argument inverts with the function, exactly as it does upstream.
     */
    egl_fb_blit(&husk_window_fb, &husk_guest_fb, !husk_flip);
    {
        static GLenum last_err = GL_NO_ERROR;
        GLenum err = glGetError();
        if (err != last_err) {
            last_err = err;
            fprintf(stderr, "[husk-gl] blit error state changed to 0x%x "
                            "at frame %llu (guest %dx%d fb=%u -> window %dx%d)\n",
                    err, (unsigned long long)husk_gl_frames,
                    husk_guest_fb.width, husk_guest_fb.height,
                    husk_guest_fb.framebuffer, husk_win_w, husk_win_h);
        }
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    /* The first few frames, then rarely: this is a synchronous read-back and
     * it stalls the pipeline, so it must not be something the frame rate pays
     * for. Rare is enough -- it answers a yes/no question. */
    /*
     * Every 1200 frames never fired. Android restored from a snapshot draws
     * about five hundred frames while the launcher comes up and then stops
     * completely, so the only thumbnails in the log were the two from before
     * anything had been drawn. Early and often, then rarely.
     */
    sample = husk_gl_frames == 1 || husk_gl_frames == 30
          || husk_gl_frames == 120 || husk_gl_frames == 300
          || (husk_gl_frames % 600) == 0;
    if (sample) {
        husk_gl_sample();
    }

    /*
     * Force the alpha channel opaque, leaving colour untouched.
     *
     * egl_texture_blit() clears to (0.1, 0.1, 0.1, 0.0) and then draws the
     * guest's texture with whatever alpha that texture carries. On a desktop
     * that is harmless, because the window is opaque and the compositor never
     * looks at alpha. Core Animation does: a CAMetalLayer is blended against
     * what is behind it, so a frame whose alpha is zero is a frame nobody can
     * see -- perfect pixels, correct size, invisible. Android's scanout is
     * routinely B8G8R8X8, where the fourth byte is defined to be ignored and
     * in practice is zero.
     *
     * The layer is also marked opaque on the UIKit side, which should make
     * this redundant. Both, because the two live in different files and the
     * symptom they prevent is a black screen with a healthy frame counter --
     * the single hardest thing in this project to diagnose from a log.
     */
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_TRUE);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

    if (eglSwapBuffers(qemu_egl_display, husk_surface) != EGL_TRUE) {
        static uint64_t complained;
        if (complained++ % 600 == 0) {
            fprintf(stderr, "[husk-gl] eglSwapBuffers failed: 0x%x "
                            "(%llu frames drawn, none presented)\n",
                    eglGetError(), (unsigned long long)husk_gl_frames);
        }
        return;
    }
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

/* Our context belongs to our listener and to no other. */
static bool husk_gl_ctx_is_compatible_dcl(DisplayGLCtx *dgc,
                                          DisplayChangeListener *dcl)
{
    return dcl->ops == &husk_gl_dcl_ops;
}

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

/*
 * Creation runs on the MAIN thread; binding runs on QEMU's.
 *
 * The previous version did everything on the QEMU thread. eglMakeCurrent
 * returned TRUE and glGetString then returned NULL -- which is what ANGLE
 * returns when no context is current, so the two disagreed. The likeliest
 * explanation left is that ANGLE cannot set up a CAMetalLayer surface off the
 * main thread: CALayer is not thread-safe, and the surface came back nominally
 * valid but without working backing.
 *
 * So the display, context and surface are created on the main thread, the
 * context is released there, and the QEMU thread makes it current afterwards.
 * A context may only be current on one thread at a time, hence the release.
 */
bool husk_display_gl_create(void *native_layer, int width, int height)
{
    husk_win_w = width;
    husk_win_h = height;

    fprintf(stderr, "[husk-gl] create: qemu_egl_init_dpy_cocoa\n");
    if (qemu_egl_init_dpy_cocoa(DISPLAY_GL_MODE_ES) < 0) {
        fprintf(stderr, "[husk-gl] qemu_egl_init_dpy_cocoa failed\n");
        return false;
    }

    fprintf(stderr, "[husk-gl] create: eglCreateContext\n");
    husk_context = qemu_egl_init_ctx();
    if (husk_context == EGL_NO_CONTEXT) {
        fprintf(stderr, "[husk-gl] eglCreateContext failed: 0x%x\n", eglGetError());
        return false;
    }

    fprintf(stderr, "[husk-gl] create: eglCreateWindowSurface on the layer\n");
    husk_surface = qemu_egl_init_surface(husk_context,
                                         (EGLNativeWindowType)native_layer);
    if (husk_surface == EGL_NO_SURFACE) {
        fprintf(stderr, "[husk-gl] eglCreateWindowSurface failed: 0x%x\n", eglGetError());
        return false;
    }

    {
        EGLint sw = -1, sh = -1;
        eglQuerySurface(qemu_egl_display, husk_surface, EGL_WIDTH, &sw);
        eglQuerySurface(qemu_egl_display, husk_surface, EGL_HEIGHT, &sh);
        fprintf(stderr, "[husk-gl] create: ANGLE reports the surface is %dx%d "
                        "(we asked for %dx%d)\n", sw, sh, width, height);
    }

    /* qemu_egl_init_ctx() left the context current here. Release it so the
     * QEMU thread can take it. */
    eglMakeCurrent(qemu_egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    fprintf(stderr, "[husk-gl] create: done, context released for the QEMU thread\n");
    return true;
}

/*
 * Answer one question -- is GL usable on this thread? -- without registering
 * anything or changing QEMU's state.
 *
 * Kept separate from bind() because the answer decides which virtio-gpu device
 * the guest gets, and that has to be decided before qemu_init() creates it.
 * Probing through bind() would mean registering a listener for a console that
 * may not exist yet.
 */
bool husk_display_gl_probe(void)
{
    const GLubyte *vendor;
    void *direct;
    const GLubyte *(*gl_get_string)(GLenum);

    if (husk_surface == EGL_NO_SURFACE || husk_context == EGL_NO_CONTEXT) {
        fprintf(stderr, "[husk-gl] probe: nothing was created\n");
        return false;
    }
    if (eglMakeCurrent(qemu_egl_display, husk_surface, husk_surface,
                       husk_context) != EGL_TRUE) {
        fprintf(stderr, "[husk-gl] probe: eglMakeCurrent failed: 0x%x\n", eglGetError());
        return false;
    }

    /*
     * Four answers now, and the fourth is the one that was missing.
     *
     * The previous probe resolved glGetString through eglGetProcAddress,
     * printed the pointer, and then never called it -- it tested dlsym and
     * epoxy instead. Both of those resolved to 0x2be73be5c, an address nowhere
     * near where ANGLE is loaded, so what they called was some other image's
     * symbol of the same name, and of course it returned NULL. The one pointer
     * known to be ANGLE's own was the one never exercised.
     */
    bool via_proc = false;

    fprintf(stderr, "[husk-gl] probe: eglGetCurrentContext=%p (created %p)\n",
            eglGetCurrentContext(), husk_context);
    fprintf(stderr, "[husk-gl] probe: egl vendor=%s version=%s apis=%s\n",
            eglQueryString(qemu_egl_display, EGL_VENDOR),
            eglQueryString(qemu_egl_display, EGL_VERSION),
            eglQueryString(qemu_egl_display, EGL_CLIENT_APIS));

    /* 1. Through EGL's own resolver: this is ANGLE answering about itself. */
    gl_get_string = (const GLubyte *(*)(GLenum))eglGetProcAddress("glGetString");
    fprintf(stderr, "[husk-gl] probe: eglGetProcAddress(glGetString)=%p\n",
            (void *)gl_get_string);
    if (gl_get_string) {
        const GLubyte *v = gl_get_string(GL_VENDOR);
        const GLubyte *r = gl_get_string(GL_RENDERER);
        via_proc = (v != NULL);
        fprintf(stderr, "[husk-gl] probe: VIA eglGetProcAddress vendor=%s renderer=%s\n",
                v ? (const char *)v : "(null)",
                r ? (const char *)r : "(null)");
    }

    /* 2. By name, which is what epoxy ends up doing, and which finds the
     *    wrong image. Kept so the two can be compared in one log. */
    direct = dlsym(RTLD_DEFAULT, "GL_GetString");
    fprintf(stderr, "[husk-gl] probe: dlsym(GL_GetString)=%p%s\n", direct,
            direct ? "" : " (not found)");

    /* 3. Epoxy's dispatch, which is what virglrenderer will actually use. */
    vendor = glGetString(GL_VENDOR);
    fprintf(stderr, "[husk-gl] probe: epoxy vendor=%s renderer=%s\n",
            vendor ? (const char *)vendor : "(null)",
            (const char *)glGetString(GL_RENDERER));

    fprintf(stderr, "[husk-gl] probe: VERDICT angle=%s epoxy=%s\n",
            via_proc ? "WORKS" : "no", vendor ? "WORKS" : "no");

    return vendor != NULL;
}

bool husk_display_gl_bind(void)
{
    QemuConsole *con;
    const GLubyte *vendor;

    if (husk_surface == EGL_NO_SURFACE || husk_context == EGL_NO_CONTEXT) {
        fprintf(stderr, "[husk-gl] bind: nothing was created\n");
        return false;
    }

    fprintf(stderr, "[husk-gl] bind: eglMakeCurrent\n");
    if (eglMakeCurrent(qemu_egl_display, husk_surface, husk_surface,
                       husk_context) != EGL_TRUE) {
        fprintf(stderr, "[husk-gl] eglMakeCurrent failed: 0x%x\n", eglGetError());
        return false;
    }

    vendor = glGetString(GL_VENDOR);
    fprintf(stderr, "[husk-gl] bind: GL_VENDOR=%s GL_RENDERER=%s GL_VERSION=%s\n",
            vendor ? (const char *)vendor : "(null)",
            (const char *)glGetString(GL_RENDERER),
            (const char *)glGetString(GL_VERSION));

    /* No usable dispatch means no shaders. Compiling them anyway is what
     * segfaulted before, calling through a pointer resolution never produced. */
    if (!vendor) {
        fprintf(stderr, "[husk-gl] GL dispatch unusable; falling back to software\n");
        return false;
    }

    /*
     * Preflight the entry points the shader path needs.
     *
     * qemu_gl_init_shader() compiles and links, and every GL call it makes goes
     * through epoxy's dispatch. An entry point epoxy cannot resolve arrives
     * there as a NULL function pointer and is called anyway -- which is a
     * SIGSEGV with nothing in the log but the line before it. Checking first
     * turns that into a named symbol and a fallback to the software display,
     * which is slow but alive.
     */
    {
        static const char *const needed[] = {
            "glCreateShader", "glShaderSource", "glCompileShader",
            "glGetShaderiv", "glGetShaderInfoLog", "glCreateProgram",
            "glAttachShader", "glLinkProgram", "glGetProgramiv",
            "glGetProgramInfoLog", "glDeleteShader", "glUseProgram",
            "glGenVertexArrays", "glBindVertexArray", "glGenBuffers",
            "glBindBuffer", "glBufferData", "glVertexAttribPointer",
            "glEnableVertexAttribArray", "glGetAttribLocation",
            "glGetUniformLocation", "glUniform1i", "glActiveTexture",
            "glBindTexture", "glDrawArrays", "glViewport", "glClear",
            "glClearColor",
            /* egl_fb_setup_for_tex() and egl_texture_blit() reach for these,
             * and the first list forgot all of them. */
            "glGenFramebuffers", "glBindFramebuffer", "glFramebufferTexture2D",
            "glDeleteFramebuffers", "glDeleteTextures", "glGenTextures",
            "glTexParameteri", "glTexImage2D", "glBlitFramebuffer",
            "glCheckFramebufferStatus", "glDisable", "glGetError",
            "glReadPixels", "glColorMask", "glGetIntegerv",
            "glBlitFramebuffer", "glTexParameteri", "glBindTexture",
        };
        bool missing = false;
        for (size_t i = 0; i < ARRAY_SIZE(needed); i++) {
            if (!eglGetProcAddress(needed[i])) {
                fprintf(stderr, "[husk-gl] missing entry point: %s\n", needed[i]);
                missing = true;
            }
        }
        if (missing) {
            fprintf(stderr, "[husk-gl] GL is incomplete; using the software "
                            "display rather than crashing in the shader\n");
            return false;
        }
        fprintf(stderr, "[husk-gl] all %zu shader entry points resolve\n",
                ARRAY_SIZE(needed));
    }

    husk_gls = qemu_gl_init_shader();
    if (!husk_gls) {
        fprintf(stderr, "[husk-gl] shader init failed\n");
        return false;
    }

    husk_gl_ctx.ops = &husk_gl_ctx_ops;

    fprintf(stderr, "[husk-gl] bind: qemu_console_lookup_by_index(0)\n");
    con = qemu_console_lookup_by_index(0);
    if (!con) {
        fprintf(stderr, "[husk-gl] no console 0\n");
        return false;
    }
    /*
     * console_compatible_with() reaches straight through con->hw_ops without
     * checking it, so a console that is not a graphics console segfaults inside
     * registration rather than being rejected. Report what we got before
     * handing it over.
     */
    fprintf(stderr, "[husk-gl] bind: console=%p graphic=%d gl_block=%d\n",
            (void *)con, qemu_console_is_graphic(con) ? 1 : 0, 0);

    husk_gl_dcl.con = con;
    fprintf(stderr, "[husk-gl] bind: qemu_console_set_display_gl_ctx\n");
    qemu_console_set_display_gl_ctx(con, &husk_gl_ctx);
    fprintf(stderr, "[husk-gl] bind: register_displaychangelistener\n");
    register_displaychangelistener(&husk_gl_dcl);
    fprintf(stderr, "[husk-gl] bind: listener registered\n");

    fprintf(stderr, "[husk-gl] GL display up: %dx%d\n", husk_win_w, husk_win_h);
    return true;
}
