/* SPDX-License-Identifier: GPL-2.0-or-later */
#define _DARWIN_C_SOURCE
#include "husk-tl-unity.h"

#include <pthread.h>
#include <setjmp.h>
#include <stdarg.h>
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
#include "husk-tl-arm32.h"
#include "husk-tl-ld.h"

void tl_jni_hle_install(void);
void tl_hle_configure(const char *pkg, const char *apk, const char *data, int w, int h);
jobj *tl_hle_activity(void);
void tl_set_data_dir(const char *dir);
void tl_nwindow_configure(int w, int h, void *layer);

static struct {
    tl_unity_config cfg;
    jobj *activity;          /* the Context handed to the engine */
    jobj *player;            /* the UnityPlayer object natives are called on */
    atomic_ulong frames;
    atomic_bool stop;
    atomic_bool paused;
    atomic_ullong perf_ns, perf_max_ns;     /* time inside nativeRender since the last snapshot, and the worst call */
    atomic_ulong perf_frames;
    struct timespec perf_since;
    pthread_t thread;
    bool thread_started;
} U;

typedef int32_t (*onload_fn)(void *vm, void *reserved);

static void il2cpp_log_cb(const char *s) { tl_log_line("I/IL2CPP: %s", s ? s : ""); }

static void (*il2_fmt_ex)(void *, char *, int);
static void (*il2_fmt_st)(void *, char *, int);
static void *(*il2_cxa_cur)(void);
static void (*il2_raise)(void *);
static void (*il2_unhandled)(void *);
static void *(*il2_begin_catch)(void *);
static void (*il2_end_catch)(void);

/* AOT mixed pages put C++ typeinfo on the writable copy and .eh_frame on the r-x image, so a throw's
 * catch never matches. The terminate handler longjmps back to the UnityMain call that was in flight. */
static _Thread_local jmp_buf g_unity_jmp;
static _Thread_local int g_unity_jmp_armed;   /* 1: init on UnityMain (longjmp); 2: a frame (do not longjmp out of nativeRender) */

static void il2cpp_dump_ex(void *ex, const char *why)
{
    char msg[1024] = {0}, st[4096] = {0};
    if (il2_fmt_ex && ex) il2_fmt_ex(ex, msg, (int)sizeof msg);
    if (il2_fmt_st && ex) il2_fmt_st(ex, st, (int)sizeof st);
    tl_log_line("unity: %s: %s", why, msg[0] ? msg : "(no IL2CPP message)");
    if (st[0]) tl_log_line("unity: IL2CPP stack:\n%s", st);
}

/* Il2CppExceptionWrapper is `struct { Il2CppException *ex; }` thrown as a C++ object. */
static void *il2cpp_from_wrapper(void *obj)
{
    if (!obj) return NULL;
    void *ex = *(void **)obj;
    return ex;
}

static void il2cpp_terminate(void)
{
    static _Thread_local int in_term;
    if (in_term) pthread_exit(NULL);
    in_term = 1;
    void *obj = il2_cxa_cur ? il2_cxa_cur() : NULL;
    void *ex = il2cpp_from_wrapper(obj);
    if (ex) il2cpp_dump_ex(ex, "uncaught Il2CppExceptionWrapper");
    else il2cpp_dump_ex(obj, "uncaught thrown object");
    if (g_unity_jmp_armed == 1) {
        in_term = 0;
        g_unity_jmp_armed = 0;
        longjmp(g_unity_jmp, 1);
    }
    tl_log_line("unity: uncaught IL2CPP exception on %s; that thread exits", g_unity_jmp_armed == 2 ? "UnityMain (a frame)" : "a worker");
    pthread_exit(NULL);
}

static void il2cpp_raise_wrap(void *ex)
{
    il2cpp_dump_ex(ex, "il2cpp_raise_exception");
    if (il2_raise) il2_raise(ex);
    abort();
}

static void *il2_sym(tl_lib *il2, const char *name)
{
    void *p = tl_ld_sym(il2, name);
    if (!p) p = tl_ld_sym_deep(il2, name, NULL);
    return p;
}

