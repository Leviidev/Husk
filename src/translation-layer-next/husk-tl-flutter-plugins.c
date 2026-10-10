/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * The Android halves of the Flutter plugins most apps use, answered natively. A plugin's Dart side sends method calls (or
 * Pigeon-generated API calls) over a channel; on Android a Java class answers them. These are those answers:
 *
 *   shared_preferences      a key-value store, kept in the app's data directory
 *   path_provider           the app's directories
 *   package_info_plus       the app's name, package and version
 *   device_info_plus        an Android phone, described the way Build does it
 *   url_launcher            links opened by the host
 *   connectivity_plus       online over Wi-Fi
 *
 * A Pigeon call's arguments are a list, and its reply is a list: [result] on success, [code, message, details] on failure.
 */
#include "husk-tl-flutter-plugins.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <time.h>
#include <unistd.h>

#include "husk-tl-bionic.h"
#include "husk-tl-flutter-codec.h"

static char g_data[512], g_pkg[128], g_label[128] = "", g_version[32] = "1.0", g_build[16] = "1";
static void (*g_open_url)(const char *url);

void tl_flutter_plugins_configure(const char *data_dir, const char *package, const char *label, const char *version, long build)
{
    snprintf(g_data, sizeof(g_data), "%s", data_dir);
    snprintf(g_pkg, sizeof(g_pkg), "%s", package);
    snprintf(g_label, sizeof(g_label), "%s", label && *label ? label : package);
    if (version && *version) snprintf(g_version, sizeof(g_version), "%s", version);
    if (build > 0) snprintf(g_build, sizeof(g_build), "%ld", build);
}
void tl_flutter_set_url_opener(void (*fn)(const char *url)) { g_open_url = fn; }

/* ------------------------------------------------------------- replies */

static uint8_t *pigeon_ok(sv *result, size_t *len)
{
    sv *l = sv_list(1);
    sv_set(l, 0, result ? result : sv_null());
    uint8_t *b = sv_encode_message(l, len);
    sv_free(l);
    return b;
}
static uint8_t *pigeon_error(const char *code, const char *msg, size_t *len)
{
    sv *l = sv_list(3);
    sv_set(l, 0, sv_string(code)); sv_set(l, 1, sv_string(msg)); sv_set(l, 2, sv_null());
    uint8_t *b = sv_encode_message(l, len);
    sv_free(l);
    return b;
}
static uint8_t *method_ok(sv *result, size_t *len) { uint8_t *b = sv_encode_success(result, len); sv_free(result); return b; }

/* A method channel call: [method name, arguments]. */
static bool method_call(const uint8_t *d, size_t len, char *name, size_t n, sv **args)
{
    size_t used = 0;
    sv *m = sv_decode(d, len, &used);
    if (!m || m->type != SV_STRING) { sv_free(m); return false; }
    snprintf(name, n, "%s", m->s);
    sv_free(m);
    *args = used < len ? sv_decode(d + used, len - used, NULL) : sv_null();
    if (!*args) *args = sv_null();
    return true;
}

/* ------------------------------------------------------------- shared_preferences */

static pthread_mutex_t g_prefs_mu = PTHREAD_MUTEX_INITIALIZER;
static sv *g_prefs;                 /* a map of key to value */

static void prefs_path(char *out, size_t n) { snprintf(out, n, "%s/shared_prefs/FlutterSharedPreferences.smc", g_data); }

