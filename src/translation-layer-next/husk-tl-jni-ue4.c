/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * The Java side of an Unreal Engine 4 game, implemented in C: the methods of com.epicgames.ue4.GameActivity that the engine calls through JNI
 * (AndroidThunkJava_*), for the facts it asks for at start-up. A method with no implementation here logs once and returns zero, which is
 * what most of them (notifications, purchases, Google sign-in) want.
 */
#define _DARWIN_C_SOURCE
#include "husk-tl-jni.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "husk-tl-bionic.h"
#include "husk-tl-host-device.h"

static jvalue vl(void *p) { jvalue v; v.j = 0; v.l = p; return v; }
static jvalue vi(int i) { jvalue v; v.j = 0; v.i = i; return v; }
static jvalue vj(int64_t j) { jvalue v; v.j = j; return v; }
static jvalue vz(int z) { jvalue v; v.j = 0; v.z = z != 0; return v; }
static jvalue vf(float f) { jvalue v; v.j = 0; v.f = f; return v; }
static const char *S(const jobj *o) { const char *s = tl_jni_string(o); return s ? s : ""; }

#define CLS "com/epicgames/ue4/GameActivity"

static struct { char pkg[160], data[512], ext[700], apk[1024]; jobj *activity; } U;

/*
 * <meta-data> of the manifest, which the engine reads through GetMetaData*: the project's settings as the packager recorded them.
 * These are ARK's, and only the fallback: the game's own manifest is read first (tl_hle_manifest_meta).
 */
static const struct { const char *key, *value; } k_meta[] = {
    { "com.epicgames.ue4.GameActivity.EngineVersion", "4.26.2" }, { "com.epicgames.ue4.GameActivity.EngineBranch", "++UE4+Release-4.26" },
    { "com.epicgames.ue4.GameActivity.ProjectVersion", "1.0.0.0" }, { "com.epicgames.ue4.GameActivity.DepthBufferPreference", "24" },
    { "com.epicgames.ue4.GameActivity.bPackageDataInsideApk", "false" }, { "com.epicgames.ue4.GameActivity.bVerifyOBBOnStartUp", "false" },
    { "com.epicgames.ue4.GameActivity.bShouldHideUI", "true" }, { "com.epicgames.ue4.GameActivity.ProjectName", "ShooterGame" },
    { "com.epicgames.ue4.GameActivity.AppType", "" }, { "com.epicgames.ue4.GameActivity.bHasOBBFiles", "false" },
    { "com.epicgames.ue4.GameActivity.BuildConfiguration", "Shipping" }, { "com.epicgames.ue4.GameActivity.CookedFlavors", "ASTC" },
    { "com.epicgames.ue4.GameActivity.bValidateTextureFormats", "true" }, { "com.epicgames.ue4.GameActivity.bUseExternalFilesDir", "true" },
    { "com.epicgames.ue4.GameActivity.bPublicLogFiles", "true" }, { "com.epicgames.ue4.GameActivity.bUseDisplayCutout", "false" },
    { "com.epicgames.ue4.GameActivity.bAllowIMU", "true" }, { "com.epicgames.ue4.GameActivity.bSupportsVulkan", "false" },
    { "com.epicgames.ue4.GameActivity.StartupPermissions", "" },
};
const char *tl_hle_manifest_meta(const char *key);
const char *tl_ue4_meta(const char *key)
{
    /* Not in the manifest: GameActivity computes it from the display's metrics. x dpi, y dpi. */
    if (!strcmp(key, "ue4.displaymetrics.dpi")) {
        static char dpi_buf[32];
        int dpi = (int)(tl_host_display_density() * 160.0f + 0.5f);
        snprintf(dpi_buf, sizeof(dpi_buf), "%d.0,%d.0", dpi, dpi);
        return dpi_buf;
    }
    /* Likewise AudioManager's answers (PROPERTY_OUTPUT_FRAMES_PER_BUFFER, PROPERTY_OUTPUT_SAMPLE_RATE) and the display's refresh rate. A zero buffer size is not an
     * answer the engine can survive: it rounds its callback buffer up to a multiple of it, and a multiple of zero is never reached. */
    if (!strcmp(key, "audiomanager.framesPerBuffer")) return "256";
    if (!strcmp(key, "audiomanager.optimalSampleRate")) return "48000";
    if (!strcmp(key, "ue4.display.getRefreshRate")) {
        static char hz_buf[16];
        snprintf(hz_buf, sizeof(hz_buf), "%d", (int)(tl_host_display_max_refresh() + 0.5f));
        return hz_buf;
    }
    /* A setting forced from the environment ("key=value;key=value", keys without the com.epicgames.ue4.GameActivity. prefix), for trying things. */
    const char *force = getenv("TL_UE4_META");
    if (force) {
        static char buf[160]; size_t pl = strlen("com.epicgames.ue4.GameActivity.");
        if (!strncmp(key, "com.epicgames.ue4.GameActivity.", pl)) {
            for (const char *p = force; *p; ) {
                const char *eq = strchr(p, '='), *end = strchr(p, ';'); if (!end) end = p + strlen(p);
                if (eq && eq < end && (size_t)(eq - p) == strlen(key + pl) && !strncmp(p, key + pl, eq - p)) { snprintf(buf, sizeof(buf), "%.*s", (int)(end - eq - 1), eq + 1); return buf; }
                p = *end ? end + 1 : end;
            }
        }
    }
    const char *m = tl_hle_manifest_meta(key);
    if (m) return m;
    for (size_t i = 0; i < sizeof(k_meta) / sizeof(k_meta[0]); i++) if (!strcmp(key, k_meta[i].key)) return k_meta[i].value;
    return NULL;
}
#define meta tl_ue4_meta
static void GA_hasMeta(tl_jcall *c) { c->ret = vz(meta(S(c->args[0].l)) != NULL); }
static void GA_metaString(tl_jcall *c) { const char *v = meta(S(c->args[0].l)); c->ret = vl(v ? tl_jni_new_string(v) : NULL); }
static void GA_metaBool(tl_jcall *c) { const char *v = meta(S(c->args[0].l)); c->ret = vz(v && !strcmp(v, "true")); }
static void GA_metaInt(tl_jcall *c) { const char *v = meta(S(c->args[0].l)); c->ret = vi(v ? atoi(v) : 0); }
static void GA_metaLong(tl_jcall *c) { const char *v = meta(S(c->args[0].l)); c->ret = vj(v ? atoll(v) : 0); }
static void GA_metaFloat(tl_jcall *c) { const char *v = meta(S(c->args[0].l)); c->ret = vf(v ? (float)atof(v) : 0); }

