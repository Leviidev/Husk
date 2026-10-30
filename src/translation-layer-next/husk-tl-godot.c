/* SPDX-License-Identifier: GPL-2.0-or-later */
#define _DARWIN_C_SOURCE
#include "husk-tl-godot.h"

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
#include "husk-tl-jni.h"
#include "husk-tl-ld.h"
#include "husk-tl-internal.h"

void tl_jni_hle_install(void);
void tl_hle_configure(const char *pkg, const char *apk, const char *data, int w, int h);
jobj *tl_hle_activity(void);
jobj *tl_hle_assets(void);
void tl_set_data_dir(const char *dir);
void tl_nwindow_configure(int w, int h, void *layer);
void *tl_nwindow_get(void);

#define LIB "org/godotengine/godot/GodotLib"

static struct {
    tl_godot_config cfg;
    char apk[1024], data[512], pkg[128], frame_dir[512], angle_egl[600], angle_gles[600];
    int major;
    atomic_ulong frames;
    atomic_bool stop, paused, ended;
    atomic_ullong perf_ns, perf_max_ns;
    atomic_ulong perf_frames;
    struct timespec perf_since;
    pthread_t thread;
    bool thread_started;
} U;

static jvalue vl(void *p) { jvalue v; v.j = 0; v.l = p; return v; }

/* The native a GodotLib method binds to: Godot exports them under their JNI names. */
static void *native_of(const char *name, const char *sig)
{
    void *fn = tl_jni_native(LIB, name, sig);
    if (!fn) {
        char mangled[200];
        snprintf(mangled, sizeof(mangled), "Java_org_godotengine_godot_GodotLib_%s", name);
        tl_lib *lib = tl_ld_find_lib("libgodot_android.so");
        if (lib) fn = tl_ld_sym(lib, mangled);
    }
    if (!fn) tl_log_line("godot: GodotLib.%s%s is not in the library", name, sig);
    return fn;
}

/* Which Godot this is, from the version string the library carries ("3.6.stable", "4.3.stable"). */
static int read_major(void)
{
    tl_lib *lib = tl_ld_find_lib("libgodot_android.so");
    if (!lib) return 0;
    /* The JNI signatures differ between 3 and 4: only 4 has GodotLib.onSurfaceCreated-era natives such as
     * "Java_org_godotengine_godot_GodotLib_onNightModeChanged" or "..._getProjectResourceDir". */
    if (tl_ld_sym(lib, "Java_org_godotengine_godot_GodotLib_getProjectResourceDir") || tl_ld_sym(lib, "Java_org_godotengine_godot_GodotLib_onNightModeChanged")
        || tl_ld_sym(lib, "Java_org_godotengine_godot_GodotLib_isEditorHint")) return 4;
    return 3;
}

int tl_godot_major(void) { return U.major; }

static jobj *command_line(void)
{
    char *list[32]; int n = 0;
    for (int z = 0; n == 0; z++) {
        const tl_zip *zip = tl_ld_apk_at(z);
        if (!zip) break;
        const tl_zip_entry *e = tl_zip_find(zip, "assets/_cl_");
        if (!e) continue;
        const uint8_t *d; size_t len; bool owned; char err[160];
        if (!tl_zip_data(zip, e, 1 << 20, &d, &len, &owned, err, sizeof(err))) break;
        size_t o = 4;
        int count = len >= 4 ? (int)(d[0] | d[1] << 8 | d[2] << 16 | (uint32_t)d[3] << 24) : 0;
        for (int i = 0; i < count && n < 32 && o + 4 <= len; i++) {
            uint32_t l = d[o] | d[o + 1] << 8 | d[o + 2] << 16 | (uint32_t)d[o + 3] << 24;
            o += 4;
            if (o + l > len) break;
            list[n] = malloc(l + 1); memcpy(list[n], d + o, l); list[n][l] = 0; n++;
            o += l;
        }
        if (owned) free((void *)d);
    }
    /* Godot 4 games exported for the Forward+ or Mobile renderer ask for Vulkan with an Android window surface, which MoltenVK
     * does not have, and then fail to make any display ("all display drivers failed"). Husk gives Godot a GL ES context, so run
     * them on Godot's own OpenGL renderer (Compatibility), which every Godot 4 game can use. TL_GODOT_VULKAN keeps their choice. */
    if (U.major >= 4 && !getenv("TL_GODOT_VULKAN") && n + 4 <= 32) {
        static const char *gl[] = { "--rendering-driver", "opengl3", "--rendering-method", "gl_compatibility" };
        for (int i = 0; i < 4; i++) list[n++] = strdup(gl[i]);
    }
    jobj *arr = tl_jni_new_obj_array(tl_jni_class("java/lang/String"), (uint32_t)n);
    for (int i = 0; i < n; i++) { arr->oarr.v[i] = tl_jni_new_string(list[i]); tl_log_line("godot: argument %s", list[i]); free(list[i]); }
    return arr;
}