static void prefs_load(void)
{
    if (g_prefs) return;
    char p[700]; prefs_path(p, sizeof(p));
    FILE *f = fopen(p, "rb");
    if (f) {
        fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
        uint8_t *b = n > 0 ? malloc((size_t)n) : NULL;
        if (b && fread(b, 1, (size_t)n, f) == (size_t)n) g_prefs = sv_decode_message(b, (size_t)n);
        free(b); fclose(f);
    }
    if (!g_prefs || g_prefs->type != SV_MAP) { sv_free(g_prefs); g_prefs = sv_map(0); }
}
static void prefs_save(void)
{
    char p[700], dir[700]; prefs_path(p, sizeof(p));
    snprintf(dir, sizeof(dir), "%s/shared_prefs", g_data); mkdir(dir, 0755);
    size_t n; uint8_t *b = sv_encode_message(g_prefs, &n);
    char tmp[720]; snprintf(tmp, sizeof(tmp), "%s.tmp", p);
    FILE *f = fopen(tmp, "wb");
    if (f) { fwrite(b, 1, n, f); fclose(f); rename(tmp, p); }
    free(b);
}
static long prefs_find(const char *key)
{
    for (size_t i = 0; i < g_prefs->n; i++) if (g_prefs->items[i].type == SV_STRING && !strcmp(g_prefs->items[i].s, key)) return (long)i;
    return -1;
}
static void prefs_put(const char *key, const sv *val)
{
    long i = prefs_find(key);
    if (i >= 0) { sv_map_set(g_prefs, (size_t)i, sv_string(key), val ? sv_copy(val) : sv_null()); return; }
    /* grow: rebuild the map one larger */
    sv *m = sv_map(g_prefs->n + 1);
    for (size_t k = 0; k < g_prefs->n; k++) sv_map_set(m, k, sv_copy(&g_prefs->items[k]), sv_copy(&g_prefs->items[g_prefs->n + k]));
    sv_map_set(m, g_prefs->n, sv_string(key), val ? sv_copy(val) : sv_null());
    sv_free(g_prefs); g_prefs = m;
}
static bool prefs_del(const char *key)
{
    long at = prefs_find(key);
    if (at < 0) return false;
    sv *m = sv_map(g_prefs->n - 1);
    for (size_t k = 0, j = 0; k < g_prefs->n; k++) if ((long)k != at) { sv_map_set(m, j, sv_copy(&g_prefs->items[k]), sv_copy(&g_prefs->items[g_prefs->n + k])); j++; }
    sv_free(g_prefs); g_prefs = m;
    return true;
}
static bool allowed(const char *key, const char *prefix, const sv *allow)
{
    if (prefix && strncmp(key, prefix, strlen(prefix))) return false;
    if (!allow || allow->type != SV_LIST) return true;
    for (size_t i = 0; i < allow->n; i++) if (allow->items[i].type == SV_STRING && !strcmp(allow->items[i].s, key)) return true;
    return false;
}
static sv *prefs_all(const char *prefix, const sv *allow)
{
    size_t n = 0;
    for (size_t i = 0; i < g_prefs->n; i++) if (allowed(g_prefs->items[i].s, prefix, allow)) n++;
    sv *m = sv_map(n);
    for (size_t i = 0, j = 0; i < g_prefs->n; i++)
        if (allowed(g_prefs->items[i].s, prefix, allow)) { sv_map_set(m, j, sv_copy(&g_prefs->items[i]), sv_copy(&g_prefs->items[g_prefs->n + i])); j++; }
    return m;
}
static void prefs_clear(const char *prefix, const sv *allow)
{
    for (size_t i = g_prefs->n; i-- > 0;) if (allowed(g_prefs->items[i].s, prefix, allow)) prefs_del(g_prefs->items[i].s);
}

/* The Pigeon API (shared_preferences_android 2.2+): SharedPreferencesApi.<method>, and the newer SharedPreferencesAsyncApi. */
static uint8_t *prefs_pigeon(const char *method, const sv *args, size_t *len)
{
    const char *key = sv_str(sv_at(args, 0));
    const sv *val = sv_at(args, 1);
    uint8_t *r = NULL;
    pthread_mutex_lock(&g_prefs_mu);
    prefs_load();
    if (!strcmp(method, "getAll")) r = pigeon_ok(prefs_all(key /* prefix */, sv_at(args, 1)), len);
    else if (!strncmp(method, "set", 3) && key && val) { prefs_put(key, val); prefs_save(); r = pigeon_ok(sv_bool(true), len); }
    else if (!strcmp(method, "remove") && key) { bool ok = prefs_del(key); prefs_save(); r = pigeon_ok(sv_bool(ok), len); }
    else if (!strcmp(method, "clear")) { prefs_clear(key, sv_at(args, 1)); prefs_save(); r = pigeon_ok(sv_bool(true), len); }
    pthread_mutex_unlock(&g_prefs_mu);
    return r;
}

