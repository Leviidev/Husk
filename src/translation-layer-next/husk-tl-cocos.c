/* SPDX-License-Identifier: GPL-2.0-or-later */
#define _DARWIN_C_SOURCE
#include "husk-tl-cocos.h"
#include "husk-tl-geode.h"

#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "husk-tl-bionic.h"
#include "husk-tl-dexindex.h"
#include "husk-tl-egl.h"
#include "husk-tl-gamepad.h"
#include "husk-tl-jni.h"
#include "husk-tl-ld.h"

void tl_jni_hle_install(void);
void tl_hle_configure(const char *pkg, const char *apk, const char *data, int w, int h);
jobj *tl_hle_activity(void);
void tl_set_data_dir(const char *dir);
void tl_nwindow_configure(int w, int h, void *layer);
void *tl_nwindow_get(void);
void tl_cocos_hle_install(const char *pkg, const char *apk, const char *data, int w, int h);

#define CLS_RENDERER "org/cocos2dx/lib/Cocos2dxRenderer"

static _Atomic long long g_interval_ns = 1000000000ll / 60;     /* Cocos2dxRenderer.sAnimationInterval */

void tl_cocos_set_interval(double seconds)
{
    if (seconds > 0.002 && seconds < 1.0) atomic_store(&g_interval_ns, (long long)(seconds * 1e9));
}

static struct {
    tl_cocos_config cfg;
    char apk[1024], data[512], pkg[128], frame_dir[512], angle_egl[600], angle_gles[600];
    atomic_ulong frames;
    atomic_bool stop, paused, ended;
    atomic_ullong perf_ns, perf_max_ns;
    atomic_ulong perf_frames;
    struct timespec perf_since;
    pthread_t thread;
    bool thread_started;
} U;

/* The native a Java method would have bound: registered by the library, or found by its JNI name. */
static void *native_of(const char *cls, const char *name, const char *sig, const char *mangled)
{
    void *fn = tl_jni_native(cls, name, sig);
    if (!fn) {
        tl_lib *lib = tl_ld_find_lib("libcocos2dcpp.so");
        if (lib) fn = tl_ld_sym(lib, mangled);
    }
    if (!fn) tl_log_line("cocos: native %s.%s%s is not provided by the library", cls, name, sig);
    return fn;
}

static void load_library(const char *name)
{
    jvalue a; a.j = 0; a.l = tl_jni_new_string(name);
    tl_jni_call(tl_jni_class_object("java/lang/System"), "loadLibrary", "(Ljava/lang/String;)V", &a);
}

