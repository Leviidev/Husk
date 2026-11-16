/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * The Java side of Minecraft (Bedrock) on Android, implemented in C.
 *
 * Its MainActivity is a Java class of several hundred methods, and the native game calls back into it for what only
 * Android can answer: the device, the storage, the keyboard, the licence, the network, sign-in. PairIP (Google's
 * app-protection layer) has virtualised that class in the APK, so it cannot be run; what it does is rebuilt here from
 * the same class in an earlier build that is not protected, and from what the game is seen to ask. What is not
 * implemented logs once as UNIMPLEMENTED (husk-tl-jni.c) and returns zero.
 */
#define _DARWIN_C_SOURCE
#include "husk-tl-jni.h"

#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <time.h>
#include <unistd.h>
#include <TargetConditionals.h>
#if TARGET_OS_IPHONE
#include <os/proc.h>
#endif

#include "husk-tl-bionic.h"
#include "husk-tl-dexindex.h"
#include "husk-tl-gameactivity.h"
#include "husk-tl-ld.h"

static struct {
    char pkg[128], apk[1024], data[512], files[600], ext[700], prefs[700];
    int width, height;
    struct timespec started;
    jobj *activity;
} M;

static jvalue vl(void *p) { jvalue v; v.j = 0; v.l = p; return v; }
static jvalue vi(int i) { jvalue v; v.j = 0; v.i = i; return v; }
static jvalue vj(int64_t j) { jvalue v; v.j = j; return v; }
static jvalue vz(int z) { jvalue v; v.j = 0; v.z = z != 0; return v; }
static jvalue vf(float f) { jvalue v; v.j = 0; v.f = f; return v; }
#define STR(s) tl_jni_new_string(s)
#define C(name) tl_jni_class(name)
static const char *S(const jobj *o) { const char *s = tl_jni_string(o); return s ? s : ""; }
static jobj *make(const char *cls) { return tl_jni_new_object(C(cls)); }

static void Noop(tl_jcall *c) { (void)c; }
static void RetTrue(tl_jcall *c) { c->ret = vz(1); }
static void RetFalse(tl_jcall *c) { c->ret = vz(0); }
static void RetZero(tl_jcall *c) { c->ret = vi(0); }
static void RetEmptyString(tl_jcall *c) { c->ret = vl(STR("")); }
static void RetNull(tl_jcall *c) { c->ret = vl(NULL); }

/* -------------------------------------------------------------- preferences */

/*
 * What MainActivity keeps in default SharedPreferences (the device id, the secure-storage keys, the sign-in profile):
 * text, in memory, written back to one file on every change.
 */
typedef struct { char *key, *val; } kv;
static kv *g_kv; static int g_nkv, g_capkv;
static pthread_mutex_t g_kv_mu = PTHREAD_MUTEX_INITIALIZER;

static void kv_save(void)
{
    char tmp[720]; snprintf(tmp, sizeof(tmp), "%s.tmp", M.prefs);
    FILE *f = fopen(tmp, "w");
    if (!f) return;
    for (int i = 0; i < g_nkv; i++) {
        for (const char *p = g_kv[i].key; *p; p++) { if (*p == '\\' || *p == '\n' || *p == '\t') fputc('\\', f); fputc(*p == '\n' ? 'n' : *p == '\t' ? 't' : *p, f); }
        fputc('\t', f);
        for (const char *p = g_kv[i].val; *p; p++) { if (*p == '\\' || *p == '\n' || *p == '\t') fputc('\\', f); fputc(*p == '\n' ? 'n' : *p == '\t' ? 't' : *p, f); }
        fputc('\n', f);
    }
    fclose(f);
    rename(tmp, M.prefs);
}
static char *kv_unesc(const char *s, size_t n)
{
    char *o = malloc(n + 1); size_t k = 0;
    for (size_t i = 0; i < n; i++) { if (s[i] == '\\' && i + 1 < n) { i++; o[k++] = s[i] == 'n' ? '\n' : s[i] == 't' ? '\t' : s[i]; } else o[k++] = s[i]; }
    o[k] = 0; return o;
}
static void kv_load(void)
{
    FILE *f = fopen(M.prefs, "r");
    if (!f) return;
    char *line = NULL; size_t cap = 0; ssize_t n;
    while ((n = getline(&line, &cap, f)) > 0) {
        if (line[n - 1] == '\n') line[--n] = 0;
        char *tab = memchr(line, '\t', (size_t)n);
        if (!tab) continue;
        if (g_nkv == g_capkv) { g_capkv = g_capkv ? g_capkv * 2 : 32; g_kv = realloc(g_kv, (size_t)g_capkv * sizeof(kv)); }
        g_kv[g_nkv].key = kv_unesc(line, (size_t)(tab - line)); g_kv[g_nkv].val = kv_unesc(tab + 1, (size_t)(n - (tab - line) - 1)); g_nkv++;
    }
    free(line); fclose(f);
}
static const char *kv_get(const char *key)
{
    const char *v = NULL;
    pthread_mutex_lock(&g_kv_mu);
    for (int i = 0; i < g_nkv; i++) if (!strcmp(g_kv[i].key, key)) { v = g_kv[i].val; break; }
    pthread_mutex_unlock(&g_kv_mu);
    return v;
}
static void kv_put(const char *key, const char *val)
{
    pthread_mutex_lock(&g_kv_mu);
    int i;
    for (i = 0; i < g_nkv; i++) if (!strcmp(g_kv[i].key, key)) break;
    if (i < g_nkv) { free(g_kv[i].val); g_kv[i].val = strdup(val); }
    else {
        if (g_nkv == g_capkv) { g_capkv = g_capkv ? g_capkv * 2 : 32; g_kv = realloc(g_kv, (size_t)g_capkv * sizeof(kv)); }
        g_kv[g_nkv].key = strdup(key); g_kv[g_nkv].val = strdup(val); g_nkv++;
    }
    kv_save();
    pthread_mutex_unlock(&g_kv_mu);
}

/* ------------------------------------------------------------------ helpers */

static jvalue build_string(const char *field) { return tl_jni_get_static("android/os/Build", field, "Ljava/lang/String;"); }