static int g_il2_hooks;

static void il2cpp_host_hooks(void)
{
    /* Uncaught C# otherwise aborts as Il2CppExceptionWrapper with no message. */
    if (g_il2_hooks) return;
    tl_lib *il2 = tl_ld_find_lib("libil2cpp.so");
    if (!il2) il2 = tl_ld_find_lib("/data/app/lib/arm64/libil2cpp.so");
    if (!il2) return;
    g_il2_hooks = 1;
    void (*reg)(void (*)(const char *)) = (void (*)(void (*)(const char *)))il2_sym(il2, "il2cpp_register_log_callback");
    if (reg) { reg(il2cpp_log_cb); tl_log_line("unity: IL2CPP log callback installed"); }
    void (*pol)(int) = (void (*)(int))il2_sym(il2, "il2cpp_runtime_unhandled_exception_policy_set");
    if (pol) { pol(0); tl_log_line("unity: IL2CPP unhandled-exception policy is legacy"); }
    il2_fmt_ex = (void (*)(void *, char *, int))il2_sym(il2, "il2cpp_format_exception");
    il2_fmt_st = (void (*)(void *, char *, int))il2_sym(il2, "il2cpp_format_stack_trace");
    il2_cxa_cur = (void *(*)(void))il2_sym(il2, "__cxa_current_primary_exception");
    if (!il2_cxa_cur) il2_cxa_cur = (void *(*)(void))il2_sym(il2, "_ZSt17current_exceptionv");
    void (*set_term)(void (*)(void)) = (void (*)(void (*)(void)))il2_sym(il2, "_ZSt13set_terminatePFvvE");
    if (!set_term) set_term = (void (*)(void (*)(void)))il2_sym(il2, "set_terminate");
    if (set_term) { set_term(il2cpp_terminate); tl_log_line("unity: IL2CPP terminate handler installed"); }
    il2_raise = (void (*)(void *))il2_sym(il2, "il2cpp_raise_exception");
    if (il2_raise) tl_ld_interpose("il2cpp_raise_exception", (void *)il2cpp_raise_wrap);
    il2_unhandled = (void (*)(void *))il2_sym(il2, "il2cpp_unhandled_exception");
    il2_begin_catch = (void *(*)(void *))il2_sym(il2, "__cxa_begin_catch");
    il2_end_catch = (void (*)(void))il2_sym(il2, "__cxa_end_catch");
    tl_log_line("unity: IL2CPP dump fmt=%d stack=%d cxa=%d term=%d raise=%d unhandled=%d catch=%d",
                il2_fmt_ex != NULL, il2_fmt_st != NULL, il2_cxa_cur != NULL, set_term != NULL, il2_raise != NULL,
                il2_unhandled != NULL, il2_begin_catch != NULL && il2_end_catch != NULL);
}

static void il2cpp_on_lib_init(tl_lib *L)
{
    const char *n = tl_ld_lib_name(L);
    if (!n) return;
    const char *b = strrchr(n, '/');
    b = b ? b + 1 : n;
    if (!strcmp(b, "libil2cpp.so")) il2cpp_host_hooks();
}

static void keyboard_install(void);
static void keyboard_deliver(void);

static void *native_of(const char *cls, const char *name, const char *sig)
{
    void *fn = tl_jni_native(cls, name, sig);
    /* Unity 6 registers the player's lifecycle natives (nativeRender, nativePause, ...) on the class that hosts it in an
     * activity, not on UnityPlayer itself */
    if (!fn && !strcmp(cls, "com/unity3d/player/UnityPlayer")) {
        fn = tl_jni_native("com/unity3d/player/UnityPlayerForActivityOrService", name, sig);
        if (!fn) fn = tl_jni_native("com/unity3d/player/UnityPlayerForGameActivity", name, sig);
    }
    if (!fn) tl_log_line("unity: native %s.%s%s was not registered", cls, name, sig);
    return fn;
}

