/* SPDX-License-Identifier: GPL-2.0-or-later */
#define _DARWIN_C_SOURCE
#include "husk-tl-sdl.h"

#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <CommonCrypto/CommonDigest.h>

#include "husk-tl-bionic.h"
#include "husk-tl-dexindex.h"
#include "husk-tl-egl.h"
#include "husk-tl-gamepad.h"
#include "husk-tl-jni.h"
#include "husk-tl-internal.h"
#include "husk-tl-ld.h"

void tl_jni_hle_install(void);
void tl_hle_configure(const char *pkg, const char *apk, const char *data, int w, int h);
void tl_hle_set_activity(jobj *a);
void tl_set_data_dir(const char *dir);
void tl_nwindow_configure(int w, int h, void *layer);
void tl_fmod_install(void);
void tl_assetstream_install(void);
void tl_sdl_apps_install(const char *package);

#define SDLA "org/libsdl/app/SDLActivity"
#define SDLAUDIO "org/libsdl/app/SDLAudioManager"
#define SDLCTRL "org/libsdl/app/SDLControllerManager"
#define STKA "org/supertuxkart/stk/SuperTuxKartActivity"
#define SDLHID "org/libsdl/app/HIDDeviceManager"

static struct {
    tl_ga_config cfg;
    char apk[1024], data[512], pkg[128], frame_dir[512], angle_egl[600], angle_gles[600], activity_class[160], sdl_lib[64], main_lib[64], main_fn[32];
    jobj *activity, *surface;
    pthread_t ui, sdl;
    bool started, sdl2;           /* sdl2: the game ships SDL 2 (natives exported by name, a smaller Java surface) rather than SDL 3 (registered, larger) */
    atomic_bool touch_ready;
    bool touch_as_mouse;                  /* an old SDL 2 that finds its touch screen through inputGetInputDeviceIds and does not turn touches into mouse events */
    atomic_int fingers;
    char signature[100];          /* SHA-256 of the APK signing certificate, "aa:bb:..." as PackageInfo.signatures would give it */
} S;

static jvalue vl(void *p) { jvalue v; v.j = 0; v.l = p; return v; }
static jvalue vi(int i) { jvalue v; v.j = 0; v.i = i; return v; }
static jvalue vz(int z) { jvalue v; v.j = 0; v.z = z != 0; return v; }
static const char *Str(const jobj *o) { const char *s = tl_jni_string(o); return s ? s : ""; }

/* ------------------------------------------------------------------ Java side */

/* The methods of SDLActivity and its helpers that SDL's native code calls back into. */
static void A_context(tl_jcall *c) { c->ret = vl(S.activity ? tl_jni_ref(S.activity) : NULL); }
static void A_surface(tl_jcall *c)
{
    if (!S.surface) S.surface = tl_jni_new_object(tl_jni_class("android/view/Surface"));
    c->ret = vl(tl_jni_ref(S.surface));
}
/* The soft keyboard: a game asks for it with showTextInput and hides it with sendMessage(COMMAND_TEXTEDIT_HIDE); the app shows the real one and types into SDL. */
static void (*g_kbd)(int);
void tl_sdl_set_keyboard_handler(void (*fn)(int action)) { g_kbd = fn; }
static void A_showTextInput(tl_jcall *c) { if (getenv("TL_SDL_TRACE")) tl_log_line("sdl: showTextInput"); if (g_kbd) g_kbd(1); c->ret = vz(1); }
static void A_sendMessage(tl_jcall *c) { if (c->args[0].i == 3 /* COMMAND_TEXTEDIT_HIDE */ && g_kbd) g_kbd(2); c->ret = vz(1); }
static void A_false(tl_jcall *c) { c->ret = vz(0); }
static void A_true(tl_jcall *c) { c->ret = vz(1); }
/* Dungeon Crawl's own addition to SDLActivity: the shorter side of the display in pixels, which it scales its interface by. */
static void A_refDisplaySize(tl_jcall *c) { c->ret = vi(S.cfg.width < S.cfg.height ? S.cfg.width : S.cfg.height); }
static void A_zero(tl_jcall *c) { c->ret = vi(0); }
static void A_minus1(tl_jcall *c) { c->ret = vi(-1); }
static void A_void(tl_jcall *c) { (void)c; }
static void A_locales(tl_jcall *c) { c->ret = vl(tl_jni_new_string("en_US")); }
static void A_emptyString(tl_jcall *c) { c->ret = vl(tl_jni_new_string("")); }

/*
 * ClassLoader.loadClass, which the game uses (through the activity's loader, as FindClass sees only the system's) to reach its own Java helper classes by their
 * dotted names. The class is there if the framework or the APK has it.
 */
bool tl_dexidx_has_class(const char *name);
static void CL_findClass(tl_jcall *c)
{
    char name[300]; snprintf(name, sizeof(name), "%s", Str(c->args[0].l));
    for (char *p = name; *p; p++) if (*p == '.') *p = '/';
    bool framework = !strncmp(name, "android/", 8) || !strncmp(name, "java/", 5) || !strncmp(name, "javax/", 6) || !strncmp(name, "dalvik/", 7);
    if (framework || tl_dexidx_has_class(name)) { c->ret = vl(tl_jni_class_object(name)); return; }
    tl_log_line("sdl: ClassLoader could not find %s", name);
    tl_jni_throw("java/lang/ClassNotFoundException", name);
    c->ret = vl(NULL);
}


/*
 * The APK's signing certificate, which an app reads back through PackageInfo.signatures to check it is the one it was released under. The certificate is in the
 * APK Signing Block (scheme v2/v3), just before the central directory; its hash is what apps compare.
 */
static uint32_t rd32(const uint8_t *p) { uint32_t v; memcpy(&v, p, 4); return v; }
static uint64_t rd64(const uint8_t *p) { uint64_t v; memcpy(&v, p, 8); return v; }
static bool apk_cert_sha256(const char *path, char *out, size_t cap)
{
    int fd = open(path, O_RDONLY);
    if (fd < 0) return false;
    struct stat st; fstat(fd, &st);
    size_t tail = st.st_size < 70000 ? (size_t)st.st_size : 70000;
    uint8_t *t = malloc(tail);
    bool ok = false;
    if (!t || pread(fd, t, tail, st.st_size - (off_t)tail) != (ssize_t)tail) goto done;
    long e = -1;
    for (long i = (long)tail - 22; i >= 0; i--) if (!memcmp(t + i, "PK\5\6", 4)) { e = i; break; }
    if (e < 0) goto done;
    uint64_t cd = rd32(t + e + 16);
    uint8_t foot[24];
    if (cd < 32 || pread(fd, foot, 24, (off_t)cd - 24) != 24 || memcmp(foot + 8, "APK Sig Block 42", 16)) goto done;
    uint64_t size = rd64(foot);
    if (size < 32 || size > 64u << 20 || size + 8 > cd) goto done;
    uint8_t *blk = malloc(size + 8);
    if (!blk || pread(fd, blk, size + 8, (off_t)(cd - size - 8)) != (ssize_t)(size + 8)) { free(blk); goto done; }
    for (uint64_t p = 8; p + 12 <= size - 16 + 8 && !ok;) {
        uint64_t n = rd64(blk + p);
        uint32_t id = rd32(blk + p + 8);
        if (n < 4 || p + 8 + n > size + 8) break;
        if (id == 0x7109871au || id == 0xf05368c0u) {
            const uint8_t *v = blk + p + 12;                          /* signers: length-prefixed signer, length-prefixed signed data, digests, certificates */
            uint32_t sig_len = rd32(v), signer_len = rd32(v + 4), signed_len = rd32(v + 8), dig_len = rd32(v + 12);
            (void)sig_len; (void)signer_len; (void)signed_len;
            const uint8_t *certs = v + 16 + dig_len;
            uint32_t certs_len = rd32(certs), first_len = rd32(certs + 4);
            if (certs_len >= 4 + first_len && first_len > 0 && first_len < 8192) {
                uint8_t h[CC_SHA256_DIGEST_LENGTH];
                CC_SHA256(certs + 8, first_len, h);
                size_t k = 0;
                for (int i = 0; i < CC_SHA256_DIGEST_LENGTH && k + 4 < cap; i++) k += (size_t)snprintf(out + k, cap - k, "%s%02x", i ? ":" : "", h[i]);
                ok = true;
            }
        }
        p += 8 + n;
    }
    free(blk);
done:
    free(t); close(fd);
    return ok;
}

