/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * The Java world: the parts of Android's framework (and of the APK's own Java) that native
 * code reaches through JNI, implemented in C. What is here is what Unity actually asked
 * for; everything else logs once as UNIMPLEMENTED and returns zero.
 */
#define _DARWIN_C_SOURCE
#include "husk-tl-jni.h"

#include <dirent.h>
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "husk-tl-bionic.h"
#include "husk-tl-internal.h"
#include "husk-tl-ld.h"
#include "husk-tl-dexindex.h"

/* ------------------------------------------------------------------ state */

void tl_http_install(void);

static struct {
    char pkg[128], apk[1024], data[512], files[600], cache[600], ext_files[700], ext_cache[700], native_lib[64];
    int width, height;
    float density;
    char version_name[64];
    int version_code;
    jobj *activity, *resources, *assets, *appinfo, *pm, *display, *metrics, *config, *window, *wm, *looper, *handler;
} H;

static jvalue vl(void *p) { jvalue v; v.j = 0; v.l = p; return v; }
static jvalue vi(int i) { jvalue v; v.j = 0; v.i = i; return v; }
static jvalue vz(int z) { jvalue v; v.j = 0; v.z = z != 0; return v; }
static jvalue vf(float f) { jvalue v; v.j = 0; v.f = f; return v; }

static void mkdirs(const char *path)
{
    char tmp[1024]; snprintf(tmp, sizeof(tmp), "%s", path);
    for (char *p = tmp + 1; *p; p++) if (*p == '/') { *p = 0; mkdir(tmp, 0755); *p = '/'; }
    mkdir(tmp, 0755);
}

#define C(name) tl_jni_class(name)
#define STR(s) tl_jni_new_string(s)
static const char *S(const jobj *o) { const char *s = tl_jni_string(o); return s ? s : ""; }
static void set_str(jobj *o, const char *f, const char *v) { tl_jni_set_field(o, f, "Ljava/lang/String;", vl(STR(v))); }
static void set_int(jobj *o, const char *f, int v) { tl_jni_set_field(o, f, "I", vi(v)); }
static void set_float(jobj *o, const char *f, float v) { tl_jni_set_field(o, f, "F", vf(v)); }
static jobj *make(const char *cls) { return tl_jni_new_object(C(cls)); }

/* -------------------------------------------------------------- java.io.File */

static jobj *new_file(const char *path)
{
    jobj *f = make("java/io/File");
    set_str(f, "path", path);
    return f;
}
static const char *file_path(const jobj *f) { return S(tl_jni_get_field((jobj *)f, "path", "Ljava/lang/String;").l); }

static void File_init_s(tl_jcall *c) { set_str(c->self, "path", S(c->args[0].l)); }
static void File_init_fs(tl_jcall *c)
{
    char p[1100]; snprintf(p, sizeof(p), "%s/%s", file_path(c->args[0].l), S(c->args[1].l));
    set_str(c->self, "path", p);
}
static void File_getPath(tl_jcall *c) { c->ret = vl(STR(file_path(c->self))); }
static void File_getName(tl_jcall *c)
{
    const char *p = file_path(c->self), *s = strrchr(p, '/');
    c->ret = vl(STR(s ? s + 1 : p));
}
static void File_getParent(tl_jcall *c)
{
    char p[1100]; snprintf(p, sizeof(p), "%s", file_path(c->self));
    char *s = strrchr(p, '/');
    if (!s) { c->ret = vl(NULL); return; }
    if (s == p) s[1] = 0; else *s = 0;
    c->ret = vl(STR(p));
}
static void File_getParentFile(tl_jcall *c)
{
    char p[1100]; snprintf(p, sizeof(p), "%s", file_path(c->self));
    char *s = strrchr(p, '/');
    if (!s) { c->ret = vl(NULL); return; }
    if (s == p) s[1] = 0; else *s = 0;
    c->ret = vl(new_file(p));
}
static void File_exists(tl_jcall *c) { struct stat st; c->ret = vz(stat(file_path(c->self), &st) == 0); }
static void File_isDirectory(tl_jcall *c) { struct stat st; c->ret = vz(stat(file_path(c->self), &st) == 0 && S_ISDIR(st.st_mode)); }
static void File_isFile(tl_jcall *c) { struct stat st; c->ret = vz(stat(file_path(c->self), &st) == 0 && S_ISREG(st.st_mode)); }
static void File_canRead(tl_jcall *c) { c->ret = vz(access(file_path(c->self), R_OK) == 0); }
static void File_canWrite(tl_jcall *c) { c->ret = vz(access(file_path(c->self), W_OK) == 0); }
static void File_mkdirs(tl_jcall *c) { mkdirs(file_path(c->self)); struct stat st; c->ret = vz(stat(file_path(c->self), &st) == 0); }
static void File_mkdir(tl_jcall *c) { c->ret = vz(mkdir(file_path(c->self), 0755) == 0); }
static void File_delete(tl_jcall *c) { c->ret = vz(unlink(file_path(c->self)) == 0 || rmdir(file_path(c->self)) == 0); }
static void File_length(tl_jcall *c) { struct stat st; jvalue v; v.j = stat(file_path(c->self), &st) == 0 ? st.st_size : 0; c->ret = v; }
static void File_toString(tl_jcall *c) { c->ret = vl(STR(file_path(c->self))); }

/* --------------------------------------------------------------- java.lang */

static void Object_toString(tl_jcall *c)
{
    char b[200]; snprintf(b, sizeof(b), "%s@%p", c->self ? tl_jni_class_name(c->self) : "null", (void *)c->self);
    c->ret = vl(STR(b));
}
static void Object_hashCode(tl_jcall *c) { c->ret = vi((int)((uintptr_t)c->self >> 4)); }
static void Object_equals(tl_jcall *c) { c->ret = vz(c->self == c->args[0].l); }
static void String_equals(tl_jcall *c) { const char *a = tl_jni_string(c->self), *b = tl_jni_string(c->args[0].l); c->ret = vz(a && b && !strcmp(a, b)); }
static void String_toString(tl_jcall *c) { c->ret = vl(tl_jni_ref(c->self)); }
static void String_length(tl_jcall *c) { c->ret = vi((int)strlen(S(c->self))); }
static void String_isEmpty(tl_jcall *c) { c->ret = vz(S(c->self)[0] == 0); }
static void String_hashCode(tl_jcall *c) { int h = 0; for (const unsigned char *p = (const unsigned char *)S(c->self); *p; p++) h = h * 31 + *p; c->ret = vi(h); }

/* --------------------------------------------------------------- Context */

static void Context_getPackageName(tl_jcall *c) { c->ret = vl(STR(H.pkg)); }
static void Context_getPackageCodePath(tl_jcall *c) { c->ret = vl(STR(H.apk)); }
static void Context_getPackageResourcePath(tl_jcall *c) { c->ret = vl(STR(H.apk)); }
static void Context_getFilesDir(tl_jcall *c) { c->ret = vl(new_file(H.files)); }
static void Context_getCacheDir(tl_jcall *c) { c->ret = vl(new_file(H.cache)); }
static void Context_getCodeCacheDir(tl_jcall *c) { char p[700]; snprintf(p, sizeof(p), "%s/code_cache", H.data); mkdirs(p); c->ret = vl(new_file(p)); }
static void Context_getNoBackupFilesDir(tl_jcall *c) { char p[700]; snprintf(p, sizeof(p), "%s/no_backup", H.data); mkdirs(p); c->ret = vl(new_file(p)); }
static void Context_getExternalFilesDir(tl_jcall *c) { c->ret = vl(new_file(H.ext_files)); }
static void Context_getExternalCacheDir(tl_jcall *c) { c->ret = vl(new_file(H.ext_cache)); }
static void Context_getDir(tl_jcall *c) { char p[700]; snprintf(p, sizeof(p), "%s/app_%s", H.data, S(c->args[0].l)); mkdirs(p); c->ret = vl(new_file(p)); }
static void Context_getResources(tl_jcall *c) { c->ret = vl(H.resources); }
static void Context_getAssets(tl_jcall *c) { c->ret = vl(H.assets); }
static void Context_getApplicationInfo(tl_jcall *c) { c->ret = vl(H.appinfo); }
static void Context_getPackageManager(tl_jcall *c) { c->ret = vl(H.pm); }
static void Context_getApplicationContext(tl_jcall *c) { c->ret = vl(H.activity); }
extern jobj *tl_loop_main_looper(void);
static void Context_getMainLooper(tl_jcall *c) { c->ret = vl(tl_loop_main_looper()); }
static void Context_getContentResolver(tl_jcall *c) { c->ret = vl(make("android/content/ContentResolver")); }
static void Context_getClassLoader(tl_jcall *c) { c->ret = vl(make("dalvik/system/PathClassLoader")); }
static void Context_getSharedPreferences(tl_jcall *c)
{
    jobj *p = make("android/content/SharedPreferences");
    set_str(p, "name", S(c->args[0].l));
    c->ret = vl(p);
}
static void Context_checkCallingOrSelfPermission(tl_jcall *c) { c->ret = vi(0); }   /* PERMISSION_GRANTED */
static void Context_checkSelfPermission(tl_jcall *c) { c->ret = vi(0); }

/* getSystemService(name): one object per kind of service. */
static void Context_getSystemService(tl_jcall *c)
{
    const char *n = S(c->args[0].l);
    static const struct { const char *name, *cls; } svc[] = {
        { "window", "android/view/WindowManager" }, { "audio", "android/media/AudioManager" },
        { "connectivity", "android/net/ConnectivityManager" }, { "sensor", "android/hardware/SensorManager" },
        { "power", "android/os/PowerManager" }, { "vibrator", "android/os/Vibrator" },
        { "display", "android/hardware/display/DisplayManager" }, { "location", "android/location/LocationManager" },
        { "input", "android/hardware/input/InputManager" }, { "input_method", "android/view/inputmethod/InputMethodManager" },
        { "phone", "android/telephony/TelephonyManager" }, { "clipboard", "android/content/ClipboardManager" },
        { "wifi", "android/net/wifi/WifiManager" }, { "media_router", "android/media/MediaRouter" },
        { "activity", "android/app/ActivityManager" }, { "layout_inflater", "android/view/LayoutInflater" },
        { "accessibility", "android/view/accessibility/AccessibilityManager" }, { "uimode", "android/app/UiModeManager" },
    };
    for (size_t i = 0; i < sizeof(svc) / sizeof(svc[0]); i++) {
        if (!strcmp(n, svc[i].name)) {
            static jobj *cache[32];
            if (!cache[i]) cache[i] = make(svc[i].cls);
            if (!strcmp(n, "window")) { c->ret = vl(H.wm); return; }
            c->ret = vl(cache[i]);
            return;
        }
    }
    char note[160]; snprintf(note, sizeof(note), "getSystemService(\"%s\") is not provided (returning null)", n);
    tl_note_once(note);
    c->ret = vl(NULL);
}

/* --------------------------------------------------------------- Activity */