static void new_uuid(char *out, bool dashes)
{
    uint8_t b[16];
    arc4random_buf(b, sizeof(b));
    b[6] = (b[6] & 0x0F) | 0x40; b[8] = (b[8] & 0x3F) | 0x80;
    size_t k = 0;
    for (int i = 0; i < 16; i++) {
        if (dashes && (i == 4 || i == 6 || i == 8 || i == 10)) out[k++] = '-';
        k += (size_t)snprintf(out + k, 3, "%02x", b[i]);
    }
}

static uint64_t phys_memory(void) { uint64_t m = 0; size_t n = sizeof(m); sysctlbyname("hw.memsize", &m, &n, NULL, 0); return m; }
/* What the game may count on: the phone's jetsam budget, not its RAM. */
static int64_t avail_memory(void)
{
#if TARGET_OS_IPHONE
    return (int64_t)os_proc_available_memory();
#else
    return 3ll << 30;
#endif
}
#define MEM_THRESHOLD (512ll << 20)
static int64_t total_memory(void) { int64_t t = avail_memory() + (1ll << 30); uint64_t p = phys_memory(); if (p && (uint64_t)t > p) t = (int64_t)p; return t; }

static jobj *string_array(const char *const *v, int n)
{
    jobj *a = tl_jni_new_obj_array(C("java/lang/String"), (uint32_t)n);
    for (int i = 0; i < n; i++) a->oarr.v[i] = STR(v[i]);
    return a;
}

/* ------------------------------------------------------------- MainActivity */

static void MA_internalPath(tl_jcall *c) { c->ret = vl(STR(M.data)); }
static void MA_externalPath(tl_jcall *c) { c->ret = vl(STR(M.ext)); }
static void MA_legacyExternalPath(tl_jcall *c) { c->ret = vl(STR("")); }       /* scoped storage: no shared /sdcard to write to */
static void MA_legacyDeviceId(tl_jcall *c) { const char *v = kv_get("snooperId"); c->ret = vl(STR(v ? v : "")); }
static void MA_createUUID(tl_jcall *c) { char u[40]; new_uuid(u, false); c->ret = vl(STR(u)); }
static void MA_setCachedDeviceId(tl_jcall *c) { kv_put("deviceId", S(c->args[0].l)); }
static void MA_deviceModel(tl_jcall *c)
{
    char m[200]; snprintf(m, sizeof(m), "%s %s", S(build_string("MANUFACTURER").l), S(build_string("MODEL").l));
    for (char *p = m; *p; p++) if (*p >= 'a' && *p <= 'z') *p -= 32;
    c->ret = vl(STR(m));
}
static void MA_androidVersion(tl_jcall *c) { c->ret = vi(tl_jni_get_static("android/os/Build$VERSION", "SDK_INT", "I").i); }
static void MA_apiVersion(tl_jcall *c)
{
    static const struct { const char *n; int v; } t[] = { {"BASE",1},{"BASE_1_1",2},{"CUPCAKE",3},{"DONUT",4},{"ECLAIR",5},{"ECLAIR_0_1",6},{"ECLAIR_MR1",7},
        {"FROYO",8},{"GINGERBREAD",9},{"GINGERBREAD_MR1",10},{"HONEYCOMB",11},{"HONEYCOMB_MR1",12},{"HONEYCOMB_MR2",13},{"ICE_CREAM_SANDWICH",14},
        {"ICE_CREAM_SANDWICH_MR1",15},{"JELLY_BEAN",16},{"JELLY_BEAN_MR1",17},{"JELLY_BEAN_MR2",18},{"KITKAT",19},{"KITKAT_WATCH",20},{"LOLLIPOP",21},
        {"LOLLIPOP_MR1",22},{"M",23},{"N",24},{"N_MR1",25},{"O",26},{"O_MR1",27},{"P",28},{"Q",29},{"R",30},{"S",31},{"S_V2",32},{"TIRAMISU",33},
        {"UPSIDE_DOWN_CAKE",34},{"VANILLA_ICE_CREAM",35},{"BAKLAVA",36} };
    const char *n = S(c->args[0].l);
    for (size_t i = 0; i < sizeof(t) / sizeof(t[0]); i++) if (!strcmp(t[i].n, n)) { c->ret = vi(t[i].v); return; }
    c->ret = vi(-1);
}
static void MA_locale(tl_jcall *c) { c->ret = vl(STR("en_US")); }
static void MA_screenWidth(tl_jcall *c) { c->ret = vi(M.width > M.height ? M.width : M.height); }
static void MA_screenHeight(tl_jcall *c) { c->ret = vi(M.width > M.height ? M.height : M.width); }
static void MA_displayWidth(tl_jcall *c) { c->ret = vi(M.width); }
static void MA_displayHeight(tl_jcall *c) { c->ret = vi(M.height); }
static void MA_platformDpi(tl_jcall *c) { c->ret = vi(420); }
static void MA_platformStringVar(tl_jcall *c) { c->ret = vl(c->args[0].i == 0 ? STR(S(build_string("MODEL").l)) : NULL); }
static void MA_timeFromStart(tl_jcall *c)
{
    struct timespec now; clock_gettime(CLOCK_MONOTONIC, &now);
    c->ret = vj((int64_t)(now.tv_sec - M.started.tv_sec) * 1000 + (now.tv_nsec - M.started.tv_nsec) / 1000000);
}
static void MA_totalMemory(tl_jcall *c) { c->ret = vj(total_memory()); }
static void MA_freeMemory(tl_jcall *c) { int64_t f = avail_memory() - MEM_THRESHOLD; c->ret = vj(f > 0 ? f : 0); }
static void MA_memoryLimit(tl_jcall *c) { c->ret = vj(total_memory() - MEM_THRESHOLD); }
static void MA_lowMemoryThreshold(tl_jcall *c) { c->ret = vj(MEM_THRESHOLD); }
static void MA_usedMemory(tl_jcall *c) { int64_t u = total_memory() - avail_memory(); c->ret = vj(u > 0 ? u : 0); }
static void MA_debugMemoryInfo(tl_jcall *c) { c->ret = vj(0); }
static void MA_totalSpace(tl_jcall *c) { struct statfs s; c->ret = vj(statfs(S(c->args[0].l), &s) == 0 ? (int64_t)s.f_blocks * s.f_bsize : 0); }
static void MA_usableSpace(tl_jcall *c) { struct statfs s; c->ret = vj(statfs(S(c->args[0].l), &s) == 0 ? (int64_t)s.f_bavail * s.f_bsize : 0); }
static void MA_secureGet(tl_jcall *c) { const char *v = kv_get(S(c->args[0].l)); c->ret = vl(STR(v ? v : "")); }
static void MA_secureSet(tl_jcall *c) { kv_put(S(c->args[0].l), S(c->args[1].l)); }
static void MA_kvString(tl_jcall *c, const char *key) { const char *v = kv_get(key); c->ret = vl(STR(v ? v : "")); }
static void MA_profileId(tl_jcall *c) { MA_kvString(c, "profileId"); }
static void MA_profileName(tl_jcall *c) { MA_kvString(c, "profileName"); }
static void MA_clientId(tl_jcall *c) { MA_kvString(c, "clientId"); }
static void MA_accessToken(tl_jcall *c) { MA_kvString(c, "accessToken"); }
static jobj *g_mc_singletons[6];
static void MA_hardwareInfo(tl_jcall *c) { if (!g_mc_singletons[0]) g_mc_singletons[0] = make("com/mojang/minecraftpe/HardwareInformation"); c->ret = vl(tl_jni_ref(g_mc_singletons[0])); }
static void MA_batteryMonitor(tl_jcall *c) { if (!g_mc_singletons[1]) g_mc_singletons[1] = make("com/mojang/minecraftpe/BatteryMonitor"); c->ret = vl(tl_jni_ref(g_mc_singletons[1])); }
static void MA_thermalMonitor(tl_jcall *c) { if (!g_mc_singletons[2]) g_mc_singletons[2] = make("com/mojang/minecraftpe/ThermalMonitor"); c->ret = vl(tl_jni_ref(g_mc_singletons[2])); }
static void MA_crashManager(tl_jcall *c) { if (!g_mc_singletons[3]) g_mc_singletons[3] = make("com/mojang/minecraftpe/CrashManager"); c->ret = vl(tl_jni_ref(g_mc_singletons[3])); }
static void MA_appExitInfoHelper(tl_jcall *c) { if (!g_mc_singletons[4]) g_mc_singletons[4] = make("com/mojang/minecraftpe/AppExitInfoHelper"); c->ret = vl(tl_jni_ref(g_mc_singletons[4])); }
static void MA_ipAddresses(tl_jcall *c) { const char *a[] = { "192.168.1.2" }; c->ret = vl(string_array(a, 1)); }
static void MA_broadcastAddresses(tl_jcall *c) { const char *a[] = { "192.168.1.255" }; c->ret = vl(string_array(a, 1)); }
static void MA_userInputStatus(tl_jcall *c) { c->ret = vi(-1); }                 /* no dialog is open */
static void MA_userInputString(tl_jcall *c) { const char *a[] = { "" }; c->ret = vl(string_array(a, 1)); }
static void MA_keyboardHeight(tl_jcall *c) { c->ret = vf(0.f); }