/* The game's own Java helpers (com.vectorunit.Vu*Helper): one instance each, and calls that have no answer here (ads, billing, analytics, notifications) do nothing. */
static jobj *singleton(const char *cls, jobj **slot) { if (!*slot) *slot = tl_jni_new_object(tl_jni_class(cls)); return tl_jni_ref(*slot); }
#define HELPER(fn, cls) static void fn(tl_jcall *c) { static jobj *o; c->ret = vl(singleton(cls, &o)); }
HELPER(H_sys, "com/vectorunit/VuSysHelper")
HELPER(H_billing, "com/vectorunit/VuBillingHelper")
HELPER(H_age, "com/vectorunit/VuAgeHelper")
HELPER(H_ad, "com/vectorunit/VuAdHelper")
HELPER(H_analytics, "com/vectorunit/VuAnalyticsHelper")
HELPER(H_notification, "com/vectorunit/VuNotificationHelper")
static void H_signature(tl_jcall *c) { c->ret = vl(tl_jni_new_string(S.signature)); }
static void H_alert(tl_jcall *c) { tl_log_line("sdl: the game shows an alert: \"%s\" / \"%s\"", Str(c->args[0].l), Str(c->args[1].l)); }
static void H_toast(tl_jcall *c) { tl_log_line("sdl: the game shows a toast: \"%s\"", Str(c->args[0].l)); }

/* A native of the game's own library, found by its JNI name (the game does not register them). */
static void *game_native(const char *mangled)
{
    tl_lib *lib = tl_ld_find_lib("libmain.so");
    void *fn = lib ? tl_ld_sym(lib, mangled) : NULL;
    if (!fn) tl_log_line("sdl: the game has no native %s", mangled);
    return fn;
}

/* VuSysHelper.refreshSafeAreaInsets reads the window's display cutout and reports it to the game; this screen has none. */
static int g_inset_l, g_inset_t, g_inset_r, g_inset_b;
static void H_refreshInsets(tl_jcall *c)
{
    (void)c;
    void (*set)(void *, void *, int, int, int, int) = game_native("Java_com_vectorunit_VuSysHelper_nativeSetSafeInsets");
    if (set) set(tl_jni_env(), tl_jni_class_object("com/vectorunit/VuSysHelper"), g_inset_l, g_inset_t, g_inset_r, g_inset_b);
}
void tl_sdl_set_safe_insets(int left, int top, int right, int bottom) { g_inset_l = left; g_inset_t = top; g_inset_r = right; g_inset_b = bottom; }

/* Game services: nobody is signed in to Google Play Games; the game is told so when it asks, and carries on. */
HELPER(H_services, "com/vectorunit/VuGameServicesHelper")
static void H_startSignIn(tl_jcall *c)
{
    (void)c;
    void (*fail)(void *, void *) = game_native("Java_com_vectorunit_VuGameServicesHelper_nativeOnSignInFailure");
    if (fail) fail(tl_jni_env(), tl_jni_class_object("com/vectorunit/VuGameServicesHelper"));
}

/* The age check answers "unknown", which is the game's own native callback; without it the game would wait for it. */
static void H_checkAge(tl_jcall *c)
{
    (void)c;
    void (*cb)(void *, void *) = game_native("Java_com_vectorunit_VuAgeHelper_nativeOnAgeSignalUnknown");
    if (cb) cb(tl_jni_env(), tl_jni_class_object("com/vectorunit/VuAgeHelper"));
}

/* JNI's name for an exported native: Java_<class with / as _>_<method>, with a literal underscore written _1. */
static void jni_mangle(char *out, size_t cap, const char *cls, const char *name)
{
    size_t n = 0;
    const char *parts[] = { "Java_", cls, "_", name };
    for (int i = 0; i < 4; i++)
        for (const char *p = parts[i]; *p && n + 3 < cap; p++) {
            if (i == 1 && *p == '/') out[n++] = '_';
            else if (*p == '_' && i != 0 && i != 2) { out[n++] = '_'; out[n++] = '1'; }
            else out[n++] = *p;
        }
    out[n] = 0;
}

/* A native of SDLActivity (or its helpers), if it has one: SDL3 registers them when it loads, SDL2 exports them under their JNI names. */
static void *find_native(const char *cls, const char *name, const char *sig)
{
    void *fn = tl_jni_native(cls, name, sig);
    if (!fn) fn = tl_jni_native(cls, name, NULL);              /* registered with another return type or arity: SDL's natives are unique by name */
    if (fn) return fn;
    char mangled[256];
    jni_mangle(mangled, sizeof(mangled), cls, name);
    const char *libs[] = { S.sdl_lib, S.main_lib, "libSDL2.so", "libmain.so" };
    for (int i = 0; i < 4 && !fn; i++) { tl_lib *lib = libs[i][0] ? tl_ld_find_lib(libs[i]) : NULL; if (lib) fn = tl_ld_sym(lib, mangled); }
    return fn;
}

/* A symbol of the game's own library (or SDL's), by name: for a game's Java shim that calls one of its natives. */
void *tl_sdl_game_symbol(const char *name)
{
    const char *libs[] = { S.main_lib, S.sdl_lib };
    for (int i = 0; i < 2; i++) { tl_lib *lib = libs[i][0] ? tl_ld_find_lib(libs[i]) : NULL; void *fn = lib ? tl_ld_sym(lib, name) : NULL; if (fn) return fn; }
    return NULL;
}

static void *native_of(const char *cls, const char *name, const char *sig)
{
    void *fn = find_native(cls, name, sig);
    if (!fn) tl_log_line("sdl: native %s.%s%s is not there", cls, name, sig);
    return fn;
}

/* SDLActivity.initTouch walks the input devices and reports each touch screen; there is one. */
static void A_initTouch(tl_jcall *c)
{
    (void)c;
    void (*add)(void *, void *, int, void *) = native_of(SDLA, "nativeAddTouch", "(ILjava/lang/String;)V");
    if (add) add(tl_jni_env(), tl_jni_class_object(SDLA), 1, tl_jni_new_string("Touchscreen"));
    atomic_store(&S.touch_ready, true);
}

/*
 * Controllers. SDL asks the activity which devices there are (SDLControllerManager.pollInputDevices) and is told of each with nativeAddJoystick; after that the
 * buttons and axes are native calls too. The gamepad layer shows every pad as an Xbox Wireless Controller, and says what changed through a sink.
 */
#define PAD_ID(slot) (100 + (slot))
static atomic_int g_pads_announced;
static void A_pollInputDevices(tl_jcall *c)
{
    (void)c;
    void (*add)(void *, void *, int, void *, void *, int, int, int, int, int, int, uint8_t) =
        S.sdl2 ? NULL : native_of(SDLCTRL, "nativeAddJoystick", "(ILjava/lang/String;Ljava/lang/String;IIIIIIZ)V");
    void (*remove)(void *, void *, int) = native_of(SDLCTRL, "nativeRemoveJoystick", "(I)V");
    for (int slot = 0; slot < TL_PADS; slot++) {
        bool connected = tl_pad_connected(slot), announced = (atomic_load(&g_pads_announced) >> slot) & 1;
        if (connected && !announced && add) {
            char desc[40]; snprintf(desc, sizeof(desc), "husk-xbox-%d", slot);
            /* 0x045e:0x02fd, an Xbox One S over Bluetooth; buttons A B X Y Back Guide Start Lstick Rstick L1 R1 and the D-pad; six axes (two sticks and two triggers), no hat -- the D-pad is buttons. */
            add(tl_jni_env(), tl_jni_class_object(SDLCTRL), PAD_ID(slot), tl_jni_new_string("Xbox Wireless Controller"), tl_jni_new_string(desc), 0x045e, 0x02fd, 0x7fff, 6, 0x003f, 0, 0);
            atomic_fetch_or(&g_pads_announced, 1 << slot);
        } else if (!connected && announced && remove) {
            remove(tl_jni_env(), tl_jni_class_object(SDLCTRL), PAD_ID(slot));
            atomic_fetch_and(&g_pads_announced, ~(1 << slot));
        }
    }
}