/* SharedPreferencesAsyncApi: getX(key, options), setX(key, value, options), getAll(allowList, options), getKeys(allowList, options), clear(allowList, options). */
static uint8_t *prefs_async(const char *method, const sv *args, size_t *len)
{
    uint8_t *r = NULL;
    pthread_mutex_lock(&g_prefs_mu);
    prefs_load();
    const char *key = sv_str(sv_at(args, 0));
    if (!strncmp(method, "get", 3) && strcmp(method, "getAll") && strcmp(method, "getKeys") && key) {
        long i = prefs_find(key);
        r = pigeon_ok(i >= 0 ? sv_copy(&g_prefs->items[g_prefs->n + (size_t)i]) : sv_null(), len);
    } else if (!strcmp(method, "getAll")) r = pigeon_ok(prefs_all(NULL, sv_at(args, 0)), len);
    else if (!strcmp(method, "getKeys")) {
        sv *all = prefs_all(NULL, sv_at(args, 0));
        sv *keys = sv_list(all->n);
        for (size_t i = 0; i < all->n; i++) sv_set(keys, i, sv_copy(&all->items[i]));
        sv_free(all);
        r = pigeon_ok(keys, len);
    } else if (!strncmp(method, "set", 3) && key && sv_at(args, 1)) { prefs_put(key, sv_at(args, 1)); prefs_save(); r = pigeon_ok(NULL, len); }
    else if (!strcmp(method, "clear")) { prefs_clear(NULL, sv_at(args, 0)); prefs_save(); r = pigeon_ok(NULL, len); }
    pthread_mutex_unlock(&g_prefs_mu);
    return r;
}

/* The method channel (older plugin versions): getAll, setBool/setInt/setDouble/setString/setStringList {key, value}, remove {key}, clear. */
static uint8_t *prefs_method(const char *method, const sv *args, size_t *len)
{
    uint8_t *r = NULL;
    pthread_mutex_lock(&g_prefs_mu);
    prefs_load();
    const char *key = sv_str(sv_get(args, "key"));
    const char *prefix = sv_str(sv_get(args, "prefix"));
    if (!strcmp(method, "getAll")) r = method_ok(prefs_all("flutter.", NULL), len);
    else if (!strcmp(method, "getAllWithPrefix") || !strcmp(method, "getAllWithParameters")) r = method_ok(prefs_all(prefix, sv_get(args, "allowList")), len);
    else if (!strncmp(method, "set", 3) && key) { prefs_put(key, sv_get(args, "value")); prefs_save(); r = method_ok(sv_bool(true), len); }
    else if (!strcmp(method, "remove") && key) { prefs_del(key); prefs_save(); r = method_ok(sv_bool(true), len); }
    else if (!strncmp(method, "clear", 5)) { prefs_clear(prefix ? prefix : "flutter.", sv_get(args, "allowList")); prefs_save(); r = method_ok(sv_bool(true), len); }
    pthread_mutex_unlock(&g_prefs_mu);
    return r;
}

/* ------------------------------------------------------------- the others */

static const char *dir_for(const char *m)
{
    if (strstr(m, "Temporary") || strstr(m, "Cache")) return "cache";
    if (strstr(m, "Support")) return "files";
    if (strstr(m, "Documents")) return "app_flutter";
    if (strstr(m, "External") || strstr(m, "Storage") || strstr(m, "Downloads")) return "sdcard";
    return NULL;
}
static sv *dir_value(const char *sub)
{
    char p[700]; snprintf(p, sizeof(p), "%s/%s", g_data, sub); mkdir(p, 0755);
    return sv_string(p);
}

static sv *package_info(void)
{
    sv *m = sv_map(6);
    sv_map_set(m, 0, sv_string("appName"), sv_string(g_label));
    sv_map_set(m, 1, sv_string("packageName"), sv_string(g_pkg));
    sv_map_set(m, 2, sv_string("version"), sv_string(g_version));
    sv_map_set(m, 3, sv_string("buildNumber"), sv_string(g_build));
    sv_map_set(m, 4, sv_string("buildSignature"), sv_string(""));
    sv_map_set(m, 5, sv_string("installerStore"), sv_null());
    return m;
}

