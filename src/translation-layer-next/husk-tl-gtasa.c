/* SPDX-License-Identifier: GPL-2.0-or-later */
#define _DARWIN_C_SOURCE
#include "husk-tl-gtasa.h"

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
void tl_hle_set_activity(jobj *a);
jobj *tl_hle_assets(void);
void tl_set_data_dir(const char *dir);
void tl_nwindow_configure(int w, int h, void *layer);

#define OSW "com/rockstargames/oswrapper/"
#define CLS_BASE OSW "GameActivityBase"
#define CLS_ACT "com/rockstargames/gtasa/GameActivity"
#define CLS_SVC OSW "GamePlatformServices"
#define CLS_VIEW OSW "GameView"
#define CLS_DEV OSW "DeviceInfo"
#define CLS_NATIVE OSW "GameNative"

static struct {
    tl_ga_config cfg;
    char apk[1024], data[512], pkg[128], frame_dir[512], angle_egl[600], angle_gles[600];
    jobj *activity, *view, *services, *surface, *device;
    pthread_t thread;
    bool started;
    atomic_bool paused, running;
} G;

static jvalue vl(void *p) { jvalue v; v.j = 0; v.l = p; return v; }
static jvalue vi(int i) { jvalue v; v.j = 0; v.i = i; return v; }
static jvalue vz(int z) { jvalue v; v.j = 0; v.z = z != 0; return v; }
static const char *Str(const jobj *o) { const char *s = tl_jni_string(o); return s ? s : ""; }

/* ------------------------------------------------------- what the game thread is asked to do */

/*
 * The wrapper runs every call into the library on one thread and queues what other threads want done (a touch, a controller, an answer from the Rockstar service);
 * between frames that thread takes the queue. Same here.
 */
enum { EV_TOUCH, EV_PAD_KEY, EV_PAD_AXES, EV_PAD_CONNECT, EV_PAD_DISCONNECT, EV_PAUSE, EV_RESUME, EV_GATE, EV_SIGNIN, EV_SIGNOUT, EV_INITIAL, EV_CLOUD, EV_DELETION, EV_ID,
       EV_HTTP_ERROR, EV_PLAYLIST };
typedef struct ev { int kind, a, b; float f[6]; struct ev *next; } ev;
static struct { pthread_mutex_t mu; ev *head, *tail; } Q = { .mu = PTHREAD_MUTEX_INITIALIZER };

static void post(int kind, int a, int b, const float *f)
{
    ev *e = calloc(1, sizeof(*e));
    e->kind = kind; e->a = a; e->b = b;
    if (f) memcpy(e->f, f, sizeof(e->f));
    pthread_mutex_lock(&Q.mu);
    if (Q.tail) Q.tail->next = e; else Q.head = e;
    Q.tail = e;
    pthread_mutex_unlock(&Q.mu);
}
static ev *take(void)
{
    pthread_mutex_lock(&Q.mu);
    ev *e = Q.head;
    if (e) { Q.head = e->next; if (!Q.head) Q.tail = NULL; }
    pthread_mutex_unlock(&Q.mu);
    return e;
}

/* ------------------------------------------------------------------ Java side */

/* GamePlatformServices: the questions the library asks of Android. */
static void S_appVersion(tl_jcall *c) { c->ret = vl(tl_jni_new_string(getenv("TL_GTA_VERSION") ? getenv("TL_GTA_VERSION") : "2.11.311")); }
static void S_zero(tl_jcall *c) { c->ret = vi(0); }
static void S_false(tl_jcall *c) { c->ret = vz(0); }
static void S_void(tl_jcall *c) { (void)c; }
static void S_locale(tl_jcall *c) { c->ret = vi(0); }                    /* English */
static void S_log(tl_jcall *c) { (void)c; }
static void S_splashImage(tl_jcall *c) { tl_log_line("gta: splash image %s", Str(c->args[0].l)); }
static void S_splashText(tl_jcall *c) { tl_log_line("gta: splash text \"%s\"", Str(c->args[0].l)); }
static void S_openLink(tl_jcall *c) { tl_log_line("gta: the game opens %s", Str(c->args[0].l)); }
static void S_quit(tl_jcall *c) { (void)c; tl_log_line("gta: the game asks to quit"); }