/* Android key codes the gamepad layer sends, in the order of SDL's own table. */
static void pad_key(jobj *ev, int device, int action, int keycode, int64_t down_ms, int64_t event_ms)
{
    (void)down_ms; (void)event_ms;
    int slot = device - 41;                                     /* the gamepad layer's device ids start at 41 */
    if (getenv("TL_PAD_TRACE")) tl_log_line("sdl: pad key %s %d on device %d (slot %d, announced %#x)", action == 0 ? "down" : "up", keycode, device, slot, atomic_load(&g_pads_announced));
    if (slot >= 0 && slot < TL_PADS && ((atomic_load(&g_pads_announced) >> slot) & 1)) {
        uint8_t (*fn)(void *, void *, int, int) = native_of(SDLCTRL, action == 0 ? "onNativePadDown" : "onNativePadUp", "(II)Z");
        if (fn) fn(tl_jni_env(), tl_jni_class_object(SDLCTRL), PAD_ID(slot), keycode);
    }
    tl_jni_unref(ev);
}
static void pad_motion(jobj *ev, int device, int source, int64_t down_ms, int64_t event_ms)
{
    (void)source; (void)down_ms; (void)event_ms;
    int slot = device - 41;
    float a[48];
    if (slot >= 0 && slot < TL_PADS && ((atomic_load(&g_pads_announced) >> slot) & 1) && tl_input_event_axes(ev, a)) {
        void (*joy)(void *, void *, int, int, float) = native_of(SDLCTRL, "onNativeJoy", "(IIF)V");
        if (joy) {
            /* Android axes X, Y, Z, RZ, LTRIGGER, RTRIGGER are SDL's axes 0..5, each normalised to -1..1 (a trigger at rest is -1). */
            const float v[6] = { a[0], a[1], a[11], a[14], a[17] * 2.0f - 1.0f, a[18] * 2.0f - 1.0f };
            for (int i = 0; i < 6; i++) joy(tl_jni_env(), tl_jni_class_object(SDLCTRL), PAD_ID(slot), i, v[i]);
        }
    }
    tl_jni_unref(ev);
}


/* ---- SDL 2's differences from SDL 3, and SuperTuxKart's own activity. */
static jvalue vf(float f) { jvalue v; v.j = 0; v.f = f; return v; }
static void A_float0(tl_jcall *c) { c->ret = vf(0.0f); }
static void A_one(tl_jcall *c) { c->ret = vi(1); }
static void A_two(tl_jcall *c) { c->ret = vi(2); }
static void A_nullObject(tl_jcall *c) { c->ret = vl(NULL); }

/* SDLActivity.getDisplayDPI: the screen's DisplayMetrics (SDL 2 reads xdpi/ydpi from it). */
static void A_displayDPI(tl_jcall *c)
{
    jobj *m = tl_jni_new_object(tl_jni_class("android/util/DisplayMetrics"));
    jvalue f; f.j = 0; f.f = 460.0f; tl_jni_set_field(m, "xdpi", "F", f); tl_jni_set_field(m, "ydpi", "F", f);
    f.f = 3.0f; tl_jni_set_field(m, "density", "F", f); tl_jni_set_field(m, "scaledDensity", "F", f);
    jvalue i; i.j = 0; i.i = 460; tl_jni_set_field(m, "densityDpi", "I", i);
    i.i = S.cfg.width; tl_jni_set_field(m, "widthPixels", "I", i);
    i.i = S.cfg.height; tl_jni_set_field(m, "heightPixels", "I", i);
    c->ret = vl(m);
}

void tl_sdl_display_metrics(tl_jcall *c) { A_displayDPI(c); }

/* The "extracting data" bar of SuperTuxKart: the game unpacks its assets itself on the first start and reports how far it is. */
static void STK_progress(tl_jcall *c)
{
    static int last = -1;
    if (c->args[0].i != last && (c->args[0].i % 10 == 0 || c->args[0].i >= 99)) tl_log_line("sdl: the game reports its data %d%% unpacked", c->args[0].i);
    last = c->args[0].i;
}
static void STK_splash(tl_jcall *c) { (void)c; tl_log_line("sdl: the game hides its splash screen"); }

/*
 * SDL 2's Java audio path (before it played through OpenSL ES itself): the app asks SDLAudioManager to open an output with a rate, a width and a channel count, then writes
 * buffers of samples to it from a thread of its own, each write returning when the audio has taken them. That is the audio sink every other engine here uses.
 */
extern void (*tl_cocos_audio_hook)(const int16_t *samples, int frames, int channels, int rate);
static struct { int rate, channels; } AU = { 44100, 2 };
static void AU_open(tl_jcall *c)
{
    AU.rate = c->args[0].i > 0 ? c->args[0].i : 44100; AU.channels = c->args[2].z ? 2 : 1;
    tl_log_line("sdl: audio opened, %d Hz, %d channel(s), %d frames", AU.rate, AU.channels, c->args[3].i);
    c->ret = vi(0);                                           /* 0 is success; the buffer size is whatever the app asked for */
}
static void AU_writeShort(tl_jcall *c)
{
    jobj *a = c->args[0].l;
    if (a && tl_cocos_audio_hook && a->arr.len >= (uint32_t)AU.channels) tl_cocos_audio_hook((const int16_t *)a->arr.data, (int)a->arr.len / AU.channels, AU.channels, AU.rate);
}
static void AU_writeFloat(tl_jcall *c)
{
    jobj *a = c->args[0].l;
    if (!a || !tl_cocos_audio_hook || a->arr.len < (uint32_t)AU.channels) return;
    int16_t *tmp = malloc(a->arr.len * sizeof(int16_t));
    if (!tmp) return;
    for (uint32_t i = 0; i < a->arr.len; i++) { float f = ((const float *)a->arr.data)[i]; tmp[i] = (int16_t)(f > 1 ? 32767 : f < -1 ? -32767 : f * 32767.0f); }
    tl_cocos_audio_hook(tmp, (int)a->arr.len / AU.channels, AU.channels, AU.rate);
    free(tmp);
}
static void AU_writeByte(tl_jcall *c)                                                     /* 8-bit unsigned samples */
{
    jobj *a = c->args[0].l;
    if (!a || !tl_cocos_audio_hook || a->arr.len < (uint32_t)AU.channels) return;
    int16_t *tmp = malloc(a->arr.len * sizeof(int16_t));
    if (!tmp) return;
    for (uint32_t i = 0; i < a->arr.len; i++) tmp[i] = (int16_t)((((const uint8_t *)a->arr.data)[i] - 128) << 8);
    tl_cocos_audio_hook(tmp, (int)a->arr.len / AU.channels, AU.channels, AU.rate);
    free(tmp);
}
static void AU_captureOpen(tl_jcall *c) { c->ret = vi(-1); }
/* SDLActivity.inputGetInputDeviceIds(sources): the devices of those kinds. Old SDL 2 finds its touch screen this way (the same one initTouch reports for newer SDL);
 * no gamepad is listed, those arrive through pollInputDevices. */
static void A_inputDeviceIds(tl_jcall *c)
{
    int sources = c->args[0].i;
    if (getenv("TL_SDL_TRACE")) tl_log_line("sdl: inputGetInputDeviceIds(%#x)", sources);
    bool touch = (sources & 0x1002) == 0x1002 || (sources & 0x100008) || (sources & 0x200000);          /* SOURCE_TOUCHSCREEN, _TOUCHPAD, _TOUCH_NAVIGATION */
    jobj *a = tl_jni_new_prim_array('I', touch ? 1 : 0);
    if (touch) { ((int *)a->arr.data)[0] = 1; S.touch_as_mouse = true; }
    c->ret = vl(a);
}