bool tl_cocos_start(const tl_cocos_config *cfg)
{
    U.cfg = *cfg;
    snprintf(U.apk, sizeof(U.apk), "%s", cfg->apk_path);
    snprintf(U.data, sizeof(U.data), "%s", cfg->data_dir);
    snprintf(U.pkg, sizeof(U.pkg), "%s", cfg->package_name);
    U.cfg.apk_path = U.apk; U.cfg.data_dir = U.data; U.cfg.package_name = U.pkg;
    if (cfg->frame_dir) { snprintf(U.frame_dir, sizeof(U.frame_dir), "%s", cfg->frame_dir); U.cfg.frame_dir = U.frame_dir; }
    if (cfg->angle_egl) { snprintf(U.angle_egl, sizeof(U.angle_egl), "%s", cfg->angle_egl); U.cfg.angle_egl = U.angle_egl; }
    if (cfg->angle_gles) { snprintf(U.angle_gles, sizeof(U.angle_gles), "%s", cfg->angle_gles); U.cfg.angle_gles = U.angle_gles; }

    tl_set_data_dir(cfg->data_dir);
    tl_nwindow_configure(cfg->width, cfg->height, cfg->metal_layer);
    int n = tl_dexidx_open(cfg->apk_path);
    tl_log_line("cocos: %d classes in the APK's DEX", n);
    if (!tl_ld_add_apk(cfg->apk_path)) return false;
    if (cfg->angle_egl && !tl_egl_init(cfg->angle_egl, cfg->angle_gles, cfg->frame_dir, cfg->frame_every)) return false;
    tl_jni_init();
    tl_hle_configure(cfg->package_name, cfg->apk_path, cfg->data_dir, cfg->width, cfg->height);
    tl_jni_hle_install();
    tl_cocos_hle_install(cfg->package_name, cfg->apk_path, cfg->data_dir, cfg->width, cfg->height);

    /* GeometryJump.<clinit>: System.loadLibrary("fmod"); System.loadLibrary("cocos2dcpp"). Each runs its JNI_OnLoad. */
    load_library("fmod");
    if (tl_jni_pending()) { tl_log_line("cocos: loading libfmod.so failed"); return false; }
    load_library("cocos2dcpp");
    if (tl_jni_pending()) { tl_log_line("cocos: loading libcocos2dcpp.so failed"); return false; }

    /* BaseRobTopActivity.onCreate: JniToCpp.setupHSSAssets(sourceDir, externalStorageDirectory). */
    typedef void (*str2_fn)(void *env, void *cls, void *a, void *b);
    str2_fn setup = (str2_fn)native_of("com/customRobTop/JniToCpp", "setupHSSAssets", "(Ljava/lang/String;Ljava/lang/String;)V",
                                       "Java_com_customRobTop_JniToCpp_setupHSSAssets");
    if (setup) {
        char ext[700]; snprintf(ext, sizeof(ext), "%s/sdcard", cfg->data_dir);
        setup(tl_jni_env(), tl_jni_class_object("com/customRobTop/JniToCpp"), tl_jni_new_string(cfg->apk_path), tl_jni_new_string(ext));
        tl_log_line("cocos: setupHSSAssets done");
    }
    /* Cocos2dxHelper.init: nativeSetApkPath(applicationInfo.sourceDir). */
    typedef void (*str1_fn)(void *env, void *cls, void *a);
    str1_fn setapk = (str1_fn)native_of("org/cocos2dx/lib/Cocos2dxHelper", "nativeSetApkPath", "(Ljava/lang/String;)V",
                                        "Java_org_cocos2dx_lib_Cocos2dxHelper_nativeSetApkPath");
    if (!setapk) return false;
    setapk(tl_jni_env(), tl_jni_class_object("org/cocos2dx/lib/Cocos2dxHelper"), tl_jni_new_string(cfg->apk_path));
    tl_log_line("cocos: nativeSetApkPath done");
    if (tl_jni_pending()) return false;
    /* Geode, when asked for: last, as its launcher does, so the game is set up when it hooks in. */
    if (tl_geode_configured() && !tl_geode_load(tl_hle_activity())) tl_log_line("cocos: Geode did not load; the game starts without it");
    return true;
}

/* ------------------------------------------------------------- text bitmaps */

void tl_cocos_deliver_bitmap(int width, int height, const uint8_t *rgba)
{
    typedef void (*bitmap_fn)(void *env, void *cls, int w, int h, void *pixels);
    static bitmap_fn fn;
    if (!fn) fn = (bitmap_fn)native_of("org/cocos2dx/lib/Cocos2dxBitmap", "nativeInitBitmapDC", "(II[B)V", "Java_org_cocos2dx_lib_Cocos2dxBitmap_nativeInitBitmapDC");
    if (!fn) return;
    size_t n = (size_t)width * (size_t)height * 4;
    jobj *arr = tl_jni_new_prim_array('B', (uint32_t)n);
    memcpy(arr->arr.data, rgba, n);
    fn(tl_jni_env(), tl_jni_class_object("org/cocos2dx/lib/Cocos2dxBitmap"), width, height, arr);
    tl_jni_unref(arr);
}

/* JniToCpp.resumeSound(): the activity's way of telling the game its sound may come back (after the screen is unlocked). */
void tl_cocos_resume_sound(void)
{
    typedef void (*fn_t)(void *env, void *cls);
    fn_t fn = (fn_t)native_of("com/customRobTop/JniToCpp", "resumeSound", "()V", "Java_com_customRobTop_JniToCpp_resumeSound");
    if (fn) fn(tl_jni_env(), tl_jni_class_object("com/customRobTop/JniToCpp"));
}

/* -------------------------------------------------------------------- input */

/*
 * Everything the game is told arrives on the GL thread, as Cocos2dxGLSurfaceView does with queueEvent: touches,
 * typed text, keys, and the request for the text the game's field holds. One queue keeps them in order.
 */
enum { EV_TOUCH, EV_INSERT, EV_DELETE, EV_KEY, EV_CONTENT };
typedef struct { int kind, phase, id; float x, y; char *text; void (*cb)(const char *); } event;
static struct {
    pthread_mutex_t lock;
    event q[256];
    int n;
} T = { .lock = PTHREAD_MUTEX_INITIALIZER };

static void enqueue(event e)
{
    pthread_mutex_lock(&T.lock);
    if (T.n < 256) T.q[T.n++] = e; else free(e.text);
    pthread_mutex_unlock(&T.lock);
}