/* GameTextInput: the field's state, the keyboard's visibility and the field's Return action, for the iPhone's keyboard
 * (husk-tl-gameactivity.c does the typing). */
static void GTI_setState(tl_jcall *c)
{
    jobj *st = c->args[0].l;
    if (!st) return;
    jvalue t = tl_jni_get_field(st, "text", "Ljava/lang/String;");
    tl_ga_text_state(tl_jni_string(t.l), tl_jni_get_field(st, "selectionStart", "I").i, tl_jni_get_field(st, "selectionEnd", "I").i);
}
/* GameTextInput makes the field's Java string from UTF-8 bytes: Charset.forName("UTF-8").decode(ByteBuffer).toString(). */
static void CS_forName(tl_jcall *c) { c->ret = vl(tl_jni_new_object(tl_jni_class("java/nio/charset/Charset"))); }
static void CS_decode(tl_jcall *c)
{
    jobj *buf = c->args[0].l, *cb = tl_jni_new_object(tl_jni_class("java/nio/CharBuffer"));
    const char *p = buf ? tl_jni_get_field(buf, "address", "J").l : NULL;
    int64_t n = buf ? tl_jni_get_field(buf, "capacity", "J").j : 0;
    char tmp[16384];
    if (n < 0) n = 0;
    if (n > (int64_t)sizeof(tmp) - 1) n = sizeof(tmp) - 1;
    if (p && n) memcpy(tmp, p, (size_t)n);
    tmp[n] = 0;
    jvalue v; v.j = 0; v.l = tl_jni_new_string(tmp);
    tl_jni_set_field(cb, "str", "Ljava/lang/String;", v);
    c->ret = vl(cb);
}
static void CB_toString(tl_jcall *c) { c->ret = tl_jni_get_field(c->self, "str", "Ljava/lang/String;"); }
static void GTI_keyboard(tl_jcall *c) { tl_ga_keyboard(c->args[0].z != 0); }
static void GTI_imeFields(tl_jcall *c) { tl_ga_ime_options(c->args[2].i); }
void (*tl_cocos_open_url_hook_mc)(const char *url);
static void MA_launchUri(tl_jcall *c) { tl_log_line("minecraft: launchUri %s", S(c->args[0].l)); if (tl_cocos_open_url_hook_mc) tl_cocos_open_url_hook_mc(S(c->args[0].l)); }
static void MA_quit(tl_jcall *c) { (void)c; tl_log_line("minecraft: the game asked to quit"); if (tl_guest_exit_hook) tl_guest_exit_hook(0); }
static void MA_setClipboard(tl_jcall *c) { tl_log_line("minecraft: clipboard <- %.60s", S(c->args[0].l)); }