#define M_(c, n, s, f) { c, n, s, f }
static const tl_jhle k_hle[] = {
    M_(SDLA, "getContext", "()Landroid/content/Context;", A_context),
    M_(SDLA, "getNativeSurface", "()Landroid/view/Surface;", A_surface),
    M_(SDLA, "initTouch", "()V", A_initTouch),
    M_(SDLA, "setActivityTitle", "(Ljava/lang/String;)Z", A_true),
    M_(SDLA, "isAndroidTV", "()Z", A_false),
    M_(SDLA, "isTablet", "()Z", A_false),
    M_(SDLA, "isChromebook", "()Z", A_false),
    M_(SDLA, "isDeXMode", "()Z", A_false),
    M_(SDLA, "getManifestEnvironmentVariables", "()Z", A_true),
    M_(SDLA, "getPreferredLocales", "()Ljava/lang/String;", A_locales),
    M_(SDLA, "setOrientation", "(IIZLjava/lang/String;)V", A_void),
    M_(SDLA, "shouldMinimizeOnFocusLoss", "()Z", A_false),
    M_(SDLA, "isScreenKeyboardShown", "()Z", A_false),
    M_(SDLA, "showTextInput", "(IIIII)Z", A_showTextInput),
    M_(SDLA, "supportsRelativeMouse", "()Z", A_false),
    M_(SDLA, "setRelativeMouseEnabled", "(Z)Z", A_false),
    M_(SDLA, "setWindowStyle", "(Z)V", A_void),
    M_(SDLA, "minimizeWindow", "()V", A_void),
    M_(SDLA, "sendMessage", "(II)Z", A_sendMessage),
    M_(SDLA, "openURL", "(Ljava/lang/String;)Z", A_false),
    M_(SDLA, "requestPermission", "(Ljava/lang/String;I)V", A_void),
    M_(SDLA, "showToast", "(Ljava/lang/String;IIII)Z", A_true),
    M_(SDLA, "clipboardGetText", "()Ljava/lang/String;", A_emptyString),
    M_(SDLA, "clipboardHasText", "()Z", A_false),
    M_(SDLA, "clipboardSetText", "(Ljava/lang/String;)V", A_void),
    M_(SDLA, "createCustomCursor", "([IIIII)I", A_zero),
    M_(SDLA, "setCustomCursor", "(I)Z", A_false),
    M_(SDLA, "setSystemCursor", "(I)Z", A_false),
    M_(SDLA, "showFileDialog", "([Ljava/lang/String;ZZI)Z", A_false),
    M_(SDLA, "openFileDescriptor", "(Ljava/lang/String;Ljava/lang/String;)I", A_minus1),
    M_("java/lang/ClassLoader", "findClass", "(Ljava/lang/String;)Ljava/lang/Class;", CL_findClass),
    M_("java/lang/ClassLoader", "loadClass", "(Ljava/lang/String;)Ljava/lang/Class;", CL_findClass),
    M_("dalvik/system/PathClassLoader", "findClass", "(Ljava/lang/String;)Ljava/lang/Class;", CL_findClass),
    M_("dalvik/system/PathClassLoader", "loadClass", "(Ljava/lang/String;)Ljava/lang/Class;", CL_findClass),
    M_("com/vectorunit/VuSysHelper", "getInstance", "()Lcom/vectorunit/VuSysHelper;", H_sys),
    M_("com/vectorunit/VuSysHelper", "getSignature", "()Ljava/lang/String;", H_signature),
    M_("com/vectorunit/VuSysHelper", "hasSystemFeature", "(Ljava/lang/String;)Z", A_false),
    M_("com/vectorunit/VuSysHelper", "isTablet", "()Z", A_false),
    M_("com/vectorunit/VuSysHelper", "openURL", "(Ljava/lang/String;)Z", A_false),
    M_("com/vectorunit/VuSysHelper", "showAlert", "(Ljava/lang/String;Ljava/lang/String;)V", H_alert),
    M_("com/vectorunit/VuSysHelper", "showToast", "(Ljava/lang/String;)V", H_toast),
    M_("com/vectorunit/VuSysHelper", "refreshSafeAreaInsets", "()V", H_refreshInsets),
    M_("com/vectorunit/VuGameServicesHelper", "getInstance", "()Lcom/vectorunit/VuGameServicesHelper;", H_services),
    M_("com/vectorunit/VuGameServicesHelper", "startSignIn", "()V", H_startSignIn),
    M_("com/vectorunit/VuBillingHelper", "getInstance", "()Lcom/vectorunit/VuBillingHelper;", H_billing),
    M_("com/vectorunit/VuAgeHelper", "getInstance", "()Lcom/vectorunit/VuAgeHelper;", H_age),
    M_("com/vectorunit/VuAgeHelper", "checkAgeSignal", "()V", H_checkAge),
    M_("com/vectorunit/VuAdHelper", "getInstance", "()Lcom/vectorunit/VuAdHelper;", H_ad),
    M_("com/vectorunit/VuAdHelper", "areAdsPossible", "()Z", A_false),
    M_("com/vectorunit/VuAdHelper", "canShowPrivacyOptions", "()Z", A_false),
    M_("com/vectorunit/VuAnalyticsHelper", "getInstance", "()Lcom/vectorunit/VuAnalyticsHelper;", H_analytics),
    M_("com/vectorunit/VuNotificationHelper", "getInstance", "()Lcom/vectorunit/VuNotificationHelper;", H_notification),
    M_(SDLAUDIO, "audioSetThreadPriority", "(ZI)V", A_void),
    M_(SDLAUDIO, "audioOpen", "(IZZI)I", AU_open),
    M_(SDLAUDIO, "audioInit", "(IZZI)I", AU_open),
    M_(SDLAUDIO, "audioWriteShortBuffer", "([S)V", AU_writeShort),
    M_(SDLAUDIO, "audioWriteFloatBuffer", "([F)V", AU_writeFloat),
    M_(SDLAUDIO, "audioWriteByteBuffer", "([B)V", AU_writeByte),
    M_(SDLAUDIO, "audioClose", "()V", A_void),
    M_(SDLAUDIO, "audioQuit", "()V", A_void),
    M_(SDLAUDIO, "captureOpen", "(IZZI)I", AU_captureOpen),
    M_(SDLA, "inputGetInputDeviceIds", "(I)[I", A_inputDeviceIds),
    M_(SDLCTRL, "pollInputDevices", "()V", A_pollInputDevices),
    M_(SDLCTRL, "pollHapticDevices", "()V", A_void),
    M_(SDLCTRL, "hapticRun", "(IFI)V", A_void),
    M_(SDLCTRL, "hapticRumble", "(IFFI)V", A_void),
    M_(SDLCTRL, "hapticStop", "(I)V", A_void),
    /* SDL 2's SDLActivity: the same questions as SDL 3's, a few asked with other signatures. */
    M_(SDLA, "showTextInput", "(IIII)Z", A_showTextInput),
    M_(SDLA, "openURL", "(Ljava/lang/String;)I", A_minus1),
    M_(SDLA, "showToast", "(Ljava/lang/String;IIII)I", A_zero),
    M_(SDLA, "getDisplayDPI", "()Landroid/util/DisplayMetrics;", A_displayDPI),
    M_(SDLA, "jniRefDisplaySize", "()I", A_refDisplaySize),
    M_(SDLA, "getCurrentOrientation", "()I", A_one),
    M_(SDLA, "manualBackButton", "()V", A_void),
    M_(SDLA, "destroyCustomCursor", "(I)V", A_void),
    /* SDL 2's HID bridge: it is told there are no USB or Bluetooth HID devices to find, and nothing to open. */
    M_(SDLHID, "initialize", "(ZZ)Z", A_true),
    M_(SDLHID, "openDevice", "(I)Z", A_false),
    M_(SDLHID, "closeDevice", "(I)V", A_void),
    M_(SDLHID, "sendOutputReport", "(I[B)I", A_minus1),
    M_(SDLHID, "sendFeatureReport", "(I[B)I", A_minus1),
    M_(SDLHID, "getFeatureReport", "(I[B)Z", A_false),
    /* SuperTuxKart's activity: its edit box, the display cutout paddings, the unpacking progress, the DNS lookups. */
    M_(STKA, "getScreenSize", "()I", A_two),
    M_(STKA, "getInitialOrientation", "()I", A_one),
    M_(STKA, "getKeyboardHeight", "()I", A_zero),
    M_(STKA, "getMovedHeight", "()I", A_zero),
    M_(STKA, "getTopPadding", "()F", A_float0),
    M_(STKA, "getBottomPadding", "()F", A_float0),
    M_(STKA, "getLeftPadding", "()F", A_float0),
    M_(STKA, "getRightPadding", "()F", A_float0),
    M_(STKA, "isHardwareKeyboardConnected", "()Z", A_false),
    M_(STKA, "showKeyboard", "(II)V", A_void),
    M_(STKA, "hideKeyboard", "(Z)V", A_void),
    M_(STKA, "hideSplashScreen", "()V", STK_splash),
    M_(STKA, "showExtractProgress", "(I)V", STK_progress),
    M_(STKA, "getDNSTxtRecords", "(Ljava/lang/String;)[Ljava/lang/String;", A_nullObject),
    M_(STKA, "getDNSSrvRecords", "(Ljava/lang/String;)V", A_void),
    { NULL, NULL, NULL, NULL }
};