bool tl_unity_start(const tl_unity_config *cfg)
{
    U.cfg = *cfg;
    tl_set_data_dir(cfg->data_dir);
    tl_nwindow_configure(cfg->width, cfg->height, cfg->metal_layer);
    int n = tl_dexidx_open(cfg->apk_path);
    tl_log_line("unity: %d classes in the APK's DEX", n);
    if (!tl_ld_add_apk(cfg->apk_path)) return false;
    /* A player built to require ES 3.1 or 3.2 (its manifest's glEsVersion) asks only for those contexts and gives up on "your
     * device does not match the hardware requirements" when ANGLE refuses: it gets the 3.0 context described as 3.1/3.2 */
    { int tl_manifest_gles(const char *); void tl_egl_es3_claim(int); int gv = tl_manifest_gles(cfg->apk_path);
      if (getenv("TL_GLES_CLAIM")) gv = 0x30000 | atoi(getenv("TL_GLES_CLAIM"));   /* TL_GLES_CLAIM=<minor>: as if the manifest said 3.<minor> */
      if (gv >= 0x30001) { tl_egl_es3_claim(gv & 0xffff); tl_log_line("egl: the manifest asks for OpenGL ES 3.%d: the driver is described so", gv & 0xffff); } }
    if (cfg->angle_egl && !tl_egl_init(cfg->angle_egl, cfg->angle_gles, cfg->frame_dir, cfg->frame_every)) return false;
    tl_jni_init();
    tl_hle_configure(cfg->package_name, cfg->apk_path, cfg->data_dir, cfg->width, cfg->height);
    tl_jni_hle_install();
    keyboard_install();
    tl_ld_on_init(il2cpp_on_lib_init);

    /* new UnityPlayer(activity): the Java side's own constructor calls loadNative(dir),
     * which is System.load(dir + "/libmain.so") followed by NativeLoader.load(dir). */
    U.activity = tl_hle_activity();
    U.player = tl_jni_new_object(tl_jni_class("com/unity3d/player/UnityPlayer"));

    /* Android's main thread has its Looper before any app code runs: Unity's libmain asks for it while loading (JNI_OnLoad
     * -> ALooper_forThread), and the NULL it gets here is kept as the engine's main-thread queue and dereferenced later. */
    { void *(*looper_prepare)(int) = tl_bionic_find("ALooper_prepare"); if (looper_prepare) looper_prepare(0); }

    tl_lib *main_lib = tl_ld_load("libmain.so");
    if (!main_lib) return false;
    tl_ld_init(main_lib);
    onload_fn onload = (onload_fn)tl_ld_sym(main_lib, "JNI_OnLoad");
    if (!onload) { tl_log_line("unity: libmain.so has no JNI_OnLoad"); return false; }
    int32_t ver = tl_a32_is_fn((void *)onload) ? tl_a32_jni_onload(main_lib) : onload(tl_jni_vm(), NULL);
    tl_log_line("unity: libmain JNI_OnLoad -> %#x", ver);
    if (tl_jni_pending()) { tl_log_line("unity: an exception is pending after libmain's JNI_OnLoad"); return false; }

    typedef uint8_t (*load_fn)(void *env, void *cls, void *dir);
    load_fn load = (load_fn)native_of("com/unity3d/player/NativeLoader", "load", "(Ljava/lang/String;)Z");
    if (!load) return false;
    jobj *dir = tl_jni_new_string("/data/app/lib/arm64");
    uint8_t ok = load(tl_jni_env(), tl_jni_class_object("com/unity3d/player/NativeLoader"), dir);
    tl_log_line("unity: NativeLoader.load -> %d", ok);
    if (ok) il2cpp_host_hooks();
    return ok != 0;
}

/*
 * Call a registered UnityPlayer native: (env, player, a, b). Fixed arity on purpose: a
 * variadic pointer type would put the arguments on the stack, where guest code (compiled
 * for AAPCS64) does not look for them.
 */
