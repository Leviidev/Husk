/* SPDX-License-Identifier: GPL-2.0-or-later */
/* GameMaker games: YoYo's runner (libyoyo.so) driven from C. See husk-tl-gamemaker.h. */
#define _DARWIN_C_SOURCE
#include "husk-tl-gamemaker.h"

#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
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
void tl_audiotrack_hle_install(void);

#define LIB "com/yoyogames/runner/RunnerJNILib"

static struct {
    tl_gm_config cfg;
    char apk[1024], data[512], save[600], pkg[128], frame_dir[512], angle_egl[600], angle_gles[600];
    atomic_ulong frames;
    atomic_bool stop, paused, ended;
    atomic_ullong perf_ns, perf_max_ns;
    atomic_ulong perf_frames;
    struct timespec perf_since;
    pthread_t thread;
    bool thread_started;
} G;

static jvalue vl(void *p) { jvalue v; v.j = 0; v.l = p; return v; }
static jvalue vi(int i) { jvalue v; v.j = 0; v.i = i; return v; }
static jvalue vz(int z) { jvalue v; v.j = 0; v.z = z != 0; return v; }
static jvalue vd(double d) { jvalue v; v.d = d; return v; }
static jobj *STR(const char *s) { return tl_jni_new_string(s); }
static const char *S(jobj *o) { const char *s = tl_jni_string(o); return s ? s : ""; }

/* A RunnerJNILib native: the runner exports them under their JNI names. */
static void *native_of(const char *name)
{
    char mangled[200];
    snprintf(mangled, sizeof(mangled), "Java_com_yoyogames_runner_RunnerJNILib_%s", name);
    tl_lib *lib = tl_ld_find_lib("libyoyo.so");
    void *fn = lib ? tl_ld_sym(lib, mangled) : NULL;
    if (!fn) tl_log_line("gamemaker: RunnerJNILib.%s is not in the runner", name);
    return fn;
}

/* ---------------------------------------------------------------- the game's options */

/* assets/options.ini, where the export keeps its settings ([Android] OrientLandscape=1, ...). */
static char *read_options(const char *apk)
{
    tl_zip z; char err[160];
    if (!tl_zip_open(&z, apk, err, sizeof(err))) return NULL;
    char *out = NULL;
    const tl_zip_entry *e = tl_zip_find(&z, "assets/options.ini");
    const uint8_t *d; size_t len; bool owned;
    if (e && tl_zip_data(&z, e, 1 << 20, &d, &len, &owned, err, sizeof(err))) {
        out = malloc(len + 1); memcpy(out, d, len); out[len] = 0;
        if (owned) free((void *)d);
    }
    tl_zip_close(&z);
    return out;
}

static int option_int(const char *ini, const char *key, int dflt)
{
    if (!ini) return dflt;
    size_t n = strlen(key);
    for (const char *p = ini; (p = strstr(p, key)); p += n) {
        if ((p == ini || p[-1] == '\n') && p[n] == '=') return atoi(p + n + 1);
    }
    return dflt;
}

int tl_gm_orientation(const char *apk_path)
{
    char *ini = read_options(apk_path);
    int land = option_int(ini, "OrientLandscape", 0) || option_int(ini, "OrientLandscapeFlipped", 0);
    int port = option_int(ini, "OrientPortrait", 0) || option_int(ini, "OrientPortraitFlipped", 0);
    free(ini);
    return port && !land ? 1 : land && !port ? 0 : -1;
}

bool tl_gm_is_game(const char *apk_path)
{
    tl_zip z; char err[160];
    if (!tl_zip_open(&z, apk_path, err, sizeof(err))) return false;
    bool yes = tl_zip_find(&z, "lib/arm64-v8a/libyoyo.so") != NULL;
    tl_zip_close(&z);
    return yes;
}

/* ---------------------------------------------------------------- RunnerJNILib's Java, in C */

static void Noop(tl_jcall *c) { (void)c; }
static void RetZero(tl_jcall *c) { c->ret = vi(0); }
static void RetOne(tl_jcall *c) { c->ret = vi(1); }
static void RetFalse(tl_jcall *c) { c->ret = vz(0); }
static void RetNull(tl_jcall *c) { c->ret = vl(NULL); }
static void RetMinusOne(tl_jcall *c) { c->ret = vi(-1); }