/* HTTP: the Rockstar services are not reached from here; each request is answered with an error, which the game carries on from. */
static void S_httpGet(tl_jcall *c) { tl_log_line("gta: http GET #%d %s", c->args[0].i, Str(c->args[1].l)); post(EV_HTTP_ERROR, c->args[0].i, 0, NULL); }
static void S_httpHead(tl_jcall *c) { tl_log_line("gta: http HEAD #%d %s", c->args[0].i, Str(c->args[1].l)); post(EV_HTTP_ERROR, c->args[0].i, 0, NULL); }
static void S_httpPost(tl_jcall *c) { tl_log_line("gta: http POST #%d %s", c->args[0].i, Str(c->args[1].l)); post(EV_HTTP_ERROR, c->args[0].i, 0, NULL); }

/* The music playlist (the device\'s own library): opened at once, and empty -- the game waits for the answer before it goes on. */
static void S_playlistOpen(tl_jcall *c) { tl_log_line("gta: playlist %s", Str(c->args[0].l)); post(EV_PLAYLIST, 1, 0, NULL); }

/* The intro movies: each is "over" before it starts (isMoviePlaying is false), which is how the game moves on. */
static void S_playMovie(tl_jcall *c) { tl_log_line("gta: movie %s (skipped)", Str(c->args[0].l)); }

/* Rockstar Social Club: nobody is signed in and the gate is open; each request is answered at once, from the game thread. */
static void S_rsGate(tl_jcall *c) { tl_log_line("gta: Rockstar gate %d", c->args[0].i); post(EV_GATE, c->args[0].i, 1, NULL); }
static void S_rsSignIn(tl_jcall *c) { (void)c; tl_log_line("gta: Rockstar sign-in"); post(EV_SIGNIN, 0, 0, NULL); }
static void S_rsSignOut(tl_jcall *c) { (void)c; post(EV_SIGNOUT, 0, 0, NULL); }
static void S_rsInitial(tl_jcall *c) { (void)c; tl_log_line("gta: Rockstar initial screen"); post(EV_INITIAL, 0, 0, NULL); }
static void S_rsCloud(tl_jcall *c) { (void)c; post(EV_CLOUD, 0, 0, NULL); }
static void S_rsDeletion(tl_jcall *c) { (void)c; post(EV_DELETION, 0, 0, NULL); }
static void S_rsFetchId(tl_jcall *c) { (void)c; post(EV_ID, 0, 0, NULL); }

/*
 * com.nvidia.devtech.NvUtil: the old NVIDIA wrapper Rockstar's port still asks for the few facts only the activity knows, by name. The storage root is the one that matters:
 * the game keeps its saves and settings there.
 */
static jobj *g_nvutil;
static void NV_getInstance(tl_jcall *c)
{
    if (!g_nvutil) g_nvutil = tl_jni_new_object(tl_jni_class("com/nvidia/devtech/NvUtil"));
    c->ret = vl(tl_jni_ref(g_nvutil));
}
static const char *nv_value(const char *key)
{
    static char root[700];
    if (!strcmp(key, "STORAGE_ROOT")) { snprintf(root, sizeof(root), "%s/sdcard/Android/data/%s/files", G.data, G.pkg); return root; }
    return NULL;
}
static void NV_has(tl_jcall *c) { c->ret = vz(nv_value(Str(c->args[0].l)) != NULL); }
static void NV_get(tl_jcall *c) { const char *k = Str(c->args[0].l), *v = nv_value(k); tl_log_line("gta: NvUtil.getAppLocalValue(%s) = %s", k, v ? v : "(none)"); c->ret = vl(v ? tl_jni_new_string(v) : NULL); }
static void NV_set(tl_jcall *c) { tl_log_line("gta: NvUtil.setAppLocalValue(%s, %s)", Str(c->args[0].l), Str(c->args[1].l)); }
static void NV_param(tl_jcall *c) { tl_log_line("gta: NvUtil.getParameter(%s)", Str(c->args[0].l)); c->ret = vl(NULL); }