/* ------------------------------------------------------------------- start */

static void load_library(const char *name)
{
    jvalue a; a.j = 0; a.l = tl_jni_new_string(name);
    tl_jni_call(tl_jni_class_object("java/lang/System"), "loadLibrary", "(Ljava/lang/String;)V", &a);
}

static void mkdirs(const char *path)
{
    char t[1024]; snprintf(t, sizeof(t), "%s", path);
    for (char *p = t + 1; *p; p++) if (*p == '/') { *p = 0; mkdir(t, 0755); *p = '/'; }
    mkdir(t, 0755);
}

bool tl_sdl_add_package(const char *apk_path) { return tl_ld_add_apk(apk_path); }

/* Which of the app's libraries is SDL and which is the game: most ship libSDL2/libSDL3 and libmain, but some name their own library, and some link SDL into it. */
struct libscan { bool sdl2, sdl3, main; char best[64]; uint64_t best_size; char cand[4][64]; int ncand; };
static void scan_lib(const char *name, uint64_t size, void *user)
{
    struct libscan *r = user;
    if (!strcmp(name, "libSDL2.so")) { r->sdl2 = true; return; }
    if (!strcmp(name, "libSDL3.so")) { r->sdl3 = true; return; }
    if (!strcmp(name, "libmain.so")) { r->main = true; return; }
    static const char *const support[] = { "libc++_shared", "libSDL", "libhidapi", "libmpg123", "libopenal", "libfmod", "libsentry", "libFirebase", "libcrashlytics" };
    for (size_t i = 0; i < sizeof(support) / sizeof(support[0]); i++) if (!strncmp(name, support[i], strlen(support[i]))) return;
    if (r->ncand < 4) snprintf(r->cand[r->ncand++], 64, "%s", name);
    if (size > r->best_size) { r->best_size = size; snprintf(r->best, sizeof(r->best), "%s", name); }
}

/* The manifest's activity that extends SDLActivity: the launcher is sometimes a menu in front of it (Dungeon Crawl, Wesnoth). */
/* One string of an Android binary XML string pool, as UTF-8. */
static bool axml_string(const uint8_t *pool, size_t pool_size, uint32_t index, char *out, size_t n)
{
    uint32_t count = rd32(pool + 8), flags = rd32(pool + 16), strings = rd32(pool + 20);
    if (index >= count || 28 + 4ull * index + 4 > pool_size) return false;
    size_t off = strings + rd32(pool + 28 + 4 * index);
    if (off + 4 > pool_size) return false;
    const uint8_t *p = pool + off;
    if (flags & 0x100) {
        size_t l = *p++; if (l & 0x80) p++;
        size_t b = *p++; if (b & 0x80) b = ((b & 0x7F) << 8) | *p++;
        if (b >= n) b = n - 1;
        memcpy(out, p, b); out[b] = 0;
    } else {
        size_t l = (size_t)(p[0] | (p[1] << 8)); p += 2;
        if (l & 0x8000) { l = ((l & 0x7FFF) << 16) | (size_t)(p[0] | (p[1] << 8)); p += 2; }
        size_t k = 0;
        for (size_t i = 0; i < l && k + 1 < n; i++) { uint16_t c = (uint16_t)(p[2 * i] | (p[2 * i + 1] << 8)); out[k++] = c < 0x80 ? (char)c : '?'; }
        out[k] = 0;
    }
    return true;
}

static bool find_sdl_activity(const char *apk, const char *package, char *out, size_t cap)
{
    tl_zip z; char err[160]; bool found = false;
    if (!tl_zip_open(&z, apk, err, sizeof(err))) return false;
    const tl_zip_entry *e = tl_zip_find(&z, "AndroidManifest.xml");
    const uint8_t *data; size_t len; bool owned = false;
    if (e && tl_zip_data(&z, e, 8u << 20, &data, &len, &owned, err, sizeof(err)) && len > 8) {
        const uint8_t *pool = NULL; size_t pool_size = 0;
        for (size_t off = (size_t)(data[2] | (data[3] << 8)); off + 8 <= len && !found; ) {
            uint16_t type = (uint16_t)(data[off] | (data[off + 1] << 8)); uint32_t size = rd32(data + off + 4);
            if (size < 8 || off + size > len) break;
            if (type == 0x0001) { pool = data + off; pool_size = size; }
            else if (type == 0x0102 && pool && off + 36 <= len) {
                const uint8_t *el = data + off; char tag[24];
                if (axml_string(pool, pool_size, rd32(el + 20), tag, sizeof(tag)) && (!strcmp(tag, "activity") || !strcmp(tag, "activity-alias"))) {
                    uint16_t astart = (uint16_t)(el[24] | (el[25] << 8)), asize = (uint16_t)(el[26] | (el[27] << 8)), acount = (uint16_t)(el[28] | (el[29] << 8));
                    for (unsigned i = 0; i < acount; i++) {
                        const uint8_t *at = el + 16 + astart + (size_t)i * asize; char an[24], av[200] = "";
                        if (at + 20 > data + len || !axml_string(pool, pool_size, rd32(at + 4), an, sizeof(an)) || strcmp(an, "name")) continue;
                        if (rd32(at + 8) != 0xFFFFFFFFu) axml_string(pool, pool_size, rd32(at + 8), av, sizeof(av));
                        char full[260];
                        if (av[0] == '.') snprintf(full, sizeof(full), "%s%s", package, av);
                        else if (!strchr(av, '.')) snprintf(full, sizeof(full), "%s.%s", package, av);
                        else snprintf(full, sizeof(full), "%s", av);
                        for (char *c = full; *c; c++) if (*c == '.') *c = '/';
                        /* its superclasses, up the dex, until SDLActivity */
                        char cur[260], sup[260]; snprintf(cur, sizeof(cur), "%s", full);
                        for (int hop = 0; hop < 8 && !found; hop++) {
                            if (!strcmp(cur, SDLA)) { snprintf(out, cap, "%s", full); found = true; break; }
                            if (!tl_dexidx_super(cur, sup, sizeof(sup))) break;
                            snprintf(cur, sizeof(cur), "%s", sup);
                        }
                    }
                }
            }
            off += size;
        }
    }
    if (owned) free((void *)data);
    tl_zip_close(&z);
    return found;
}