typedef void (*native_call_fn)(void *env, void *self, uintptr_t a, uintptr_t b);
static void call_native_on(const char *cls, jobj *self, const char *name, const char *sig, uintptr_t a, uintptr_t b)
{
    void *fn = native_of(cls, name, sig);
    if (fn) ((native_call_fn)fn)(tl_jni_env(), self, a, b);
}
static void call_native(const char *name, const char *sig, uintptr_t a, uintptr_t b)
{
    call_native_on("com/unity3d/player/UnityPlayer", U.player, name, sig, a, b);
}
#define NATIVE_VOID(name, sig, a, b) call_native(name, sig, (uintptr_t)(a), (uintptr_t)(b))

static void *unity_main(void *arg)
{
    (void)arg;
    pthread_setname_np("UnityMain");
    tl_log_line("unity: UnityMain thread started");

    volatile int phase = 0;
    if (setjmp(g_unity_jmp)) tl_log_line("unity: continuing after an uncaught IL2CPP exception (phase %d)", phase);
    g_unity_jmp_armed = 1;

    /* The jobs UnityPlayer queues for this thread, in the order it queues them. */
    if (phase < 1) {
        phase = 1;
        jobj *surface = tl_jni_new_object(tl_jni_class("android/view/Surface"));
        NATIVE_VOID("nativeRecreateGfxState", "(ILandroid/view/Surface;)V", 0, surface);
        tl_log_line("unity: nativeRecreateGfxState done");
        NATIVE_VOID("nativeSendSurfaceChangedEvent", "()V", 0, 0);
        NATIVE_VOID("nativeResume", "()V", 0, 0);
        tl_log_line("unity: nativeResume done");
        NATIVE_VOID("nativeFocusChanged", "(Z)V", 1, 0);
    }
    /* What Unity's Java side tells the engine as it starts on a phone: the media volume (AudioVolumeHandler reports it, and
     * an engine that never hears it takes the device as muted) and that its sound is not muted. */
    if (phase < 2) {
        phase = 2;
        jobj *volume = tl_jni_new_object(tl_jni_class("com/unity3d/player/AudioVolumeHandler"));
        call_native_on("com/unity3d/player/AudioVolumeHandler", volume, "onAudioVolumeChanged", "(I)V", 7, 0);
        NATIVE_VOID("nativeMuteMasterAudio", "(Z)V", 0, 0);
        tl_log_line("unity: told the engine the volume (7 of 15) and that sound is on");
    }

    void *render = native_of("com/unity3d/player/UnityPlayer", "nativeRender", "()Z");
    bool was_paused = false;
    phase = 3;
    g_unity_jmp_armed = 2;
    while (render && !atomic_load(&U.stop)) {
        if (atomic_load(&U.paused)) {
            if (!was_paused) { NATIVE_VOID("nativeFocusChanged", "(Z)V", 0, 0); NATIVE_VOID("nativePause", "()Z", 0, 0); was_paused = true; tl_log_line("unity: paused"); }
            usleep(20000);
            continue;
        }
        if (was_paused) { NATIVE_VOID("nativeResume", "()V", 0, 0); NATIVE_VOID("nativeFocusChanged", "(Z)V", 1, 0); was_paused = false; tl_log_line("unity: resumed"); }
        keyboard_deliver();
        struct timespec t0, t1;
        clock_gettime(CLOCK_MONOTONIC, &t0);
        uint8_t keep = ((uint8_t (*)(void *, void *))render)(tl_jni_env(), U.player);
        clock_gettime(CLOCK_MONOTONIC, &t1);
        {
            unsigned long long ns = (unsigned long long)((t1.tv_sec - t0.tv_sec) * 1000000000ll + (t1.tv_nsec - t0.tv_nsec));
            atomic_fetch_add(&U.perf_ns, ns);
            atomic_fetch_add(&U.perf_frames, 1);
            unsigned long long m = atomic_load(&U.perf_max_ns);
            while (ns > m && !atomic_compare_exchange_weak(&U.perf_max_ns, &m, ns)) {}
        }
        atomic_fetch_add(&U.frames, 1);
        if (!keep) { tl_log_line("unity: nativeRender returned false: the engine asked to quit"); break; }
        usleep(2000);
    }
    g_unity_jmp_armed = 0;
    return NULL;
}