static void Activity_getWindow(tl_jcall *c) { c->ret = vl(H.window); }
static void Activity_getWindowManager(tl_jcall *c) { c->ret = vl(H.wm); }
static void Activity_getRequestedOrientation(tl_jcall *c) { c->ret = vi(-1); }
static void Activity_getIntent(tl_jcall *c) { c->ret = vl(make("android/content/Intent")); }
static void Activity_isFinishing(tl_jcall *c) { c->ret = vz(0); }
static void Activity_getComponentName(tl_jcall *c) { c->ret = vl(make("android/content/ComponentName")); }

/* ---------------------------------------------------- Resources, metrics */

static void Resources_getAssets(tl_jcall *c) { c->ret = vl(H.assets); }
static void Resources_getConfiguration(tl_jcall *c) { c->ret = vl(H.config); }
static void Resources_getDisplayMetrics(tl_jcall *c) { c->ret = vl(H.metrics); }
static void Resources_getIdentifier(tl_jcall *c) { c->ret = vi(0); }

static void fill_metrics(jobj *m)
{
    set_int(m, "widthPixels", H.width); set_int(m, "heightPixels", H.height);
    set_float(m, "density", H.density); set_float(m, "scaledDensity", H.density);
    set_int(m, "densityDpi", (int)(H.density * 160));
    set_float(m, "xdpi", H.density * 160); set_float(m, "ydpi", H.density * 160);
}
static void Display_getMetrics(tl_jcall *c) { fill_metrics(c->args[0].l); }
static void Display_getRotation(tl_jcall *c) { c->ret = vi(0); }
static void Display_getRefreshRate(tl_jcall *c) { c->ret = vf(60.f); }
static void Display_getDisplayId(tl_jcall *c) { c->ret = vi(0); }
static void Display_getWidth(tl_jcall *c) { c->ret = vi(H.width); }
static void Display_getHeight(tl_jcall *c) { c->ret = vi(H.height); }
static void Display_getName(tl_jcall *c) { c->ret = vl(STR("Built-in Screen")); }
static void WindowManager_getDefaultDisplay(tl_jcall *c) { c->ret = vl(H.display); }

/* ----------------------------------------------------- package information */

static void PM_getApplicationInfo(tl_jcall *c) { c->ret = vl(H.appinfo); }
static void PM_hasSystemFeature(tl_jcall *c)
{
    const char *f = S(c->args[0].l);
    c->ret = vz(!strcmp(f, "android.hardware.touchscreen") || !strcmp(f, "android.hardware.touchscreen.multitouch")
             || !strcmp(f, "android.hardware.opengles.aep") || !strncmp(f, "android.hardware.vulkan", 23) ? 0 : 0);
}
static void PM_getPackageInfo(tl_jcall *c)
{
    jobj *p = make("android/content/pm/PackageInfo");
    set_str(p, "packageName", H.pkg);
    set_str(p, "versionName", H.version_name[0] ? H.version_name : "1.0");
    set_int(p, "versionCode", H.version_code ? H.version_code : 1);
    c->ret = vl(p);
}

/*
 * The app's own version, read from its manifest (Android binary XML): what PackageInfo reports, and what a game compares against the minimum its servers will
 * serve. A made-up version makes a game believe it is out of date.
 */
static uint32_t ax32(const uint8_t *p) { return p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t)p[3] << 24); }
static uint16_t ax16(const uint8_t *p) { return (uint16_t)(p[0] | (p[1] << 8)); }
static bool ax_string(const uint8_t *pool, size_t pool_size, uint32_t index, char *out, size_t n)
{
    uint32_t count = ax32(pool + 8), flags = ax32(pool + 16), strings = ax32(pool + 20);
    if (index >= count || 28 + 4ull * index + 4 > pool_size) return false;
    size_t off = strings + ax32(pool + 28 + 4 * index);
    if (off + 4 > pool_size) return false;
    const uint8_t *q = pool + off;
    if (flags & 0x100) {
        size_t l = *q++; if (l & 0x80) q++;
        size_t b = *q++; if (b & 0x80) b = ((b & 0x7F) << 8) | *q++;
        if (b >= n) b = n - 1;
        memcpy(out, q, b); out[b] = 0;
    } else {
        size_t l = ax16(q); q += 2;
        if (l & 0x8000) { l = ((l & 0x7FFF) << 16) | ax16(q); q += 2; }
        size_t k = 0;
        for (size_t i = 0; i < l && k + 1 < n; i++) out[k++] = (char)ax16(q + 2 * i);
        out[k] = 0;
    }
    return true;
}
static void read_manifest_version(const char *apk)
{
    tl_zip z; char err[160];
    if (!tl_zip_open(&z, apk, err, sizeof(err))) return;
    const tl_zip_entry *e = tl_zip_find(&z, "AndroidManifest.xml");
    const uint8_t *data; size_t len; bool owned = false;
    if (e && tl_zip_data(&z, e, 8u << 20, &data, &len, &owned, err, sizeof(err)) && len > 8 && ax16(data) == 0x0003) {
        const uint8_t *pool = NULL; size_t pool_size = 0;
        for (size_t off = ax16(data + 2); off + 8 <= len; ) {
            uint16_t type = ax16(data + off); uint32_t size = ax32(data + off + 4);
            if (size < 8 || off + size > len) break;
            if (type == 0x0001) { pool = data + off; pool_size = size; }
            else if (type == 0x0102 && pool) {                      /* the first element is <manifest> */
                const uint8_t *el = data + off;
                uint16_t astart = ax16(el + 24), asize = ax16(el + 26), acount = ax16(el + 28);
                for (unsigned i = 0; i < acount; i++) {
                    const uint8_t *at = el + 16 + astart + (size_t)i * asize;
                    char name[40];
                    if (!ax_string(pool, pool_size, ax32(at + 4), name, sizeof(name))) continue;
                    uint8_t vtype = at[15]; uint32_t vdata = ax32(at + 16);
                    if (!strcmp(name, "versionName")) {
                        if (ax32(at + 8) != 0xFFFFFFFFu) ax_string(pool, pool_size, ax32(at + 8), H.version_name, sizeof(H.version_name));
                        else if (vtype == 0x03) ax_string(pool, pool_size, vdata, H.version_name, sizeof(H.version_name));
                    } else if (!strcmp(name, "versionCode") && (vtype == 0x10 || vtype == 0x11)) H.version_code = (int)vdata;
                }
                break;
            }
            off += size;
        }
    }
    if (owned) free((void *)data);
    tl_zip_close(&z);
}

/*
 * The <meta-data> elements of the manifest, by name: an Unreal Engine game reads its project's settings (engine version, project name, whether Vulkan is
 * supported) back through the activity from them, and they differ from game to game.
 */
#define MAX_META 64
static struct { char key[110]; char val[130]; } g_meta[MAX_META];
static int g_nmeta;
static void read_manifest_meta(const char *apk)
{
    g_nmeta = 0;
    tl_zip z; char err[160];
    if (!tl_zip_open(&z, apk, err, sizeof(err))) return;
    const tl_zip_entry *e = tl_zip_find(&z, "AndroidManifest.xml");
    const uint8_t *data; size_t len; bool owned = false;
    if (e && tl_zip_data(&z, e, 8u << 20, &data, &len, &owned, err, sizeof(err)) && len > 8 && ax16(data) == 0x0003) {
        const uint8_t *pool = NULL; size_t pool_size = 0;
        for (size_t off = ax16(data + 2); off + 8 <= len; ) {
            uint16_t type = ax16(data + off); uint32_t size = ax32(data + off + 4);
            if (size < 8 || off + size > len) break;
            if (type == 0x0001) { pool = data + off; pool_size = size; }
            else if (type == 0x0102 && pool && off + 36 <= len) {
                const uint8_t *el = data + off; char tag[24];
                if (ax_string(pool, pool_size, ax32(el + 20), tag, sizeof(tag)) && !strcmp(tag, "meta-data") && g_nmeta < MAX_META) {
                    uint16_t astart = ax16(el + 24), asize = ax16(el + 26), acount = ax16(el + 28);
                    char key[110] = "", val[130] = ""; bool has_val = false;
                    for (unsigned i = 0; i < acount; i++) {
                        const uint8_t *at = el + 16 + astart + (size_t)i * asize; char an[16];
                        if (at + 20 > data + len || !ax_string(pool, pool_size, ax32(at + 4), an, sizeof(an))) continue;
                        uint8_t vtype = at[15]; uint32_t vdata = ax32(at + 16);
                        if (!strcmp(an, "name")) { if (ax32(at + 8) != 0xFFFFFFFFu) ax_string(pool, pool_size, ax32(at + 8), key, sizeof(key)); else if (vtype == 0x03) ax_string(pool, pool_size, vdata, key, sizeof(key)); }
                        else if (!strcmp(an, "value")) {
                            has_val = true;
                            if (vtype == 0x03) { if (!ax_string(pool, pool_size, vdata, val, sizeof(val))) val[0] = 0; }
                            else if (vtype == 0x12) snprintf(val, sizeof(val), "%s", vdata ? "true" : "false");
                            else if (vtype == 0x10 || vtype == 0x11) snprintf(val, sizeof(val), "%d", (int)vdata);
                            else if (vtype == 0x04) { float f; memcpy(&f, &vdata, 4); snprintf(val, sizeof(val), "%g", f); }
                            else has_val = false;               /* a resource reference: nothing a game asks for by name */
                        }
                    }
                    if (key[0] && has_val) { snprintf(g_meta[g_nmeta].key, sizeof(g_meta[0].key), "%s", key); snprintf(g_meta[g_nmeta].val, sizeof(g_meta[0].val), "%s", val); g_nmeta++; }
                }
            }
            off += size;
        }
    }
    if (owned) free((void *)data);
    tl_zip_close(&z);
}
const char *tl_hle_manifest_meta(const char *key)
{
    for (int i = 0; i < g_nmeta; i++) if (!strcmp(g_meta[i].key, key)) return g_meta[i].val;
    return NULL;
}

/*
 * Settings.Secure.ANDROID_ID: the id a game uses to tell this device from the others, and which servers tie an account to. It must be this install's own and
 * stable: a constant shared by every install is one account for everyone (a game's server happily hands back whoever had it first), and one that changed on
 * every launch would be a new player each time. So it is made once, at random, and kept in the app's data directory.
 */
const char *tl_hle_android_id(void)
{
    static char id[40];
    if (id[0]) return id;
    char path[700]; snprintf(path, sizeof(path), "%s/android_id", H.data);
    FILE *f = fopen(path, "r");
    if (f) { if (!fgets(id, sizeof(id), f)) id[0] = 0; fclose(f); }
    size_t n = strlen(id);
    while (n && (id[n - 1] == '\n' || id[n - 1] == '\r')) id[--n] = 0;
    if (n != 16) {
        uint8_t r[8]; arc4random_buf(r, sizeof(r));
        for (int i = 0; i < 8; i++) snprintf(id + 2 * i, 3, "%02x", r[i]);
        f = fopen(path, "w");
        if (f) { fputs(id, f); fclose(f); }
    }
    return id;
}
static void Secure_getString(tl_jcall *c) { c->ret = vl(STR(tl_hle_android_id())); }

