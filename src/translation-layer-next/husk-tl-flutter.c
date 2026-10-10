/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Flutter apps. A Flutter app is the Flutter engine (libflutter.so), the app's Dart code compiled ahead of time (libapp.so) and
 * its assets (assets/flutter_assets); the Java side is a thin embedding that starts the engine, hands it a surface, feeds it
 * input and vsync, and carries platform-channel messages between Dart and Android. Here that embedding is this file.
 *
 * The engine registers its natives on io.flutter.embedding.engine.FlutterJNI when it loads; their signatures change from one
 * Flutter release to the next, so every one is called with the signature the app's own FlutterJNI declares (read from its dex).
 */
#define _DARWIN_C_SOURCE
#include "husk-tl-flutter.h"

#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "husk-tl-bionic.h"
#include "husk-tl-codewrite.h"
#include "husk-tl-dexindex.h"
#include "husk-tl-egl.h"
#include "husk-tl-flutter-codec.h"
#include "husk-tl-flutter-plugins.h"
#include "husk-tl-flutter-text.h"
#include "husk-tl-internal.h"
#include "husk-tl-jni.h"
#include "husk-tl-ld.h"
#include "husk-tl-vulkan.h"

void tl_jni_hle_install(void);
void tl_hle_configure(const char *pkg, const char *apk, const char *data, int w, int h);
void tl_hle_set_activity(jobj *a);
void tl_set_data_dir(const char *dir);
void tl_nwindow_configure(int w, int h, void *layer);
void tl_assetstream_install(void);

#define FJNI "io/flutter/embedding/engine/FlutterJNI"

static struct {
    tl_ga_config cfg;
    char apk[1024], data[512], pkg[128], frame_dir[512], angle_egl[600], angle_gles[600];
    jobj *activity, *jni, *surface, *assets;
    int64_t shell;                        /* nativeAttach's handle, the first argument of every engine call */
    pthread_t platform;
    bool started;
    atomic_bool first_frame;
    float ratio;
    int inset_t, inset_r, inset_b, inset_l;
    int kb_bottom;                  /* how much of the view the keyboard covers, in pixels: the view's bottom inset */
    int pointer_fields;                   /* longs per pointer in a pointer data packet (the engine's kPointerDataFieldCount) */
} F = { .ratio = 3.0f, .pointer_fields = 36 };

static void post_event(int kind, int phase);
static jvalue vl(void *p) { jvalue v; v.j = 0; v.l = p; return v; }
static jvalue vi(int i) { jvalue v; v.j = 0; v.i = i; return v; }
static jvalue vz(int z) { jvalue v; v.j = 0; v.z = z != 0; return v; }
static jvalue vf(float f) { jvalue v; v.j = 0; v.f = f; return v; }
static const char *Str(const jobj *o) { const char *s = tl_jni_string(o); return s ? s : ""; }
static bool trace(void) { static int t = -1; if (t < 0) t = getenv("TL_FLUTTER_TRACE") ? 1 : 0; return t; }

/* ------------------------------------------------------------------ calling the engine */

/*
 * A call into one of the engine's natives with the arguments laid out as Android's arm64 ABI wants them: integers and references in
 * x0-x7 then 8-byte stack slots, floats in s0-s7. The prototype below puts them in the same places under Apple's ABI (non-variadic,
 * every stack argument 8 bytes wide), and a callee simply ignores the extra ones.
 */
typedef int64_t (*gcall_fn)(int64_t, int64_t, int64_t, int64_t, int64_t, int64_t, int64_t, int64_t,
                            float, float, float, float, float, float, float, float,
                            int64_t, int64_t, int64_t, int64_t, int64_t, int64_t, int64_t, int64_t,
                            int64_t, int64_t, int64_t, int64_t, int64_t, int64_t, int64_t, int64_t,
                            int64_t, int64_t, int64_t, int64_t, int64_t, int64_t, int64_t, int64_t);
#define NSTACK 24

/* The native's signature as the app declares it, given its name: the dex knows. */
static const char *declared_sig(const char *name, char *out, size_t n)
{
    bool st;
    return tl_dexidx_method_sig(FJNI, name, out, n, &st) ? out : NULL;
}

/*
 * Call FlutterJNI.<name> with these Java arguments (in declaration order). Instance natives get the FlutterJNI object, static ones
 * its class. Returns the native's result as a 64-bit value (a long, a reference, a boolean in the low byte).
 */