/* runOnUiThread(() -> nativeRunNativeCallbackOnUiThread(cb)), and wait for it. */
typedef struct { int64_t cb; pthread_mutex_t mu; pthread_cond_t cv; bool done; } ui_call;
static void ui_call_run(void *arg)
{
    ui_call *u = arg;
    typedef void (*fn_t)(void *env, void *self, int64_t cb);
    fn_t fn = (fn_t)tl_jni_native("com/mojang/minecraftpe/MainActivity", "nativeRunNativeCallbackOnUiThread", "(J)V");
    if (!fn) { tl_lib *lib = tl_ld_find_lib("libminecraftpe.so"); fn = lib ? (fn_t)tl_ld_sym(lib, "Java_com_mojang_minecraftpe_MainActivity_nativeRunNativeCallbackOnUiThread") : NULL; }
    if (fn) fn(tl_jni_env(), M.activity, u->cb);
    pthread_mutex_lock(&u->mu); u->done = true; pthread_cond_signal(&u->cv); pthread_mutex_unlock(&u->mu);
}
static void MA_runNativeCallbackOnUiThread(tl_jcall *c)
{
    ui_call u = { .cb = c->args[0].j, .mu = PTHREAD_MUTEX_INITIALIZER, .cv = PTHREAD_COND_INITIALIZER };
    if (tl_ga_is_ui_thread()) { ui_call_run(&u); return; }
    tl_ga_post(ui_call_run, &u);
    pthread_mutex_lock(&u.mu);
    while (!u.done) pthread_cond_wait(&u.cv, &u.mu);
    pthread_mutex_unlock(&u.mu);
}

static void MA_requestIntegrityToken(tl_jcall *c)
{
    (void)c;
    typedef void (*fn_t)(void *env, void *self, void *msg);
    tl_lib *lib = tl_ld_find_lib("libminecraftpe.so");
    fn_t fn = lib ? (fn_t)tl_ld_sym(lib, "Java_com_mojang_minecraftpe_MainActivity_nativeSetIntegrityTokenErrorMessage") : NULL;
    if (fn) fn(tl_jni_env(), M.activity, STR("Play Integrity is not available"));
}

/* ------------------------------------------------- HardwareInformation etc. */

static void HW_board(tl_jcall *c) { c->ret = vl(STR(S(build_string("BOARD").l))); }
static void HW_cpuFeatures(tl_jcall *c) { c->ret = vl(STR("fp asimd evtstrm aes pmull sha1 sha2 crc32 atomics fphp asimdhp cpuid asimdrdm lrcpc dcpop asimddp")); }
static void HW_cpuName(tl_jcall *c) { c->ret = vl(STR("Apple A-series")); }
static void HW_cpuType(tl_jcall *c) { c->ret = vl(STR("arm64-v8a")); }
static void HW_deviceModelName(tl_jcall *c) { MA_deviceModel(c); }
static void HW_numClusters(tl_jcall *c) { c->ret = vi(2); }
static void HW_numCores(tl_jcall *c) { int n = 6; size_t sz = sizeof(n); sysctlbyname("hw.ncpu", &n, &sz, NULL, 0); c->ret = vi(n); }
static void HW_perfCores(tl_jcall *c) { int n = 6; size_t sz = sizeof(n); sysctlbyname("hw.perflevel0.logicalcpu", &n, &sz, NULL, 0); c->ret = vi(n); }
static void HW_socName(tl_jcall *c) { c->ret = vl(STR("Apple")); }
static void HW_androidVersion(tl_jcall *c) { char v[40]; snprintf(v, sizeof(v), "Android %s", S(tl_jni_get_static("android/os/Build$VERSION", "RELEASE", "Ljava/lang/String;").l)); c->ret = vl(STR(v)); }
static void HW_installer(tl_jcall *c) { c->ret = vl(STR("com.android.vending")); }
static void HW_secureId(tl_jcall *c) { const char *v = kv_get("androidId"); if (!v) { char u[40]; new_uuid(u, false); u[16] = 0; kv_put("androidId", u); v = kv_get("androidId"); } c->ret = vl(STR(v)); }
static void HW_signatures(tl_jcall *c) { c->ret = vi(0); }

static void Crash_uploadURI(tl_jcall *c) { c->ret = vl(STR("")); }
static void Crash_uploadFile(tl_jcall *c) { c->ret = vl(STR("")); }
static void Notify_token(tl_jcall *c) { c->ret = vl(STR("")); }

/* ------------------------------------------------------------------- store */

/*
 * The Google Play store: the game is treated as bought, with nothing to buy. The real store answers asynchronously
 * through the listener the game gave it (NativeStoreListener, whose natives call back into the game); so does this,
 * from the UI thread, once the call that asked has returned.
 */
static void *store_native(const char *name)
{
    char mangled[200];
    snprintf(mangled, sizeof(mangled), "Java_com_mojang_minecraftpe_store_NativeStoreListener_%s", name);
    tl_lib *lib = tl_ld_find_lib("libminecraftpe.so");
    return lib ? tl_ld_sym(lib, mangled) : NULL;
}
static int64_t listener_ptr(jobj *l) { return l ? tl_jni_get_field(l, "mStoreListener", "J").j : 0; }

typedef struct { jobj *listener; int what; } store_job;
static void store_job_run(void *arg)
{
    store_job *j = arg;
    int64_t ptr = listener_ptr(j->listener);
    void *env = tl_jni_env();
    if (j->what == 0) {
        void (*fn)(void *, void *, int64_t, uint8_t) = store_native("onStoreInitialized");
        if (fn) fn(env, j->listener, ptr, 1);
    } else if (j->what == 1) {
        void (*fn)(void *, void *, int64_t, void *) = store_native("onQueryPurchasesSuccess");
        if (fn) fn(env, j->listener, ptr, tl_jni_new_obj_array(C("com/mojang/minecraftpe/store/Purchase"), 0));
    } else {
        void (*fn)(void *, void *, int64_t, void *) = store_native("onQueryProductsSuccess");
        if (fn) fn(env, j->listener, ptr, tl_jni_new_obj_array(C("com/mojang/minecraftpe/store/Product"), 0));
    }
    free(j);
}
static void store_post(jobj *store, int what)
{
    jobj *l = store ? tl_jni_get_field(store, "listener", "Ljava/lang/Object;").l : NULL;
    if (!l) return;
    store_job *j = malloc(sizeof(*j)); j->listener = l; j->what = what;
    tl_ga_post(store_job_run, j);
}