jobj *tl_hle_assets(void);
static void GA_assetManager(tl_jcall *c) { c->ret = vl(tl_jni_ref(tl_hle_assets())); }

/*
 * ClassLoader.findClass / loadClass, which native code uses (through the activity's loader, as FindClass sees only the system's) to reach the game's own
 * Java classes by their dotted names. The class is there if the framework or the APK has it.
 */
bool tl_dexidx_has_class(const char *name);
static void CL_findClass(tl_jcall *c)
{
    char name[300]; snprintf(name, sizeof(name), "%s", S(c->args[0].l));
    for (char *p = name; *p; p++) if (*p == '.') *p = '/';
    bool framework = !strncmp(name, "android/", 8) || !strncmp(name, "java/", 5) || !strncmp(name, "javax/", 6) || !strncmp(name, "dalvik/", 7);
    if (framework || tl_dexidx_has_class(name)) { c->ret = vl(tl_jni_class_object(name)); return; }
    tl_log_line("ue4: ClassLoader could not find %s", name);
    tl_jni_throw("java/lang/ClassNotFoundException", name);
    c->ret = vl(NULL);
}

/*
 * AndroidThunkJava_ForceQuit: the engine has met something it cannot go on after. Its log is compiled out of a shipping build, so the call chain is
 * the only account of why; it is printed, and the thread stops (on Android the process would be gone in a moment).
 */
#include <unistd.h>
#include "husk-tl-ld.h"
static void GA_forceQuit(tl_jcall *c)
{
    (void)c;
    tl_log_line("ue4: the engine asked to quit (AndroidThunkJava_ForceQuit). Called from:");
    void **fp = __builtin_frame_address(0);
    for (int i = 0; fp && i < 28; i++) {
        if (((uintptr_t)fp & 7) || (uintptr_t)fp < 0x100000) break;
        void *lr = fp[1];
        const char *lib = NULL; const void *sym = NULL;
        const char *name = tl_ld_symbol_at(lr, &lib, &sym);
        tl_log_line("ue4:   %p  %s  %s+%#lx", lr, lib ? lib : "?", name ? name : "?", sym ? (unsigned long)((const char *)lr - (const char *)sym) : 0ul);
        void **next = fp[0];
        if (next <= fp) break;
        fp = next;
    }
    for (;;) sleep(1000);
}