/* java.lang.Thread.currentThread().setName(...): the library names the threads it starts. */
static jobj *g_thread;
static void T_current(tl_jcall *c)
{
    if (!g_thread) g_thread = tl_jni_new_object(tl_jni_class("java/lang/Thread"));
    c->ret = vl(tl_jni_ref(g_thread));
}

#define M_(c, n, s, f) { c, n, s, f }
static const tl_jhle k_hle[] = {
    M_(CLS_SVC, "getAppVersion", "()Ljava/lang/String;", S_appVersion),
    M_(CLS_SVC, "getDeviceLocale", "()I", S_locale),
    M_(CLS_SVC, "openLink", "(Ljava/lang/String;)V", S_openLink),
    M_(CLS_SVC, "quit", "()V", S_quit),
    M_(CLS_SVC, "hideSplashScreen", "()V", S_log),
    M_(CLS_SVC, "showSplashScreen", "()V", S_log),
    M_(CLS_SVC, "isSplashScreenVisible", "()Z", S_false),
    M_(CLS_SVC, "setSplashImage", "(Ljava/lang/String;)V", S_splashImage),
    M_(CLS_SVC, "setSplashText", "(Ljava/lang/String;)V", S_splashText),
    M_(CLS_SVC, "httpCancel", "(I)V", S_void),
    M_(CLS_SVC, "httpGet", "(ILjava/lang/String;[Ljava/lang/String;[Ljava/lang/String;)V", S_httpGet),
    M_(CLS_SVC, "httpHead", "(ILjava/lang/String;)V", S_httpHead),
    M_(CLS_SVC, "httpPost", "(ILjava/lang/String;[Ljava/lang/String;[Ljava/lang/String;[B)V", S_httpPost),
    M_(CLS_SVC, "isMoviePlaying", "()Z", S_false),
    M_(CLS_SVC, "pauseMovie", "(Z)V", S_void),
    M_(CLS_SVC, "playMovie", "(Ljava/lang/String;Z)V", S_playMovie),
    M_(CLS_SVC, "stopMovie", "()V", S_void),
    M_(CLS_SVC, "setMovieText", "(Ljava/lang/String;)V", S_void),
    M_(CLS_SVC, "setMovieTextScale", "(I)V", S_void),
    M_(CLS_SVC, "playlistCount", "()I", S_zero),
    M_(CLS_SVC, "playlistIsPlaying", "()Z", S_false),
    M_(CLS_SVC, "playlistOpen", "(Ljava/lang/String;)V", S_playlistOpen),
    M_(CLS_SVC, "playlistPause", "()V", S_void),
    M_(CLS_SVC, "playlistPlay", "()V", S_void),
    M_(CLS_SVC, "playlistSetVolume", "(F)V", S_void),
    M_(CLS_SVC, "playlistStop", "()V", S_void),
    M_(CLS_SVC, "rockstarAccountDeletion", "()V", S_rsDeletion),
    M_(CLS_SVC, "rockstarFetchId", "()V", S_rsFetchId),
    M_(CLS_SVC, "rockstarInTrial", "()Z", S_false),
    M_(CLS_SVC, "rockstarRequestReview", "()V", S_void),
    M_(CLS_SVC, "rockstarSetLocalePriority", "(Ljava/lang/String;)V", S_void),
    M_(CLS_SVC, "rockstarShowCloudDisabled", "()V", S_rsCloud),
    M_(CLS_SVC, "rockstarShowGate", "(I)V", S_rsGate),
    M_(CLS_SVC, "rockstarShowInitial", "()V", S_rsInitial),
    M_(CLS_SVC, "rockstarSignIn", "()V", S_rsSignIn),
    M_(CLS_SVC, "rockstarSignOut", "()V", S_rsSignOut),
    M_("com/nvidia/devtech/NvUtil", "getInstance", "()Lcom/nvidia/devtech/NvUtil;", NV_getInstance),
    M_("com/nvidia/devtech/NvUtil", "hasAppLocalValue", "(Ljava/lang/String;)Z", NV_has),
    M_("com/nvidia/devtech/NvUtil", "getAppLocalValue", "(Ljava/lang/String;)Ljava/lang/String;", NV_get),
    M_("com/nvidia/devtech/NvUtil", "setAppLocalValue", "(Ljava/lang/String;Ljava/lang/String;)V", NV_set),
    M_("com/nvidia/devtech/NvUtil", "getParameter", "(Ljava/lang/String;)Ljava/lang/String;", NV_param),
    M_("java/lang/Thread", "currentThread", "()Ljava/lang/Thread;", T_current),
    M_("java/lang/Thread", "setName", "(Ljava/lang/String;)V", S_void),
    { NULL, NULL, NULL, NULL }
};

