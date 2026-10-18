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
#include <fcntl.h>
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
static void Display_isHdr(tl_jcall *c) { c->ret = vz(0); }
static void Display_getAppVsyncOffsetNanos(tl_jcall *c) { c->ret.j = 0; }
static void Display_getPresentationDeadlineNanos(tl_jcall *c) { c->ret.j = 16666667; }
static void Window_setFlags(tl_jcall *c) { (void)c; }
static void Display_getSupportedRefreshRates(tl_jcall *c)
{
    jobj *rates = tl_jni_new_prim_array('F', 1);
    ((float *)rates->arr.data)[0] = 60.f;
    c->ret = vl(rates);
}
static void Display_getMode(tl_jcall *c) { c->ret = vl(make("android/view/Display$Mode")); }
static void Display_getSupportedModes(tl_jcall *c)
{
    /* Unity takes the maximum refresh rate from this array during startup. A null JNI
     * result turns into an empty managed sequence and Enumerable.Max throws. */
    jobj *modes = tl_jni_new_obj_array(C("android/view/Display$Mode"), 1);
    modes->oarr.v[0] = make("android/view/Display$Mode");
    tl_note_once("Display.getSupportedModes returned one 60 Hz mode");
    c->ret = vl(modes);
}
static void DisplayMode_getModeId(tl_jcall *c) { c->ret = vi(1); }
static void DisplayMode_getPhysicalWidth(tl_jcall *c) { c->ret = vi(H.width); }
static void DisplayMode_getPhysicalHeight(tl_jcall *c) { c->ret = vi(H.height); }
static void DisplayMode_getRefreshRate(tl_jcall *c) { c->ret = vf(60.f); }
static void DisplayMode_getAlternativeRefreshRates(tl_jcall *c) { c->ret = vl(tl_jni_new_prim_array('F', 0)); }
static void WindowManager_getDefaultDisplay(tl_jcall *c) { c->ret = vl(H.display); }

/* Java's sensor query is used by Unity before it starts the game. Match Android's contract, but
 * expose only motion sensors that the same CoreMotion backend can actually provide on this phone. */
extern bool tl_sensor_available(int type);
typedef struct { int type; const char *name, *string_type; float range, resolution; } tl_sensor_spec;
static const tl_sensor_spec k_sensor_specs[] = {
    { 1, "Accelerometer", "android.sensor.accelerometer", 156.9f, 0.0024f },
    { 2, "Magnetometer", "android.sensor.magnetic_field", 4912.f, 0.15f },
    { 4, "Gyroscope", "android.sensor.gyroscope", 34.9f, 0.0011f },
    { 9, "Gravity", "android.sensor.gravity", 19.6f, 0.0024f },
    { 10, "Linear Acceleration", "android.sensor.linear_acceleration", 156.9f, 0.0024f },
    { 11, "Rotation Vector", "android.sensor.rotation_vector", 1.f, 5.96e-8f },
    { 15, "Game Rotation Vector", "android.sensor.game_rotation_vector", 1.f, 5.96e-8f },
};
static jobj *k_sensor_objects[16];
static const tl_sensor_spec *sensor_spec(int type)
{
    for (size_t i = 0; i < sizeof(k_sensor_specs) / sizeof(k_sensor_specs[0]); i++)
        if (k_sensor_specs[i].type == type) return &k_sensor_specs[i];
    return NULL;
}
static jobj *sensor_object(int type)
{
    const tl_sensor_spec *s = sensor_spec(type);
    if (!s || !tl_sensor_available(type)) return NULL;
    if (!k_sensor_objects[type]) {
        jobj *o = make("android/hardware/Sensor");
        set_int(o, "mType", type); set_str(o, "mName", s->name); set_str(o, "mStringType", s->string_type);
        set_float(o, "mRange", s->range); set_float(o, "mResolution", s->resolution);
        k_sensor_objects[type] = o;
    }
    return k_sensor_objects[type];
}
static void SensorManager_getDefaultSensor(tl_jcall *c) { c->ret = vl(sensor_object(c->args[0].i)); }
static void SensorManager_getDefaultWakeSensor(tl_jcall *c)
{
    c->ret = vl(c->args[1].z ? NULL : sensor_object(c->args[0].i));
}
static void Sensor_getType(tl_jcall *c) { c->ret = tl_jni_get_field(c->self, "mType", "I"); }
static void Sensor_getName(tl_jcall *c) { c->ret = tl_jni_get_field(c->self, "mName", "Ljava/lang/String;"); }
static void Sensor_getStringType(tl_jcall *c) { c->ret = tl_jni_get_field(c->self, "mStringType", "Ljava/lang/String;"); }
static void Sensor_getMaximumRange(tl_jcall *c) { c->ret = tl_jni_get_field(c->self, "mRange", "F"); }
static void Sensor_getResolution(tl_jcall *c) { c->ret = tl_jni_get_field(c->self, "mResolution", "F"); }
static void Sensor_getVendor(tl_jcall *c) { c->ret = vl(STR("Apple")); }
static void Sensor_getVersion(tl_jcall *c) { c->ret = vi(1); }
static void Sensor_getPower(tl_jcall *c) { c->ret = vf(0.5f); }
static void Sensor_getMinDelay(tl_jcall *c) { c->ret = vi(5000); }
static void Sensor_getMaxDelay(tl_jcall *c) { c->ret = vi(200000); }
static void Sensor_getId(tl_jcall *c) { c->ret = tl_jni_get_field(c->self, "mType", "I"); }
static void Sensor_false(tl_jcall *c) { c->ret = vz(0); }
static void Sensor_getFifoCount(tl_jcall *c) { c->ret = vi(0); }
static void Sensor_getReportingMode(tl_jcall *c) { c->ret = vi(0); }

/* ----------------------------------------------------- package information */

static void PM_getApplicationInfo(tl_jcall