/*
 * AndroidThunkJava_InitHMDs: on Android the activity answers by calling back into the engine (nativeInitHMDs), and the engine's start-up waits for that.
 * There is no headset to report, so it is just the call.
 */
static void GA_initHMDs(tl_jcall *c)
{
    (void)c;
    tl_lib *lib = tl_ld_find_lib("libUE4.so");
    void (*fn)(void *, void *) = lib ? (void (*)(void *, void *))tl_ld_sym(lib, "Java_com_epicgames_ue4_GameActivity_nativeInitHMDs") : NULL;
    if (fn) fn(tl_jni_env(), U.activity);
    else tl_log_line("ue4: nativeInitHMDs is not provided by libUE4");
}

/* com.epicgames.mobile.eossdk.EOSSDK: the statics Epic Online Services' native library calls while it starts */
#define EOS "com/epicgames/mobile/eossdk/EOSSDK"
static void EOS_void(tl_jcall *c) { (void)c; }
static void EOS_context(tl_jcall *c) { c->ret = vl(U.activity ? tl_jni_ref(U.activity) : NULL); }
static void EOS_osVersion(tl_jcall *c) { c->ret = vl(tl_jni_new_string("14")); }
static void EOS_market(tl_jcall *c) { c->ret = vl(tl_jni_new_string("US")); }
static void EOS_nullString(tl_jcall *c) { c->ret.l = NULL; }
static void EOS_true(tl_jcall *c) { c->ret = vz(1); }
static void EOS_orientation(tl_jcall *c) { c->ret = vi(1); }
static void EOS_zeroLong(tl_jcall *c) { c->ret = vj(0); }

/* android.app.ActivityThread, which native code reaches through the hidden API to find the application's Context */
static void AT_current(tl_jcall *c)
{
    static jobj *thread;
    if (!thread) thread = tl_jni_new_object(tl_jni_class("android/app/ActivityThread"));
    c->ret = vl(tl_jni_ref(thread));
}

static void GA_get(tl_jcall *c) { c->ret = vl(U.activity ? tl_jni_ref(U.activity) : NULL); }
static void GA_packageName(tl_jcall *c) { c->ret = vl(tl_jni_new_string(U.pkg)); }
static void GA_false(tl_jcall *c) { c->ret = vz(0); }
static void GA_true(tl_jcall *c) { c->ret = vz(1); }
static void GA_zero(tl_jcall *c) { c->ret = vi(0); }
static void GA_nullObj(tl_jcall *c) { c->ret.l = NULL; }
static void GA_emptyString(tl_jcall *c) { c->ret = vl(tl_jni_new_string("")); }
static void GA_commandLine(tl_jcall *c) { c->ret = vl(tl_jni_new_string(getenv("TL_UE4_CMDLINE") ? getenv("TL_UE4_CMDLINE") : "")); }
static void GA_fontDir(tl_jcall *c) { c->ret = vl(tl_jni_new_string("/system/fonts/")); }
const char *tl_hle_android_id(void);
static void GA_androidId(tl_jcall *c) { c->ret = vl(tl_jni_new_string(tl_hle_android_id())); }
static void GA_refresh(tl_jcall *c) { c->ret = vi(60); }
static void GA_refreshRates(tl_jcall *c) { jobj *a = tl_jni_new_prim_array('I', 1); ((int *)a->arr.data)[0] = 60; c->ret = vl(a); }
static void GA_orientation(tl_jcall *c) { c->ret = vi(1); }                    /* landscape */
static void GA_netType(tl_jcall *c) { c->ret = vi(1); }                        /* Wi-Fi */
static void GA_netTime(tl_jcall *c) { c->ret = vj(0); }
extern bool tl_pad_connected(int slot);
static void GA_gamepad(tl_jcall *c) { c->ret = vz(tl_pad_connected(0)); }
static void GA_listInputDevices(tl_jcall *c) { c->ret = vl(tl_jni_new_string("")); }
static void GA_isOBBInAPK(tl_jcall *c) { c->ret = vz(0); }
static void GA_boolObj(tl_jcall *c)                                            /* java.lang.Boolean true */
{
    jobj *b = tl_jni_new_object(tl_jni_class("java/lang/Boolean"));
    jvalue v = vz(1); tl_jni_set_field(b, "value", "Z", v);
    c->ret = vl(b);
}