bool tl_unity_run(void)
{
    /* The UnityPlayer constructor's last native step before it starts the thread. */
    /* Unity 6 adds the kind of host it runs in (0: an activity or service, 1: a GameActivity) and, in some players, a
     * string beside it; which one a player has is its RegisterNatives' to say. Asking for the wrong one means initJni
     * never runs, and the engine then holds NULL for the Java side's classes (GetMethodID(NULL class, ...) later). */
    void *init3 = tl_jni_native("com/unity3d/player/UnityPlayer", "initJni", "(Landroid/content/Context;ILjava/lang/String;)V");
    void *init2 = init3 ? NULL : tl_jni_native("com/unity3d/player/UnityPlayer", "initJni", "(Landroid/content/Context;I)V");
    if (init3) {
        jobj *kind = tl_jni_new_string("");
        ((void (*)(void *, jobj *, jobj *, int, jobj *))init3)(tl_jni_env(), U.player, U.activity, 0, kind);
    } else if (init2) {
        ((void (*)(void *, jobj *, jobj *, int))init2)(tl_jni_env(), U.player, U.activity, 0);
    } else {
        NATIVE_VOID("initJni", "(Landroid/content/Context;)V", U.activity, 0);
    }
    tl_log_line("unity: initJni done");
    /* The rest of the UnityPlayer constructor: the helpers it builds, whose constructors each call a
     * native that gives the engine its reference to the helper's Java class. Without these the engine
     * holds NULL where it expects a class and fails later, far from the cause. */
    /* ...and the web request helper's class, which UnityPlayer's constructor hands the engine so it can find the request methods. */
    NATIVE_VOID("nativeInitWebRequest", "(Ljava/lang/Class;)V", tl_jni_class_object("com/unity3d/player/UnityWebRequest"), 0);
    tl_log_line("unity: nativeInitWebRequest done");
    jobj *cam = tl_jni_new_object(tl_jni_class("com/unity3d/player/Camera2Wrapper"));
    call_native_on("com/unity3d/player/Camera2Wrapper", cam, "initCamera2Jni", "()V", 0, 0);
    jobj *hfp = tl_jni_new_object(tl_jni_class("com/unity3d/player/HFPStatus"));
    call_native_on("com/unity3d/player/HFPStatus", hfp, "initHFPStatusJni", "()V", 0, 0);
    jobj *olock = tl_jni_new_object(tl_jni_class("com/unity3d/player/OrientationLockListener"));
    call_native_on("com/unity3d/player/OrientationLockListener", olock, "nativeUpdateOrientationLockState", "(I)V", 1, 0);
    pthread_attr_t a;
    pthread_attr_init(&a);
    pthread_attr_setstacksize(&a, 16u << 20);
    if (pthread_create(&U.thread, &a, unity_main, NULL) != 0) return false;
    U.thread_started = true;
    return true;
}

/* -------------------------------------------------------------- keyboard */

/*
 * TouchScreenKeyboard (an InputField's keyboard, a name to enter): the engine calls UnityPlayer.showSoftInput with the field's text
 * and kind, and the Java side shows a dialog whose edits it reports back through natives. Here the iPhone's keyboard is the dialog:
 * the field is kept from showSoftInput / setSoftInputStr, each key edits it, and the edits reach the engine on its own thread,
 * between frames, as the dialog's Runnables do (nativeSetInputString, then nativeSoftInputClosed when it is done).
 */
static struct {
    pthread_mutex_t lock;
    void (*hook)(int action, int kind);     /* the app: 1 shows the keyboard (with the kind), 2 hides it */
    char text[4096];
    int limit;                               /* characters the field takes, 0 for any */
    bool shown, send_text, send_closed, send_canceled;
} K = { .lock = PTHREAD_MUTEX_INITIALIZER };

void tl_unity_set_keyboard_handler(void (*hook)(int action, int kind)) { K.hook = hook; }