static void NSL_init(tl_jcall *c) { tl_jni_set_field(c->self, "mStoreListener", "J", c->args[0]); }
static void Store_create(tl_jcall *c)
{
    jobj *store = make("com/mojang/minecraftpe/store/googleplay/GooglePlayStore");
    tl_jni_set_field(store, "listener", "Ljava/lang/Object;", vl(c->args[1].l ? tl_jni_ref(c->args[1].l) : NULL));
    c->ret = vl(store);
    store_post(store, 0);                                    /* onStoreInitialized(true) */
}
static void Store_queryPurchases(tl_jcall *c) { store_post(c->self, 1); }
static void Store_queryProducts(tl_jcall *c) { store_post(c->self, 2); }
static void Store_id(tl_jcall *c) { c->ret = vl(STR("android.googleplay")); }
static void Store_extra(tl_jcall *c) { c->ret = vl(make("com/mojang/minecraftpe/store/ExtraLicenseResponseData")); }

/* -------------------------------------------------- Xbox Live / PlayFab helpers */

static void XAL_deviceId(tl_jcall *c) { const char *v = kv_get("xalDeviceId"); if (!v) { char u[40]; new_uuid(u, true); kv_put("xalDeviceId", u); v = kv_get("xalDeviceId"); } c->ret = vl(STR(v)); }
static void XAL_osVersion(tl_jcall *c) { c->ret = vl(STR(S(tl_jni_get_static("android/os/Build$VERSION", "RELEASE", "Ljava/lang/String;").l))); }
static void XAL_storagePath(tl_jcall *c) { char p[800]; snprintf(p, sizeof(p), "%s/xal", M.files); mkdir(p, 0755); c->ret = vl(STR(p)); }
static void XAL_randomBytes(tl_jcall *c)
{
    int n = c->args[0].i;
    jobj *a = tl_jni_new_prim_array('B', (uint32_t)(n > 0 ? n : 0));
    if (n > 0) arc4random_buf(a->arr.data, (size_t)n);
    c->ret = vl(a);
}
static void XAL_appContext(tl_jcall *c) { c->ret = vl(M.activity); }
static void XAL_locale(tl_jcall *c) { c->ret = vl(STR("en-US")); }
static void Playfab_uuid(tl_jcall *c) { char u[40]; new_uuid(u, true); c->ret = vl(STR(u)); }
static void DateTime_is24(tl_jcall *c) { c->ret = vz(0); }

/* ------------------------------------------------------------- class loader */

/*
 * A native thread cannot FindClass the app's classes (it sees only the system loader), so the game keeps
 * MainActivity's class loader and asks it by name, dotted. The answer is the class if the APK or the framework
 * has it, and ClassNotFoundException if not.
 */
static void CL_findClass(tl_jcall *c)
{
    char name[300]; snprintf(name, sizeof(name), "%s", S(c->args[0].l));
    for (char *p = name; *p; p++) if (*p == '.') *p = '/';
    bool framework = !strncmp(name, "android/", 8) || !strncmp(name, "java/", 5) || !strncmp(name, "javax/", 6) || !strncmp(name, "dalvik/", 7);
    if (framework || tl_dexidx_has_class(name)) { c->ret = vl(tl_jni_class_object(name)); return; }
    tl_jni_throw("java/lang/ClassNotFoundException", name);
    c->ret = vl(NULL);
}

/* ------------------------------------------------------------ locale (config) */

static void Config_getLocales(tl_jcall *c) { if (!g_mc_singletons[5]) g_mc_singletons[5] = make("android/os/LocaleList"); c->ret = vl(tl_jni_ref(g_mc_singletons[5])); }
static void LocaleList_size(tl_jcall *c) { c->ret = vi(1); }
static void LocaleList_get(tl_jcall *c) { c->ret = vl(make("java/util/Locale")); }
static void Locale_script(tl_jcall *c) { c->ret = vl(STR("")); }
static void Locale_variant(tl_jcall *c) { c->ret = vl(STR("")); }

/* ------------------------------------------------------------------- tables */

static const struct { const char *name, *super; } k_classes[] = {
    { "androidx/appcompat/app/AppCompatActivity", "android/app/Activity" },
    { "com/google/androidgamesdk/GameActivity", "androidx/appcompat/app/AppCompatActivity" },
    { "com/mojang/minecraftpe/MainActivity", "com/google/androidgamesdk/GameActivity" },
    { "com/mojang/minecraftpe/HardwareInformation", "java/lang/Object" }, { "com/mojang/minecraftpe/BatteryMonitor", "java/lang/Object" },
    { "com/mojang/minecraftpe/ThermalMonitor", "java/lang/Object" }, { "com/mojang/minecraftpe/CrashManager", "java/lang/Object" },
    { "com/mojang/minecraftpe/AppExitInfoHelper", "java/lang/Object" }, { "com/mojang/minecraftpe/DateTimeHelper", "java/lang/Object" },
    { "com/mojang/minecraftpe/NotificationListenerService", "java/lang/Object" }, { "com/mojang/minecraftpe/BrazeManager", "java/lang/Object" },
    { "com/mojang/minecraftpe/store/Store", "java/lang/Object" }, { "com/mojang/minecraftpe/store/StoreFactory", "java/lang/Object" },
    { "com/mojang/minecraftpe/store/googleplay/GooglePlayStore", "com/mojang/minecraftpe/store/Store" },
    { "com/mojang/minecraftpe/store/ExtraLicenseResponseData", "java/lang/Object" }, { "com/mojang/minecraftpe/store/NativeStoreListener", "java/lang/Object" },
    { "com/mojang/minecraftpe/store/Product", "java/lang/Object" }, { "com/mojang/minecraftpe/store/Purchase", "java/lang/Object" },
    { "com/google/androidgamesdk/gametextinput/InputConnection", "java/lang/Object" }, { "com/google/androidgamesdk/gametextinput/State", "java/lang/Object" },
    { "java/nio/charset/Charset", "java/lang/Object" }, { "java/nio/CharBuffer", "java/lang/Object" },
    { "java/lang/ClassNotFoundException", "java/lang/Exception" }, { "android/os/LocaleList", "java/lang/Object" }, { "com/microsoft/xal/androidjava/DeviceInfo", "java/lang/Object" }, { "com/microsoft/xal/androidjava/Storage", "java/lang/Object" },
    { "com/microsoft/xal/crypto/SecureRandom", "java/lang/Object" }, { "com/microsoft/xbox/idp/interop/Interop", "java/lang/Object" },
    { "com/microsoft/xboxlive/LocalStorage", "java/lang/Object" }, { "com/microsoft/playfab/utilities/multiplayer/AndroidJniHelperMultiplayer", "java/lang/Object" },
};