#define M_(c, n, s, f) { c, n, s, f }
static const tl_jhle k_hle[] = {
    M_(CLS, "AndroidThunkJava_HasMetaDataKey", "(Ljava/lang/String;)Z", GA_hasMeta),
    M_(CLS, "AndroidThunkJava_GetMetaDataString", "(Ljava/lang/String;)Ljava/lang/String;", GA_metaString),
    M_(CLS, "AndroidThunkJava_GetMetaDataBoolean", "(Ljava/lang/String;)Z", GA_metaBool),
    M_(CLS, "AndroidThunkJava_GetMetaDataInt", "(Ljava/lang/String;)I", GA_metaInt),
    M_(CLS, "AndroidThunkJava_GetMetaDataLong", "(Ljava/lang/String;)J", GA_metaLong),
    M_(CLS, "AndroidThunkJava_GetMetaDataFloat", "(Ljava/lang/String;)F", GA_metaFloat),
    M_(CLS, "Get", "()Lcom/epicgames/ue4/GameActivity;", GA_get),
    M_(CLS, "AndroidThunkJava_ForceQuit", "()V", GA_forceQuit),
    M_(CLS, "AndroidThunkJava_InitHMDs", "()V", GA_initHMDs),
    M_("android/app/ActivityThread", "currentActivityThread", "()Landroid/app/ActivityThread;", AT_current),
    M_("android/app/ActivityThread", "currentApplication", "()Landroid/app/Application;", EOS_context),
    M_("android/app/ActivityThread", "getApplication", "()Landroid/app/Application;", EOS_context),
    M_(EOS, "GetApplicationContext", "()Landroid/content/Context;", EOS_context),
    M_(EOS, "GetActivity", "()Landroid/app/Activity;", EOS_context),
    M_(EOS, "setUserAgent", "(Ljava/lang/String;)V", EOS_void),
    M_(EOS, "GetOSVersion", "()Ljava/lang/String;", EOS_osVersion),
    M_(EOS, "GetSalesMarketIdentifier", "()Ljava/lang/String;", EOS_market),
    M_(EOS, "Keychain_ReadValue", "(Ljava/lang/String;)Ljava/lang/String;", EOS_nullString),
    M_(EOS, "Keychain_WriteValue", "(Ljava/lang/String;Ljava/lang/String;)Z", EOS_true),
    M_(EOS, "getDeviceOrientation", "()I", EOS_orientation),
    M_(EOS, "verifyPlatformsOptions", "(Ljava/lang/String;)Z", EOS_true),
    M_(EOS, "PrewarmURL", "(Ljava/lang/String;)J", EOS_zeroLong),
    M_(EOS, "LaunchURL", "(Ljava/lang/String;)J", EOS_zeroLong),
    M_(CLS, "AndroidThunkJava_GetAssetManager", "()Landroid/content/res/AssetManager;", GA_assetManager),
    M_("java/lang/ClassLoader", "findClass", "(Ljava/lang/String;)Ljava/lang/Class;", CL_findClass),
    M_("java/lang/ClassLoader", "loadClass", "(Ljava/lang/String;)Ljava/lang/Class;", CL_findClass),
    M_("dalvik/system/PathClassLoader", "findClass", "(Ljava/lang/String;)Ljava/lang/Class;", CL_findClass),
    M_("dalvik/system/PathClassLoader", "loadClass", "(Ljava/lang/String;)Ljava/lang/Class;", CL_findClass),
    M_(CLS, "getAppPackageName", "()Ljava/lang/String;", GA_packageName),
    M_(CLS, "isOBBInAPK", "()Z", GA_isOBBInAPK),
    M_(CLS, "isStandaloneMode", "()Ljava/lang/Boolean;", GA_boolObj),
    M_(CLS, "isValidGameActivity", "()Ljava/lang/Boolean;", GA_boolObj),
    M_(CLS, "AndroidThunkJava_GetCommandLine", "()Ljava/lang/String;", GA_commandLine),
    M_(CLS, "AndroidThunkJava_GetFontDirectory", "()Ljava/lang/String;", GA_fontDir),
    M_(CLS, "AndroidThunkJava_GetAndroidId", "()Ljava/lang/String;", GA_androidId),
    M_(CLS, "AndroidThunkJava_GetNativeDisplayRefreshRate", "()I", GA_refresh),
    M_(CLS, "AndroidThunkJava_GetSupportedNativeDisplayRefreshRates", "()[I", GA_refreshRates),
    M_(CLS, "AndroidThunkJava_GetDeviceOrientation", "()I", GA_orientation),
    M_(CLS, "AndroidThunkJava_GetNetworkConnectionType", "()I", GA_netType),
    M_(CLS, "AndroidThunkJava_GetNetworkTimeMillis", "()J", GA_netTime),
    M_(CLS, "AndroidThunkJava_IsGamepadAttached", "()Z", GA_gamepad),
    M_(CLS, "AndroidThunkJava_ListInputDevices", "(I)Ljava/lang/String;", GA_listInputDevices),
    M_(CLS, "AndroidThunkJava_GetFunnelId", "()Ljava/lang/String;", GA_emptyString),
    M_(CLS, "AndroidThunkJava_GetLoginId", "()Ljava/lang/String;", GA_emptyString),
    M_(CLS, "AndroidThunkJava_GetIntentExtrasString", "(Ljava/lang/String;)Ljava/lang/String;", GA_nullObj),
    M_(CLS, "AndroidThunkJava_GetIntentExtrasBoolean", "(Ljava/lang/String;)Z", GA_false),
    M_(CLS, "AndroidThunkJava_GetIntentExtrasInt", "(Ljava/lang/String;)I", GA_zero),
    M_(CLS, "AndroidThunkJava_HasIntentExtrasKey", "(Ljava/lang/String;)Z", GA_false),
    M_(CLS, "AndroidThunkJava_IsMusicActive", "()Z", GA_false),
    M_(CLS, "AndroidThunkJava_IsScreensaverEnabled", "()Z", GA_false),
    M_(CLS, "AndroidThunkJava_IsScreenCaptureDisabled", "()Z", GA_false),
    M_(CLS, "AndroidThunkJava_IapIsAllowedToMakePurchases", "()Z", GA_false),
    M_(CLS, "AndroidThunkJava_GooglePAD_Available", "()Z", GA_false),
    M_(CLS, "AndroidThunkJava_IsAllowedRemoteNotifications", "()Z", GA_false),
    { NULL, NULL, NULL, NULL }
};