/* ------------------------------------------------------------------- start */

static void load_library(const char *name)
{
    jvalue a; a.j = 0; a.l = tl_jni_new_string(name);
    tl_jni_call(tl_jni_class_object("java/lang/System"), "loadLibrary", "(Ljava/lang/String;)V", &a);
}

static void *game_native(const char *name)
{
    char mangled[200]; snprintf(mangled, sizeof(mangled), "Java_com_rockstargames_oswrapper_GameNative_%s", name);
    tl_lib *lib = tl_ld_find_lib("libGame.so");
    void *fn = lib ? tl_ld_sym(lib, mangled) : NULL;
    if (!fn) tl_log_line("gta: the game has no native %s", name);
    return fn;
}

static void set_str(jobj *o, const char *f, const char *v) { tl_jni_set_field(o, f, "Ljava/lang/String;", vl(tl_jni_new_string(v))); }
static void set_int(jobj *o, const char *f, int v) { tl_jni_set_field(o, f, "I", vi(v)); }
static void set_bool(jobj *o, const char *f, int v) { tl_jni_set_field(o, f, "Z", vz(v)); }

bool tl_gta_start(const tl_ga_config *cfg)
{
    G.cfg = *cfg;
    snprintf(G.apk, sizeof(G.apk), "%s", cfg->apk_path);
    snprintf(G.data, sizeof(G.data), "%s", cfg->data_dir);
    snprintf(G.pkg, sizeof(G.pkg), "%s", cfg->package_name);
    G.cfg.apk_path = G.apk; G.cfg.data_dir = G.data; G.cfg.package_name = G.pkg;
    if (cfg->frame_dir) { snprintf(G.frame_dir, sizeof(G.frame_dir), "%s", cfg->frame_dir); G.cfg.frame_dir = G.frame_dir; }
    if (cfg->angle_egl) { snprintf(G.angle_egl, sizeof(G.angle_egl), "%s", cfg->angle_egl); G.cfg.angle_egl = G.angle_egl; }
    if (cfg->angle_gles) { snprintf(G.angle_gles, sizeof(G.angle_gles), "%s", cfg->angle_gles); G.cfg.angle_gles = G.angle_gles; }

    tl_set_data_dir(cfg->data_dir);
    tl_nwindow_configure(cfg->width, cfg->height, cfg->metal_layer);
    int n = tl_dexidx_open(cfg->apk_path);
    tl_log_line("gta: %d classes in the APK's DEX", n);
    if (!tl_ld_add_apk(cfg->apk_path)) return false;
    if (cfg->angle_egl && !tl_egl_init(cfg->angle_egl, cfg->angle_gles, cfg->frame_dir, cfg->frame_every)) return false;
    tl_jni_init();
    tl_hle_configure(cfg->package_name, cfg->apk_path, cfg->data_dir, cfg->width, cfg->height);
    tl_jni_hle_install();

    tl_jni_declare("android/view/Surface", "java/lang/Object");
    tl_jni_declare(CLS_BASE, "android/app/Activity");
    tl_jni_declare(CLS_ACT, CLS_BASE);
    tl_jni_declare(CLS_VIEW, "android/view/SurfaceView");
    tl_jni_declare(CLS_SVC, "java/lang/Object");
    tl_jni_declare(CLS_DEV, "java/lang/Object");
    tl_jni_declare("com/nvidia/devtech/NvUtil", "java/lang/Object");
    tl_jni_register_hle(k_hle);

    G.activity = tl_jni_new_object(tl_jni_class(CLS_ACT));
    tl_hle_set_activity(G.activity);
    G.view = tl_jni_new_object(tl_jni_class(CLS_VIEW));
    G.services = tl_jni_new_object(tl_jni_class(CLS_SVC));
    tl_jni_set_field(G.services, "activity", "L" CLS_BASE ";", vl(G.activity));
    tl_jni_set_field(G.services, "view", "L" CLS_VIEW ";", vl(G.view));
    G.surface = tl_jni_new_object(tl_jni_class("android/view/Surface"));

    /* DeviceInfo(Context): the facts the library records about the device. */
    G.device = tl_jni_new_object(tl_jni_class(CLS_DEV));
    set_int(G.device, "cpuFrequency", 3000000);
    set_str(G.device, "hardware", "shiba"); set_str(G.device, "manufacturer", "Google"); set_str(G.device, "model", "Pixel 8"); set_str(G.device, "product", "shiba");
    set_bool(G.device, "hasTouchScreen", 1); set_bool(G.device, "hasVibrator", 0); set_bool(G.device, "isPhone", 1); set_bool(G.device, "isTvDevice", 0);
    set_int(G.device, "osVersion", 34);

    /* GameNative.<clinit>: System.loadLibrary("Game"). */
    load_library("Game");
    if (tl_jni_pending()) { tl_log_line("gta: loading libGame.so failed"); return false; }
    tl_log_line("gta: libraries loaded");
    setenv("TL_PAD_DPAD", "keys", 0);
    G.started = true;
    return true;
}