static int64_t fj_call(const char *name, const jvalue *args, int nargs)
{
    char sig[192];
    if (!declared_sig(name, sig, sizeof(sig))) { tl_log_line("flutter: the app's FlutterJNI has no %s", name); return 0; }
    void *fn = tl_jni_native(FJNI, name, sig);
    if (!fn) fn = tl_jni_native(FJNI, name, NULL);
    if (!fn) { tl_log_line("flutter: the engine registered no %s%s", name, sig); return 0; }
    bool is_static = false;
    tl_dexidx_method_sig(FJNI, name, sig, sizeof(sig), &is_static);
    int64_t ints[8 + NSTACK] = { 0 }; float fl[8] = { 0 };
    int ni = 0, nf = 0, ai = 0;
    ints[ni++] = (int64_t)(intptr_t)tl_jni_env();
    ints[ni++] = (int64_t)(intptr_t)(is_static ? tl_jni_class_object(FJNI) : F.jni);
    for (const char *p = sig + 1; *p && *p != ')'; p++) {
        if (ai >= nargs) { tl_log_line("flutter: %s%s wants more arguments than given", name, sig); return 0; }
        jvalue a = args[ai++];
        switch (*p) {
        case 'F': if (nf < 8) fl[nf++] = a.f; break;
        case 'D': tl_log_line("flutter: %s takes a double, not supported", name); return 0;
        case 'J': ints[ni++] = a.j; break;
        case 'Z': ints[ni++] = a.z; break;
        case 'B': case 'S': case 'C': case 'I': ints[ni++] = a.i; break;
        case '[': while (*p == '[') p++; if (*p == 'L') p = strchr(p, ';'); ints[ni++] = (int64_t)(intptr_t)a.l; break;
        case 'L': p = strchr(p, ';'); ints[ni++] = (int64_t)(intptr_t)a.l; break;
        default: return 0;
        }
        if (ni > 8 + NSTACK) { tl_log_line("flutter: %s has too many arguments", name); return 0; }
    }
    if (trace()) tl_log_line("flutter: -> %s%s", name, sig);
    gcall_fn g = (gcall_fn)fn;
    int64_t *s = ints + 8;
    return g(ints[0], ints[1], ints[2], ints[3], ints[4], ints[5], ints[6], ints[7],
             fl[0], fl[1], fl[2], fl[3], fl[4], fl[5], fl[6], fl[7],
             s[0], s[1], s[2], s[3], s[4], s[5], s[6], s[7], s[8], s[9], s[10], s[11],
             s[12], s[13], s[14], s[15], s[16], s[17], s[18], s[19], s[20], s[21], s[22], s[23]);
}

/* How many Java parameters FlutterJNI.<name> takes, in the app's version. */
static int param_count(const char *name)
{
    char sig[192];
    if (!declared_sig(name, sig, sizeof(sig))) return -1;
    int n = 0;
    for (const char *p = sig + 1; *p && *p != ')'; p++) {
        while (*p == '[') p++;
        if (*p == 'L') p = strchr(p, ';');
        n++;
    }
    return n;
}

static jobj *direct_buffer(const void *data, size_t len)
{
    /* A copy that lives as long as the buffer object: the engine reads it during the call. */
    void *copy = malloc(len ? len : 1);
    if (len) memcpy(copy, data, len);
    void *(*mk)(void *, void *, int64_t) = (void *(*)(void *, void *, int64_t))((void *const *)*(void *const *const *)tl_jni_env())[229];
    return mk(tl_jni_env(), copy, (int64_t)len);
}

/* ------------------------------------------------------------------ platform channels */

/* Send Dart a message on a channel, expecting no reply. */
static void send_message(const char *channel, const void *data, size_t len)
{
    jvalue a[5] = { { .j = F.shell }, vl(tl_jni_new_string(channel)), vl(direct_buffer(data, len)), vi((int)len), vi(0) };
    if (param_count("nativeDispatchPlatformMessage") >= 5) fj_call("nativeDispatchPlatformMessage", a, 5);
    else fj_call("nativeDispatchPlatformMessage", a, 4);
}
static void send_string(const char *channel, const char *s) { send_message(channel, s, strlen(s)); }

/* The method name of a JSONMethodCodec call: {"method":"...","args":...}. */
static bool json_method(const uint8_t *d, size_t len, char *out, size_t n)
{
    const char *m = memmem(d, len, "\"method\"", 8);
    if (!m) return false;
    const char *q = memchr(m + 8, '"', len - (size_t)(m + 8 - (const char *)d));
    if (!q) return false;
    const char *e = memchr(q + 1, '"', len - (size_t)(q + 1 - (const char *)d));
    if (!e) return false;
    snprintf(out, n, "%.*s", (int)(e - q - 1), q + 1);
    return true;
}

/* Reply to a message from Dart: with these bytes, or with nothing (which Dart reads as a missing plugin / null). */
static void reply(int id, const void *data, size_t len)
{
    if (!id) return;
    if (data) {
        jvalue a[4] = { { .j = F.shell }, vi(id), vl(direct_buffer(data, len)), vi((int)len) };
        fj_call("nativeInvokePlatformMessageResponseCallback", a, 4);
    } else {
        jvalue a[2] = { { .j = F.shell }, vi(id) };
        fj_call("nativeInvokePlatformMessageEmptyResponseCallback", a, 2);
    }
}
static void reply_json(int id, const char *json) { reply(id, json, strlen(json)); }