/* ------------------------------------------------------------------ Build */

static void install_build(void)
{
    jvalue v;
    #define BS(n, s) do { v.l = STR(s); tl_jni_set_static("android/os/Build", n, "Ljava/lang/String;", v); } while (0)
    BS("MANUFACTURER", "Google"); BS("BRAND", "google"); BS("MODEL", "Pixel 8"); BS("DEVICE", "shiba"); BS("PRODUCT", "shiba");
    BS("HARDWARE", "shiba"); BS("BOARD", "shiba"); BS("ID", "UP1A.231105.001"); BS("DISPLAY", "UP1A.231105.001");
    BS("TYPE", "user"); BS("TAGS", "release-keys"); BS("HOST", "abfarm"); BS("USER", "android-build");
    BS("FINGERPRINT", "google/shiba/shiba:14/UP1A.231105.001/11000000:user/release-keys"); BS("BOOTLOADER", "unknown");
    BS("CPU_ABI", "arm64-v8a"); BS("CPU_ABI2", ""); BS("SOC_MODEL", "Tensor G3"); BS("SOC_MANUFACTURER", "Google");
    #undef BS
    v.l = STR("14"); tl_jni_set_static("android/os/Build$VERSION", "RELEASE", "Ljava/lang/String;", v);
    v.l = STR("REL"); tl_jni_set_static("android/os/Build$VERSION", "CODENAME", "Ljava/lang/String;", v);
    v.l = STR("11000000"); tl_jni_set_static("android/os/Build$VERSION", "INCREMENTAL", "Ljava/lang/String;", v);
    v.l = STR("2024-05-05"); tl_jni_set_static("android/os/Build$VERSION", "SECURITY_PATCH", "Ljava/lang/String;", v);
    v.j = 0; v.i = 34; tl_jni_set_static("android/os/Build$VERSION", "SDK_INT", "I", v);
    v.j = 0; v.i = 34; tl_jni_set_static("android/os/Build$VERSION", "PREVIEW_SDK_INT", "I", v);
    v.j = 0; v.l = STR("mounted"); tl_jni_set_static("android/os/Environment", "MEDIA_MOUNTED", "Ljava/lang/String;", v);
    v.l = STR("location"); tl_jni_set_static("android/content/Context", "LOCATION_SERVICE", "Ljava/lang/String;", v);
    v.l = STR("window"); tl_jni_set_static("android/content/Context", "WINDOW_SERVICE", "Ljava/lang/String;", v);
    v.l = STR("audio"); tl_jni_set_static("android/content/Context", "AUDIO_SERVICE", "Ljava/lang/String;", v);
    v.l = STR("input_method"); tl_jni_set_static("android/content/Context", "INPUT_METHOD_SERVICE", "Ljava/lang/String;", v);
    v.l = STR("sensor"); tl_jni_set_static("android/content/Context", "SENSOR_SERVICE", "Ljava/lang/String;", v);
    v.l = STR("vibrator"); tl_jni_set_static("android/content/Context", "VIBRATOR_SERVICE", "Ljava/lang/String;", v);
    v.l = STR("connectivity"); tl_jni_set_static("android/content/Context", "CONNECTIVITY_SERVICE", "Ljava/lang/String;", v);
    v.l = STR("power"); tl_jni_set_static("android/content/Context", "POWER_SERVICE", "Ljava/lang/String;", v);
    v.l = STR("display"); tl_jni_set_static("android/content/Context", "DISPLAY_SERVICE", "Ljava/lang/String;", v);
    v.l = STR("input"); tl_jni_set_static("android/content/Context", "INPUT_SERVICE", "Ljava/lang/String;", v);
    v.l = STR("phone"); tl_jni_set_static("android/content/Context", "TELEPHONY_SERVICE", "Ljava/lang/String;", v);
    v.l = STR("clipboard"); tl_jni_set_static("android/content/Context", "CLIPBOARD_SERVICE", "Ljava/lang/String;", v);
}

/* --------------------------------------------------------- misc statics */

static void Env_getExternalStorageState(tl_jcall *c) { c->ret = vl(STR("mounted")); }
static void Env_getExternalStorageDirectory(tl_jcall *c) { c->ret = vl(new_file(H.ext_files)); }
static void Process_setThreadPriority(tl_jcall *c) { (void)c; }
static void Process_myPid(tl_jcall *c) { c->ret = vi(getpid()); }
static void Process_myTid(tl_jcall *c) { uint64_t t = 0; pthread_threadid_np(NULL, &t); c->ret = vi((int)t); }
/* System.load / loadLibrary: what ART does for a library -- map it, and call its JNI_OnLoad once with the JavaVM. */
static tl_lib *g_onload_done[96];
static int g_nonload;
static pthread_mutex_t g_onload_mu = PTHREAD_MUTEX_INITIALIZER;
static bool load_native_library(const char *base);
bool tl_jni_load_library(const char *base) { return load_native_library(base); }
static bool load_native_library(const char *base)
{
    tl_lib *L = tl_ld_load(base);
    if (!L) return false;
    tl_ld_init(L);
    pthread_mutex_lock(&g_onload_mu);
    bool first = true;
    for (int i = 0; i < g_nonload; i++) if (g_onload_done[i] == L) first = false;
    if (first && g_nonload < 96) g_onload_done[g_nonload++] = L;
    pthread_mutex_unlock(&g_onload_mu);
    if (first) {
        int32_t (*onload)(void *vm, void *reserved) = (int32_t (*)(void *, void *))tl_ld_sym(L, "JNI_OnLoad");
        if (onload) {
            int32_t ver = onload(tl_jni_vm(), NULL);
            tl_log_line("jni: %s JNI_OnLoad -> %#x", base, ver);
        }
    }
    return true;
}
static void System_load(tl_jcall *c)
{
    const char *path = S(c->args[0].l);
    const char *base = strrchr(path, '/');
    base = base ? base + 1 : path;
    struct stat st;
    if (!load_native_library(base != path && stat(path, &st) == 0 && st.st_size > 0 ? path : base)) {
        char msg[300]; snprintf(msg, sizeof(msg), "dlopen failed: library \"%s\" not found", path);
        tl_jni_throw("java/lang/UnsatisfiedLinkError", msg);
    }
}
static void System_loadLibrary(tl_jcall *c)
{
    char base[200]; snprintf(base, sizeof(base), "lib%s.so", S(c->args[0].l));
    if (!load_native_library(base)) {
        char msg[300]; snprintf(msg, sizeof(msg), "dlopen failed: library \"%s\" not found", base);
        tl_jni_throw("java/lang/UnsatisfiedLinkError", msg);
    }
}
static void System_getProperty(tl_jcall *c) { const char *k = S(c->args[0].l); const char *v = !strcmp(k, "os.arch") ? "aarch64" : !strcmp(k, "http.agent") ? "Dalvik/2.1.0 (Linux; U; Android 14; Pixel 8)" : NULL; c->ret = vl(v ? STR(v) : NULL); }
static void System_currentTimeMillis(tl_jcall *c) { struct timespec ts; clock_gettime(CLOCK_REALTIME, &ts); jvalue v; v.j = (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000; c->ret = v; }
static void System_nanoTime(tl_jcall *c) { struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts); jvalue v; v.j = (int64_t)ts.tv_sec * 1000000000 + ts.tv_nsec; c->ret = v; }
static void UnityPlayer_initializeGoogleAr(tl_jcall *c) { c->ret = vz(0); }

/* JNIBridge builds Java proxies for interfaces C# implements. A proxy that never answers is
 * enough until something calls through one. */
static void JNIBridge_newInterfaceProxy(tl_jcall *c)
{
    jobj *p = make("java/lang/reflect/Proxy");
    jvalue h; h.j = c->args[0].j;
    tl_jni_set_field(p, "handle", "J", h);
    c->ret = vl(p);
}
/* ReflectionHelper finds a member by name and signature with Java reflection; native code turns what it
 * returns back into an ID. A class with no such member answers null, as the real helper does. */
/* Unity builds signatures from Class.getName(), which uses dots where JNI uses slashes; the real helper matches either. */
static const char *slashed(const char *sig, char *buf, size_t n)
{
    snprintf(buf, n, "%s", sig);
    for (char *p = buf; *p; p++) if (*p == '.') *p = '/';
    return buf;
}
static void Reflection_getConstructorID(tl_jcall *c)
{
    char b[400];
    c->ret = vl(tl_jni_reflect_method(c->args[0].l, "<init>", slashed(S(c->args[1].l), b, sizeof(b)), false));
}
static void Reflection_getMethodID(tl_jcall *c)
{
    char b[400];
    jobj *m = tl_jni_reflect_method(c->args[0].l, S(c->args[1].l), slashed(S(c->args[2].l), b, sizeof(b)), c->args[3].z != 0);
    if (!m) tl_log_line("jni: reflection found no %smethod %s.%s%s", c->args[3].z ? "static " : "", tl_jni_class_name(c->args[0].l), S(c->args[1].l), b);
    c->ret = vl(m);
}
static void Reflection_getFieldID(tl_jcall *c)
{
    char b[400];
    c->ret = vl(tl_jni_reflect_field(c->args[0].l, S(c->args[1].l), slashed(S(c->args[2].l), b, sizeof(b)), c->args[3].z != 0));
}
static void Reflection_getFieldSignature(tl_jcall *c)
{
    const char *sig = tl_jni_reflected_field_sig(c->args[0].l);
    c->ret = vl(STR(sig ? sig : ""));
}
static void Member_getDeclaringClass(tl_jcall *c) { c->ret = vl(tl_jni_reflected_declaring_class(c->self)); }


/* ------------------------------------------------- the app's own small Java classes */