bool tl_godot_start(const tl_godot_config *cfg)
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
    tl_log_line("godot: %d classes in the APK's DEX", n);
    if (!tl_ld_add_apk(cfg->apk_path)) return false;
    if (cfg->angle_egl && !tl_egl_init(cfg->angle_egl, cfg->angle_gles, cfg->frame_dir, cfg->frame_every)) return false;
    tl_jni_init();
    tl_hle_configure(cfg->package_name, cfg->apk_path, cfg->data_dir, cfg->width, cfg->height);
    tl_jni_hle_install();
    tl_godot_hle_install(cfg->package_name, cfg->apk_path, cfg->data_dir, cfg->width, cfg->height);

    /* Godot.onCreate: System.loadLibrary("godot_android") (JNI_OnLoad keeps the VM). */
    jvalue a = vl(tl_jni_new_string("godot_android"));
    tl_jni_call(tl_jni_class_object("java/lang/System"), "loadLibrary", "(Ljava/lang/String;)V", &a);
    if (tl_jni_pending()) { tl_log_line("godot: loading libgodot_android.so failed"); return false; }
    U.major = read_major();
    tl_log_line("godot: Godot %d", U.major);

    /* GodotLib.initialize(activity, godot, assets, io, netUtils, directoryAccess, fileAccess, useApkExpansion) */
    jobj *godot = tl_jni_new_object(tl_jni_class("org/godotengine/godot/Godot"));
    jobj *io = tl_jni_new_object(tl_jni_class("org/godotengine/godot/GodotIO"));
    jobj *net = tl_jni_new_object(tl_jni_class("org/godotengine/godot/utils/GodotNetUtils"));
    jobj *dirs = tl_jni_new_object(tl_jni_class("org/godotengine/godot/io/directory/DirectoryAccessHandler"));
    jobj *files = tl_jni_new_object(tl_jni_class("org/godotengine/godot/io/file/FileAccessHandler"));
    /* Godot 3 and early 4 pass the activity first; later 4.x drop it and answer whether it went well. Which one this is, the
     * game's own GodotLib says. */
    static const char *sig_act = "(Landroid/app/Activity;Lorg/godotengine/godot/Godot;Landroid/content/res/AssetManager;Lorg/godotengine/godot/GodotIO;"
        "Lorg/godotengine/godot/utils/GodotNetUtils;Lorg/godotengine/godot/io/directory/DirectoryAccessHandler;"
        "Lorg/godotengine/godot/io/file/FileAccessHandler;Z)V";
    static const char *sig_noact = "(Lorg/godotengine/godot/Godot;Landroid/content/res/AssetManager;Lorg/godotengine/godot/GodotIO;"
        "Lorg/godotengine/godot/utils/GodotNetUtils;Lorg/godotengine/godot/io/directory/DirectoryAccessHandler;"
        "Lorg/godotengine/godot/io/file/FileAccessHandler;Z)Z";
    if (tl_dexidx_declares_method(LIB, "initialize", sig_noact, NULL)) {
        typedef uint8_t (*init7_fn)(void *env, void *cls, void *godot, void *assets, void *io, void *net, void *dirs, void *files, uint8_t exp);
        init7_fn init = (init7_fn)native_of("initialize", sig_noact);
        if (!init) return false;
        if (!init(tl_jni_env(), tl_jni_class_object(LIB), godot, tl_hle_assets(), io, net, dirs, files, 0)) { tl_log_line("godot: initialize refused"); return false; }
    } else {
        typedef void (*init8_fn)(void *env, void *cls, void *act, void *godot, void *assets, void *io, void *net, void *dirs, void *files, uint8_t exp);
        init8_fn init = (init8_fn)native_of("initialize", sig_act);
        if (!init) return false;
        init(tl_jni_env(), tl_jni_class_object(LIB), tl_hle_activity(), godot, tl_hle_assets(), io, net, dirs, files, 0);
    }
    if (tl_jni_pending()) { tl_log_line("godot: initialize left an exception"); tl_jni_clear(); }
    tl_log_line("godot: initialize done");

    /* GodotLib.setup(commandLine, tts): the export's command line, which Godot.java reads from assets/_cl_ -- a count, then
     * each argument as a length and its bytes, little-endian. */
    jobj *args = command_line();
    jobj *tts = tl_jni_new_object(tl_jni_class("org/godotengine/godot/tts/GodotTTS"));
    uint8_t ok = 1;
    if (tl_dexidx_declares_method(LIB, "setup", "([Ljava/lang/String;)V", NULL)) {
        /* Godot 3.5 and older: no text-to-speech, and nothing to say whether it worked. */
        typedef void (*setup1_fn)(void *env, void *cls, void *args);
        setup1_fn setup = (setup1_fn)native_of("setup", "([Ljava/lang/String;)V");
        if (!setup) return false;
        setup(tl_jni_env(), tl_jni_class_object(LIB), args);
    } else {
        typedef uint8_t (*setup_fn)(void *env, void *cls, void *args, void *tts);
        setup_fn setup = (setup_fn)native_of("setup", "([Ljava/lang/String;Lorg/godotengine/godot/tts/GodotTTS;)Z");
        if (!setup) return false;
        ok = setup(tl_jni_env(), tl_jni_class_object(LIB), args, tts);
    }
    if (tl_jni_pending()) { tl_log_line("godot: setup left an exception"); tl_jni_clear(); }
    tl_log_line("godot: setup -> %d", ok);
    return ok != 0;
}