/* What to do when the app closes itself (SystemNavigator.pop: back on its first screen). */
static void (*g_close)(void);
void tl_flutter_set_close_handler(void (*fn)(void)) { g_close = fn; }

/* A message from Dart on a channel: answered here, or with an empty reply. */
static void on_message(const char *channel, const uint8_t *d, size_t len, int id)
{
    char m[96] = "";
    if (!strcmp(channel, "flutter/platform")) {
        json_method(d, len, m, sizeof(m));
        if (trace()) tl_log_line("flutter: platform %s", m);
        if (!strcmp(m, "Clipboard.hasStrings")) reply_json(id, "[{\"value\":false}]");
        else if (!strcmp(m, "Clipboard.getData")) reply_json(id, "[null]");
        else if (!strcmp(m, "SystemNavigator.pop")) {
            tl_log_line("flutter: the app asked to close");
            reply_json(id, "[null]");
            if (g_close) g_close();
        }
        else reply_json(id, "[null]");                 /* system chrome, orientations, sounds, haptics: done (or not needed) */
        return;
    }
    if (!strcmp(channel, "flutter/textinput")) {
        json_method(d, len, m, sizeof(m));
        if (trace()) tl_log_line("flutter: textinput %s", m);
        reply_json(id, tl_flutter_text_message(m, d, len));
        return;
    }
    if (!strcmp(channel, "flutter/navigation") || !strcmp(channel, "flutter/mousecursor")
        || !strcmp(channel, "flutter/scribe") || !strcmp(channel, "flutter/spellcheck")) {
        if (json_method(d, len, m, sizeof(m)) && trace()) tl_log_line("flutter: %s %s", channel, m);
        reply_json(id, "[null]");
        return;
    }
    if (!strcmp(channel, "flutter/keyboard")) {
        /* getKeyboardState: no keys held (an empty map) */
        sv *none = sv_map(0); size_t rl; uint8_t *r = sv_encode_success(none, &rl);
        reply(id, r, rl); free(r); sv_free(none);
        return;
    }
    {
        bool handled = false; size_t rl = 0;
        uint8_t *r = tl_flutter_plugin_message(channel, d, len, &rl, &handled);
        if (handled) {
            if (trace()) tl_log_line("flutter: plugin %s -> %zu bytes", channel, r ? rl : 0);
            reply(id, r, rl); free(r);
            return;
        }
    }
    if (trace() || id) {
        /* name the method, if the message is a method call in either codec */
        char what[96] = "";
        if (!json_method(d, len, what, sizeof(what))) { size_t used = 0; sv *mname = sv_decode(d, len, &used); if (mname && mname->type == SV_STRING) snprintf(what, sizeof(what), "%s", mname->s); sv_free(mname); }
        tl_log_line("flutter: no handler for %s%s%s (%zu bytes)%s", channel, what[0] ? " " : "", what, len, id ? ", replied empty" : "");
    }
    reply(id, NULL, 0);
}

/* FlutterJNI.handlePlatformMessage(channel, message, replyId[, messageData]): the engine passing a message from Dart. */
static void J_handlePlatformMessage(tl_jcall *c)
{
    const char *channel = Str(c->args[0].l);
    jobj *msg = c->args[1].l;
    int id = c->args[2].i;
    const uint8_t *d = NULL; size_t len = 0;
    if (msg) { d = tl_jni_get_field(msg, "address", "J").l; len = (size_t)tl_jni_get_field(msg, "capacity", "J").j; }
    on_message(channel, d, len, id);
    /* newer engines keep the message alive until the embedding says it is done with it */
    char sig[192];
    if (declared_sig("nativeCleanupMessageData", sig, sizeof(sig)) && param_count("handlePlatformMessage") >= 4) {
        jvalue a[1] = { { .j = c->args[3].j } };
        fj_call("nativeCleanupMessageData", a, 1);
    }
}

/* A reply from Dart to a message we sent: none of ours wait for one. */
static void J_handleResponse(tl_jcall *c) { (void)c; }

static void J_onFirstFrame(tl_jcall *c) { (void)c; if (!atomic_exchange(&F.first_frame, true)) tl_log_line("flutter: first frame"); }
static void J_void(tl_jcall *c) { (void)c; }
static void J_false(tl_jcall *c) { c->ret = vz(0); }
static void J_one(tl_jcall *c) { c->ret = vf(1.0f); }

/* getScaledFontSize(fontSize, configurationId): no font scaling. */
static void J_scaledFont(tl_jcall *c) { c->ret = vf(c->args[0].f); }