/* Whether the app asks for a portrait screen: the first activity that names an orientation (android:screenOrientation) decides. Landscape, sensor and "unspecified" are the
 * landscape the runtime always used. Values: 0 landscape, 1 portrait, 6 sensorLandscape, 7 sensorPortrait, 8 reverseLandscape, 9 reversePortrait, 11 userLandscape, 12 userPortrait. */
bool tl_sdl_manifest_portrait(const char *apk)
{
    tl_zip z; char err[160]; bool portrait = false, decided = false;
    if (!tl_zip_open(&z, apk, err, sizeof(err))) return false;
    const tl_zip_entry *e = tl_zip_find(&z, "AndroidManifest.xml");
    const uint8_t *data; size_t len; bool owned = false;
    if (e && tl_zip_data(&z, e, 8u << 20, &data, &len, &owned, err, sizeof(err)) && len > 8) {
        const uint8_t *pool = NULL; size_t pool_size = 0;
        for (size_t off = (size_t)(data[2] | (data[3] << 8)); off + 8 <= len && !decided; ) {
            uint16_t type = (uint16_t)(data[off] | (data[off + 1] << 8)); uint32_t size = rd32(data + off + 4);
            if (size < 8 || off + size > len) break;
            if (type == 0x0001) { pool = data + off; pool_size = size; }
            else if (type == 0x0102 && pool && off + 36 <= len) {
                const uint8_t *el = data + off; char tag[24];
                if (axml_string(pool, pool_size, rd32(el + 20), tag, sizeof(tag)) && (!strcmp(tag, "activity") || !strcmp(tag, "activity-alias"))) {
                    uint16_t astart = (uint16_t)(el[24] | (el[25] << 8)), asize = (uint16_t)(el[26] | (el[27] << 8)), acount = (uint16_t)(el[28] | (el[29] << 8));
                    for (unsigned i = 0; i < acount; i++) {
                        const uint8_t *at = el + 16 + astart + (size_t)i * asize; char an[32];
                        if (at + 20 > data + len || !axml_string(pool, pool_size, rd32(at + 4), an, sizeof(an)) || strcmp(an, "screenOrientation")) continue;
                        int v = (int)rd32(at + 16);
                        if (v == 1 || v == 7 || v == 9 || v == 12) { portrait = true; decided = true; }
                        else if (v == 0 || v == 6 || v == 8 || v == 11) decided = true;
                    }
                }
            }
            off += size;
        }
    }
    if (owned) free((void *)data);
    tl_zip_close(&z);
    return portrait;
}

bool tl_sdl_start(const tl_ga_config *cfg, const char *activity_class)
{
    S.cfg = *cfg;
    snprintf(S.apk, sizeof(S.apk), "%s", cfg->apk_path);
    snprintf(S.data, sizeof(S.data), "%s", cfg->data_dir);
    snprintf(S.pkg, sizeof(S.pkg), "%s", cfg->package_name);
    snprintf(S.activity_class, sizeof(S.activity_class), "%s", activity_class ? activity_class : "");
    S.cfg.apk_path = S.apk; S.cfg.data_dir = S.data; S.cfg.package_name = S.pkg;
    if (cfg->frame_dir) { snprintf(S.frame_dir, sizeof(S.frame_dir), "%s", cfg->frame_dir); S.cfg.frame_dir = S.frame_dir; }
    if (cfg->angle_egl) { snprintf(S.angle_egl, sizeof(S.angle_egl), "%s", cfg->angle_egl); S.cfg.angle_egl = S.angle_egl; }
    if (cfg->angle_gles) { snprintf(S.angle_gles, sizeof(S.angle_gles), "%s", cfg->angle_gles); S.cfg.angle_gles = S.angle_gles; }

    char dir[700];
    snprintf(dir, sizeof(dir), "%s/files", S.data); mkdirs(dir);
    snprintf(dir, sizeof(dir), "%s/sdcard/Android/data/%s/files", S.data, S.pkg); mkdirs(dir);

    tl_set_data_dir(cfg->data_dir);
    tl_nwindow_configure(cfg->width, cfg->height, cfg->metal_layer);
    int n = tl_dexidx_open(cfg->apk_path);
    tl_log_line("sdl: %d classes in the APK's DEX", n);
    if (!tl_ld_add_apk(cfg->apk_path)) return false;
    /* The activity SDL runs under: the one the caller named if it is an SDLActivity, else the manifest's activity that is (Dungeon Crawl and Wesnoth put a menu in front). */
    {
        bool ok = false;
        char cur[260], sup[260];
        snprintf(cur, sizeof(cur), "%s", S.activity_class);
        for (int hop = 0; S.activity_class[0] && hop < 8 && !ok; hop++) {
            if (!strcmp(cur, SDLA)) ok = true;
            else if (tl_dexidx_super(cur, sup, sizeof(sup))) snprintf(cur, sizeof(cur), "%s", sup);
            else break;
        }
        if (!ok) {
            char found[260];
            if (find_sdl_activity(cfg->apk_path, cfg->package_name, found, sizeof(found))) { snprintf(S.activity_class, sizeof(S.activity_class), "%s", found); tl_log_line("sdl: the SDL activity is %s", found); }
            else if (!S.activity_class[0]) { tl_log_line("sdl: the manifest has no activity that extends SDLActivity"); return false; }
        }
    }
    /* Which libraries are SDL and the game, and which SDL: said by what the APK ships and by the SDLActivity it carries. */
    struct libscan ls = { 0 };
    tl_ld_apk_libs(scan_lib, &ls);
    snprintf(S.sdl_lib, sizeof(S.sdl_lib), "%s", ls.sdl3 ? "libSDL3.so" : ls.sdl2 ? "libSDL2.so" : "");
    snprintf(S.main_lib, sizeof(S.main_lib), "%s", ls.main ? "libmain.so" : ls.best);
    {
        bool st;
        if (tl_dexidx_has_class(SDLA)) S.sdl2 = !tl_dexidx_declares_method(SDLA, "nativeSetNaturalOrientation", "", &st);
        else S.sdl2 = !ls.sdl3;
    }
    tl_log_line("sdl: SDL %d, library %s, game library %s", S.sdl2 ? 2 : 3, S.sdl_lib[0] ? S.sdl_lib : "(linked into the game)", S.main_lib);
    if (cfg->angle_egl && !tl_egl_init(cfg->angle_egl, cfg->angle_gles, cfg->frame_dir, cfg->frame_every)) return false;
    tl_jni_init();
    tl_hle_configure(cfg->package_name, cfg->apk_path, cfg->data_dir, cfg->width, cfg->height);
    tl_jni_hle_install();
    tl_jni_declare("android/app/NativeActivity", "android/app/Activity");
    tl_jni_declare(SDLA, "android/app/Activity");
    tl_jni_declare(S.activity_class, SDLA);
    tl_jni_declare("android/view/Surface", "java/lang/Object");
    tl_jni_register_hle(k_hle);
    tl_assetstream_install();
    tl_sdl_apps_install(S.pkg);
    tl_fmod_install();
    if (!apk_cert_sha256(cfg->apk_path, S.signature, sizeof(S.signature))) { S.signature[0] = 0; tl_log_line("sdl: the APK has no signing certificate to read"); }
    S.activity = tl_jni_new_object(tl_jni_class(S.activity_class));
    tl_hle_set_activity(S.activity);

    /* SDLActivity.loadLibraries: the libraries getLibraries() names. SDL 3 games name SDL3 and their own library; an SDL 2 game (SuperTuxKart) names only SDL2 and the
     * main library is opened by nativeRunMain; one with SDL linked into its own library (Luanti, TheXTech, Brogue) names just that. */
    {
        char name[80]; const char *load[3]; int nl = 0;
        if (S.sdl_lib[0]) { snprintf(name, sizeof(name), "%s", S.sdl_lib + 3); name[strlen(name) - 3] = 0; load[nl++] = strdup(name); }
        if ((!S.sdl_lib[0] || !S.sdl2) && S.main_lib[0]) { snprintf(name, sizeof(name), "%s", S.main_lib + 3); name[strlen(name) - 3] = 0; load[nl++] = strdup(name); }
        for (int i = 0; i < nl; i++) {
            load_library(load[i]);
            if (tl_jni_pending()) { tl_log_line("sdl: loading lib%s.so failed", load[i]); return false; }
        }
        /* Several libraries of the game's own and none called libmain (LOVE: a big liblove library and a small one that holds SDL_main): the one that exports SDL_main is the entry. */
        if (!ls.main && ls.ncand > 1) {
            for (int i = 0; i < ls.ncand; i++) {
                snprintf(name, sizeof(name), "%s", ls.cand[i] + 3); name[strlen(name) - 3] = 0;
                if (strcmp(ls.cand[i], S.main_lib)) { load_library(name); if (tl_jni_pending()) tl_jni_clear(); }
                tl_lib *cl = tl_ld_find_lib(ls.cand[i]);
                if (cl && (tl_ld_sym(cl, "SDL_main") || tl_ld_sym(cl, "SDL_Main"))) { snprintf(S.main_lib, sizeof(S.main_lib), "%s", ls.cand[i]); break; }
            }
            tl_log_line("sdl: entry library %s", S.main_lib);
        }
    }
    /* The game's own onCreate then loads FMOD (whose JNI_OnLoad is what lets it find the Java side) and initialises it. */
    static const char *const fmod[] = { "fmod", "fmodstudio", NULL };
    if (tl_ld_has_lib("libfmod.so")) {
        for (int i = 0; fmod[i]; i++) load_library(fmod[i]);
        if (tl_jni_pending()) tl_jni_clear();
    }
    /* The function SDLActivity runs on its thread: SDL_main, which a game that links SDL into its own library may name SDL_Main. */
    snprintf(S.main_fn, sizeof(S.main_fn), "SDL_main");
    {
        tl_lib *ml = S.main_lib[0] ? tl_ld_find_lib(S.main_lib) : NULL;
        if (ml && !tl_ld_sym(ml, "SDL_main") && tl_ld_sym(ml, "SDL_Main")) snprintf(S.main_fn, sizeof(S.main_fn), "SDL_Main");
    }
    tl_log_line("sdl: libraries loaded (entry %s)", S.main_fn);
    /* The D-pad as buttons (DPAD_UP...), the way SDL maps them, rather than as the hat the gamepad layer sends by default. Read by the layer at the first controller update. */
    setenv("TL_PAD_DPAD", "keys", 0);
    static const tl_pad_sink sink = { pad_key, pad_motion };
    if (!S.sdl2) tl_pad_set_sink(&sink);
    S.started = true;
    return true;
}