static sv *str_list(const char *const *v, size_t n) { sv *l = sv_list(n); for (size_t i = 0; i < n; i++) sv_set(l, i, sv_string(v[i])); return l; }

static sv *device_info(void)
{
    static const char *const keys[] = { "brand", "device", "display", "fingerprint", "hardware", "host", "id", "manufacturer", "model",
                                        "product", "tags", "type", "bootloader", "board" };
    static const char *const vals[] = { "husk", "husk", "Husk", "husk/husk/husk:16/HUSK/1:user/release-keys", "husk", "husk", "HUSK",
                                        "Husk", "Husk", "husk", "release-keys", "user", "unknown", "husk" };
    size_t nk = sizeof(keys) / sizeof(keys[0]);
    sv *version = sv_map(7);
    sv_map_set(version, 0, sv_string("sdkInt"), sv_int(36));
    sv_map_set(version, 1, sv_string("release"), sv_string("16"));
    sv_map_set(version, 2, sv_string("codename"), sv_string("REL"));
    sv_map_set(version, 3, sv_string("incremental"), sv_string("1"));
    sv_map_set(version, 4, sv_string("securityPatch"), sv_string("2026-09-01"));
    sv_map_set(version, 5, sv_string("previewSdkInt"), sv_int(0));
    sv_map_set(version, 6, sv_string("baseOS"), sv_string(""));
    static const char *const abis[] = { "arm64-v8a" };
    /* disk and memory: the real ones, from the app's data volume and the device */
    struct statfs fs; int64_t disk_total = 0, disk_free = 0;
    if (statfs(g_data, &fs) == 0) { disk_total = (int64_t)fs.f_blocks * fs.f_bsize; disk_free = (int64_t)fs.f_bavail * fs.f_bsize; }
    int64_t ram = 0; size_t rl = sizeof(ram); sysctlbyname("hw.memsize", &ram, &rl, NULL, 0);
    struct timespec now; clock_gettime(CLOCK_REALTIME, &now);
    sv *m = sv_map(nk + 14);
    size_t i = 0;
    for (; i < nk; i++) sv_map_set(m, i, sv_string(keys[i]), sv_string(vals[i]));
    sv_map_set(m, i++, sv_string("version"), version);
    sv_map_set(m, i++, sv_string("isPhysicalDevice"), sv_bool(true));
    sv_map_set(m, i++, sv_string("supportedAbis"), str_list(abis, 1));
    sv_map_set(m, i++, sv_string("supported64BitAbis"), str_list(abis, 1));
    sv_map_set(m, i++, sv_string("supported32BitAbis"), sv_list(0));
    sv_map_set(m, i++, sv_string("systemFeatures"), sv_list(0));
    sv_map_set(m, i++, sv_string("isLowRamDevice"), sv_bool(false));
    sv_map_set(m, i++, sv_string("serialNumber"), sv_string("unknown"));
    sv_map_set(m, i++, sv_string("name"), sv_string("Husk"));
    sv_map_set(m, i++, sv_string("time"), sv_int((int64_t)now.tv_sec * 1000));
    sv_map_set(m, i++, sv_string("totalDiskSize"), sv_int(disk_total));
    sv_map_set(m, i++, sv_string("freeDiskSize"), sv_int(disk_free));
    sv_map_set(m, i++, sv_string("physicalRamSize"), sv_int(ram / (1024 * 1024)));
    sv_map_set(m, i++, sv_string("availableRamSize"), sv_int(ram / (2 * 1024 * 1024)));
    return m;
}

static void open_url(const char *url)
{
    tl_log_line("flutter: the app opens %s", url ? url : "(nothing)");
    if (g_open_url && url) g_open_url(url);
}

/* ------------------------------------------------------------- dispatch */