static void J_GetAssetManager(tl_jcall *c) { c->ret = vl(tl_hle_assets()); }
static void J_GetApplicationContext(tl_jcall *c) { c->ret = vl(tl_hle_activity()); }
static void J_GetUDID(tl_jcall *c) { c->ret = vl(STR("8f3a5c1d2e4b6a70")); }
static void J_ShowMessage(tl_jcall *c) { tl_log_line("gamemaker: message: %s", S(c->args[0].l)); }
static void J_OpenURL(tl_jcall *c) { tl_log_line("gamemaker: open %s", S(c->args[0].l)); }
static void J_ExitApplication(tl_jcall *c) { (void)c; tl_log_line("gamemaker: the game exited"); atomic_store(&G.ended, true); }
static void J_clipboardGet(tl_jcall *c) { c->ret = vl(STR("")); }
static void J_Certificates(tl_jcall *c) { c->ret = vl(tl_jni_new_obj_array(tl_jni_class("java/nio/ByteBuffer"), 0)); }
static void J_InputString(tl_jcall *c) { c->ret = vl(c->args[1].l ? c->args[1].l : STR("")); }
static void J_GamepadValues(tl_jcall *c) { c->ret = vl(tl_jni_new_prim_array('F', 0)); }
static void J_GamepadDescription(tl_jcall *c) { c->ret = vl(STR("")); }
static void J_VideoZero(tl_jcall *c) { c->ret = vd(0); }

/* The device, as a ds_map the runner reads os_version, os_device and the like from: made by the runner's own CreateVersionDSMap. */
static void J_OsGetInfo(tl_jcall *c)
{
    typedef int (*mk_fn)(void *env, void *cls, int sdk, void *release, void *model, void *device, void *manufacturer, void *abi,
                         void *abi2, void *bootloader, void *board, void *osver, void *region, void *version, uint8_t keyboard);
    mk_fn mk = (mk_fn)native_of("CreateVersionDSMap");
    c->ret = vi(mk ? mk(tl_jni_env(), tl_jni_class_object(LIB), 34, STR("14"), STR("Pixel 8"), STR("shiba"), STR("Google"),
                        STR("arm64-v8a"), STR(""), STR("unknown"), STR("shiba"), STR("6.1"), STR("US"), STR("1.0"), 0) : -1);
}

/* Asynchronous HTTP: the runner waits for HttpResult; it gets a failure (status 404, no body) so the game moves on. */
static void http_fail(int id)
{
    typedef void (*res_fn)(void *env, void *cls, void *data, int status, int id, void *url, void *headers);
    res_fn res = (res_fn)native_of("HttpResult");
    if (res) res(tl_jni_env(), tl_jni_class_object(LIB), NULL, 404, id, STR(""), STR(""));
}
static void J_HttpGet(tl_jcall *c) { tl_log_line("gamemaker: http get %s (offline)", S(c->args[0].l)); http_fail(c->args[2].i); }
static void J_HttpPost(tl_jcall *c) { tl_log_line("gamemaker: http post %s (offline)", S(c->args[0].l)); http_fail(c->args[3].i); }
static void J_HttpRequest(tl_jcall *c) { tl_log_line("gamemaker: http %s %s (offline)", S(c->args[1].l), S(c->args[0].l)); http_fail(c->args[5].i); }