/* computePlatformResolvedLocale(String[] supported, as language/country/script triples): the first one the app supports. */
static void J_resolvedLocale(tl_jcall *c)
{
    jobj *in = c->args[0].l;
    jobj *out = tl_jni_new_obj_array(tl_jni_class("java/lang/String"), 3);
    for (uint32_t i = 0; i < 3; i++) out->oarr.v[i] = tl_jni_new_string(in && in->oarr.len >= 3 ? Str(in->oarr.v[i]) : (i == 0 ? "en" : i == 1 ? "US" : ""));
    c->ret = vl(out);
}

/* ------------------------------------------------------------------ vsync */

/* The engine's own vsync waiter asks the Java side for a frame when it cannot use AChoreographer: FlutterJNI.asyncWaitForVsync(cookie). */
static void vsync_cb(int64_t frame_ns, void *data)
{
    int64_t cookie = (int64_t)(intptr_t)data;
    jvalue a[3] = { { .j = frame_ns }, { .j = frame_ns + 16666667 }, { .j = cookie } };
    fj_call("nativeOnVsync", a, 3);
}
static void J_asyncWaitForVsync(tl_jcall *c)
{
    void *(*inst)(void) = tl_bionic_find("AChoreographer_getInstance");
    void (*post)(void *, void *, void *) = tl_bionic_find("AChoreographer_postFrameCallback64");
    void *ch = inst ? inst() : NULL;
    if (ch && post) post(ch, (void *)vsync_cb, (void *)(intptr_t)c->args[0].j);
}

static const tl_jhle k_hle[] = {
    { FJNI, "handlePlatformMessage", NULL, J_handlePlatformMessage },
    { FJNI, "handlePlatformMessageResponse", NULL, J_handleResponse },
    { FJNI, "onFirstFrame", "()V", J_onFirstFrame },
    { FJNI, "onPreEngineRestart", "()V", J_void },
    { FJNI, "onEndFrame", "()V", J_void },
    { FJNI, "updateSemantics", NULL, J_void },
    { FJNI, "updateCustomAccessibilityActions", NULL, J_void },
    { FJNI, "asyncWaitForVsync", "(J)V", J_asyncWaitForVsync },
    { FJNI, "getScaledFontSize", "(FI)F", J_scaledFont },
    { FJNI, "computePlatformResolvedLocale", "([Ljava/lang/String;)[Ljava/lang/String;", J_resolvedLocale },
    { FJNI, "IsSurfaceControlEnabled", "()Z", J_false },
    { FJNI, "isCodePointEmoji", "(I)Z", J_false },
    { FJNI, "getRefreshRate", NULL, J_one },
    { NULL, NULL, NULL, NULL }
};

/* ------------------------------------------------------------------ start */

/* The Dart SDK version the engine was built with, from the "x.y.z (stable)" string in its read-only data. */
static bool tl_flutter_dart_version(tl_lib *lib, int *major, int *minor)
{
    const uint8_t *base = tl_ld_lib_base(lib);
    /* the version string sits in the first loadable segment's read-only data: that segment's size, from the ELF header */
    uint64_t phoff; uint16_t phent, phnum;
    memcpy(&phoff, base + 0x20, 8); memcpy(&phent, base + 0x36, 2); memcpy(&phnum, base + 0x38, 2);
    size_t size = 0;
    for (unsigned i = 0; i < phnum && !size; i++) {
        const uint8_t *ph = base + phoff + (size_t)i * phent;
        uint32_t type; memcpy(&type, ph, 4);
        if (type == 1) { uint64_t fsz; memcpy(&fsz, ph + 32, 8); size = (size_t)fsz; }
    }
    if (size < 64) return false;
    static const char *const ch[] = { " (stable)", " (beta)" };
    for (int c = 0; c < 2; c++) {
        size_t cl = strlen(ch[c]);
        for (const uint8_t *p = base; p + cl < base + size; p++) {
            p = memchr(p, ' ', (size_t)(base + size - cl - p));
            if (!p) break;
            if (memcmp(p, ch[c], cl)) continue;
            const uint8_t *q = p;
            while (q > base && ((q[-1] >= '0' && q[-1] <= '9') || q[-1] == '.')) q--;
            if (q < p && sscanf((const char *)q, "%d.%d.", major, minor) == 2 && *major >= 2 && *major < 10) return true;
        }
    }
    return false;
}

bool tl_flutter_is_app(const char *apk_path)
{
    tl_zip z; char err[160];
    if (!tl_zip_open(&z, apk_path, err, sizeof(err))) return false;
    bool yes = tl_zip_find(&z, "lib/arm64-v8a/libflutter.so") != NULL;
    tl_zip_close(&z);
    return yes;
}