/* SDLMain: SDL_main runs on a thread of its own, started once the surface is ready. */
static void *sdl_main_thread(void *arg)
{
    (void)arg;
    pthread_setname_np("SDLThread");
    void *env = tl_jni_env();
    void *cls = tl_jni_class_object(SDLA);
    void (*init_main)(void *, void *) = find_native(SDLA, "nativeInitMainThread", "()V");
    int (*run_main)(void *, void *, void *, void *, void *) = native_of(SDLA, "nativeRunMain", "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/Object;)I");
    void (*cleanup)(void *, void *) = find_native(SDLA, "nativeCleanupMainThread", "()V");
    if (!run_main) return NULL;
    if (init_main) init_main(env, cls);
    tl_log_line("sdl: SDL_main starting");
    int r = run_main(env, cls, tl_jni_new_string(S.main_lib[0] ? S.main_lib : "libmain.so"), tl_jni_new_string(S.main_fn[0] ? S.main_fn : "SDL_main"), tl_jni_new_obj_array(tl_jni_class("java/lang/String"), 0));
    tl_log_line("sdl: SDL_main returned %d", r);
    if (cleanup) cleanup(env, cls);
    return NULL;
}

static void *ui_main(void *arg)
{
    (void)arg;
    pthread_setname_np("UiThread");
    ((void *(*)(int))tl_bionic_find("ALooper_prepare"))(0);
    void *env = tl_jni_env();
    void *cls = tl_jni_class_object(SDLA);
    int w = S.cfg.width, h = S.cfg.height;

    /* SDL.setupJNI: each of SDL's Java classes tells its native half where to find the methods it will call. */
    int (*setup)(void *, void *) = native_of(SDLA, "nativeSetupJNI", "()I");
    if (setup) setup(env, cls);
    int (*setup_audio)(void *, void *) = native_of(SDLAUDIO, "nativeSetupJNI", "()I");
    if (setup_audio) setup_audio(env, tl_jni_class_object(SDLAUDIO));
    int (*setup_ctrl)(void *, void *) = native_of(SDLCTRL, "nativeSetupJNI", "()I");
    if (setup_ctrl) setup_ctrl(env, tl_jni_class_object(SDLCTRL));

    /* SDL 2's activity also starts its HID bridge (HIDDeviceManager.acquire): the native half keeps the VM and the bridge object to call back into, and SDL's joystick
     * start-up asks it for devices -- with neither set it dereferences a null VM. */
    if (S.sdl2 && tl_dexidx_has_class(SDLHID)) {
        void (*hid_register)(void *, void *) = find_native(SDLHID, "HIDDeviceRegisterCallback", "()V");
        if (hid_register) hid_register(env, tl_jni_new_object(tl_jni_class(SDLHID)));
    }

    /* The game's own activity hands its command line over before SDL starts. */
    void (*set_cmdline)(void *, void *, void *) = find_native(S.activity_class, "nativeSetCmdLine", "(Ljava/lang/String;)V");
    if (set_cmdline) set_cmdline(env, tl_jni_class_object(S.activity_class), tl_jni_new_string(""));

    if (S.sdl2) {
        /* SDLSurface.surfaceChanged: the screen's size, then the rotation (SDL_ORIENTATION_LANDSCAPE, as a phone held sideways), in that order. */
        /* Newer SDL 2 takes the density as well as the refresh rate: the app's own SDLActivity says which. */
        bool st;
        void *res = find_native(SDLA, "nativeSetScreenResolution", "(IIIIF)V");
        if (res && tl_dexidx_declares_method(SDLA, "nativeSetScreenResolution", "(IIIIFF)V", &st)) ((void (*)(void *, void *, int, int, int, int, float, float))res)(env, cls, w, h, w, h, 3.0f, 60.0f);
        else if (res && tl_dexidx_declares_method(SDLA, "nativeSetScreenResolution", "(IIIIIF)V", &st)) ((void (*)(void *, void *, int, int, int, int, int, float))res)(env, cls, w, h, w, h, 0x16462004 /* SDL_PIXELFORMAT_RGBA8888 */, 60.0f);
        else if (res) ((void (*)(void *, void *, int, int, int, int, float))res)(env, cls, w, h, w, h, 60.0f);
        else {
            /* The oldest SDL 2 sets the display with onNativeResize(width, height, pixel format, refresh rate). */
            void *rs = find_native(SDLA, "onNativeResize", "(IIIF)V");
            if (rs && tl_dexidx_declares_method(SDLA, "onNativeResize", "(IIIF)V", &st)) ((void (*)(void *, void *, int, int, int, float))rs)(env, cls, w, h, 0x16462004, 60.0f);
        }
        void (*orient)(void *, void *, int) = find_native(SDLA, "onNativeOrientationChanged", "(I)V");
        if (orient) orient(env, cls, w < h ? 3 : 1);          /* SDL_ORIENTATION_PORTRAIT / LANDSCAPE */
    } else {
        /* onCreate: orientation, rotation, the screen. SDL_ORIENTATION_PORTRAIT is the natural one of a phone; the surface is landscape, so rotated once. */
        void (*nat_orient)(void *, void *, int) = native_of(SDLA, "nativeSetNaturalOrientation", "(I)V");
        if (nat_orient) nat_orient(env, cls, 3);
        void (*rotation)(void *, void *, int) = native_of(SDLA, "onNativeRotationChanged", "(I)V");
        if (rotation) rotation(env, cls, w < h ? 0 : 1);       /* ROTATION_0 on a portrait surface */
        void (*insets)(void *, void *, int, int, int, int) = native_of(SDLA, "onNativeInsetsChanged", "(IIII)V");
        if (insets) insets(env, cls, 0, 0, 0, 0);
        void (*resolution)(void *, void *, int, int, int, int, float, float) = native_of(SDLA, "nativeSetScreenResolution", "(IIIIFF)V");
        if (resolution) resolution(env, cls, w, h, w, h, 3.0f, 60.0f);
    }

    /* SDLSurface.surfaceCreated / surfaceChanged, then the activity resuming with focus. */
    void (*vv)(void *, void *);
    bool st2;
    if ((vv = find_native(SDLA, "onNativeSurfaceCreated", "()V"))) vv(env, cls);
    if (!tl_dexidx_declares_method(SDLA, "onNativeResize", "(IIIF)V", &st2) && (vv = find_native(SDLA, "onNativeResize", "()V"))) vv(env, cls);
    if ((vv = find_native(SDLA, "onNativeSurfaceChanged", "()V"))) vv(env, cls);
    void (*focus)(void *, void *, uint8_t) = find_native(SDLA, "nativeFocusChanged", "(Z)V");
    if (focus) focus(env, cls, 1);
    /* No nativeResume: SDLActivity does not send one when it starts the app thread, and SDL starts un-paused. A resume that was not preceded by a pause makes SDL release
     * the GL context it believes it saved, and with nothing saved there is nothing to put back. */
    tl_log_line("sdl: lifecycle delivered");

    pthread_attr_t a; pthread_attr_init(&a); pthread_attr_setstacksize(&a, 8u << 20);
    if (pthread_create(&S.sdl, &a, sdl_main_thread, NULL) != 0) tl_log_line("sdl: cannot start the SDL thread");
    pthread_attr_destroy(&a);

    /* A real activity gets its window focus after SDL's window exists (the focus event above came first and found no window to give it to), so say it again then. */
    for (int i = 0; i < 300 && tl_egl_frames_presented() == 0; i++) usleep(10000);
    usleep(100000);
    if (focus) focus(env, cls, 1);

    /* The activity's message loop: nothing to serve beyond keeping the thread (and its looper) alive. */
    int (*poll_once)(int, int *, int *, void **) = tl_bionic_find("ALooper_pollOnce");
    for (;;) { poll_once(200, NULL, NULL, NULL); if (tl_jni_pending()) tl_jni_clear(); }
    return NULL;
}