static void *native_quiet(const char *name, const char *sig)
{
    void *fn = tl_jni_native("com/unity3d/player/UnityPlayer", name, sig);
    if (!fn) fn = tl_jni_native("com/unity3d/player/UnityPlayerForActivityOrService", name, sig);
    if (!fn) fn = tl_jni_native("com/unity3d/player/UnityPlayerForGameActivity", name, sig);
    return fn;
}

static size_t utf8_chars(const char *s) { size_t n = 0; for (; *s; s++) if ((*s & 0xC0) != 0x80) n++; return n; }

/* showSoftInput(String text, int type, boolean correction, boolean multiline, boolean secure, boolean alert, String placeholder,
 * int characterLimit, boolean hideInputField[, boolean hideInput]): the same first eight arguments in every player since 5.x */
static void UnityPlayer_showSoftInput(tl_jcall *c)
{
    const char *text = c->args[0].l ? tl_jni_string(c->args[0].l) : NULL;
    int kind = c->args[1].i & 0xFF;
    if (c->args[3].z) kind |= 0x200;
    if (c->args[4].z) kind |= 0x100;
    pthread_mutex_lock(&K.lock);
    snprintf(K.text, sizeof(K.text), "%s", text ? text : "");
    K.limit = c->args[7].i > 0 ? c->args[7].i : 0;
    K.shown = true;
    K.send_closed = K.send_canceled = false;
    pthread_mutex_unlock(&K.lock);
    tl_log_line("unity: the game asks for the keyboard (type %d%s%s, limit %d)", kind & 0xFF, kind & 0x200 ? ", multi-line" : "", kind & 0x100 ? ", secure" : "", K.limit);
    if (K.hook) K.hook(1, kind);
}
static void UnityPlayer_hideSoftInput(tl_jcall *c)
{
    (void)c;
    pthread_mutex_lock(&K.lock);
    bool was = K.shown;
    K.shown = false;
    pthread_mutex_unlock(&K.lock);
    if (was && K.hook) K.hook(2, 0);
}
static void UnityPlayer_setSoftInputStr(tl_jcall *c)
{
    const char *text = c->args[0].l ? tl_jni_string(c->args[0].l) : NULL;
    pthread_mutex_lock(&K.lock);
    snprintf(K.text, sizeof(K.text), "%s", text ? text : "");
    pthread_mutex_unlock(&K.lock);
}
static void Unity_noop(tl_jcall *c) { (void)c; }

void tl_unity_insert_text(const char *utf8)
{
    pthread_mutex_lock(&K.lock);
    if (K.shown && utf8) {
        size_t have = strlen(K.text), chars = utf8_chars(K.text);
        for (const char *p = utf8; *p;) {
            size_t len = 1;
            while ((p[len] & 0xC0) == 0x80) len++;
            if ((K.limit && chars >= (size_t)K.limit) || have + len >= sizeof(K.text)) break;
            memcpy(K.text + have, p, len);
            have += len; chars++; p += len;
        }
        K.text[have] = 0;
        K.send_text = true;
    }
    pthread_mutex_unlock(&K.lock);
}

void tl_unity_delete_backward(void)
{
    pthread_mutex_lock(&K.lock);
    size_t n = strlen(K.text);
    if (K.shown && n) {
        while (n && (K.text[n - 1] & 0xC0) == 0x80) n--;
        if (n) n--;
        K.text[n] = 0;
        K.send_text = true;
    }
    pthread_mutex_unlock(&K.lock);
}

/* Done (true) or the keyboard closed without it (false): what the dialog's OK and its dismissal report. */
void tl_unity_finish_text(bool done)
{
    pthread_mutex_lock(&K.lock);
    if (K.shown) {
        K.shown = false;
        K.send_text = true;
        if (done) K.send_closed = true; else K.send_canceled = true;
    }
    pthread_mutex_unlock(&K.lock);
}

void tl_unity_text_copy(char *out, unsigned long cap)
{
    if (!cap) return;
    pthread_mutex_lock(&K.lock);
    snprintf(out, cap, "%s", K.text);
    pthread_mutex_unlock(&K.lock);
}