/* ------------------------------------------------------------------ input */

typedef struct { int kind, phase, id, key; bool down; float x, y; } event;
enum { EV_TOUCH, EV_KEY };
static pthread_mutex_t q_lock = PTHREAD_MUTEX_INITIALIZER;
static event q[256];
static int q_n;
static struct { int ids[10]; float x[10], y[10]; int n; } T;      /* the pointers down, as the GL thread last told the engine */

static void enqueue(event e)
{
    pthread_mutex_lock(&q_lock);
    if (q_n < 256) q[q_n++] = e;
    pthread_mutex_unlock(&q_lock);
}

void tl_godot_touch(int phase, int id, float x, float y) { enqueue((event){ .kind = EV_TOUCH, .phase = phase, .id = id, .x = x, .y = y }); }
void tl_godot_key(int keycode, bool down) { enqueue((event){ .kind = EV_KEY, .key = keycode, .down = down }); }

/*
 * GodotLib.dispatchTouchEvent(event, actionPointerId, pointerCount, positions, doubleTap): an Android MotionEvent's
 * action, and every pointer still down as (id, x, y) -- Godot 4 adds pressure and tilt, six floats a pointer.
 */
static void send_touch(const event *e)
{
    typedef void (*touch_fn)(void *env, void *cls, int ev, int pointer, int count, void *pos, uint8_t dbl);
    static touch_fn fn;
    if (!fn) fn = (touch_fn)native_of("dispatchTouchEvent", "(III[FZ)V");
    if (!fn) return;
    int idx = -1;
    for (int i = 0; i < T.n; i++) if (T.ids[i] == e->id) idx = i;
    int action;
    if (e->phase == 0) {
        if (idx < 0 && T.n < 10) { idx = T.n++; T.ids[idx] = e->id; }
        if (idx < 0) return;
        T.x[idx] = e->x; T.y[idx] = e->y;
        action = T.n == 1 ? 0 /* ACTION_DOWN */ : 5 /* ACTION_POINTER_DOWN */;
    } else if (e->phase == 1) {
        if (idx < 0) return;
        T.x[idx] = e->x; T.y[idx] = e->y;
        action = 2;                                                     /* ACTION_MOVE */
    } else {
        if (idx < 0) return;
        T.x[idx] = e->x; T.y[idx] = e->y;
        action = e->phase == 3 ? 3 : T.n == 1 ? 1 : 6;                  /* CANCEL, UP, POINTER_UP */
    }
    int per = U.major >= 4 ? 6 : 3;
    jobj *pos = tl_jni_new_prim_array('F', (uint32_t)(T.n * per));
    float *p = (float *)pos->arr.data;
    for (int i = 0; i < T.n; i++) {
        p[i * per] = (float)T.ids[i]; p[i * per + 1] = T.x[i]; p[i * per + 2] = T.y[i];
        if (per == 6) { p[i * per + 3] = 1.f; p[i * per + 4] = 0.f; p[i * per + 5] = 0.f; }
    }
    fn(tl_jni_env(), tl_jni_class_object(LIB), action, e->id, T.n, pos, 0);
    tl_jni_unref(pos);
    if (e->phase >= 2) {
        if (e->phase == 3) T.n = 0;
        else { for (int i = idx; i + 1 < T.n; i++) { T.ids[i] = T.ids[i + 1]; T.x[i] = T.x[i + 1]; T.y[i] = T.y[i + 1]; } T.n--; }
    }
}