void tl_ue4_hle_install(const char *pkg, const char *apk, const char *data, const char *ext_files)
{
    snprintf(U.pkg, sizeof(U.pkg), "%s", pkg);
    snprintf(U.apk, sizeof(U.apk), "%s", apk);
    snprintf(U.data, sizeof(U.data), "%s", data);
    snprintf(U.ext, sizeof(U.ext), "%s", ext_files);
    tl_jni_declare("android/app/NativeActivity", "android/app/Activity");
    tl_jni_declare(CLS, "android/app/NativeActivity");
    tl_jni_declare("com/epicgames/ue4/GameApplication", "android/app/Application");
    tl_jni_declare("java/lang/Boolean", "java/lang/Object");
    tl_jni_register_hle(k_hle);
    /* The enum constants EOS reads off EOSOverlay.BrowserStatus: with no class initialiser to run they would be null, which it takes as a broken SDK. */
    static const char *const status[] = { "BEGIN_LOAD", "CLOSED", "CRASHED", "END_LOAD", "LOAD_ERROR" };
    for (size_t i = 0; i < sizeof(status) / sizeof(status[0]); i++) {
        jvalue v; v.j = 0; v.l = tl_jni_new_object(tl_jni_class("com/epicgames/mobile/eossdk/EOSOverlay$BrowserStatus"));
        tl_jni_set_static("com/epicgames/mobile/eossdk/EOSOverlay$BrowserStatus", status[i], "Lcom/epicgames/mobile/eossdk/EOSOverlay$BrowserStatus;", v);
    }
}

void tl_ue4_set_activity(jobj *activity) { U.activity = activity; }