#define MAIN "com/mojang/minecraftpe/MainActivity"
#define HWI "com/mojang/minecraftpe/HardwareInformation"
#define STORE "com/mojang/minecraftpe/store/Store"
#define M_(c, n, s, f) { c, n, s, f }
static const tl_jhle k_hle[] = {
    M_(MAIN, "getInternalStoragePath", "()Ljava/lang/String;", MA_internalPath),
    M_(MAIN, "getExternalStoragePath", "()Ljava/lang/String;", MA_externalPath),
    M_(MAIN, "getLegacyExternalStoragePath", "(Ljava/lang/String;)Ljava/lang/String;", MA_legacyExternalPath),
    M_(MAIN, "getLegacyDeviceID", "()Ljava/lang/String;", MA_legacyDeviceId),
    M_(MAIN, "createUUID", "()Ljava/lang/String;", MA_createUUID),
    M_(MAIN, "setCachedDeviceId", "(Ljava/lang/String;)V", MA_setCachedDeviceId),
    M_(MAIN, "getDeviceModel", "()Ljava/lang/String;", MA_deviceModel),
    M_(MAIN, "getAndroidVersion", "()I", MA_androidVersion),
    M_(MAIN, "getAPIVersion", "(Ljava/lang/String;)I", MA_apiVersion),
    M_(MAIN, "getLocale", "()Ljava/lang/String;", MA_locale),
    M_(MAIN, "getScreenWidth", "()I", MA_screenWidth), M_(MAIN, "getScreenHeight", "()I", MA_screenHeight),
    M_(MAIN, "getDisplayWidth", "()I", MA_displayWidth), M_(MAIN, "getDisplayHeight", "()I", MA_displayHeight),
    M_(MAIN, "getPlatformDpi", "()I", MA_platformDpi),
    M_(MAIN, "hasWriteExternalStoragePermission", "()Z", RetTrue),
    M_(MAIN, "checkLicense", "()I", RetZero),
    M_(MAIN, "isDemo", "()Z", RetFalse),
    M_(MAIN, "getPlatformStringVar", "(I)Ljava/lang/String;", MA_platformStringVar),
    M_(MAIN, "getTimeFromProcessStart", "()J", MA_timeFromStart),
    M_(MAIN, "getTotalMemory", "()J", MA_totalMemory), M_(MAIN, "getFreeMemory", "()J", MA_freeMemory),
    M_(MAIN, "getMemoryLimit", "()J", MA_memoryLimit), M_(MAIN, "getLowMemoryThreshold", "()J", MA_lowMemoryThreshold),
    M_(MAIN, "getUsedMemory", "()J", MA_usedMemory), M_(MAIN, "getDebugMemoryInfo", "(Ljava/lang/String;)J", MA_debugMemoryInfo),
    M_(MAIN, "getTotalSpace", "(Ljava/lang/String;)J", MA_totalSpace), M_(MAIN, "getUsableSpace", "(Ljava/lang/String;)J", MA_usableSpace),
    M_(MAIN, "calculateAvailableDiskFreeSpace", "(Ljava/lang/String;)J", MA_usableSpace),
    M_(MAIN, "supportsSizeQuery", "(Ljava/lang/String;)Z", RetTrue),
    M_(MAIN, "hasHardwareKeyboard", "()Z", RetFalse), M_(MAIN, "isTablet", "()Z", RetFalse), M_(MAIN, "isChromebook", "()Z", RetFalse),
    M_(MAIN, "getIsRunningInBrowserStack", "()Z", RetFalse), M_(MAIN, "hasBuyButtonWhenInvalidLicense", "()Z", RetFalse),
    M_(MAIN, "getSecureStorageKey", "(Ljava/lang/String;)Ljava/lang/String;", MA_secureGet),
    M_(MAIN, "setSecureStorageKey", "(Ljava/lang/String;Ljava/lang/String;)V", MA_secureSet),
    M_(MAIN, "getProfileId", "()Ljava/lang/String;", MA_profileId), M_(MAIN, "getProfileName", "()Ljava/lang/String;", MA_profileName),
    M_(MAIN, "getClientId", "()Ljava/lang/String;", MA_clientId), M_(MAIN, "getAccessToken", "()Ljava/lang/String;", MA_accessToken),
    M_(MAIN, "getHardwareInfo", "()Lcom/mojang/minecraftpe/HardwareInformation;", MA_hardwareInfo),
    M_(MAIN, "getBatteryMonitor", "()Lcom/mojang/minecraftpe/BatteryMonitor;", MA_batteryMonitor),
    M_(MAIN, "getThermalMonitor", "()Lcom/mojang/minecraftpe/ThermalMonitor;", MA_thermalMonitor),
    M_(MAIN, "initializeCrashManager", "(Ljava/lang/String;Ljava/lang/String;)Lcom/mojang/minecraftpe/CrashManager;", MA_crashManager),
    M_(MAIN, "initializeAppExitInfoHelper", "()Lcom/mojang/minecraftpe/AppExitInfoHelper;", MA_appExitInfoHelper),
    M_(MAIN, "getIPAddresses", "()[Ljava/lang/String;", MA_ipAddresses), M_(MAIN, "getBroadcastAddresses", "()[Ljava/lang/String;", MA_broadcastAddresses),
    M_(MAIN, "getUserInputStatus", "()I", MA_userInputStatus), M_(MAIN, "getUserInputString", "()[Ljava/lang/String;", MA_userInputString),
    M_(MAIN, "getKeyboardHeight", "()F", MA_keyboardHeight),
    M_(MAIN, "launchUri", "(Ljava/lang/String;)V", MA_launchUri), M_(MAIN, "quit", "()V", MA_quit),
    M_(MAIN, "setClipboard", "(Ljava/lang/String;)V", MA_setClipboard),
    M_(MAIN, "runNativeCallbackOnUiThread", "(J)V", MA_runNativeCallbackOnUiThread),
    M_(MAIN, "requestIntegrityToken", "(Ljava/lang/String;)V", MA_requestIntegrityToken),
    M_(MAIN, "tick", "()V", Noop), M_(MAIN, "updateLocalization", "(Ljava/lang/String;Ljava/lang/String;)V", Noop),
    M_(MAIN, "vibrate", "(I)V", Noop), M_(MAIN, "setKeepScreenOnFlag", "(Z)V", Noop), M_(MAIN, "setVolume", "(F)V", Noop),
    M_(MAIN, "setIsPowerVR", "(Z)V", Noop), M_(MAIN, "lockCursor", "()V", Noop), M_(MAIN, "unlockCursor", "()V", Noop),
    M_(MAIN, "deviceIdCorrelationStart", "()V", Noop), M_(MAIN, "buyGame", "()V", Noop),
    M_(MAIN, "initializeMulticast", "()V", Noop), M_(MAIN, "acquireMulticast", "()V", Noop), M_(MAIN, "releaseMulticast", "()V", Noop),
    M_(MAIN, "setMulticastReferenceCounting", "(Z)V", Noop), M_(MAIN, "isMulticastHeld", "()Z", RetFalse),
    M_(MAIN, "isTTSEnabled", "()Z", RetFalse), M_(MAIN, "isTTSInstalled", "()Z", RetFalse), M_(MAIN, "isTextToSpeechInProgress", "()Z", RetFalse),
    M_(MAIN, "supportsTTSLanguage", "(Ljava/lang/String;)Z", RetFalse),

    M_(HWI, "getBoard", "()Ljava/lang/String;", HW_board), M_(HWI, "getCPUFeatures", "()Ljava/lang/String;", HW_cpuFeatures),
    M_(HWI, "getCPUName", "()Ljava/lang/String;", HW_cpuName), M_(HWI, "getCPUType", "()Ljava/lang/String;", HW_cpuType),
    M_(HWI, "getDeviceModelName", "()Ljava/lang/String;", HW_deviceModelName), M_(HWI, "getLocale", "()Ljava/lang/String;", MA_locale),
    M_(HWI, "getNumClusters", "()I", HW_numClusters), M_(HWI, "getNumCores", "()I", HW_numCores),
    M_(HWI, "getPerformanceCoreCount", "()I", HW_perfCores), M_(HWI, "getSerialNumber", "()Ljava/lang/String;", RetEmptyString),
    M_(HWI, "getSoCName", "()Ljava/lang/String;", HW_socName), M_(HWI, "getAndroidVersion", "()Ljava/lang/String;", HW_androidVersion),
    M_(HWI, "getInstallerPackageName", "()Ljava/lang/String;", HW_installer), M_(HWI, "getIsRooted", "()Z", RetFalse),
    M_(HWI, "getSecureId", "()Ljava/lang/String;", HW_secureId), M_(HWI, "getSignaturesHashCode", "()I", HW_signatures),
    M_("com/mojang/minecraftpe/CrashManager", "getCrashUploadURI", "()Ljava/lang/String;", Crash_uploadURI),
    M_("com/mojang/minecraftpe/CrashManager", "getExceptionUploadURI", "()Ljava/lang/String;", Crash_uploadURI),
    M_("com/mojang/minecraftpe/CrashManager", "uploadCrashFile", "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)Ljava/lang/String;", Crash_uploadFile),
    M_("com/mojang/minecraftpe/AppExitInfoHelper", "readyForAppExitInfo", "()V", Noop),
    M_("com/mojang/minecraftpe/DateTimeHelper", "Is24HourTimeFormat", "()Z", DateTime_is24),
    M_("com/mojang/minecraftpe/NotificationListenerService", "getDeviceRegistrationToken", "()Ljava/lang/String;", Notify_token),
    M_("com/mojang/minecraftpe/BrazeManager", "disableBrazeSDK", "()V", Noop), M_("com/mojang/minecraftpe/BrazeManager", "enableBrazeSDK", "()V", Noop),
    M_("com/mojang/minecraftpe/BrazeManager", "isBrazeSDKDisabled", "()Z", RetTrue), M_("com/mojang/minecraftpe/BrazeManager", "setBrazeID", "(Ljava/lang/String;)V", Noop),

    M_("com/mojang/minecraftpe/store/StoreFactory", "createGooglePlayStore", "(Ljava/lang/String;Lcom/mojang/minecraftpe/store/StoreListener;)Lcom/mojang/minecraftpe/store/Store;", Store_create),
    M_(STORE, "hasVerifiedLicense", "()Z", RetTrue), M_(STORE, "receivedLicenseResponse", "()Z", RetTrue),
    M_(STORE, "getStoreId", "()Ljava/lang/String;", Store_id), M_(STORE, "getProductSkuPrefix", "()Ljava/lang/String;", RetEmptyString),
    M_(STORE, "getRealmsSkuPrefix", "()Ljava/lang/String;", RetEmptyString),
    M_(STORE, "getExtraLicenseData", "()Lcom/mojang/minecraftpe/store/ExtraLicenseResponseData;", Store_extra),
    M_(STORE, "queryProducts", "([Ljava/lang/String;)V", Store_queryProducts), M_(STORE, "queryPurchases", "()V", Store_queryPurchases),
    M_(STORE, "purchase", "(Ljava/lang/String;ZLjava/lang/String;)V", Noop), M_(STORE, "purchaseGame", "()V", Noop),
    M_(STORE, "acknowledgePurchase", "(Ljava/lang/String;Ljava/lang/String;)V", Noop), M_(STORE, "destructor", "()V", Noop),
    M_("com/mojang/minecraftpe/store/NativeStoreListener", "<init>", "(J)V", NSL_init),
    M_("com/mojang/minecraftpe/store/ExtraLicenseResponseData", "getValidationTime", "()J", RetZero),
    M_("com/mojang/minecraftpe/store/ExtraLicenseResponseData", "getRetryUntilTime", "()J", RetZero),
    M_("com/mojang/minecraftpe/store/ExtraLicenseResponseData", "getRetryAttempts", "()J", RetZero),

    M_("com/microsoft/xal/androidjava/DeviceInfo", "GetDeviceId", "(Landroid/content/Context;)Ljava/lang/String;", XAL_deviceId),
    M_("com/microsoft/xal/androidjava/DeviceInfo", "GetOsVersion", "()Ljava/lang/String;", XAL_osVersion),
    M_("com/microsoft/xal/androidjava/Storage", "getStoragePath", "(Landroid/content/Context;)Ljava/lang/String;", XAL_storagePath),
    M_("com/microsoft/xal/androidjava/XalInitTelemetry", "initOneDS", "(Landroid/content/Context;)V", Noop),
    M_("com/microsoft/xal/androidjava/PresenceManager", "attach", "()V", Noop),
    M_("com/microsoft/xal/crypto/SecureRandom", "GenerateRandomBytes", "(I)[B", XAL_randomBytes),
    M_("com/microsoft/xbox/idp/interop/Interop", "GetLocalStoragePath", "(Landroid/content/Context;)Ljava/lang/String;", XAL_storagePath),
    M_("com/microsoft/xbox/idp/interop/Interop", "getApplicationContext", "()Landroid/content/Context;", XAL_appContext),
    M_("com/microsoft/xbox/idp/interop/Interop", "getLocale", "()Ljava/lang/String;", XAL_locale),
    M_("com/microsoft/xboxlive/LocalStorage", "getPath", "(Landroid/content/Context;)Ljava/lang/String;", XAL_storagePath),
    M_("com/microsoft/playfab/utilities/multiplayer/AndroidJniHelperMultiplayer", "createUUID", "()Ljava/lang/String;", Playfab_uuid),
    M_("com/google/androidgamesdk/gametextinput/InputConnection", "setState", "(Lcom/google/androidgamesdk/gametextinput/State;)V", GTI_setState),
    M_("java/nio/charset/Charset", "forName", "(Ljava/lang/String;)Ljava/nio/charset/Charset;", CS_forName),
    M_("java/nio/charset/Charset", "decode", "(Ljava/nio/ByteBuffer;)Ljava/nio/CharBuffer;", CS_decode),
    M_("java/nio/CharBuffer", "toString", "()Ljava/lang/String;", CB_toString),
    M_("com/google/androidgamesdk/GameActivity", "setTextInputState", "(Lcom/google/androidgamesdk/gametextinput/State;)V", GTI_setState),
    M_("com/google/androidgamesdk/gametextinput/InputConnection", "setSoftKeyboardActive", "(ZI)V", GTI_keyboard),
    M_("com/google/androidgamesdk/gametextinput/InputConnection", "restartInput", "()V", Noop),
    M_("java/lang/ClassLoader", "findClass", "(Ljava/lang/String;)Ljava/lang/Class;", CL_findClass),
    M_("java/lang/ClassLoader", "loadClass", "(Ljava/lang/String;)Ljava/lang/Class;", CL_findClass),
    M_("android/content/res/Configuration", "getLocales", "()Landroid/os/LocaleList;", Config_getLocales),
    M_("android/os/LocaleList", "size", "()I", LocaleList_size), M_("android/os/LocaleList", "get", "(I)Ljava/util/Locale;", LocaleList_get),
    M_("java/util/Locale", "getScript", "()Ljava/lang/String;", Locale_script), M_("java/util/Locale", "getVariant", "()Ljava/lang/String;", Locale_variant),
    M_("com/google/androidgamesdk/GameActivity", "finish", "()V", MA_quit),
    M_("com/google/androidgamesdk/GameActivity", "setWindowFlags", "(II)V", Noop),
    M_("com/google/androidgamesdk/GameActivity", "setImeEditorInfoFields", "(III)V", GTI_imeFields),
    { NULL, NULL, NULL, NULL }
};