static const tl_jhle k_runner[] = {
    { LIB, "GetAssetManager", "()Ljava/lang/Object;", J_GetAssetManager },
    { LIB, "GetApplicationContext", "()Landroid/content/Context;", J_GetApplicationContext },
    { LIB, "GetUDID", "()Ljava/lang/String;", J_GetUDID },
    { LIB, "UsingGL2", "()I", RetOne },
    { LIB, "GetDefaultFrameBuffer", "()I", RetZero },
    { LIB, "GLSupportsASTC", "()I", RetZero },
    { LIB, "HasVsyncHandler", "()I", RetZero },
    { LIB, "WaitForVsync", "()V", Noop },
    { LIB, "OsGetInfo", "()I", J_OsGetInfo },
    { LIB, "CheckPermission", "(Ljava/lang/String;)I", RetOne },
    { LIB, "RequestPermission", "(Ljava/lang/String;)V", Noop },
    { LIB, "isNetworkConnected", "()Z", RetFalse },
    { LIB, "ShowMessage", "(Ljava/lang/String;)V", J_ShowMessage },
    { LIB, "ShowMessageAsync", "(Ljava/lang/String;I)V", J_ShowMessage },
    { LIB, "ShowQuestion", "(Ljava/lang/String;)I", RetOne },
    { LIB, "ShowQuestionAsync", "(Ljava/lang/String;I)V", Noop },
    { LIB, "InputString", "(Ljava/lang/String;Ljava/lang/String;)Ljava/lang/String;", J_InputString },
    { LIB, "InputStringAsync", "(Ljava/lang/String;Ljava/lang/String;I)V", Noop },
    { LIB, "OpenURL", "(Ljava/lang/String;)V", J_OpenURL },
    { LIB, "ExitApplication", "()V", J_ExitApplication },
    { LIB, "MoveTaskToBack", "()V", Noop },
    { LIB, "SetThreadPriority", "(I)V", Noop },
    { LIB, "powersaveEnable", "(Z)V", Noop },
    { LIB, "setSystemUIVisibilityFlags", "(I)V", Noop },
    { LIB, "RestrictOrientation", "(ZZZZZ)V", Noop },
    { LIB, "DynamicAssetExists", "(Ljava/lang/String;)I", RetZero },
    { LIB, "CallExtensionFunction", "(Ljava/lang/String;Ljava/lang/String;I[D[Ljava/lang/Object;)Ljava/lang/Object;", RetNull },
    { LIB, "CallExtensionFunction", "(Ljava/lang/String;Ljava/lang/String;I[Ljava/lang/Object;)Ljava/lang/Object;", RetNull },
    { LIB, "EnumerateCertificates", "()[Ljava/nio/ByteBuffer;", J_Certificates },
    { LIB, "clipboardGetText", "()Ljava/lang/String;", J_clipboardGet },
    { LIB, "clipboardHasText", "()Z", RetFalse },
    { LIB, "clipboardSetText", "(Ljava/lang/String;)V", Noop },
    { LIB, "VirtualKeyboardToggle", "(ZIIIZ[I)V", Noop },
    { LIB, "VirtualKeyboardGetStatus", "()Z", RetFalse },
    { LIB, "VirtualKeyboardGetHeight", "()I", RetZero },
    { LIB, "OnKeyboardStringSet", "([I)V", Noop },
    { LIB, "AcquireMulticastLock", "()V", Noop },
    { LIB, "ReleaseMulticastLock", "()V", Noop },
    { LIB, "ClearGamepads", "()V", Noop },
    { LIB, "EnumerateGamepadDevices", "()V", Noop },
    { LIB, "GamepadsCount", "()I", RetZero },
    { LIB, "GamepadConnected", "(I)Z", RetFalse },
    { LIB, "GamepadDescription", "(I)Ljava/lang/String;", J_GamepadDescription },
    { LIB, "GamepadAxesValues", "(I)[F", J_GamepadValues },
    { LIB, "GamepadButtonValues", "(I)[F", J_GamepadValues },
    { LIB, "GamepadGMLMapping", "(II)I", RetMinusOne },
    { LIB, "HttpGet", "(Ljava/lang/String;II)V", J_HttpGet },
    { LIB, "HttpPost", "(Ljava/lang/String;Ljava/lang/String;II)V", J_HttpPost },
    { LIB, "HttpRequest", "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;[BII)V", J_HttpRequest },
    { LIB, "cloudStringSave", "(Ljava/lang/String;Ljava/lang/String;I)V", Noop },
    { LIB, "cloudSynchronise", "(I)V", Noop },
    { LIB, "LeaveRating", "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V", Noop },
    { LIB, "ShowLogin", "(Ljava/lang/String;Ljava/lang/String;I)V", Noop },
    { LIB, "PushLocalNotification", "(FLjava/lang/String;Ljava/lang/String;Ljava/lang/String;)V", Noop },
    { LIB, "PushGetLocalNotification", "(II)I", RetMinusOne },
    { LIB, "PushCancelLocalNotification", "(I)I", RetZero },
    { LIB, "DumpUsedMemory", "()V", Noop },
    { LIB, "VideoOpen", "(Ljava/lang/String;)V", Noop },
    { LIB, "VideoClose", "()V", Noop },
    { LIB, "VideoDraw", "(Ljava/nio/ByteBuffer;)Z", RetFalse },
    { LIB, "VideoPause", "()V", Noop },
    { LIB, "VideoResume", "()V", Noop },
    { LIB, "VideoStatus", "()D", J_VideoZero },
    { LIB, "VideoGetStatus", "()D", J_VideoZero },
    { LIB, "VideoGetDuration", "()D", J_VideoZero },
    { LIB, "VideoGetPosition", "()D", J_VideoZero },
    { LIB, "VideoGetVolume", "()D", J_VideoZero },
    { LIB, "VideoGetFormat", "()D", J_VideoZero },
    { LIB, "VideoIsLooping", "()D", J_VideoZero },
    { LIB, "VideoW", "()D", J_VideoZero },
    { LIB, "VideoH", "()D", J_VideoZero },
    { LIB, "VideoSeekTo", "(D)V", Noop },
    { LIB, "VideoSetVolume", "(D)V", Noop },
    { LIB, "VideoEnableLoop", "(D)V", Noop },
    { NULL, NULL, NULL, NULL },
};