static void mkdirs(const char *path)
{
    char t[1024]; snprintf(t, sizeof(t), "%s", path);
    for (char *p = t + 1; *p; p++) if (*p == '/') { *p = 0; mkdir(t, 0755); *p = '/'; }
    mkdir(t, 0755);
}

void tl_flutter_set_pixel_ratio(float r) { if (r > 0.5f) F.ratio = r; }

bool tl_flutter_start(const tl_ga_config *cfg)
{
    F.cfg = *cfg;
    snprintf(F.apk, sizeof(F.apk), "%s", cfg->apk_path);
    snprintf(F.data, sizeof(F.data), "%s", cfg->data_dir);
    snprintf(F.pkg, sizeof(F.pkg), "%s", cfg->package_name);
    F.cfg.apk_path = F.apk; F.cfg.data_dir = F.data; F.cfg.package_name = F.pkg;
    if (cfg->frame_dir) { snprintf(F.frame_dir, sizeof(F.frame_dir), "%s", cfg->frame_dir); F.cfg.frame_dir = F.frame_dir; }
    if (cfg->angle_egl) { snprintf(F.angle_egl, sizeof(F.angle_egl), "%s", cfg->angle_egl); F.cfg.angle_egl = F.angle_egl; }
    if (cfg->angle_gles) { snprintf(F.angle_gles, sizeof(F.angle_gles), "%s", cfg->angle_gles); F.cfg.angle_gles = F.angle_gles; }

    char dir[700];
    static const char *const subs[] = { "files", "cache", "app_flutter", "code_cache", "sdcard" };
    for (size_t i = 0; i < sizeof(subs) / sizeof(subs[0]); i++) { snprintf(dir, sizeof(dir), "%s/%s", F.data, subs[i]); mkdirs(dir); }

    /* The Dart VM finds the app's BSS by its distance from the snapshot's code and writes the few words there at start-up: that address
     * is in the executable view of libapp.so, so those stores go through the writable view (husk-tl-codewrite.c). */
    /* Every Android process has these from init; Skia finds the system fonts under $ANDROID_ROOT/fonts */
    setenv("ANDROID_ROOT", "/system", 0);
    setenv("ANDROID_DATA", "/data", 0);
    tl_codewrite_enable();
    tl_flutter_plugins_configure(F.data, F.pkg, NULL, NULL, 0);
    tl_set_data_dir(cfg->data_dir);
    tl_nwindow_configure(cfg->width, cfg->height, cfg->metal_layer);
    int n = tl_dexidx_open(cfg->apk_path);
    tl_log_line("flutter: %d classes in the APK's DEX", n);
    if (!tl_dexidx_has_class(FJNI)) { tl_log_line("flutter: the APK has no %s", FJNI); return false; }
    if (!tl_ld_add_apk(cfg->apk_path)) return false;
    if (cfg->angle_egl && !tl_egl_init(cfg->angle_egl, cfg->angle_gles, cfg->frame_dir, cfg->frame_every)) return false;
    tl_jni_init();
    tl_hle_configure(cfg->package_name, cfg->apk_path, cfg->data_dir, cfg->width, cfg->height);
    tl_jni_hle_install();
    tl_jni_declare(FJNI, "java/lang/Object");
    tl_jni_declare("io/flutter/embedding/android/FlutterActivity", "android/app/Activity");
    tl_jni_declare("android/view/Surface", "java/lang/Object");
    tl_jni_register_hle(k_hle);
    tl_assetstream_install();
    F.activity = tl_jni_new_object(tl_jni_class("io/flutter/embedding/android/FlutterActivity"));
    tl_hle_set_activity(F.activity);
    F.jni = tl_jni_new_object(tl_jni_class(FJNI));

    /* FlutterLoader: System.loadLibrary("flutter"), whose JNI_OnLoad registers the natives above. */
    jvalue a; a.j = 0; a.l = tl_jni_new_string("flutter");
    tl_jni_call(tl_jni_class_object("java/lang/System"), "loadLibrary", "(Ljava/lang/String;)V", &a);
    if (tl_jni_pending()) { tl_log_line("flutter: loading libflutter.so failed"); return false; }
    if (!tl_jni_native(FJNI, "nativeAttach", NULL)) { tl_log_line("flutter: the engine registered no natives"); return false; }
    /* How many fields a pointer record has: 35 before Dart 3.0 and for Dart 3.1-3.2 (Flutter 3.13, 3.16), 36 otherwise. The Dart
     * SDK's version string is in the engine. */
    {
        tl_lib *fl = tl_ld_find_lib("libflutter.so");
        int major = 0, minor = 0;
        if (fl && tl_flutter_dart_version(fl, &major, &minor)) {
            F.pointer_fields = (major < 3 || (major == 3 && (minor == 1 || minor == 2))) ? 35 : 36;
            tl_log_line("flutter: engine built with Dart %d.%d: %d fields a pointer", major, minor, F.pointer_fields);
        }
    }
    tl_log_line("flutter: engine loaded");
    F.started = true;
    return true;
}