void tl_cocos_touch(int phase, int id, float x, float y)
{
    pthread_mutex_lock(&T.lock);
    /* a move replaces an earlier queued move of the same finger */
    if (phase == 1) for (int i = T.n - 1; i >= 0; i--) if (T.q[i].kind == EV_TOUCH && T.q[i].id == id && T.q[i].phase == 1) { T.q[i].x = x; T.q[i].y = y; pthread_mutex_unlock(&T.lock); return; }
    pthread_mutex_unlock(&T.lock);
    enqueue((event){ .kind = EV_TOUCH, .phase = phase, .id = id, .x = x, .y = y });
}

void tl_cocos_insert_text(const char *utf8) { if (utf8) enqueue((event){ .kind = EV_INSERT, .text = strdup(utf8) }); }
void tl_cocos_delete_backward(void) { enqueue((event){ .kind = EV_DELETE }); }
void tl_cocos_key_down(int keycode) { enqueue((event){ .kind = EV_KEY, .id = keycode }); }
/*
 * A controller in a cocos2d-x game (Geometry Dash plays by touch): its B button is Back, the one key the game listens for.
 * The rest of the controller has nothing to drive.
 */
static void pad_key(jobj *ev, int device, int action, int keycode, int64_t down_ms, int64_t event_ms)
{
    (void)device; (void)down_ms; (void)event_ms;
    if (action == 0 && keycode == 97 /* BUTTON_B */) tl_cocos_key_down(4 /* KEYCODE_BACK */);
    tl_jni_unref(ev);
}
static void pad_motion(jobj *ev, int device, int source, int64_t down_ms, int64_t event_ms)
{ (void)device; (void)source; (void)down_ms; (void)event_ms; tl_jni_unref(ev); }

void tl_cocos_register_pad_sink(void)
{
    static const tl_pad_sink sink = { pad_key, pad_motion };
    tl_pad_set_sink(&sink);
}

void tl_cocos_request_content_text(void (*cb)(const char *utf8)) { if (cb) enqueue((event){ .kind = EV_CONTENT, .cb = cb }); }

static void drain_events(void)
{
    typedef void (*one_fn)(void *env, void *cls, int id, float x, float y);
    typedef void (*many_fn)(void *env, void *cls, void *ids, void *xs, void *ys);
    static one_fn begin, end;
    static many_fn move, cancel;
    static void (*insert)(void *, void *, void *), (*del)(void *, void *);
    static uint8_t (*keydown)(void *, void *, int);
    static void *(*content)(void *, void *);
    if (!begin) {
        const char *r = "Java_org_cocos2dx_lib_Cocos2dxRenderer_";
        char m[160];
        #define NAT(var, type, name, sig) do { snprintf(m, sizeof(m), "%s%s", r, name); var = (type)native_of(CLS_RENDERER, name, sig, m); } while (0)
        NAT(begin, one_fn, "nativeTouchesBegin", "(IFF)V");
        NAT(end, one_fn, "nativeTouchesEnd", "(IFF)V");
        NAT(move, many_fn, "nativeTouchesMove", "([I[F[F)V");
        NAT(cancel, many_fn, "nativeTouchesCancel", "([I[F[F)V");
        NAT(insert, void (*)(void *, void *, void *), "nativeInsertText", "(Ljava/lang/String;)V");
        NAT(del, void (*)(void *, void *), "nativeDeleteBackward", "()V");
        NAT(keydown, uint8_t (*)(void *, void *, int), "nativeKeyDown", "(I)Z");
        NAT(content, void *(*)(void *, void *), "nativeGetContentText", "()Ljava/lang/String;");
        #undef NAT
    }
    event ev[256]; int n;
    pthread_mutex_lock(&T.lock);
    n = T.n; memcpy(ev, T.q, (size_t)n * sizeof(event)); T.n = 0;
    pthread_mutex_unlock(&T.lock);
    void *env = tl_jni_env(); void *cls = tl_jni_class_object(CLS_RENDERER);
    for (int i = 0; i < n; i++) {
        event *e = &ev[i];
        switch (e->kind) {
        case EV_TOUCH:
            if (e->phase == 0 && begin) begin(env, cls, e->id, e->x, e->y);
            else if (e->phase == 2 && end) end(env, cls, e->id, e->x, e->y);
            else if (e->phase == 1 || e->phase == 3) {
                many_fn fn = e->phase == 1 ? move : cancel;
                if (!fn) break;
                jobj *ids = tl_jni_new_prim_array('I', 1), *xs = tl_jni_new_prim_array('F', 1), *ys = tl_jni_new_prim_array('F', 1);
                ((int *)ids->arr.data)[0] = e->id; ((float *)xs->arr.data)[0] = e->x; ((float *)ys->arr.data)[0] = e->y;
                fn(env, cls, ids, xs, ys);
                tl_jni_unref(ids); tl_jni_unref(xs); tl_jni_unref(ys);
            }
            break;
        case EV_INSERT:
            if (insert) { jobj *str = tl_jni_new_string(e->text); insert(env, cls, str); tl_jni_unref(str); }
            free(e->text);
            break;
        case EV_DELETE: if (del) del(env, cls); break;
        case EV_KEY: if (keydown) keydown(env, cls, e->id); break;
        case EV_CONTENT:
            if (content) {
                jobj *str = content(env, cls);
                e->cb(str && tl_jni_string(str) ? tl_jni_string(str) : "");
            } else e->cb("");
            break;
        }
        if (tl_jni_pending()) tl_jni_clear();
    }
}