/* On UnityMain, before a frame: the edits since the last one. */
static void keyboard_deliver(void)
{
    pthread_mutex_lock(&K.lock);
    bool text = K.send_text, closed = K.send_closed, canceled = K.send_canceled;
    char copy[sizeof(K.text)];
    memcpy(copy, K.text, sizeof(copy));
    K.send_text = K.send_closed = K.send_canceled = false;
    pthread_mutex_unlock(&K.lock);
    if (!text && !closed && !canceled) return;
    typedef void (*str_fn)(void *env, void *self, void *s, uintptr_t b);
    typedef void (*two_fn)(void *env, void *self, uintptr_t a, uintptr_t b);
    if (text) {
        str_fn set = (str_fn)native_quiet("nativeSetInputString", "(Ljava/lang/String;)V");
        if (set) { jobj *s = tl_jni_new_string(copy); set(tl_jni_env(), U.player, s, 0); tl_jni_unref(s); }
        two_fn sel = (two_fn)native_quiet("nativeSetInputSelection", "(II)V");
        if (sel) sel(tl_jni_env(), U.player, utf8_chars(copy), 0);
    }
    if (closed || canceled) {
        two_fn end = (two_fn)native_quiet(closed ? "nativeSoftInputClosed" : "nativeSoftInputCanceled", "()V");
        if (!end && canceled) end = (two_fn)native_quiet("nativeSoftInputClosed", "()V");
        if (end) end(tl_jni_env(), U.player, 0, 0);
        two_fn visible = (two_fn)native_quiet("nativeSetKeyboardIsVisible", "(Z)V");
        if (visible) visible(tl_jni_env(), U.player, 0, 0);
        tl_log_line("unity: keyboard %s with \"%s\"", closed ? "done" : "dismissed", copy);
    }
    if (tl_jni_pending()) tl_jni_clear();
}

static void keyboard_install(void)
{
    static const char *classes[] = { "com/unity3d/player/UnityPlayer", "com/unity3d/player/UnityPlayerForActivityOrService", "com/unity3d/player/UnityPlayerForGameActivity" };
    static tl_jhle table[3 * 4 + 1];
    static bool installed;
    if (installed) return;
    installed = true;
    for (int i = 0; i < 3; i++) {
        table[i * 4 + 0] = (tl_jhle){ classes[i], "showSoftInput", NULL, UnityPlayer_showSoftInput };
        table[i * 4 + 1] = (tl_jhle){ classes[i], "hideSoftInput", "()V", UnityPlayer_hideSoftInput };
        table[i * 4 + 2] = (tl_jhle){ classes[i], "setSoftInputStr", "(Ljava/lang/String;)V", UnityPlayer_setSoftInputStr };
        table[i * 4 + 3] = (tl_jhle){ classes[i], "setHideInputField", "(Z)V", Unity_noop };
    }
    tl_jni_register_hle(table);
}

/* ----------------------------------------------------------------- touch */

extern jobj *tl_input_motion_event(int action, int count, const int *ids, const float *xs, const float *ys, int64_t down_ms, int64_t event_ms);

static struct {
    pthread_mutex_t lock;
    int n, ids[10];
    float x[10], y[10];
    int64_t down_ms;
} T = { .lock = PTHREAD_MUTEX_INITIALIZER };

static int64_t uptime_ms(void) { struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts); return (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000; }