/* nativeSetViewportMetrics: the device pixel ratio, the size, then a run of ints (padding, view insets, gesture insets, touch slop,
 * and in newer engines more) and the display-feature arrays. Filled from the app's own signature: size first, insets where they go. */
static void send_metrics(void)
{
    char sig[192];
    if (!declared_sig("nativeSetViewportMetrics", sig, sizeof(sig))) return;
    jvalue a[40]; int n = 0, ints = 0, after = 0;
    bool past_arrays = false;
    a[n++].j = F.shell;
    for (const char *p = sig + 2; *p && *p != ')' && n < 40; p++) {     /* after the J */
        if (*p == 'F') { a[n++] = vf(F.ratio); continue; }
        if (*p == '[') { while (*p == '[') p++; if (*p == 'L') p = strchr(p, ';'); a[n++] = vl(tl_jni_new_prim_array('I', 0)); past_arrays = true; continue; }
        if (*p == 'I' && past_arrays) {
            /* newer engines: the view's size constraints (min width, max width, min height, max height: a view of exactly the
             * surface), then the display's corner radii (none) */
            int k = after++;
            a[n++] = vi(k == 0 || k == 1 ? F.cfg.width : k == 2 || k == 3 ? F.cfg.height : 0);
            continue;
        }
        if (*p == 'I') {
            int v = 0;
            switch (ints++) {
            case 0: v = F.cfg.width; break;  case 1: v = F.cfg.height; break;
            case 2: v = F.inset_t; break;    case 3: v = F.inset_r; break;
            case 4: v = F.inset_b; break;    case 5: v = F.inset_l; break;
            case 8: v = F.kb_bottom; break;                            /* view insets: the keyboard */
            case 14: v = (int)(8 * F.ratio); break;                    /* physical touch slop */
            default: v = 0;
            }
            a[n++] = vi(v); continue;
        }
        if (*p == 'L') { p = strchr(p, ';'); a[n++] = vl(NULL); continue; }
        a[n++] = vi(0);
    }
    fj_call("nativeSetViewportMetrics", a, n);
}

/* The view changed size (a window being resized): the surface and the metrics, as FlutterView sends them on a layout. */
static void resize_surface(void)
{
    jvalue ch[3] = { { .j = F.shell }, vi(F.cfg.width), vi(F.cfg.height) };
    fj_call("nativeSurfaceChanged", ch, 3);
    send_metrics();
}

void tl_nwindow_resize(int w, int h);
void tl_flutter_resize(int width, int height)
{
    if (width <= 0 || height <= 0 || (width == F.cfg.width && height == F.cfg.height)) return;
    F.cfg.width = width; F.cfg.height = height;
    tl_nwindow_resize(width, height);
    post_event(3, 0);
}

/* The keyboard: its edits, and how much of the view it covers. */
void tl_flutter_key_insert(const char *utf8) { tl_flutter_text_insert(utf8); post_event(4, 0); }
void tl_flutter_key_delete(void) { tl_flutter_text_delete(); post_event(4, 0); }
void tl_flutter_key_action(void) { tl_flutter_text_action(); post_event(4, 0); }
void tl_flutter_set_keyboard_inset(int bottom)
{
    if (bottom < 0) bottom = 0;
    if (bottom == F.kb_bottom) return;
    F.kb_bottom = bottom;
    post_event(2, 0);
}

/* Android's back button: the navigator pops a route, or the app closes itself (SystemNavigator.pop). */
void tl_flutter_back(void) { post_event(5, 0); }

void tl_flutter_set_insets(int top, int right, int bottom, int left)
{
    F.inset_t = top; F.inset_r = right; F.inset_b = bottom; F.inset_l = left;
    post_event(2, 0);
}

/* ------------------------------------------------------------------ input */

/* Work for the platform thread, posted from others (the app's touch handling): a pipe its looper watches. */
typedef struct { int kind; int phase, id; float x, y; int64_t when_us; } fl_event;
static int g_post[2] = { -1, -1 };

/* The pointer data the engine reads: one record of int64/double fields per pointer, in pointer_data.h's order. */
enum { PD_EMBEDDER_ID, PD_TIME, PD_CHANGE, PD_KIND, PD_SIGNAL, PD_DEVICE, PD_POINTER, PD_X, PD_Y, PD_DX, PD_DY, PD_BUTTONS,
       PD_OBSCURED, PD_SYNTHESIZED, PD_PRESSURE, PD_PRESSURE_MIN, PD_PRESSURE_MAX, PD_DISTANCE, PD_DISTANCE_MAX, PD_SIZE,
       PD_RADIUS_MAJOR, PD_RADIUS_MINOR, PD_RADIUS_MIN, PD_RADIUS_MAX, PD_ORIENTATION, PD_TILT, PD_PLATFORM_DATA };