/* ---------------------------------------------------------------- startup */

bool tl_gm_start(const tl_gm_config *cfg)
{
    G.cfg = *cfg;
    snprintf(G.apk, sizeof(G.apk), "%s", cfg->apk_path);
    snprintf(G.data, sizeof(G.data), "%s", cfg->data_dir);
    snprintf(G.pkg, sizeof(G.pkg), "%s", cfg->package_name);
    G.cfg.apk_path = G.apk; G.cfg.data_dir = G.data; G.cfg.package_name = G.pkg;
    if (cfg->frame_dir) { snprintf(G.frame_dir, sizeof(G.frame_dir), "%s", cfg->frame_dir); G.cfg.frame_dir = G.frame_dir; }
    if (cfg->angle_egl) { snprintf(G.angle_egl, sizeof(G.angle_egl), "%s", cfg->angle_egl); G.cfg.angle_egl = G.angle_egl; }
    if (cfg->angle_gles) { snprintf(G.angle_gles, sizeof(G.angle_gles), "%s", cfg->angle_gles); G.cfg.angle_gles = G.angle_gles; }
    /* Context.getFilesDir() + "/": where the runner keeps saves and its working files. */
    snprintf(G.save, sizeof(G.save), "%s/files/", cfg->data_dir);
    mkdir(cfg->data_dir, 0755);
    char files[620]; snprintf(files, sizeof(files), "%s/files", cfg->data_dir); mkdir(files, 0755);

    tl_set_data_dir(cfg->data_dir);
    tl_nwindow_configure(cfg->width, cfg->height, cfg->metal_layer);
    int n = tl_dexidx_open(cfg->apk_path);
    tl_log_line("gamemaker: %d classes in the APK's DEX", n);
    if (!tl_ld_add_apk(cfg->apk_path)) return false;
    if (cfg->angle_egl && !tl_egl_init(cfg->angle_egl, cfg->angle_gles, cfg->frame_dir, cfg->frame_every)) return false;
    tl_jni_init();
    tl_hle_configure(cfg->package_name, cfg->apk_path, cfg->data_dir, cfg->width, cfg->height);
    tl_jni_hle_install();
    tl_jni_register_hle(k_runner);
    tl_audiotrack_hle_install();
    /* What RunnerJNILib.Init and the activity leave in static fields the runner reads. */
    tl_jni_set_static(LIB, "ms_context", "Landroid/content/Context;", vl(tl_hle_activity()));
    tl_jni_set_static(LIB, "mCurrentRefreshRate", "F", (jvalue){ .f = 60.0f });
    tl_jni_set_static(LIB, "mMaxRefreshRateSupported", "I", vi(60));
    tl_jni_set_static(LIB, "mGameSpeedControl", "I", vi(0));

    jvalue a = vl(STR("yoyo"));
    tl_jni_call(tl_jni_class_object("java/lang/System"), "loadLibrary", "(Ljava/lang/String;)V", &a);
    if (tl_jni_pending()) { tl_log_line("gamemaker: loading libyoyo.so failed"); return false; }
    return tl_ld_find_lib("libyoyo.so") != NULL;
}

/* ---------------------------------------------------------------- input */

typedef struct { int kind, phase, id, key; bool down; float x, y; } event;
enum { EV_TOUCH, EV_KEY };
static pthread_mutex_t q_lock = PTHREAD_MUTEX_INITIALIZER;
static event q[256];
static int q_n;
static int g_down;                                  /* pointers down, as the GL thread last told the runner */

static void enqueue(event e)
{
    pthread_mutex_lock(&q_lock);
    if (q_n < 256) q[q_n++] = e;
    pthread_mutex_unlock(&q_lock);
}

void tl_gm_touch(int phase, int id, float x, float y) { enqueue((event){ .kind = EV_TOUCH, .phase = phase, .id = id, .x = x, .y = y }); }
void tl_gm_key(int keycode, bool down) { enqueue((event){ .kind = EV_KEY, .key = keycode, .down = down }); }