uint8_t *tl_flutter_plugin_message(const char *ch, const uint8_t *d, size_t len, size_t *rlen, bool *handled)
{
    *handled = true;
    char m[96]; sv *args = NULL; uint8_t *r = NULL;
    static const char PIG[] = "dev.flutter.pigeon.";

    if (!strncmp(ch, PIG, sizeof(PIG) - 1)) {
        const char *api = ch + sizeof(PIG) - 1, *method = strrchr(ch, '.') + 1;
        args = sv_decode_message(d, len);
        if (!strncmp(api, "shared_preferences_android.SharedPreferencesApi.", 48)) r = prefs_pigeon(method, args, rlen);
        else if (!strncmp(api, "shared_preferences_android.SharedPreferencesAsyncApi.", 53)) r = prefs_async(method, args, rlen);
        else if (!strncmp(api, "path_provider_android.PathProviderApi.", 38)) {
            const char *sub = dir_for(method);
            if (!strcmp(method, "getExternalStoragePaths")) { sv *l = sv_list(1); sv_set(l, 0, dir_value("sdcard")); r = pigeon_ok(l, rlen); }
            else r = pigeon_ok(sub ? dir_value(sub) : sv_null(), rlen);
        } else if (!strncmp(api, "url_launcher_android.UrlLauncherApi.", 36)) {
            if (!strcmp(method, "canLaunchUrl")) r = pigeon_ok(sv_bool(true), rlen);
            else if (!strncmp(method, "launchUrl", 9) || !strncmp(method, "openUrl", 7)) { open_url(sv_str(sv_at(args, 0))); r = pigeon_ok(sv_bool(true), rlen); }
            else if (!strcmp(method, "supportsCustomTabs")) r = pigeon_ok(sv_bool(false), rlen);
            else r = pigeon_ok(NULL, rlen);
        } else if (!strncmp(api, "package_info_plus.", 18)) r = pigeon_ok(package_info(), rlen);
    } else if (!strcmp(ch, "plugins.flutter.io/shared_preferences") || !strcmp(ch, "plugins.flutter.io/shared_preferences_android")) {
        if (method_call(d, len, m, sizeof(m), &args)) r = prefs_method(m, args, rlen);
    } else if (!strcmp(ch, "plugins.flutter.io/path_provider") || !strcmp(ch, "plugins.flutter.io/path_provider_android")) {
        if (method_call(d, len, m, sizeof(m), &args)) {
            const char *sub = dir_for(m);
            r = method_ok(sub ? dir_value(sub) : sv_null(), rlen);
        }
    } else if (!strcmp(ch, "dev.fluttercommunity.plus/package_info") || !strcmp(ch, "plugins.flutter.io/package_info")) {
        if (method_call(d, len, m, sizeof(m), &args)) r = method_ok(package_info(), rlen);
    } else if (!strcmp(ch, "dev.fluttercommunity.plus/device_info") || !strcmp(ch, "plugins.flutter.io/device_info")) {
        if (method_call(d, len, m, sizeof(m), &args)) r = method_ok(device_info(), rlen);
    } else if (!strcmp(ch, "plugins.flutter.io/url_launcher") || !strcmp(ch, "plugins.flutter.io/url_launcher_android")) {
        if (method_call(d, len, m, sizeof(m), &args)) {
            if (!strcmp(m, "canLaunch")) r = method_ok(sv_bool(true), rlen);
            else if (!strcmp(m, "launch")) { open_url(sv_str(sv_get(args, "url"))); r = method_ok(sv_bool(true), rlen); }
            else r = method_ok(sv_null(), rlen);
        }
    } else if (!strcmp(ch, "dev.fluttercommunity.plus/connectivity") || !strcmp(ch, "plugins.flutter.io/connectivity")) {
        if (method_call(d, len, m, sizeof(m), &args)) {
            static const char *const wifi[] = { "wifi" };
            r = method_ok(!strcmp(ch, "plugins.flutter.io/connectivity") ? sv_string("wifi") : str_list(wifi, 1), rlen);
        }
    } else *handled = false;

    sv_free(args);
    if (*handled && !r) { *handled = true; return NULL; }      /* recognised, no answer: an empty reply */
    return r;
}