enum { CHANGE_CANCEL = 0, CHANGE_DOWN = 4, CHANGE_MOVE = 5, CHANGE_UP = 6 };

static void dispatch_touch(const fl_event *e)
{
    int nf = F.pointer_fields;
    union { int64_t i; double d; } rec[40];
    memset(rec, 0, sizeof(rec));
    static int64_t event_id;
    int change = e->phase == 0 ? CHANGE_DOWN : e->phase == 1 ? CHANGE_MOVE : e->phase == 2 ? CHANGE_UP : CHANGE_CANCEL;
    rec[PD_EMBEDDER_ID].i = ++event_id;
    rec[PD_TIME].i = e->when_us;
    rec[PD_CHANGE].i = change;
    rec[PD_KIND].i = 0;                                   /* DeviceKind::kTouch */
    rec[PD_DEVICE].i = e->id;
    rec[PD_POINTER].i = 0;
    rec[PD_X].d = e->x; rec[PD_Y].d = e->y;
    rec[PD_BUTTONS].i = (change == CHANGE_DOWN || change == CHANGE_MOVE) ? 1 : 0;
    rec[PD_PRESSURE].d = change == CHANGE_UP ? 0.0 : 1.0;
    rec[PD_PRESSURE_MAX].d = 1.0;
    rec[PD_PLATFORM_DATA].i = 0;
    /* then scrolling (0), trackpad pan (0), scale (1, as the embedding sends it), rotation (0) and, in 36-field engines, the
     * view (0, the implicit one) */
    rec[PD_PLATFORM_DATA + 7].d = 1.0;
    size_t bytes = (size_t)nf * 8;
    jvalue a[3] = { { .j = F.shell }, vl(direct_buffer(rec, bytes)), vi((int)bytes) };
    fj_call("nativeDispatchPointerDataPacket", a, 3);
}

static int on_post(int fd, int events, void *data)
{
    (void)events; (void)data;
    fl_event e;
    while (read(fd, &e, sizeof(e)) == (ssize_t)sizeof(e)) {
        if (!F.shell) continue;
        if (e.kind == 0) dispatch_touch(&e);
        else if (e.kind == 1) send_string("flutter/lifecycle", e.phase ? "AppLifecycleState.paused" : "AppLifecycleState.resumed");
        else if (e.kind == 2) send_metrics();
        else if (e.kind == 3) resize_surface();
        else if (e.kind == 4) tl_flutter_text_drain(send_string);
        else if (e.kind == 5) send_string("flutter/navigation", "{\"method\":\"popRoute\",\"args\":null}");
    }
    return 1;
}
static void post_event(int kind, int phase)
{
    if (g_post[1] < 0) return;
    fl_event e = { kind, phase, 0, 0, 0, 0 };
    (void)!write(g_post[1], &e, sizeof(e));
}

/* The app leaving or coming back to the screen: Flutter's lifecycle channel, as FlutterActivity's onPause/onResume send it. */
void tl_flutter_set_paused(bool paused)
{
    static atomic_int was = -1;
    if (atomic_exchange(&was, paused ? 1 : 0) == (paused ? 1 : 0)) return;
    post_event(1, paused ? 1 : 0);
}

void tl_flutter_touch(int phase, int id, float x, float y)
{
    if (g_post[1] < 0) return;
    struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts);
    fl_event e = { 0, phase, id, x, y, (int64_t)ts.tv_sec * 1000000 + ts.tv_nsec / 1000 };
    (void)!write(g_post[1], &e, sizeof(e));
}