void tl_mc_hle_install(const char *pkg, const char *apk, const char *data, int w, int h)
{
    snprintf(M.pkg, sizeof(M.pkg), "%s", pkg);
    snprintf(M.apk, sizeof(M.apk), "%s", apk);
    snprintf(M.data, sizeof(M.data), "%s", data);
    snprintf(M.files, sizeof(M.files), "%s/files", data);
    snprintf(M.ext, sizeof(M.ext), "%s/sdcard/Android/data/%s/files", data, pkg);
    M.width = w; M.height = h;
    clock_gettime(CLOCK_MONOTONIC, &M.started);
    char dir[640]; snprintf(dir, sizeof(dir), "%s/shared_prefs", data);
    mkdir(dir, 0755);
    snprintf(M.prefs, sizeof(M.prefs), "%s/minecraft.txt", dir);
    kv_load();
    for (size_t i = 0; i < sizeof(k_classes) / sizeof(k_classes[0]); i++) tl_jni_declare(k_classes[i].name, k_classes[i].super);
    tl_jni_register_hle(k_hle);
}

void tl_mc_set_activity(jobj *activity)
{
    M.activity = activity;
    jvalue v; v.j = 0; v.l = activity;
    tl_jni_set_static("com/mojang/minecraftpe/MainActivity", "mInstance", "Lcom/mojang/minecraftpe/MainActivity;", v);
    tl_jni_set_static("com/mojang/minecraftpe/MainActivity", "mHasStoragePermission", "Z", vz(1));
}
void tl_jni_minecraft_reset_session(void) { M.activity = NULL; memset(g_mc_singletons, 0, sizeof(g_mc_singletons)); }