/* ---------------------------------------------------------------- GL thread */

typedef void *EGLDisplay, *EGLSurface, *EGLContext, *EGLConfig;
typedef int32_t EGLint;
#define EGL_NONE 0x3038

static int64_t now_ns(void) { struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts); return (int64_t)ts.tv_sec * 1000000000ll + ts.tv_nsec; }

static void *gl_main(void *arg)
{
    (void)arg;
    pthread_setname_np("GLThread");
    tl_log_line("cocos: GL thread started");
    EGLDisplay (*getDisplay)(void *) = tl_egl_resolve("eglGetDisplay");
    unsigned (*initialize)(EGLDisplay, EGLint *, EGLint *) = tl_egl_resolve("eglInitialize");
    unsigned (*bindAPI)(unsigned) = tl_egl_resolve("eglBindAPI");
    unsigned (*chooseConfig)(EGLDisplay, const EGLint *, EGLConfig *, EGLint, EGLint *) = tl_egl_resolve("eglChooseConfig");
    EGLContext (*createContext)(EGLDisplay, EGLConfig, EGLContext, const EGLint *) = tl_egl_resolve("eglCreateContext");
    EGLSurface (*createWindowSurface)(EGLDisplay, EGLConfig, void *, const EGLint *) = tl_egl_resolve("eglCreateWindowSurface");
    unsigned (*makeCurrent)(EGLDisplay, EGLSurface, EGLSurface, EGLContext) = tl_egl_resolve("eglMakeCurrent");
    unsigned (*swapBuffers)(EGLDisplay, EGLSurface) = tl_egl_resolve("eglSwapBuffers");
    unsigned (*swapInterval)(EGLDisplay, EGLint) = tl_egl_resolve("eglSwapInterval");
    EGLint (*getError)(void) = tl_egl_resolve("eglGetError");
    if (!getDisplay || !createWindowSurface || !swapBuffers) { tl_log_line("cocos: EGL is not available"); atomic_store(&U.ended, true); return NULL; }

    EGLDisplay dpy = getDisplay(NULL);
    EGLint major = 0, minor = 0;
    initialize(dpy, &major, &minor);
    bindAPI(0x30A0 /* EGL_OPENGL_ES_API */);
    /* The game asks for RGB565 with a 16-bit depth buffer and 8 bits of stencil; ANGLE's 8888 satisfies that. */
    const EGLint want[] = { 0x3040 /* RENDERABLE_TYPE */, 0x4 /* ES2 */, 0x3033 /* SURFACE_TYPE */, 0x4 /* WINDOW */,
                            0x3024, 8, 0x3023, 8, 0x3022, 8, 0x3021, 8, 0x3025 /* DEPTH */, 16, 0x3026 /* STENCIL */, 8, EGL_NONE };
    EGLConfig cfg = NULL; EGLint ncfg = 0;
    if (!chooseConfig(dpy, want, &cfg, 1, &ncfg) || ncfg < 1) { tl_log_line("cocos: eglChooseConfig found nothing (error %#x)", getError()); atomic_store(&U.ended, true); return NULL; }
    const EGLint ctx_attr[] = { 0x3098 /* CONTEXT_CLIENT_VERSION */, 2, EGL_NONE };
    EGLContext ctx = createContext(dpy, cfg, NULL, ctx_attr);
    EGLSurface surf = createWindowSurface(dpy, cfg, tl_nwindow_get(), NULL);
    if (!ctx || !surf || !makeCurrent(dpy, surf, surf, ctx)) { tl_log_line("cocos: could not make a GL context current (error %#x)", getError()); atomic_store(&U.ended, true); return NULL; }
    if (swapInterval) swapInterval(dpy, 1);
    tl_log_line("cocos: EGL context ready (EGL %d.%d)", major, minor);

    /* Cocos2dxRenderer.onSurfaceCreated -> nativeInit(width, height) */
    typedef void (*init_fn)(void *env, void *cls, int w, int h);
    init_fn init = (init_fn)native_of(CLS_RENDERER, "nativeInit", "(II)V", "Java_org_cocos2dx_lib_Cocos2dxRenderer_nativeInit");
    typedef void (*render_fn)(void *env, void *cls);
    render_fn render = (render_fn)native_of(CLS_RENDERER, "nativeRender", "()V", "Java_org_cocos2dx_lib_Cocos2dxRenderer_nativeRender");
    typedef void (*life_fn)(void *env, void *cls);
    life_fn on_pause = (life_fn)native_of(CLS_RENDERER, "nativeOnPause", "()V", "Java_org_cocos2dx_lib_Cocos2dxRenderer_nativeOnPause");
    life_fn on_resume = (life_fn)native_of(CLS_RENDERER, "nativeOnResume", "()V", "Java_org_cocos2dx_lib_Cocos2dxRenderer_nativeOnResume");
    if (!init || !render) { atomic_store(&U.ended, true); return NULL; }
    void *env = tl_jni_env(), *cls = tl_jni_class_object(CLS_RENDERER);
    init(env, cls, U.cfg.width, U.cfg.height);
    tl_log_line("cocos: nativeInit(%d, %d) done", U.cfg.width, U.cfg.height);
    if (tl_jni_pending()) { tl_log_line("cocos: an exception is pending after nativeInit"); tl_jni_clear(); }

    bool was_paused = false;
    int64_t next = now_ns();
    while (!atomic_load(&U.stop)) {
        if (atomic_load(&U.paused)) {
            if (!was_paused) { if (on_pause) on_pause(env, cls); was_paused = true; tl_log_line("cocos: paused"); }
            usleep(20000);
            next = now_ns();
            continue;
        }
        if (was_paused) {
            if (on_resume) on_resume(env, cls);
            /* The game's own resume does not bring its music back here; the activity's resumeSound does. */
            tl_cocos_resume_sound();
            was_paused = false; next = now_ns(); tl_log_line("cocos: resumed");
        }
        drain_events();
        int64_t t0 = now_ns();
        render(env, cls);
        swapBuffers(dpy, surf);
        int64_t t1 = now_ns();
        atomic_fetch_add(&U.perf_ns, (unsigned long long)(t1 - t0));
        atomic_fetch_add(&U.perf_frames, 1);
        unsigned long long m = atomic_load(&U.perf_max_ns), ns = (unsigned long long)(t1 - t0);
        while (ns > m && !atomic_compare_exchange_weak(&U.perf_max_ns, &m, ns)) {}
        atomic_fetch_add(&U.frames, 1);
        if (tl_jni_pending()) tl_jni_clear();
        /* A GLSurfaceView renders at the display's pace; the game's own animation interval is 1/60 s. */
        next += atomic_load(&g_interval_ns);
        int64_t late = now_ns() - next;
        if (late > 100000000ll) next = now_ns();
        else if (late < 0) { struct timespec ts = { 0, (long)(-late) }; nanosleep(&ts, NULL); }
    }
    return NULL;
}