static int64_t now_ms(void) { struct timespec ts; clock_gettime(CLOCK_REALTIME, &ts); return (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000; }

static void *platform_main(void *arg)
{
    (void)arg;
    pthread_setname_np("main");
    void *looper = ((void *(*)(int))tl_bionic_find("ALooper_prepare"))(0);
    if (pipe(g_post) == 0) {
        fcntl(g_post[0], F_SETFL, O_NONBLOCK);              /* on_post drains it: a read must not wait for the next event */
        int (*add_fd)(void *, int, int, int, void *, void *) = tl_bionic_find("ALooper_addFd");
        add_fd(looper, g_post[0], -2 /* ALOOPER_POLL_CALLBACK */, 1 /* input */, (void *)on_post, NULL);
    }
    /* FlutterJNI's display statics, which the engine reads for the refresh rate and the screen it draws for */
    tl_jni_set_static(FJNI, "refreshRateFPS", "F", vf(60.0f));
    tl_jni_set_static(FJNI, "displayWidth", "F", vf((float)F.cfg.width));
    tl_jni_set_static(FJNI, "displayHeight", "F", vf((float)F.cfg.height));
    tl_jni_set_static(FJNI, "displayDensity", "F", vf(F.ratio));

    /* FlutterLoader.ensureInitializationComplete: the engine's command line, then FlutterJNI.init. */
    char cache[700], lib[64] = "--aot-shared-library-name=libapp.so";
    snprintf(cache, sizeof(cache), "--cache-dir-path=%s/cache", F.data);
    /* --no-enable-merged-platform-ui-thread: Flutter 3.29 and later run Dart on the platform thread by default, and work posted to
     * that merged queue from our input path is never run by the platform loop as it is driven here. Apps can turn the merge off in
     * their manifest (DisableMergedPlatformUIThread); this does the same, and Dart gets its own UI thread as it had before 3.29.
     * Older engines ignore the switch. */
    const char *args[] = { "--icu-symbol-prefix=_binary_icudtl_dat", lib, cache, "--leak-vm=true", getenv("TL_FLUTTER_ARGS") };
    int nargs = getenv("TL_FLUTTER_ARGS") ? 5 : 4;
    jobj *jargs = tl_jni_new_obj_array(tl_jni_class("java/lang/String"), (uint32_t)nargs);
    for (int i = 0; i < nargs; i++) jargs->oarr.v[i] = tl_jni_new_string(args[i]);
    char files[700], caches[700];
    snprintf(files, sizeof(files), "%s/files", F.data);
    snprintf(caches, sizeof(caches), "%s/cache", F.data);
    /* the last argument (newer engines) is the Android API level, from which the engine decides whether Impeller can run */
    jvalue in[7] = { vl(F.activity), vl(jargs), vl(NULL), vl(tl_jni_new_string(files)), vl(tl_jni_new_string(caches)), { .j = now_ms() }, vi(36) };
    fj_call("nativeInit", in, param_count("nativeInit"));
    if (tl_jni_pending()) { tl_log_line("flutter: nativeInit threw"); tl_jni_clear(); }
    tl_log_line("flutter: engine initialised");

    /* FlutterEngine: attach (the shell), metrics, the surface. */
    jvalue at[1] = { vl(F.jni) };
    F.shell = fj_call("nativeAttach", at, 1);
    tl_log_line("flutter: shell %#llx", (unsigned long long)F.shell);
    if (!F.shell) return NULL;
    send_metrics();
    F.surface = tl_jni_new_object(tl_jni_class("android/view/Surface"));
    jvalue sc[2] = { { .j = F.shell }, vl(F.surface) };
    fj_call("nativeSurfaceCreated", sc, 2);
    jvalue ch[3] = { { .j = F.shell }, vi(F.cfg.width), vi(F.cfg.height) };
    fj_call("nativeSurfaceChanged", ch, 3);

    /* The settings, locale and lifecycle FlutterActivity sends as it starts. */
    send_string("flutter/settings", "{\"textScaleFactor\":1.0,\"alwaysUse24HourFormat\":false,\"platformBrightness\":\"light\","
                                    "\"brieflyShowPassword\":true,\"nativeSpellCheckServiceDefined\":false,\"configurationId\":0}");
    send_string("flutter/localization", "{\"method\":\"setLocale\",\"args\":[\"en\",\"US\",\"\",\"\"]}");

    /* DartExecutor.executeDartEntrypoint: the bundle (assets/flutter_assets through the AssetManager), main() in the app's library. */
    F.assets = tl_jni_call(F.activity, "getAssets", "()Landroid/content/res/AssetManager;", NULL).l;
    jvalue run[7] = { { .j = F.shell }, vl(tl_jni_new_string("flutter_assets")), vl(NULL), vl(NULL), vl(F.assets), vl(NULL), { .j = 0 } };
    fj_call("nativeRunBundleAndSnapshotFromLibrary", run, param_count("nativeRunBundleAndSnapshotFromLibrary"));
    tl_log_line("flutter: Dart entry point started");
    send_string("flutter/lifecycle", "AppLifecycleState.resumed");

    int (*poll_once)(int, int *, int *, void **) = tl_bionic_find("ALooper_pollOnce");
    for (;;) { poll_once(-1, NULL, NULL, NULL); if (tl_jni_pending()) { tl_log_line("flutter: a Java exception on the platform thread"); tl_jni_clear(); } }
    return NULL;
}

bool tl_flutter_run(void)
{
    if (!F.started) return false;
    pthread_attr_t a; pthread_attr_init(&a); pthread_attr_setstacksize(&a, 8u << 20);
    bool ok = pthread_create(&F.platform, &a, platform_main, NULL) == 0;
    pthread_attr_destroy(&a);
    return ok;
}

unsigned long tl_flutter_frames(void) { return tl_egl_frames_presented() + tl_vk_frames_presented(); }
bool tl_flutter_first_frame(void) { return atomic_load(&F.first_frame); }