/* The DEX says getUserdataPath is currentActivity.getFilesDir().getAbsolutePath(). */
static void Kiloo_getUserdataPath(tl_jcall *c) { c->ret = vl(STR(H.files)); }
static void Locale_getDefault(tl_jcall *c) { c->ret = vl(make("java/util/Locale")); }
static void Locale_toLanguageTag(tl_jcall *c) { c->ret = vl(STR("en-US")); }
static void Locale_getLanguage(tl_jcall *c) { c->ret = vl(STR("en")); }
static void Locale_getCountry(tl_jcall *c) { c->ret = vl(STR("US")); }
static void PreciseLocale_getRegion(tl_jcall *c) { c->ret = vl(STR("US")); }
static void Chipset_name(tl_jcall *c) { c->ret = vl(STR("Apple")); }
static void Log_getStackTraceString(tl_jcall *c)
{
    const char *m = c->args[0].l ? S(tl_jni_get_field(c->args[0].l, "detailMessage", "Ljava/lang/String;").l) : "";
    char buf[400]; snprintf(buf, sizeof(buf), "%s: %s", c->args[0].l ? tl_jni_class_name(c->args[0].l) : "null", m);
    c->ret = vl(STR(buf));
}
static void DiskUtils_availableSpace(tl_jcall *c) { c->ret = vi(20000); }     /* megabytes free */
/* Unity's notifications package asks its Java manager for an instance and calls methods on it; a manager whose methods do nothing is enough. */
static void NotificationManager_get(tl_jcall *c) { c->ret = vl(make("com/unity/androidnotifications/UnityNotificationManager")); }
/* The newer proxy path: ReflectionHelper builds a java.lang.reflect.Proxy whose calls go to nativeProxyInvoke(handle, name, args). */
static void Reflection_newProxyInstance(tl_jcall *c)
{
    jobj *p = make("java/lang/reflect/Proxy");
    jvalue h; h.j = c->args[1].j;
    tl_jni_set_field(p, "handle", "J", h);
    tl_jni_set_field(p, "style", "I", vi(2));
    c->ret = vl(p);
}
static void Activity_getApplication(tl_jcall *c) { static jobj *app; if (!app) app = make("android/app/Application"); c->ret = vl(app); }
static void Zero_int(tl_jcall *c) { c->ret = vi(0); }
static void Unity_getNetworkConnectivity(tl_jcall *c) { c->ret = vi(2); }     /* ReachableViaLocalAreaNetwork: the phone has its network */

/* ------------------------------------------------- dialogs: say what they say */

static void Builder_init(tl_jcall *c) { (void)c; }
static void Builder_setTitle(tl_jcall *c) { set_str(c->self, "title", S(c->args[0].l)); c->ret = vl(tl_jni_ref(c->self)); }
static void Builder_setMessage(tl_jcall *c) { set_str(c->self, "message", S(c->args[0].l)); c->ret = vl(tl_jni_ref(c->self)); }
static void Builder_chain(tl_jcall *c) { c->ret = vl(tl_jni_ref(c->self)); }
static void Builder_create(tl_jcall *c)
{
    jobj *d = make("android/app/AlertDialog");
    set_str(d, "title", S(tl_jni_get_field(c->self, "title", "Ljava/lang/String;").l));
    set_str(d, "message", S(tl_jni_get_field(c->self, "message", "Ljava/lang/String;").l));
    c->ret = vl(d);
}
static void Dialog_show(tl_jcall *c)
{
    tl_log_line("DIALOG \"%s\": %s", S(tl_jni_get_field(c->self, "title", "Ljava/lang/String;").l),
                S(tl_jni_get_field(c->self, "message", "Ljava/lang/String;").l));
}
static void Builder_show(tl_jcall *c)
{
    jobj *d = make("android/app/AlertDialog");
    set_str(d, "title", S(tl_jni_get_field(c->self, "title", "Ljava/lang/String;").l));
    set_str(d, "message", S(tl_jni_get_field(c->self, "message", "Ljava/lang/String;").l));
    Dialog_show(&(tl_jcall){ .self = d });
    c->ret = vl(d);
}
static void Object_getClass(tl_jcall *c) { c->ret = vl(c->self && c->self->cls ? tl_jni_class_object(tl_jni_class_name(c->self)) : NULL); }
static void Class_getClassLoader(tl_jcall *c) { c->ret = vl(make("dalvik/system/PathClassLoader")); }
static void ClassLoader_findLibrary(tl_jcall *c) { char p[300]; snprintf(p, sizeof(p), "%s/lib%s.so", H.native_lib, S(c->args[0].l)); c->ret = vl(STR(p)); }
static void SP_getInt(tl_jcall *c) { c->ret = vi(c->args[1].i); }
static void SP_getString(tl_jcall *c) { c->ret = vl(c->args[1].l ? tl_jni_ref(c->args[1].l) : NULL); }
static void SP_getBoolean(tl_jcall *c) { c->ret = vz(c->args[1].z); }
static void SP_edit(tl_jcall *c) { c->ret = vl(make("android/content/SharedPreferences$Editor")); }
static void Editor_self(tl_jcall *c) { c->ret = vl(tl_jni_ref(c->self)); }
static void Editor_noop(tl_jcall *c) { (void)c; }
/* An iterator is empty unless it was made over a list (tl_jni_new_list_iterator). */
typedef struct { jobj **items; uint32_t n, i; } list_iter;
static void Iterator_hasNext(tl_jcall *c) { const list_iter *it = c->self ? c->self->native : NULL; c->ret = vz(it && it->i < it->n); }
static void Iterator_next(tl_jcall *c)
{
    list_iter *it = c->self ? c->self->native : NULL;
    if (!it || it->i >= it->n) { tl_jni_throw("java/util/NoSuchElementException", ""); c->ret = vl(NULL); return; }
    c->ret = vl(tl_jni_ref(it->items[it->i++]));
}
jobj *tl_jni_new_list_iterator(jobj *const *items, uint32_t n)
{
    jobj *o = make("java/util/Iterator");
    list_iter *it = calloc(1, sizeof(*it));
    it->items = malloc((n ? n : 1) * sizeof(jobj *));
    for (uint32_t i = 0; i < n; i++) it->items[i] = tl_jni_ref(items[i]);
    it->n = n;
    o->native = it;
    return o;
}


/* ------------------------------------------------ strings and builders */

typedef struct { char *p; size_t n, cap; } sbuf;
static sbuf *sb_of(jobj *o) { if (!o->native) o->native = calloc(1, sizeof(sbuf)); return o->native; }
static void sb_add(sbuf *b, const char *s, size_t n)
{
    if (b->n + n + 1 > b->cap) { b->cap = (b->n + n + 1) * 2; b->p = realloc(b->p, b->cap); }
    memcpy(b->p + b->n, s, n); b->n += n; b->p[b->n] = 0;
}

static void String_init_empty(tl_jcall *c) { c->ret = vl(STR("")); }
static void String_init_bytes(tl_jcall *c)
{
    jobj *a = c->args[0].l;
    char *tmp = malloc((a ? a->arr.len : 0) + 1);
    if (a) memcpy(tmp, a->arr.data, a->arr.len);
    tmp[a ? a->arr.len : 0] = 0;
    c->ret = vl(STR(tmp)); free(tmp);
}
static void String_init_bytes_range(tl_jcall *c)
{
    jobj *a = c->args[0].l; int off = c->args[1].i, len = c->args[2].i;
    if (!a || off < 0 || len < 0 || (uint32_t)(off + len) > a->arr.len) { c->ret = vl(STR("")); return; }
    char *tmp = malloc((size_t)len + 1); memcpy(tmp, (char *)a->arr.data + off, (size_t)len); tmp[len] = 0;
    c->ret = vl(STR(tmp)); free(tmp);
}
static void String_init_string(tl_jcall *c) { c->ret = vl(STR(S(c->args[0].l))); }
static void String_init_chars(tl_jcall *c)
{
    jobj *a = c->args[0].l; size_t n = a ? a->arr.len : 0;
    char *tmp = malloc(n * 3 + 1); size_t k = 0;
    for (size_t i = 0; i < n; i++) {
        uint16_t u = ((uint16_t *)a->arr.data)[i];
        if (u < 0x80) tmp[k++] = (char)u;
        else if (u < 0x800) { tmp[k++] = (char)(0xC0 | (u >> 6)); tmp[k++] = (char)(0x80 | (u & 0x3f)); }
        else { tmp[k++] = (char)(0xE0 | (u >> 12)); tmp[k++] = (char)(0x80 | ((u >> 6) & 0x3f)); tmp[k++] = (char)(0x80 | (u & 0x3f)); }
    }
    tmp[k] = 0; c->ret = vl(STR(tmp)); free(tmp);
}
static void String_getBytes(tl_jcall *c)
{
    const char *s = S(c->self); size_t n = strlen(s);
    jobj *a = tl_jni_new_prim_array('B', (uint32_t)n); memcpy(a->arr.data, s, n);
    c->ret = vl(a);
}

static void SB_init(tl_jcall *c) { sb_of(c->self); }
static void SB_init_string(tl_jcall *c) { sbuf *b = sb_of(c->self); const char *s = S(c->args[0].l); sb_add(b, s, strlen(s)); }
static void SB_append_string(tl_jcall *c) { sbuf *b = sb_of(c->self); const char *s = c->args[0].l ? S(c->args[0].l) : "null"; sb_add(b, s, strlen(s)); c->ret = vl(tl_jni_ref(c->self)); }
static void SB_append_obj(tl_jcall *c)
{
    sbuf *b = sb_of(c->self); jobj *o = c->args[0].l;
    const char *s = !o ? "null" : o->kind == TL_K_STRING ? S(o) : "[object]";
    sb_add(b, s, strlen(s)); c->ret = vl(tl_jni_ref(c->self));
}
static void SB_append_int(tl_jcall *c) { char t[24]; snprintf(t, sizeof(t), "%d", c->args[0].i); sb_add(sb_of(c->self), t, strlen(t)); c->ret = vl(tl_jni_ref(c->self)); }
static void SB_append_long(tl_jcall *c) { char t[32]; snprintf(t, sizeof(t), "%lld", (long long)c->args[0].j); sb_add(sb_of(c->self), t, strlen(t)); c->ret = vl(tl_jni_ref(c->self)); }
static void SB_append_char(tl_jcall *c) { char t[2] = { (char)c->args[0].c, 0 }; sb_add(sb_of(c->self), t, 1); c->ret = vl(tl_jni_ref(c->self)); }
static void SB_append_bool(tl_jcall *c) { const char *t = c->args[0].z ? "true" : "false"; sb_add(sb_of(c->self), t, strlen(t)); c->ret = vl(tl_jni_ref(c->self)); }
static void SB_append_float(tl_jcall *c) { char t[40]; snprintf(t, sizeof(t), "%g", (double)c->args[0].f); sb_add(sb_of(c->self), t, strlen(t)); c->ret = vl(tl_jni_ref(c->self)); }
static void SB_append_double(tl_jcall *c) { char t[40]; snprintf(t, sizeof(t), "%g", c->args[0].d); sb_add(sb_of(c->self), t, strlen(t)); c->ret = vl(tl_jni_ref(c->self)); }
static void SB_toString(tl_jcall *c) { sbuf *b = sb_of(c->self); c->ret = vl(STR(b->p ? b->p : "")); }
static void SB_length(tl_jcall *c) { c->ret = vi((int)sb_of(c->self)->n); }
static void SB_setLength(tl_jcall *c) { sbuf *b = sb_of(c->self); if (c->args[0].i >= 0 && (size_t)c->args[0].i <= b->n) { b->n = (size_t)c->args[0].i; if (b->p) b->p[b->n] = 0; } }

/* ------------------------------------------ assets, streams, Scanner */

typedef struct { const uint8_t *data; size_t len, pos; uint8_t *owned; } stream;