bool tl_cocos_run(void)
{
    pthread_attr_t a;
    pthread_attr_init(&a);
    pthread_attr_setstacksize(&a, 16u << 20);
    if (pthread_create(&U.thread, &a, gl_main, NULL) != 0) return false;
    U.thread_started = true;
    return true;
}

void tl_cocos_perf_snapshot(tl_cocos_perf *out)
{
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    unsigned long long ns = atomic_exchange(&U.perf_ns, 0), mx = atomic_exchange(&U.perf_max_ns, 0);
    unsigned long n = atomic_exchange(&U.perf_frames, 0);
    double elapsed = U.perf_since.tv_sec ? (now.tv_sec - U.perf_since.tv_sec) + (now.tv_nsec - U.perf_since.tv_nsec) / 1e9 : 0;
    U.perf_since = now;
    out->fps = elapsed > 0.05 ? (double)n / elapsed : 0;
    out->mean_ms = n ? (double)ns / n / 1e6 : 0;
    out->max_ms = (double)mx / 1e6;
}

void tl_cocos_set_paused(bool paused) { atomic_store(&U.paused, paused); }
unsigned long tl_cocos_frames(void) { return atomic_load(&U.frames); }
bool tl_cocos_ended(void) { return atomic_load(&U.ended); }

void tl_cocos_stop(void)
{
    atomic_store(&U.stop, true);
    if (U.thread_started) { pthread_join(U.thread, NULL); U.thread_started = false; }
    atomic_store(&U.stop, false);
}