void tl_unity_touch(int phase, int id, float x, float y)
{
    typedef uint8_t (*inject_fn)(void *env, void *self, void *event, uintptr_t source);
    inject_fn fn = (inject_fn)native_of("com/unity3d/player/UnityPlayer", "nativeInjectEvent", "(Landroid/view/InputEvent;I)Z");
    if (!fn) fn = (inject_fn)native_of("com/unity3d/player/UnityPlayer", "nativeInjectEvent", "(Landroid/view/InputEvent;)Z");
    if (!fn) return;
    pthread_mutex_lock(&T.lock);
    int idx = -1;
    for (int i = 0; i < T.n; i++) if (T.ids[i] == id) idx = i;
    int64_t now = uptime_ms();
    int action;
    if (phase == 0) {
        if (idx < 0 && T.n < 10) { idx = T.n++; T.ids[idx] = id; }
        if (idx < 0) { pthread_mutex_unlock(&T.lock); return; }
        T.x[idx] = x; T.y[idx] = y;
        if (T.n == 1) T.down_ms = now;
        action = T.n == 1 ? 0 /* ACTION_DOWN */ : (5 /* ACTION_POINTER_DOWN */ | (idx << 8));
    } else if (phase == 1) {
        if (idx < 0) { pthread_mutex_unlock(&T.lock); return; }
        T.x[idx] = x; T.y[idx] = y;
        action = 2; /* ACTION_MOVE */
    } else if (phase == 3) {
        action = 3; /* ACTION_CANCEL */
    } else {
        if (idx < 0) { pthread_mutex_unlock(&T.lock); return; }
        T.x[idx] = x; T.y[idx] = y;
        action = T.n == 1 ? 1 /* ACTION_UP */ : (6 /* ACTION_POINTER_UP */ | (idx << 8));
    }
    jobj *ev = tl_input_motion_event(action, T.n, T.ids, T.x, T.y, T.down_ms, now);
    /* An ended touch leaves the set after the event that reports it. */
    if (phase == 2 && idx >= 0) {
        float lx[10], ly[10]; int li[10];
        memcpy(lx, T.x, sizeof(lx)); memcpy(ly, T.y, sizeof(ly)); memcpy(li, T.ids, sizeof(li));
        int n = 0;
        for (int i = 0; i < T.n; i++) if (i != idx) { T.ids[n] = li[i]; T.x[n] = lx[i]; T.y[n] = ly[i]; n++; }
        T.n = n;
    } else if (phase == 3) {
        T.n = 0;
    }
    pthread_mutex_unlock(&T.lock);
    fn(tl_jni_env(), U.player, ev, 0);
    if (tl_jni_pending()) tl_jni_clear();
    tl_jni_unref(ev);
}

/* ------------------------------------------------------------ controller */

/* The same entry point as a touch: the engine reads a KeyEvent or a joystick MotionEvent off what it is handed. */
static void inject(jobj *ev)
{
    typedef uint8_t (*inject_fn)(void *env, void *self, void *event, uintptr_t source);
    /* The newer players take (InputEvent, int), older ones only the event; the extra argument is harmless to those. */
    inject_fn fn = (inject_fn)native_of("com/unity3d/player/UnityPlayer", "nativeInjectEvent", "(Landroid/view/InputEvent;I)Z");
    if (!fn) fn = (inject_fn)native_of("com/unity3d/player/UnityPlayer", "nativeInjectEvent", "(Landroid/view/InputEvent;)Z");
    if (fn) { fn(tl_jni_env(), U.player, ev, 0); if (tl_jni_pending()) tl_jni_clear(); }
    tl_jni_unref(ev);
}
static void pad_key(jobj *ev, int device, int action, int keycode, int64_t down_ms, int64_t event_ms)
{ (void)device; (void)action; (void)keycode; (void)down_ms; (void)event_ms; inject(ev); }
static void pad_motion(jobj *ev, int device, int source, int64_t down_ms, int64_t event_ms)
{ (void)device; (void)source; (void)down_ms; (void)event_ms; inject(ev); }

void tl_unity_register_pad_sink(void)
{
    static const tl_pad_sink sink = { pad_key, pad_motion };
    tl_pad_set_sink(&sink);
}

void tl_unity_perf_snapshot(tl_unity_perf *out)
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

void tl_unity_set_paused(bool paused) { atomic_store(&U.paused, paused); }

unsigned long tl_unity_frames(void) { return atomic_load(&U.frames); }
void tl_unity_poke(int signo) { if (U.thread_started) pthread_kill(U.thread, signo); }

void tl_unity_stop(void)
{
    atomic_store(&U.stop, true);
    if (U.thread_started) { pthread_join(U.thread, NULL); U.thread_started = false; }
    atomic_store(&U.stop, false);
}