static jobj *open_asset(const char *name)
{
    char path[1100]; snprintf(path, sizeof(path), "assets/%s", name);
    for (int i = 0;; i++) {
        const tl_zip *z = tl_ld_apk_at(i);
        if (!z) return NULL;
        const tl_zip_entry *e = tl_zip_find(z, path);
        if (!e) continue;
        const uint8_t *d; size_t n; bool owned; char err[160];
        if (!tl_zip_data(z, e, (size_t)1 << 30, &d, &n, &owned, err, sizeof(err))) return NULL;
        jobj *s = make("java/io/ByteArrayInputStream");
        stream *st = calloc(1, sizeof(*st));
        st->data = d; st->len = n; if (owned) st->owned = (uint8_t *)d;
        s->native = st;
        return s;
    }
}
static void AM_open(tl_jcall *c)
{
    jobj *s = open_asset(S(c->args[0].l));
    if (!s) { tl_jni_throw("java/io/FileNotFoundException", S(c->args[0].l)); c->ret = vl(NULL); return; }
    c->ret = vl(s);
}
static void IS_read0(tl_jcall *c) { stream *s = c->self->native; c->ret = vi(s && s->pos < s->len ? s->data[s->pos++] : -1); }
static void IS_read_arr(tl_jcall *c)
{
    stream *s = c->self->native; jobj *a = c->args[0].l;
    if (!s || !a || s->pos >= s->len) { c->ret = vi(-1); return; }
    size_t n = a->arr.len < s->len - s->pos ? a->arr.len : s->len - s->pos;
    memcpy(a->arr.data, s->data + s->pos, n); s->pos += n; c->ret = vi((int)n);
}
static void IS_read_range(tl_jcall *c)
{
    stream *s = c->self->native; jobj *a = c->args[0].l; int off = c->args[1].i, len = c->args[2].i;
    if (!s || !a || s->pos >= s->len || off < 0 || len < 0 || (uint32_t)(off + len) > a->arr.len) { c->ret = vi(-1); return; }
    size_t n = (size_t)len < s->len - s->pos ? (size_t)len : s->len - s->pos;
    memcpy((char *)a->arr.data + off, s->data + s->pos, n); s->pos += n; c->ret = vi((int)n);
}
static void IS_available(tl_jcall *c) { stream *s = c->self->native; c->ret = vi(s ? (int)(s->len - s->pos) : 0); }
static void IS_close(tl_jcall *c) { (void)c; }
static void Scanner_init(tl_jcall *c) { tl_jni_set_field(c->self, "stream", "Ljava/io/InputStream;", vl(tl_jni_ref(c->args[0].l))); }
static void Scanner_useDelimiter(tl_jcall *c) { c->ret = vl(tl_jni_ref(c->self)); }
static void Scanner_hasNext(tl_jcall *c)
{
    jobj *in = tl_jni_get_field(c->self, "stream", "Ljava/io/InputStream;").l; stream *s = in ? in->native : NULL;
    c->ret = vz(s && s->pos < s->len);
}
static void Scanner_next(tl_jcall *c)
{
    jobj *in = tl_jni_get_field(c->self, "stream", "Ljava/io/InputStream;").l; stream *s = in ? in->native : NULL;
    if (!s || s->pos >= s->len) { tl_jni_throw("java/util/NoSuchElementException", ""); c->ret = vl(NULL); return; }
    char *t = malloc(s->len - s->pos + 1); memcpy(t, s->data + s->pos, s->len - s->pos); t[s->len - s->pos] = 0;
    s->pos = s->len; c->ret = vl(STR(t)); free(t);
}

/* ------------------------------------------- collections, services, misc */

static void Map_entrySet(tl_jcall *c) { c->ret = vl(make("java/util/HashSet")); }
static void Set_iterator(tl_jcall *c) { c->ret = vl(make("java/util/Iterator")); }
static void Coll_size(tl_jcall *c) { c->ret = vi(0); }
static void Coll_isEmpty(tl_jcall *c) { c->ret = vz(1); }
static void SP_getAll(tl_jcall *c) { c->ret = vl(make("java/util/HashMap")); }
static void Intent_getExtras(tl_jcall *c) { c->ret = vl(NULL); }
static void Context_getObbDir(tl_jcall *c) { char p[700]; snprintf(p, sizeof(p), "%s/obb", H.ext_files); mkdirs(p); c->ret = vl(new_file(p)); }
static void Context_getObbDirs(tl_jcall *c)
{
    jobj *a = tl_jni_new_obj_array(C("java/io/File"), 1);
    char p[700]; snprintf(p, sizeof(p), "%s/obb", H.ext_files); mkdirs(p);
    a->oarr.v[0] = new_file(p);
    c->ret = vl(a);
}
static void Activity_setRequestedOrientation(tl_jcall *c) { (void)c; }
static void DM_getDisplay(tl_jcall *c) { c->ret = vl(H.display); }
static void Noop(tl_jcall *c) { (void)c; }
static void AudioManager_getDevices(tl_jcall *c) { c->ret = vl(tl_jni_new_obj_array(C("android/media/AudioDeviceInfo"), 0)); }
static void AudioManager_getProperty(tl_jcall *c)
{
    const char *k = S(c->args[0].l);
    c->ret = vl(!strcmp(k, "android.media.property.OUTPUT_SAMPLE_RATE") ? STR("48000")
              : !strcmp(k, "android.media.property.OUTPUT_FRAMES_PER_BUFFER") ? STR("256") : NULL);
}
static void AudioManager_getStreamVolume(tl_jcall *c) { c->ret = vi(7); }
static void AudioManager_getStreamMaxVolume(tl_jcall *c) { c->ret = vi(15); }
static void AudioManager_requestAudioFocus(tl_jcall *c) { c->ret = vi(1); /* AUDIOFOCUS_REQUEST_GRANTED */ }
static void Uri_encode(tl_jcall *c) { c->ret = vl(STR(S(c->args[0].l))); }
static void PAD_init(tl_jcall *c) { c->ret = vl(make("com/unity3d/player/PlayAssetDeliveryUnityWrapper")); }
static void PAD_playCoreApiMissing(tl_jcall *c) { c->ret = vz(1); }
/*
 * FMOD Ex's Java output (org.fmod.FMODAudioDevice), which Unity's audio uses on Android. In Java, start() runs a thread that
 * asks the native mixer for each next block -- fmodProcess(ByteBuffer) fills a direct buffer and returns FMOD_OK (0) -- and
 * writes it to an AudioTrack. Here the same thread is a host thread, and the block goes to the speakers through the shared
 * audio output (tl_cocos_audio_hook), which blocks until there is room: that is what paces FMOD's mixer, as AudioTrack.write
 * does on a phone. fmodGetInfo(0) is the mixer's sample rate and fmodGetInfo(1) its block length in frames; the output is
 * 16-bit stereo.
 */
extern void (*tl_cocos_audio_hook)(const int16_t *samples, int frames, int channels, int rate);
static struct { pthread_t thread; volatile int running; jobj *self; } g_fmodex;

static void *fmodex_main(void *arg)
{
    (void)arg;
    pthread_setname_np("FMODAudioDevice");
    typedef int32_t (*info_fn)(void *env, void *self, int32_t which);
    typedef int32_t (*process_fn)(void *env, void *self, void *buffer);
    info_fn info = (info_fn)tl_jni_native("org/fmod/FMODAudioDevice", "fmodGetInfo", "(I)I");
    process_fn process = (process_fn)tl_jni_native("org/fmod/FMODAudioDevice", "fmodProcess", "(Ljava/nio/ByteBuffer;)I");
    if (!info || !process) { tl_log_line("fmod: FMODAudioDevice has no natives registered; no sound"); return NULL; }
    void *env = tl_jni_env();
    int rate = 0, frames = 0;
    for (int tries = 0; g_fmodex.running && tries < 500; tries++) {
        rate = info(env, g_fmodex.self, 0);
        frames = info(env, g_fmodex.self, 1);
        if (rate > 0 && frames > 0) break;
        usleep(10000);
    }
    if (rate <= 0 || frames <= 0 || frames > 65536) {
        tl_log_line("fmod: the mixer never said its rate and block size (%d Hz, %d frames); no sound", rate, frames);
        return NULL;
    }
    int16_t *block = calloc((size_t)frames * 2, sizeof(int16_t));
    jobj *buffer = tl_jni_new_object(tl_jni_class("java/nio/DirectByteBuffer"));
    jvalue address, capacity; address.l = block; capacity.j = (int64_t)frames * 2 * (int64_t)sizeof(int16_t);
    tl_jni_set_field(buffer, "address", "J", address);
    tl_jni_set_field(buffer, "capacity", "J", capacity);
    tl_log_line("fmod: FMODAudioDevice playing, %d Hz stereo, %d frames a block (info 2..5: %d %d %d %d)", rate, frames,
                info(env, g_fmodex.self, 2), info(env, g_fmodex.self, 3), info(env, g_fmodex.self, 4), info(env, g_fmodex.self, 5));
    bool said = false;
    /* TL_FMOD_PROBE=1: fill the block with a marker first and log how much of it FMOD wrote -- its real block layout. */
    int probe = getenv("TL_FMOD_PROBE") ? 40 : 0;
    while (g_fmodex.running) {
        if (probe) for (int i = 0; i < frames * 2; i++) block[i] = 0x5A5A;
        int r = process(env, g_fmodex.self, buffer);
        if (probe && r == 0) {
            int last = -1, untouched = 0;
            for (int i = 0; i < frames * 2; i++) { if (block[i] != 0x5A5A) last = i; else untouched++; }
            tl_log_line("fmod probe: wrote up to sample %d of %d, %d untouched; first samples %d %d %d %d %d %d", last + 1, frames * 2, untouched,
                        block[0], block[1], block[2], block[3], block[4], block[5]);
            probe--;
        }
        if (r != 0 && !said) { tl_log_line("fmod: fmodProcess -> %d (not FMOD_OK); waiting", r); said = true; }
        { static FILE *dump; static int tried; if (!tried) { tried = 1; if (getenv("TL_FMOD_DUMP")) dump = fopen(getenv("TL_FMOD_DUMP"), "wb"); }
          if (dump && r == 0) fwrite(block, sizeof(int16_t), (size_t)frames * 2, dump); }
        if (r == 0 && tl_cocos_audio_hook) tl_cocos_audio_hook(block, frames, 2, rate);
        else usleep(2000);
    }
    free(block);
    return NULL;
}

static void FMODEx_start(tl_jcall *c)
{
    if (g_fmodex.running) return;
    g_fmodex.self = c->self;
    g_fmodex.running = 1;
    pthread_attr_t a;
    pthread_attr_init(&a);
    pthread_attr_setstacksize(&a, 1u << 20);
    if (pthread_create(&g_fmodex.thread, &a, fmodex_main, NULL) != 0) g_fmodex.running = 0;
    pthread_attr_destroy(&a);
}

static void FMODEx_stop(tl_jcall *c)
{
    (void)c;
    if (!g_fmodex.running) return;
    g_fmodex.running = 0;
    pthread_join(g_fmodex.thread, NULL);
}

static void FMOD_isRunning(tl_jcall *c) { c->ret = vz(g_fmodex.running != 0); }