/* -------------------------------------------------------------- the game thread */

/* Controllers: the gamepad layer's events, queued for the game thread. Android key codes go straight through; the sticks and triggers become one axes call. */
static void pad_key(jobj *ev_, int device, int action, int keycode, int64_t down_ms, int64_t event_ms)
{
    (void)down_ms; (void)event_ms;
    post(EV_PAD_KEY, device - 41, action == 0 ? keycode : -keycode, NULL);
    tl_jni_unref(ev_);
}
static void pad_motion(jobj *ev_, int device, int source, int64_t down_ms, int64_t event_ms)
{
    (void)source; (void)down_ms; (void)event_ms;
    float a[48];
    if (tl_input_event_axes(ev_, a)) { float f[6] = { a[0], a[1], a[11], a[14], a[17], a[18] }; post(EV_PAD_AXES, device - 41, 0, f); }
    tl_jni_unref(ev_);
}

static void *game_main(void *arg)
{
    (void)arg;
    pthread_setname_np("GameThread");
    void *env = tl_jni_env();
    void *cls = tl_jni_class_object(CLS_NATIVE);
    static const tl_pad_sink sink = { pad_key, pad_motion };
    tl_pad_set_sink(&sink);

    void (*on_created)(void *, void *, void *, uint8_t) = game_native("implOnActivityCreated");
    void (*on_setup)(void *, void *, void *, void *, void *, void *) = game_native("implOnInitialSetup");
    uint8_t (*initialized)(void *, void *) = game_native("implIsInitialized");
    void (*on_surface_created)(void *, void *) = game_native("implOnSurfaceCreated");
    void (*on_surface_changed)(void *, void *, void *, int, int) = game_native("implOnSurfaceChanged");
    void (*on_resume)(void *, void *) = game_native("implOnResume");
    void (*on_pause)(void *, void *) = game_native("implOnPause");
    void (*on_network)(void *, void *, int) = game_native("implOnNetworkChanged");
    void (*on_frame)(void *, void *, float) = game_native("implOnDrawFrame");
    void (*touch_start)(void *, void *, int, float, float) = game_native("implOnTouchStart");
    void (*touch_move)(void *, void *, int, float, float) = game_native("implOnTouchMove");
    void (*touch_end)(void *, void *, int, float, float) = game_native("implOnTouchEnd");
    void (*pad_down)(void *, void *, int, int) = game_native("implOnGamepadButtonDown");
    void (*pad_up)(void *, void *, int, int) = game_native("implOnGamepadButtonUp");
    void (*pad_axes)(void *, void *, int, float, float, float, float, float, float) = game_native("implOnGamepadAxesChanged");
    void (*pad_conn)(void *, void *, int) = game_native("implOnGamepadConnected");
    void (*pad_disc)(void *, void *, int) = game_native("implOnGamepadDisconnected");
    void (*rs_setup)(void *, void *, void *, void *) = game_native("implOnRockstarSetup");
    void (*rs_state)(void *, void *, uint8_t) = game_native("implOnRockstarStateChanged");
    void (*rs_gate)(void *, void *, int, uint8_t) = game_native("implOnRockstarGateComplete");
    void (*rs_signin)(void *, void *) = game_native("implOnRockstarSignInComplete");
    void (*rs_signout)(void *, void *) = game_native("implOnRockstarSignOutComplete");
    void (*rs_initial)(void *, void *) = game_native("implOnRockstarInitialComplete");
    void (*rs_cloud)(void *, void *) = game_native("implOnRockstarCloudDisabledComplete");
    void (*rs_deletion)(void *, void *) = game_native("implOnRockstarAccountDeletionComplete");
    void (*rs_id)(void *, void *, void *) = game_native("implOnRockstarIdChanged");
    void (*http_error)(void *, void *, int, int) = game_native("implOnHttpRequestError");
    void (*playlist_done)(void *, void *, uint8_t, int) = game_native("implOnPlaylistOpenComplete");
    if (!on_created || !on_setup || !on_frame) return NULL;

    /* GameActivityBase.onCreate: the activity's services, then the one-time set-up with the device, the assets and where the packages are. */
    on_created(env, cls, G.services, initialized ? initialized(env, cls) : 0);
    jobj *apks = tl_jni_new_obj_array(tl_jni_class("java/lang/String"), 1);
    apks->oarr.v[0] = tl_jni_new_string(G.apk);
    jobj *none = tl_jni_new_obj_array(tl_jni_class("java/lang/String"), 0);
    on_setup(env, cls, G.device, tl_hle_assets(), apks, none);
    tl_log_line("gta: initial set-up done");
    if (on_network) on_network(env, cls, 1);
    /* The Rockstar Social Club library the Java side starts (rockstarmobile.Rockstar.setup) tells the game its environment when it is ready. */
    if (rs_setup) {
        const char *e1 = getenv("TL_GTA_RS1") ? getenv("TL_GTA_RS1") : "prod", *e2 = getenv("TL_GTA_RS2") ? getenv("TL_GTA_RS2") : "";
        rs_setup(env, cls, tl_jni_new_string(e1), tl_jni_new_string(e2));
        tl_log_line("gta: Rockstar environment %s / %s", e1, e2);
    }
    if (rs_state && getenv("TL_GTA_RS_STATE")) rs_state(env, cls, 0);

    /* GameView.surfaceCreated / surfaceChanged, then the activity resuming. */
    if (on_surface_created) on_surface_created(env, cls);
    if (on_surface_changed) on_surface_changed(env, cls, G.surface, G.cfg.width, G.cfg.height);
    if (on_resume) on_resume(env, cls);
    tl_log_line("gta: lifecycle delivered");
    atomic_store(&G.running, true);

    struct timespec last; clock_gettime(CLOCK_MONOTONIC, &last);
    bool pad_known[TL_PADS] = { false };
    bool paused = false;
    for (;;) {
        for (ev *e; (e = take()); free(e)) {
            switch (e->kind) {
            case EV_TOUCH: {
                void (*fn)(void *, void *, int, float, float) = e->a == 0 ? touch_start : e->a == 1 ? touch_move : touch_end;
                if (fn) fn(env, cls, e->b, e->f[0], e->f[1]);
                break;
            }
            case EV_PAD_KEY: if (e->b >= 0 ? pad_down : pad_up) (e->b >= 0 ? pad_down : pad_up)(env, cls, e->a, e->b >= 0 ? e->b : -e->b); break;
            case EV_PAD_AXES: if (pad_axes) pad_axes(env, cls, e->a, e->f[0], e->f[1], e->f[2], e->f[3], e->f[4], e->f[5]); break;
            case EV_PAUSE: paused = true; if (on_pause) on_pause(env, cls); break;
            case EV_RESUME: paused = false; if (on_resume) on_resume(env, cls); break;
            case EV_GATE: if (rs_gate) rs_gate(env, cls, e->a, (uint8_t)e->b); break;
            case EV_SIGNIN: if (rs_signin) rs_signin(env, cls); break;
            case EV_SIGNOUT: if (rs_signout) rs_signout(env, cls); break;
            case EV_INITIAL: if (rs_initial) rs_initial(env, cls); break;
            case EV_CLOUD: if (rs_cloud) rs_cloud(env, cls); break;
            case EV_DELETION: if (rs_deletion) rs_deletion(env, cls); break;
            case EV_ID: if (rs_id) rs_id(env, cls, tl_jni_new_string("")); break;
            case EV_PLAYLIST: if (playlist_done) playlist_done(env, cls, (uint8_t)e->a, e->b); break;
            case EV_HTTP_ERROR: if (http_error) http_error(env, cls, e->a, e->b); break;
            default: break;
            }
            if (tl_jni_pending()) tl_jni_clear();
        }
        for (int s = 0; s < TL_PADS; s++) {                       /* a controller coming or going */
            bool c = tl_pad_connected(s);
            if (c && !pad_known[s] && pad_conn) pad_conn(env, cls, s);
            if (!c && pad_known[s] && pad_disc) pad_disc(env, cls, s);
            pad_known[s] = c;
        }
        if (paused) { usleep(20000); continue; }
        if (initialized && initialized(env, cls)) {
            struct timespec now; clock_gettime(CLOCK_MONOTONIC, &now);
            float dt = (float)((now.tv_sec - last.tv_sec) + (now.tv_nsec - last.tv_nsec) / 1e9);
            last = now;
            on_frame(env, cls, dt);
        } else usleep(5000);
        if (tl_jni_pending()) tl_jni_clear();
    }
    return NULL;
}

bool tl_gta_run(void)
{
    if (!G.started) return false;
    pthread_attr_t a; pthread_attr_init(&a); pthread_attr_setstacksize(&a, 16u << 20);
    bool ok = pthread_create(&G.thread, &a, game_main, NULL) == 0;
    pthread_attr_destroy(&a);
    return ok;
}

void tl_gta_touch(int phase, int id, float x, float y)
{
    if (!atomic_load(&G.running)) return;
    float f[6] = { x, y, 0, 0, 0, 0 };
    post(EV_TOUCH, phase == 0 ? 0 : phase == 1 ? 1 : 2, id, f);
}

void tl_gta_set_paused(bool paused)
{
    if (!atomic_load(&G.running)) return;
    static atomic_bool is_paused;
    if (atomic_exchange(&is_paused, paused) == paused) return;
    post(paused ? EV_PAUSE : EV_RESUME, 0, 0, NULL);
}

unsigned long tl_gta_frames(void) { return tl_egl_frames_presented(); }
void tl_gtasa_reset_session(void) { G.activity = G.view = G.services = G.surface = G.device = NULL; g_nvutil = NULL; g_thread = NULL; }