/* RunnerJNILib.TouchEvent(action, pointerId, x, y, pointerCount), as DemoGLSurfaceView sends a MotionEvent's changed pointer. */
static void send_touch(const event *e)
{
    typedef void (*touch_fn)(void *env, void *cls, int action, int id, float x, float y, int count);
    static touch_fn fn;
    if (!fn) fn = (touch_fn)native_of("TouchEvent");
    if (!fn) return;
    int action;
    if (e->phase == 0) { g_down++; action = g_down == 1 ? 0 : 5; }                         /* DOWN, POINTER_DOWN */
    else if (e->phase == 1) action = 2;                                                    /* MOVE */
    else { action = e->phase == 3 ? 3 : g_down <= 1 ? 1 : 6; }                             /* CANCEL, UP, POINTER_UP */
    fn(tl_jni_env(), tl_jni_class_object(LIB), action, e->id, e->x, e->y, g_down > 0 ? g_down : 1);
    if (e->phase >= 2) { g_down = e->phase == 3 ? 0 : g_down - 1; if (g_down < 0) g_down = 0; }
}

/* RunnerJNILib.KeyEvent(type 0 down / 1 up, keycode, unicode, source, repeat). */
static void send_key(const event *e)
{
    typedef void (*key_fn)(void *env, void *cls, int type, int code, int uni, int source, int repeat);
    static key_fn fn;
    if (!fn) fn = (key_fn)native_of("KeyEvent");
    if (fn) fn(tl_jni_env(), tl_jni_class_object(LIB), e->down ? 0 : 1, e->key, 0, 0x101 /* SOURCE_KEYBOARD */, 0);
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
    if (!getDisplay || !createWindowSurface || !swapBuffers) { tl_log_line("gamemaker: EGL is not available"); atomic_store(&G.ended, true); return NULL; }

    EGLDisplay dpy = getDisplay(NULL);
    EGLint major = 0, minor = 0;
    initialize(dpy, &major, &minor);
    bindAPI(0x30A0 /* EGL_OPENGL_ES_API */);
    /* DemoGLSurfaceView's chooser: 8888 with depth and stencil, ES 2. */
    const EGLint want[] = { 0x3040, 0x4 /* ES2 */, 0x3033, 0x4, 0x3024, 8, 0x3023, 8, 0x3022, 8, 0x3021, 8,
                            0x3025, 16, 0x3026, 8, EGL_NONE };
    EGLConfig cfg = NULL; EGLint ncfg = 0;
    if (!chooseConfig(dpy, want, &cfg, 1, &ncfg) || ncfg < 1) { tl_log_line("gamemaker: eglChooseConfig found nothing (%#x)", getError()); atomic_store(&G.ended, true); return NULL; }
    const EGLint ctx_attr[] = { 0x3098 /* CONTEXT_CLIENT_VERSION */, 2, EGL_NONE };
    EGLContext ctx = createContext(dpy, cfg, NULL, ctx_attr);
    EGLSurface surf = createWindowSurface(dpy, cfg, tl_nwindow_get(), NULL);
    if (!ctx || !surf || !makeCurrent(dpy, surf, surf, ctx)) { tl_log_line("gamemaker: no GL context (%#x)", getError()); atomic_store(&G.ended, true); return NULL; }
    if (swapInterval) swapInterval(dpy, 1);
    tl_log_line("gamemaker: EGL context ready (EGL %d.%d)", major, minor);

    void *env = tl_jni_env(), *cls = tl_jni_class_object(LIB);
    /* DemoGLSurfaceView: initGLFuncs(1) for the ES 2 renderer. */
    typedef int (*init_gl_fn)(void *env, void *cls, int gl2);
    init_gl_fn init_gl = (init_gl_fn)native_of("initGLFuncs");
    if (init_gl) tl_log_line("gamemaker: initGLFuncs -> %d", init_gl(env, cls, 1));

    /* DemoRenderer's DoStartup: Startup(apk, saveDir, package, sleepMargin, dynamicAssets), then the display's rate. */
    typedef void (*startup_fn)(void *env, void *cls, void *apk, void *save, void *pkg, int sleep_margin, uint8_t dynamic);
    startup_fn startup = (startup_fn)native_of("Startup");
    if (!startup) { atomic_store(&G.ended, true); return NULL; }
    char *ini = read_options(G.apk);
    int sleep_margin = option_int(ini, "SleepMargin", 0);
    free(ini);
    startup(env, cls, STR(G.apk), STR(G.save), STR(G.pkg), sleep_margin, 0);
    if (tl_jni_pending()) { tl_log_line("gamemaker: Startup left an exception"); tl_jni_clear(); }
    typedef void (*void_fn)(void *env, void *cls);
    void_fn freq = (void_fn)native_of("OnDisplayFrequencyChanged");
    if (freq) freq(env, cls);
    tl_log_line("gamemaker: started; rendering at %dx%d", G.cfg.width, G.cfg.height);

    typedef int (*process_fn)(void *env, void *cls, int w, int h, float ax, float ay, float az, int keypad, int orientation, float refresh);
    typedef uint8_t (*flip_fn)(void *env, void *cls);
    typedef void (*int_fn)(void *env, void *cls, int v);
    process_fn process = (process_fn)native_of("Process");
    flip_fn can_flip = (flip_fn)native_of("canFlip");
    int_fn pause_fn = (int_fn)native_of("Pause"), resume_fn = (int_fn)native_of("Resume");
    if (!process) { atomic_store(&G.ended, true); return NULL; }
    int orientation = G.cfg.width > G.cfg.height ? 0 : 1;       /* RunnerActivity.Orientation: landscape 0, portrait 1 */

    bool was_paused = false;
    int64_t next = now_ns();
    while (!atomic_load(&G.stop)) {
        if (atomic_load(&G.paused)) {
            if (!was_paused) { if (pause_fn) pause_fn(env, cls, 0); was_paused = true; }
            usleep(20000); next = now_ns(); continue;
        }
        if (was_paused) { if (resume_fn) resume_fn(env, cls, 0); was_paused = false; next = now_ns(); }
        drain_events();
        int64_t t0 = now_ns();
        /* One displayed frame: Process until the runner says it has something to show. */
        int ret = 1;
        for (int tries = 0; tries < 8; tries++) {
            ret = process(env, cls, G.cfg.width, G.cfg.height, 0.f, 0.f, -1.f, 0, orientation, 60.f);
            if (tl_jni_pending()) tl_jni_clear();
            if (ret == 0 || !can_flip || can_flip(env, cls)) break;
        }
        if (ret == 0) { tl_log_line("gamemaker: Process returned 0: the game ended"); atomic_store(&G.ended, true); break; }
        swapBuffers(dpy, surf);
        int64_t t1 = now_ns();
        atomic_fetch_add(&G.perf_ns, (unsigned long long)(t1 - t0));
        atomic_fetch_add(&G.perf_frames, 1);
        unsigned long long m = atomic_load(&G.perf_max_ns), ns = (unsigned long long)(t1 - t0);
        while (ns > m && !atomic_compare_exchange_weak(&G.perf_max_ns, &m, ns)) {}
        atomic_fetch_add(&G.frames, 1);
        if (atomic_load(&G.ended)) break;
        next += 1000000000ll / 60;
        int64_t late = now_ns() - next;
        if (late > 100000000ll) next = now_ns();
        else if (late < 0) { struct timespec ts = { 0, (long)(-late) }; nanosleep(&ts, NULL); }
    }
    return NULL;
}

bool tl_gm_run(void)
{
    pthread_attr_t a;
    pthread_attr_init(&a);
    pthread_attr_setstacksize(&a, 16u << 20);
    if (pthread_create(&G.thread, &a, gl_main, NULL) != 0) return false;
    G.thread_started = true;
    return true;
}

void tl_gm_perf_snapshot(tl_gm_perf *out)
{
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    unsigned long long ns = atomic_exchange(&G.perf_ns, 0), mx = atomic_exchange(&G.perf_max_ns, 0);
    unsigned long n = atomic_exchange(&G.perf_frames, 0);
    double elapsed = G.perf_since.tv_sec ? (now.tv_sec - G.perf_since.tv_sec) + (now.tv_nsec - G.perf_since.tv_nsec) / 1e9 : 0;
    G.perf_since = now;
    out->fps = elapsed > 0.05 ? (double)n / elapsed : 0;
    out->mean_ms = n ? (double)ns / n / 1e6 : 0;
    out->max_ms = (double)mx / 1e6;
}

void tl_gm_set_paused(bool paused) { atomic_store(&G.paused, paused); }
unsigned long tl_gm_frames(void) { return atomic_load(&G.frames); }
bool tl_gm_ended(void) { return atomic_load(&G.ended); }