/* ------------------------------------------------------------------ tables */

static const struct { const char *name, *super; } k_classes[] = {
    { "java/lang/Object", NULL }, { "java/lang/String", "java/lang/Object" }, { "java/lang/System", "java/lang/Object" }, { "java/lang/Class", "java/lang/Object" },
    { "java/lang/ClassLoader", "java/lang/Object" }, { "dalvik/system/PathClassLoader", "java/lang/ClassLoader" },
    { "java/lang/Throwable", "java/lang/Object" }, { "java/lang/Exception", "java/lang/Throwable" },
    { "java/lang/Error", "java/lang/Throwable" }, { "java/lang/NoClassDefFoundError", "java/lang/Error" },
    { "java/lang/NoSuchMethodError", "java/lang/Error" }, { "java/lang/NoSuchFieldError", "java/lang/Error" },
    { "java/lang/NullPointerException", "java/lang/Exception" }, { "java/lang/RuntimeException", "java/lang/Exception" },
    { "java/io/File", "java/lang/Object" }, { "java/lang/reflect/Proxy", "java/lang/Object" },
    { "java/nio/DirectByteBuffer", "java/lang/Object" },
    { "android/content/Context", "java/lang/Object" }, { "android/content/ContextWrapper", "android/content/Context" },
    { "android/view/ContextThemeWrapper", "android/content/ContextWrapper" }, { "android/app/Activity", "android/view/ContextThemeWrapper" },
    { "android/view/View", "java/lang/Object" }, { "android/view/ViewGroup", "android/view/View" },
    { "android/widget/FrameLayout", "android/view/ViewGroup" }, { "android/view/Surface", "java/lang/Object" },
    { "android/view/SurfaceView", "android/view/View" }, { "com/unity3d/player/UnityPlayer", "android/widget/FrameLayout" },
    { "android/content/res/Resources", "java/lang/Object" }, { "android/content/res/AssetManager", "java/lang/Object" },
    { "android/content/res/Configuration", "java/lang/Object" }, { "android/util/DisplayMetrics", "java/lang/Object" },
    { "android/content/pm/ApplicationInfo", "java/lang/Object" }, { "android/content/pm/PackageManager", "java/lang/Object" },
    { "android/content/pm/PackageInfo", "java/lang/Object" }, { "android/view/Display", "java/lang/Object" },
    { "android/view/WindowManager", "java/lang/Object" }, { "android/view/Window", "java/lang/Object" },
    { "android/os/Looper", "java/lang/Object" }, { "android/os/Handler", "java/lang/Object" },
    { "android/os/Build", "java/lang/Object" }, { "android/os/Build$VERSION", "java/lang/Object" },
    { "android/os/Environment", "java/lang/Object" }, { "android/os/Process", "java/lang/Object" },
    { "android/content/SharedPreferences", "java/lang/Object" }, { "android/content/Intent", "java/lang/Object" },
    { "bitter/jnibridge/JNIBridge", "java/lang/Object" }, { "java/util/Locale", "java/lang/Object" }, { "android/app/Application", "android/content/Context" },
    { "com/unity3d/player/ReflectionHelper", "java/lang/Object" }, { "java/lang/reflect/Member", "java/lang/Object" },
    { "java/lang/reflect/Method", "java/lang/Object" }, { "java/lang/reflect/Constructor", "java/lang/Object" }, { "java/lang/reflect/Field", "java/lang/Object" },
    { "java/lang/StringBuilder", "java/lang/Object" }, { "java/io/InputStream", "java/lang/Object" },
    { "java/io/ByteArrayInputStream", "java/io/InputStream" }, { "java/io/FileNotFoundException", "java/lang/Exception" },
    { "java/util/Scanner", "java/lang/Object" }, { "java/util/HashMap", "java/lang/Object" }, { "java/util/HashSet", "java/lang/Object" },
    { "java/util/Map", "java/lang/Object" }, { "java/util/Set", "java/lang/Object" }, { "java/util/NoSuchElementException", "java/lang/Exception" },
    { "android/hardware/display/DisplayManager", "java/lang/Object" }, { "android/media/AudioManager", "java/lang/Object" },
    { "android/media/AudioDeviceInfo", "java/lang/Object" }, { "android/net/Uri", "java/lang/Object" },
    { "org/fmod/FMODAudioDevice", "java/lang/Object" }, { "android/app/AlertDialog$Builder", "java/lang/Object" }, { "android/app/Dialog", "java/lang/Object" }, { "android/app/AlertDialog", "android/app/Dialog" },
    { "android/content/SharedPreferences$Editor", "java/lang/Object" }, { "java/util/Iterator", "java/lang/Object" },
};

/*
 * Class.forName(String): how Unity finds a class when JNI's FindClass has no answer for it, from a thread whose class loader is the
 * system's. The name is dotted. The class is there if the framework or the APK has it (the same rule FindClass applies); a
 * ClassNotFoundException is what the engine expects for one that is not.
 */
static void Class_forName(tl_jcall *c)
{
    char name[300]; snprintf(name, sizeof(name), "%s", tl_jni_string(c->args[0].l) ? tl_jni_string(c->args[0].l) : "");
    for (char *p = name; *p; p++) if (*p == '.') *p = '/';
    bool framework = !strncmp(name, "android/", 8) || !strncmp(name, "java/", 5) || !strncmp(name, "javax/", 6) || !strncmp(name, "dalvik/", 7)
                  || !strncmp(name, "libcore/", 8) || !strncmp(name, "sun/", 4) || !strncmp(name, "org/json/", 9);
    if (name[0] == '[' || framework || tl_dexidx_has_class(name)) { c->ret.l = tl_jni_class_object(name); return; }
    tl_jni_throw("java/lang/ClassNotFoundException", name);
    c->ret.l = NULL;
}