bool tl_sdl_run(void)
{
    if (!S.started) return false;
    pthread_attr_t a;
    pthread_attr_init(&a);
    pthread_attr_setstacksize(&a, 8u << 20);
    if (pthread_create(&S.ui, &a, ui_main, NULL) != 0) return false;
    return true;
}

unsigned long tl_sdl_frames(void) { return tl_egl_frames_presented(); }

/* Text and keys from the app's keyboard: SDLInputConnection.nativeCommitText for characters, onNativeKeyDown/Up for Backspace (67) and Enter (66). */
void tl_sdl_commit_text(const char *utf8)
{
    void (*commit)(void *, void *, void *, int) = find_native("org/libsdl/app/SDLInputConnection", "nativeCommitText", "(Ljava/lang/String;I)V");
    if (commit) commit(tl_jni_env(), tl_jni_class_object("org/libsdl/app/SDLInputConnection"), tl_jni_new_string(utf8), 1);
}
void tl_sdl_key(int android_keycode, bool down)
{
    void (*fn)(void *, void *, int) = find_native(SDLA, down ? "onNativeKeyDown" : "onNativeKeyUp", "(I)V");
    if (fn) fn(tl_jni_env(), tl_jni_class_object(SDLA), android_keycode);
}

/* SDLSurface.onTouch: each change is reported as a MotionEvent action with the finger it concerns and its place as a fraction of the surface. */
void tl_sdl_touch(int phase, int id, float x, float y)
{
    if (S.cfg.width <= 0 || S.cfg.height <= 0) return;
    if (!atomic_load(&S.touch_ready)) {
        /* Some SDL builds never ask the activity for its touch screens (initTouch); a touch is still delivered, so the screen is introduced with the first one. */
        void (*add)(void *, void *, int, void *) = find_native(SDLA, "nativeAddTouch", "(ILjava/lang/String;)V");
        if (add) add(tl_jni_env(), tl_jni_class_object(SDLA), 1, tl_jni_new_string("Touchscreen"));
        atomic_store(&S.touch_ready, true);
    }
    void (*touch)(void *, void *, int, int, int, float, float, float) = find_native(SDLA, "onNativeTouch", "(IIIFFF)V");
    if (!touch) { tl_log_line("sdl: no native onNativeTouch"); return; }
    if (getenv("TL_SDL_TRACE")) {
        int (*ntouch)(void) = tl_ld_sym(NULL, "SDL_GetNumTouchDevices");
        void *(*mfocus)(void) = tl_ld_sym(NULL, "SDL_GetMouseFocus");
        void *(*kfocus)(void) = tl_ld_sym(NULL, "SDL_GetKeyboardFocus");
        tl_log_line("sdl: touch phase %d id %d at %.0f,%.0f (touch devices %d, mouse focus %p, keyboard focus %p)", phase, id, x, y, ntouch ? ntouch() : -1, mfocus ? mfocus() : NULL, kfocus ? kfocus() : NULL);
    }
    int action, n;
    if (phase == 0) { n = atomic_fetch_add(&S.fingers, 1) + 1; action = n == 1 ? 0 /* ACTION_DOWN */ : 5 /* ACTION_POINTER_DOWN */; }
    else if (phase == 1) action = 2 /* ACTION_MOVE */;
    else if (phase == 2) { n = atomic_fetch_sub(&S.fingers, 1); if (n < 1) { atomic_store(&S.fingers, 0); n = 1; } action = n == 1 ? 1 /* ACTION_UP */ : 6 /* ACTION_POINTER_UP */; }
    else { atomic_store(&S.fingers, 0); action = 3 /* ACTION_CANCEL */; }
    touch(tl_jni_env(), tl_jni_class_object(SDLA), 1, id, action, x / (float)S.cfg.width, y / (float)S.cfg.height, phase == 2 ? 0.0f : 1.0f);
    if (S.touch_as_mouse && id == 0 && phase <= 2) tl_sdl_mouse(phase, x, y);
}

/* SDLActivity.onTouch for a mouse: the button, an action (0 down, 1 up, 2 move) and the place in surface pixels. */
void tl_sdl_mouse(int phase, float x, float y)
{
    void (*mouse)(void *, void *, int, int, float, float, ...) = find_native(SDLA, "onNativeMouse", "(IIFF)V");
    if (!mouse) mouse = find_native(SDLA, "onNativeMouse", "(IIFFZ)V");
    if (!mouse) { tl_log_line("sdl: no native onNativeMouse"); return; }
    /* The first argument is the state of the buttons (a MotionEvent's getButtonState), not the one that changed: the left button while it is down, none once it is up. */
    static bool down;
    if (phase == 0) down = true; else if (phase == 2) down = false;
    mouse(tl_jni_env(), tl_jni_class_object(SDLA), phase == 2 ? 0 : (down ? 1 : 0), phase == 0 ? 0 : phase == 2 ? 1 : 2, x, y, 0);
}

void tl_sdl_set_paused(bool paused)
{
    /* SDL starts un-paused, and a resume that follows no pause makes it release a GL context it never saved. So only a real change is passed on. */
    static atomic_bool is_paused;
    if (atomic_exchange(&is_paused, paused) == paused) return;
    void (*vv)(void *, void *) = find_native(SDLA, paused ? "nativePause" : "nativeResume", "()V");
    if (vv) vv(tl_jni_env(), tl_jni_class_object(SDLA));
}