/* GodotLib.key(keycode, scancode, unicode, pressed) in Godot 3; Godot 4 has key(physical, unicode, keyLabel, pressed, echo). */
static void send_key(const event *e)
{
    if (U.major >= 4) {
        typedef void (*key4_fn)(void *env, void *cls, int phys, int uni, int label, uint8_t pressed, uint8_t echo);
        static key4_fn fn;
        if (!fn) fn = (key4_fn)native_of("key", "(IIIZZ)V");
        if (fn) fn(tl_jni_env(), tl_jni_class_object(LIB), e->key, 0, e->key, e->down, 0);
    } else {
        typedef void (*key3_fn)(void *env, void *cls, int code, int scan, int uni, uint8_t pressed);
        static key3_fn fn;
        if (!fn) fn = (key3_fn)native_of("key", "(IIIZ)V");
        if (fn) fn(tl_jni_env(), tl_jni_class_object(LIB), e->key, e->key, 0, e->down);
    }
}

static void drain_events(void)
{
    event local[256]; int n;
    pthread_mutex_lock(&q_lock);
    n = q_n; memcpy(local, q, sizeof(event) * (size_t)n); q_n = 0;
    pthread_mutex_unlock(&q_lock);
    for (int i = 0; i < n; i++) {
        if (local[i].kind == EV_TOUCH) send_touch(&local[i]); else send_key(&local[i]);
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
    if (!getDisplay || !createWindowSurface || !swapBuffers) { tl_log_line("godot: EGL is not available"); atomic_store(&U.ended, true); return NULL; }

    EGLDisplay dpy = getDisplay(NULL);
    EGLint major = 0, minor = 0;
    initialize(dpy, &major, &minor);
    bindAPI(0x30A0 /* EGL_OPENGL_ES_API */);
    /* Godot's GLSurfaceView asks for 8888 with a 24-bit depth buffer and stencil, ES 3 (Godot 3's GLES3 renderer and Godot 4's
     * Compatibility renderer), and falls back to ES 2 for Godot 3's GLES2 one. */
    const EGLint want[] = { 0x3040, 0x40 /* ES3 */, 0x3033, 0x4, 0x3024, 8, 0x3023, 8, 0x3022, 8, 0x3021, 8,
                            0x3025, 24, 0x3026, 8, EGL_NONE };
    EGLConfig cfg = NULL; EGLint ncfg = 0;
    if (!chooseConfig(dpy, want, &cfg, 1, &ncfg) || ncfg < 1) { tl_log_line("godot: eglChooseConfig found nothing (%#x)", getError()); atomic_store(&U.ended, true); return NULL; }
    const EGLint ctx_attr[] = { 0x3098 /* CONTEXT_CLIENT_VERSION */, 3, EGL_NONE };
    EGLContext ctx = createContext(dpy, cfg, NULL, ctx_attr);
    EGLSurface surf = createWindowSurface(dpy, cfg, tl_nwindow_get(), NULL);
    if (!ctx || !surf || !makeCurrent(dpy, surf, surf, ctx)) { tl_log_line("godot: no GL context (%#x)", getError()); atomic_store(&U.ended, true); return NULL; }
    if (swapInterval) swapInterval(dpy, 1);
    tl_log_line("godot: EGL context ready (EGL %d.%d)", major, minor);

    void *env = tl_jni_env(), *cls = tl_jni_class_object(LIB);
    typedef void (*void_fn)(void *env, void *cls);
    typedef void (*wh_fn)(void *env, void *cls, int w, int h);
    typedef uint8_t (*step_fn)(void *env, void *cls);
    /* GodotRenderer.onSurfaceCreated -> newcontext(); onSurfaceChanged -> resize(w, h); onDrawFrame -> step(). Godot 4 passes the
     * Surface to newcontext and resize; with GL the engine does not use it. */
    if (U.major >= 4) {
        typedef void (*nc4_fn)(void *env, void *cls, void *surface);
        nc4_fn nc = (nc4_fn)native_of("newcontext", "(Landroid/view/Surface;)V");
        if (nc) nc(env, cls, tl_jni_new_object(tl_jni_class("android/view/Surface")));
        typedef void (*rs4_fn)(void *env, void *cls, void *surface, int w, int h);
        rs4_fn rs = (rs4_fn)native_of("resize", "(Landroid/view/Surface;II)V");
        if (rs) rs(env, cls, tl_jni_new_object(tl_jni_class("android/view/Surface")), U.cfg.width, U.cfg.height);
    } else {
        void_fn nc = (void_fn)native_of("newcontext", "()V");
        if (nc) nc(env, cls);
        wh_fn rs = (wh_fn)native_of("resize", "(II)V");
        if (rs) rs(env, cls, U.cfg.width, U.cfg.height);
    }
    if (tl_jni_pending()) tl_jni_clear();
    void_fn focusin = (void_fn)native_of("focusin", "()V");
    if (focusin) focusin(env, cls);
    step_fn step = (step_fn)native_of("step", "()Z");
    void_fn paused_fn = (void_fn)native_of("onRendererPaused", "()V"), resumed_fn = (void_fn)native_of("onRendererResumed", "()V");
    if (!step) { atomic_store(&U.ended, true); return NULL; }
    tl_log_line("godot: rendering at %dx%d", U.cfg.width, U.cfg.height);

    bool was_paused = false;
    int64_t next = now_ns();
    while (!atomic_load(&U.stop)) {
        if (atomic_load(&U.paused)) {
            if (!was_paused) { if (paused_fn) paused_fn(env, cls); was_paused = true; }
            usleep(20000); next = now_ns(); continue;
        }
        if (was_paused) { if (resumed_fn) resumed_fn(env, cls); was_paused = false; next = now_ns(); }
        drain_events();
        int64_t t0 = now_ns();
        /* step() says whether the frame is ready to show (false while loading, or when nothing changed in low-processor mode). */
        if (step(env, cls)) swapBuffers(dpy, surf);
        int64_t t1 = now_ns();
        if (tl_jni_pending()) tl_jni_clear();
        atomic_fetch_add(&U.perf_ns, (unsigned long long)(t1 - t0));
        atomic_fetch_add(&U.perf_frames, 1);
        unsigned long long m = atomic_load(&U.perf_max_ns), ns = (unsigned long long)(t1 - t0);
        while (ns > m && !atomic_compare_exchange_weak(&U.perf_max_ns, &m, ns)) {}
        atomic_fetch_add(&U.frames, 1);
        if (atomic_load(&U.ended)) break;
        next += 1000000000ll / 60;
        int64_t late = now_ns() - next;
        if (late > 100000000ll) next = now_ns();
        else if (late < 0) { struct timespec ts = { 0, (long)(-late) }; nanosleep(&ts, NULL); }
    }
    return NULL;
}

bool tl_godot_run(void)
{
    pthread_attr_t a;
    pthread_attr_init(&a);
    pthread_attr_setstacksize(&a, 16u << 20);
    if (pthread_create(&U.thread, &a, gl_main, NULL) != 0) return false;
    U.thread_started = true;
    return true;
}

void tl_godot_perf_snapshot(tl_godot_perf *out)
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

/* Godot.forceQuit (the game called get_tree().quit()). */
void tl_godot_quit(void) { tl_log_line("godot: the game asked to quit"); atomic_store(&U.ended, true); }

void tl_godot_set_paused(bool paused) { atomic_store(&U.paused, paused); }
unsigned long tl_godot_frames(void) { return atomic_load(&U.frames); }
bool tl_godot_ended(void) { return atomic_load(&U.ended); }
void tl_godot_stop(void)
{
    atomic_store(&U.stop, true);
    if (U.thread_started) { pthread_join(U.thread, NULL); U.thread_started = false; }
    atomic_store(&U.stop, false);
}