#define M(c, n, s, f) { c, n, s, f }
static const tl_jhle k_hle[] = {
    M("java/lang/Class", "forName", "(Ljava/lang/String;)Ljava/lang/Class;", Class_forName),
    M("java/lang/Class", "forName", "(Ljava/lang/String;)Ljava/lang/Object;", Class_forName),
    M("java/lang/System", "load", "(Ljava/lang/String;)V", System_load),
    M("java/lang/System", "loadLibrary", "(Ljava/lang/String;)V", System_loadLibrary),
    M("com/sybogames/chili/migration/KilooPlatformAndroidBridge", "<init>", "()V", Noop),
    M("com/sybogames/chili/migration/KilooPlatformAndroidBridge", "getUserdataPath", "()Ljava/lang/String;", Kiloo_getUserdataPath),
    M("java/util/Locale", "getDefault", "()Ljava/util/Locale;", Locale_getDefault),
    M("java/util/Locale", "toLanguageTag", "()Ljava/lang/String;", Locale_toLanguageTag),
    M("java/util/Locale", "getLanguage", "()Ljava/lang/String;", Locale_getLanguage),
    M("java/util/Locale", "getCountry", "()Ljava/lang/String;", Locale_getCountry),
    M("com/kokosoft/preciselocale/PreciseLocale", "getLanguage", "()Ljava/lang/String;", Locale_getLanguage),
    M("com/kokosoft/preciselocale/PreciseLocale", "getLanguageID", "()Ljava/lang/String;", Locale_getLanguage),
    M("com/kokosoft/preciselocale/PreciseLocale", "getRegion", "()Ljava/lang/String;", PreciseLocale_getRegion),
    M("com/sybo/analytics/ChipsetUtils", "GetChipsetName", "()Ljava/lang/String;", Chipset_name),
    M("com/unity/androidnotifications/UnityNotificationManager", "getNotificationManagerImpl", "(Ljava/lang/Object;Lcom/unity/androidnotifications/NotificationCallback;)Ljava/lang/Object;", NotificationManager_get),
    M("com/unity3d/player/ReflectionHelper", "newProxyInstance", "(Lcom/unity3d/player/UnityPlayer;JLjava/lang/Class;)Ljava/lang/Object;", Reflection_newProxyInstance),
    M("android/app/Activity", "getApplication", "()Landroid/app/Application;", Activity_getApplication),
    M("android/app/Activity", "getApplication", "()Ljava/lang/Object;", Activity_getApplication),
    M("com/dikra/diskutils/DiskUtils", "availableSpace", "(Z)I", DiskUtils_availableSpace),
    M("com/sybogames/chili/SidekickHelper", "getSidekickExperimentInfo", "()I", Zero_int),
    M("android/util/Log", "getStackTraceString", "(Ljava/lang/Throwable;)Ljava/lang/String;", Log_getStackTraceString),
    M("com/unity3d/player/UnityPlayer", "getNetworkConnectivity", "()I", Unity_getNetworkConnectivity),
    M("java/lang/Object", "toString", "()Ljava/lang/String;", Object_toString),
    M("java/lang/Object", "hashCode", "()I", Object_hashCode),
    M("java/lang/Object", "equals", "(Ljava/lang/Object;)Z", Object_equals),
    M("java/lang/String", "equals", "(Ljava/lang/Object;)Z", String_equals),
    M("java/lang/String", "toString", "()Ljava/lang/String;", String_toString),
    M("java/lang/String", "length", "()I", String_length),
    M("java/lang/String", "isEmpty", "()Z", String_isEmpty),
    M("java/lang/String", "hashCode", "()I", String_hashCode),
    M("java/lang/System", "getProperty", "(Ljava/lang/String;)Ljava/lang/String;", System_getProperty),
    M("java/lang/System", "currentTimeMillis", "()J", System_currentTimeMillis),
    M("java/lang/System", "nanoTime", "()J", System_nanoTime),
    M("java/io/File", "<init>", "(Ljava/lang/String;)V", File_init_s),
    M("java/io/File", "<init>", "(Ljava/io/File;Ljava/lang/String;)V", File_init_fs),
    M("java/io/File", "getPath", "()Ljava/lang/String;", File_getPath),
    M("java/io/File", "getAbsolutePath", "()Ljava/lang/String;", File_getPath),
    M("java/io/File", "getCanonicalPath", "()Ljava/lang/String;", File_getPath),
    M("java/io/File", "getName", "()Ljava/lang/String;", File_getName),
    M("java/io/File", "getParent", "()Ljava/lang/String;", File_getParent),
    M("java/io/File", "getParentFile", "()Ljava/io/File;", File_getParentFile),
    M("java/io/File", "exists", "()Z", File_exists), M("java/io/File", "isDirectory", "()Z", File_isDirectory),
    M("java/io/File", "isFile", "()Z", File_isFile), M("java/io/File", "canRead", "()Z", File_canRead),
    M("java/io/File", "canWrite", "()Z", File_canWrite), M("java/io/File", "mkdirs", "()Z", File_mkdirs),
    M("java/io/File", "mkdir", "()Z", File_mkdir), M("java/io/File", "delete", "()Z", File_delete),
    M("java/io/File", "length", "()J", File_length), M("java/io/File", "toString", "()Ljava/lang/String;", File_toString),
    M("android/content/Context", "getPackageName", "()Ljava/lang/String;", Context_getPackageName),
    M("android/content/Context", "getPackageCodePath", "()Ljava/lang/String;", Context_getPackageCodePath),
    M("android/content/Context", "getPackageResourcePath", "()Ljava/lang/String;", Context_getPackageResourcePath),
    M("android/content/Context", "getFilesDir", "()Ljava/io/File;", Context_getFilesDir),
    M("android/content/Context", "getCacheDir", "()Ljava/io/File;", Context_getCacheDir),
    M("android/content/Context", "getCodeCacheDir", "()Ljava/io/File;", Context_getCodeCacheDir),
    M("android/content/Context", "getNoBackupFilesDir", "()Ljava/io/File;", Context_getNoBackupFilesDir),
    M("android/content/Context", "getExternalFilesDir", "(Ljava/lang/String;)Ljava/io/File;", Context_getExternalFilesDir),
    M("android/content/Context", "getExternalCacheDir", "()Ljava/io/File;", Context_getExternalCacheDir),
    M("android/content/Context", "getDir", "(Ljava/lang/String;I)Ljava/io/File;", Context_getDir),
    M("android/content/Context", "getResources", "()Landroid/content/res/Resources;", Context_getResources),
    M("android/content/Context", "getAssets", "()Landroid/content/res/AssetManager;", Context_getAssets),
    M("android/content/Context", "getApplicationInfo", "()Landroid/content/pm/ApplicationInfo;", Context_getApplicationInfo),
    M("android/content/Context", "getPackageManager", "()Landroid/content/pm/PackageManager;", Context_getPackageManager),
    M("android/content/Context", "getApplicationContext", "()Landroid/content/Context;", Context_getApplicationContext),
    M("android/content/Context", "getMainLooper", "()Landroid/os/Looper;", Context_getMainLooper),
    M("android/content/Context", "getClassLoader", "()Ljava/lang/ClassLoader;", Context_getClassLoader),
    M("android/content/Context", "getSharedPreferences", "(Ljava/lang/String;I)Landroid/content/SharedPreferences;", Context_getSharedPreferences),
    M("android/content/Context", "checkCallingOrSelfPermission", "(Ljava/lang/String;)I", Context_checkCallingOrSelfPermission),
    M("android/content/Context", "checkSelfPermission", "(Ljava/lang/String;)I", Context_checkSelfPermission),
    M("android/content/Context", "getSystemService", "(Ljava/lang/String;)Ljava/lang/Object;", Context_getSystemService),
    M("android/content/Context", "getContentResolver", "()Landroid/content/ContentResolver;", Context_getContentResolver),
    M("android/app/Activity", "getWindow", "()Landroid/view/Window;", Activity_getWindow),
    M("android/app/Activity", "getWindowManager", "()Landroid/view/WindowManager;", Activity_getWindowManager),
    M("android/app/Activity", "getRequestedOrientation", "()I", Activity_getRequestedOrientation),
    M("android/app/Activity", "getIntent", "()Landroid/content/Intent;", Activity_getIntent),
    M("android/app/Activity", "isFinishing", "()Z", Activity_isFinishing),
    M("android/content/res/Resources", "getAssets", "()Landroid/content/res/AssetManager;", Resources_getAssets),
    M("android/content/res/Resources", "getConfiguration", "()Landroid/content/res/Configuration;", Resources_getConfiguration),
    M("android/content/res/Resources", "getDisplayMetrics", "()Landroid/util/DisplayMetrics;", Resources_getDisplayMetrics),
    M("android/content/res/Resources", "getIdentifier", "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)I", Resources_getIdentifier),
    M("android/view/Display", "getMetrics", "(Landroid/util/DisplayMetrics;)V", Display_getMetrics),
    M("android/view/Display", "getRealMetrics", "(Landroid/util/DisplayMetrics;)V", Display_getMetrics),
    M("android/view/Display", "getRotation", "()I", Display_getRotation),
    M("android/view/Display", "getRefreshRate", "()F", Display_getRefreshRate),
    M("android/view/Display", "getDisplayId", "()I", Display_getDisplayId),
    M("android/view/Display", "getWidth", "()I", Display_getWidth), M("android/view/Display", "getHeight", "()I", Display_getHeight),
    M("android/view/Display", "getName", "()Ljava/lang/String;", Display_getName),
    M("android/view/WindowManager", "getDefaultDisplay", "()Landroid/view/Display;", WindowManager_getDefaultDisplay),
    M("android/content/pm/PackageManager", "getApplicationInfo", "(Ljava/lang/String;I)Landroid/content/pm/ApplicationInfo;", PM_getApplicationInfo),
    M("android/content/pm/PackageManager", "hasSystemFeature", "(Ljava/lang/String;)Z", PM_hasSystemFeature),
    M("android/content/pm/PackageManager", "getPackageInfo", "(Ljava/lang/String;I)Landroid/content/pm/PackageInfo;", PM_getPackageInfo),
    M("android/os/Environment", "getExternalStorageState", "()Ljava/lang/String;", Env_getExternalStorageState),
    M("android/os/Environment", "getExternalStorageDirectory", "()Ljava/io/File;", Env_getExternalStorageDirectory),
    M("android/os/Process", "setThreadPriority", "(II)V", Process_setThreadPriority),
    M("android/os/Process", "myPid", "()I", Process_myPid), M("android/os/Process", "myTid", "()I", Process_myTid),
    M("java/lang/Object", "getClass", "()Ljava/lang/Class;", Object_getClass),
    M("java/lang/Class", "getClassLoader", "()Ljava/lang/ClassLoader;", Class_getClassLoader),
    M("android/provider/Settings$Secure", "getString", "(Landroid/content/ContentResolver;Ljava/lang/String;)Ljava/lang/String;", Secure_getString),
    M("java/lang/ClassLoader", "findLibrary", "(Ljava/lang/String;)Ljava/lang/String;", ClassLoader_findLibrary),
    M("android/app/AlertDialog$Builder", "<init>", "(Landroid/content/Context;)V", Builder_init),
    M("android/app/AlertDialog$Builder", "setTitle", "(Ljava/lang/CharSequence;)Landroid/app/AlertDialog$Builder;", Builder_setTitle),
    M("android/app/AlertDialog$Builder", "setMessage", "(Ljava/lang/CharSequence;)Landroid/app/AlertDialog$Builder;", Builder_setMessage),
    M("android/app/AlertDialog$Builder", "show", "()Landroid/app/AlertDialog;", Builder_show),
    M("android/app/AlertDialog$Builder", "setCancelable", "(Z)Landroid/app/AlertDialog$Builder;", Builder_chain),
    M("android/app/AlertDialog$Builder", "setPositiveButton", "(Ljava/lang/CharSequence;Landroid/content/DialogInterface$OnClickListener;)Landroid/app/AlertDialog$Builder;", Builder_chain),
    M("android/app/AlertDialog$Builder", "setNegativeButton", "(Ljava/lang/CharSequence;Landroid/content/DialogInterface$OnClickListener;)Landroid/app/AlertDialog$Builder;", Builder_chain),
    M("android/app/AlertDialog$Builder", "setView", "(Landroid/view/View;)Landroid/app/AlertDialog$Builder;", Builder_chain),
    M("android/app/AlertDialog$Builder", "setOnKeyListener", "(Landroid/content/DialogInterface$OnKeyListener;)Landroid/app/AlertDialog$Builder;", Builder_chain),
    M("android/app/AlertDialog$Builder", "setOnCancelListener", "(Landroid/content/DialogInterface$OnCancelListener;)Landroid/app/AlertDialog$Builder;", Builder_chain),
    M("android/app/AlertDialog$Builder", "create", "()Landroid/app/AlertDialog;", Builder_create),
    M("android/app/Dialog", "show", "()V", Dialog_show),
    M("android/app/AlertDialog", "show", "()V", Dialog_show),
    M("android/content/SharedPreferences", "getInt", "(Ljava/lang/String;I)I", SP_getInt),
    M("android/content/SharedPreferences", "getString", "(Ljava/lang/String;Ljava/lang/String;)Ljava/lang/String;", SP_getString),
    M("android/content/SharedPreferences", "getBoolean", "(Ljava/lang/String;Z)Z", SP_getBoolean),
    M("android/content/SharedPreferences", "edit", "()Landroid/content/SharedPreferences$Editor;", SP_edit),
    M("android/content/SharedPreferences$Editor", "putInt", "(Ljava/lang/String;I)Landroid/content/SharedPreferences$Editor;", Editor_self),
    M("android/content/SharedPreferences$Editor", "putString", "(Ljava/lang/String;Ljava/lang/String;)Landroid/content/SharedPreferences$Editor;", Editor_self),
    M("android/content/SharedPreferences$Editor", "putBoolean", "(Ljava/lang/String;Z)Landroid/content/SharedPreferences$Editor;", Editor_self),
    M("android/content/SharedPreferences$Editor", "apply", "()V", Editor_noop),
    M("java/util/Iterator", "hasNext", "()Z", Iterator_hasNext), M("java/util/Iterator", "next", "()Ljava/lang/Object;", Iterator_next),
    M("java/lang/String", "<init>", "()V", String_init_empty), M("java/lang/String", "<init>", "([B)V", String_init_bytes),
    M("java/lang/String", "<init>", "([BLjava/lang/String;)V", String_init_bytes), M("java/lang/String", "<init>", "([BII)V", String_init_bytes_range),
    M("java/lang/String", "<init>", "([BIILjava/lang/String;)V", String_init_bytes_range), M("java/lang/String", "<init>", "(Ljava/lang/String;)V", String_init_string),
    M("java/lang/String", "<init>", "([C)V", String_init_chars), M("java/lang/String", "getBytes", "()[B", String_getBytes),
    M("java/lang/String", "getBytes", "(Ljava/lang/String;)[B", String_getBytes),
    M("java/lang/StringBuilder", "<init>", "()V", SB_init), M("java/lang/StringBuilder", "<init>", "(I)V", SB_init),
    M("java/lang/StringBuilder", "<init>", "(Ljava/lang/String;)V", SB_init_string),
    M("java/lang/StringBuilder", "append", "(Ljava/lang/String;)Ljava/lang/StringBuilder;", SB_append_string),
    M("java/lang/StringBuilder", "append", "(Ljava/lang/Object;)Ljava/lang/StringBuilder;", SB_append_obj),
    M("java/lang/StringBuilder", "append", "(Ljava/lang/CharSequence;)Ljava/lang/StringBuilder;", SB_append_obj),
    M("java/lang/StringBuilder", "append", "(I)Ljava/lang/StringBuilder;", SB_append_int), M("java/lang/StringBuilder", "append", "(J)Ljava/lang/StringBuilder;", SB_append_long),
    M("java/lang/StringBuilder", "append", "(C)Ljava/lang/StringBuilder;", SB_append_char), M("java/lang/StringBuilder", "append", "(Z)Ljava/lang/StringBuilder;", SB_append_bool),
    M("java/lang/StringBuilder", "append", "(F)Ljava/lang/StringBuilder;", SB_append_float), M("java/lang/StringBuilder", "append", "(D)Ljava/lang/StringBuilder;", SB_append_double),
    M("java/lang/StringBuilder", "toString", "()Ljava/lang/String;", SB_toString), M("java/lang/StringBuilder", "length", "()I", SB_length),
    M("java/lang/StringBuilder", "setLength", "(I)V", SB_setLength),
    M("android/content/res/AssetManager", "open", "(Ljava/lang/String;)Ljava/io/InputStream;", AM_open),
    M("android/content/res/AssetManager", "open", "(Ljava/lang/String;I)Ljava/io/InputStream;", AM_open),
    M("java/io/InputStream", "read", "()I", IS_read0), M("java/io/InputStream", "read", "([B)I", IS_read_arr), M("java/io/InputStream", "read", "([BII)I", IS_read_range),
    M("java/io/InputStream", "available", "()I", IS_available), M("java/io/InputStream", "close", "()V", IS_close),
    M("java/util/Scanner", "<init>", "(Ljava/io/InputStream;Ljava/lang/String;)V", Scanner_init), M("java/util/Scanner", "<init>", "(Ljava/io/InputStream;)V", Scanner_init),
    M("java/util/Scanner", "useDelimiter", "(Ljava/lang/String;)Ljava/util/Scanner;", Scanner_useDelimiter),
    M("java/util/Scanner", "hasNext", "()Z", Scanner_hasNext), M("java/util/Scanner", "next", "()Ljava/lang/String;", Scanner_next), M("java/util/Scanner", "close", "()V", Noop),

    M("java/util/HashSet", "iterator", "()Ljava/util/Iterator;", Set_iterator), M("java/util/Set", "iterator", "()Ljava/util/Iterator;", Set_iterator),
    M("java/util/HashSet", "size", "()I", Coll_size), M("java/util/Set", "size", "()I", Coll_size),
    M("android/content/SharedPreferences", "getAll", "()Ljava/util/Map;", SP_getAll),
    M("android/content/Intent", "getExtras", "()Landroid/os/Bundle;", Intent_getExtras),
    M("android/content/Context", "getObbDir", "()Ljava/io/File;", Context_getObbDir), M("android/content/Context", "getObbDirs", "()[Ljava/io/File;", Context_getObbDirs),
    M("android/app/Activity", "setRequestedOrientation", "(I)V", Activity_setRequestedOrientation),
    M("android/hardware/display/DisplayManager", "getDisplay", "(I)Landroid/view/Display;", DM_getDisplay),
    M("android/hardware/display/DisplayManager", "registerDisplayListener", "(Landroid/hardware/display/DisplayManager$DisplayListener;Landroid/os/Handler;)V", Noop),
    M("android/util/DisplayMetrics", "<init>", "()V", Noop),
    M("android/media/AudioManager", "getDevices", "(I)[Landroid/media/AudioDeviceInfo;", AudioManager_getDevices),
    M("android/media/AudioManager", "getProperty", "(Ljava/lang/String;)Ljava/lang/String;", AudioManager_getProperty),
    M("android/media/AudioManager", "getStreamVolume", "(I)I", AudioManager_getStreamVolume), M("android/media/AudioManager", "getStreamMaxVolume", "(I)I", AudioManager_getStreamMaxVolume),
    M("android/media/AudioManager", "requestAudioFocus", "(Landroid/media/AudioManager$OnAudioFocusChangeListener;II)I", AudioManager_requestAudioFocus),
    M("android/net/Uri", "encode", "(Ljava/lang/String;)Ljava/lang/String;", Uri_encode),
    M("com/unity3d/player/PlayAssetDeliveryUnityWrapper", "init", "(Landroid/content/Context;)Lcom/unity3d/player/PlayAssetDeliveryUnityWrapper;", PAD_init),
    M("com/unity3d/player/PlayAssetDeliveryUnityWrapper", "playCoreApiMissing", "()Z", PAD_playCoreApiMissing),
    M("com/unity3d/player/UnityCoreAssetPacksStatusCallbacks", "<init>", "()V", Noop),
    M("com/unity3d/player/HFPStatus", "clearHFPStat", "()V", Noop),
    M("org/fmod/FMODAudioDevice", "<init>", "()V", Noop), M("org/fmod/FMODAudioDevice", "start", "()V", FMODEx_start),
    M("org/fmod/FMODAudioDevice", "stop", "()V", FMODEx_stop), M("org/fmod/FMODAudioDevice", "close", "()V", FMODEx_stop), M("org/fmod/FMODAudioDevice", "isRunning", "()Z", FMOD_isRunning),
    M("com/unity3d/player/UnityPlayer", "initializeGoogleAr", "()Z", UnityPlayer_initializeGoogleAr),
    M("bitter/jnibridge/JNIBridge", "newInterfaceProxy", "(J[Ljava/lang/Class;)Ljava/lang/Object;", JNIBridge_newInterfaceProxy),
    M("com/unity3d/player/ReflectionHelper", "getConstructorID", "(Ljava/lang/Class;Ljava/lang/String;)Ljava/lang/reflect/Constructor;", Reflection_getConstructorID),
    M("com/unity3d/player/ReflectionHelper", "getMethodID", "(Ljava/lang/Class;Ljava/lang/String;Ljava/lang/String;Z)Ljava/lang/reflect/Method;", Reflection_getMethodID),
    M("com/unity3d/player/ReflectionHelper", "getFieldID", "(Ljava/lang/Class;Ljava/lang/String;Ljava/lang/String;Z)Ljava/lang/reflect/Field;", Reflection_getFieldID),
    M("com/unity3d/player/ReflectionHelper", "getFieldSignature", "(Ljava/lang/reflect/Field;)Ljava/lang/String;", Reflection_getFieldSignature),
    M("java/lang/reflect/Field", "getDeclaringClass", "()Ljava/lang/Class;", Member_getDeclaringClass),
    M("java/lang/reflect/Method", "getDeclaringClass", "()Ljava/lang/Class;", Member_getDeclaringClass),
    M("java/lang/reflect/Constructor", "getDeclaringClass", "()Ljava/lang/Class;", Member_getDeclaringClass),
    { NULL, NULL, NULL, NULL }
};

void tl_hle_configure(const char *pkg, const char *apk, const char *data, int w, int h)
{
    snprintf(H.pkg, sizeof(H.pkg), "%s", pkg);
    snprintf(H.apk, sizeof(H.apk), "%s", apk);
    snprintf(H.data, sizeof(H.data), "%s", data);
    snprintf(H.files, sizeof(H.files), "%s/files", data);
    snprintf(H.cache, sizeof(H.cache), "%s/cache", data);
    snprintf(H.ext_files, sizeof(H.ext_files), "%s/sdcard/Android/data/%s/files", data, pkg);
    snprintf(H.ext_cache, sizeof(H.ext_cache), "%s/sdcard/Android/data/%s/cache", data, pkg);
    snprintf(H.native_lib, sizeof(H.native_lib), "/data/app/lib/arm64");
    H.width = w; H.height = h; H.density = 3.0f;
    H.version_name[0] = 0; H.version_code = 0;
    read_manifest_version(apk);
    read_manifest_meta(apk);
    mkdirs(H.files); mkdirs(H.cache); mkdirs(H.ext_files); mkdirs(H.ext_cache);
}

jobj *tl_hle_activity(void) { return H.activity; }
int tl_hle_version_code(void) { return H.version_code; }
/* An app whose activity is its own class (Minecraft's MainActivity) swaps its instance in for the generic one. */
void tl_hle_set_activity(jobj *a) { H.activity = a; }
jobj *tl_hle_assets(void) { return H.assets; }
jobj *tl_hle_config(void) { return H.config; }

extern void tl_loop_install(void);
extern void tl_input_install(void);
extern void tl_tls_install(void);
extern void tl_gms_install(void);
void tl_jni_hle_install(void)
{
    for (size_t i = 0; i < sizeof(k_classes) / sizeof(k_classes[0]); i++) tl_jni_declare(k_classes[i].name, k_classes[i].super);
    tl_jni_register_hle(k_hle);
    install_build();
    tl_loop_install();
    tl_input_install();
    tl_tls_install();
    tl_http_install();
    tl_gms_install();

    H.activity = make("android/app/Activity");
    H.resources = make("android/content/res/Resources");
    H.assets = make("android/content/res/AssetManager");
    H.config = make("android/content/res/Configuration");
    set_int(H.config, "orientation", H.width > H.height ? 2 : 1);     /* ORIENTATION_LANDSCAPE / PORTRAIT */
    set_float(H.config, "fontScale", 1.0f);
    set_int(H.config, "screenLayout", 0x22); set_int(H.config, "keyboard", 1); set_int(H.config, "navigation", 1);
    set_int(H.config, "touchscreen", 3); set_int(H.config, "screenWidthDp", (int)(H.width / H.density));
    set_int(H.config, "screenHeightDp", (int)(H.height / H.density)); set_int(H.config, "densityDpi", (int)(H.density * 160));
    H.metrics = make("android/util/DisplayMetrics");
    fill_metrics(H.metrics);
    H.appinfo = make("android/content/pm/ApplicationInfo");
    set_str(H.appinfo, "sourceDir", H.apk); set_str(H.appinfo, "publicSourceDir", H.apk);
    set_str(H.appinfo, "nativeLibraryDir", H.native_lib); set_str(H.appinfo, "dataDir", H.data);
    set_str(H.appinfo, "packageName", H.pkg); set_int(H.appinfo, "flags", 0x8000 /* FLAG_HAS_CODE */);
    set_int(H.appinfo, "targetSdkVersion", 36); set_int(H.appinfo, "minSdkVersion", 24);
    /* A Google Play install's split APKs (64-bit libraries, asset packs): where Unity finds its data when it is in a pack. */
    int nsplit = 0;
    while (tl_ld_queued_split(nsplit)) nsplit++;
    if (nsplit) {
        jobj *dirs = tl_jni_new_obj_array(tl_jni_class("java/lang/String"), (uint32_t)nsplit);
        for (int i = 0; i < nsplit; i++) dirs->oarr.v[i] = STR(tl_ld_queued_split(i));
        tl_jni_set_field(H.appinfo, "splitSourceDirs", "[Ljava/lang/String;", vl(dirs));
        tl_jni_set_field(H.appinfo, "splitPublicSourceDirs", "[Ljava/lang/String;", vl(tl_jni_ref(dirs)));
    }
    H.pm = make("android/content/pm/PackageManager");
    H.display = make("android/view/Display");
    H.wm = make("android/view/WindowManager");
    H.window = make("android/view/Window");
    tl_jni_set_static("com/unity3d/player/UnityPlayer", "currentActivity", "Landroid/app/Activity;", vl(H.activity));
    tl_jni_set_static("com/unity3d/player/UnityPlayer", "currentContext", "Landroid/content/Context;", vl(H.activity));
}
