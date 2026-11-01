/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Optional AOSP framework.jar on the boot class path, plus a native slice of
 * system_server (ServiceManager + the named services ActivityThread looks up).
 *
 * Binder replies used to be 64 zeros. AOSP stubs then read a 0-length interface
 * descriptor, an empty PackageInfo, and "screen off". Ping, INTERFACE, and the
 * usual AMS/PMS/WMS/power answers are filled in so those classes construct.
 */
#include "husk-tl-aosp.h"
#include "husk-tl-binder.h"
#include "husk-tl-servicemanager.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void tl_log_line(const char *fmt, ...);

#define PING_TRANSACTION 0x5f504e47u
#define DUMP_TRANSACTION 0x5f44554du
#define SHELL_COMMAND_TRANSACTION 0x5f434d44u
#define INTERFACE_TRANSACTION 0x5f4e5446u

static bool g_using;
static char g_pkg[200];
static int g_width = 1080, g_height = 1920;

static bool readable(const char *p) { return p && p[0] && access(p, R_OK) == 0; }

const char *husk_aosp_framework_path(const char *java_root, char *out, int cap)
{
    if (!java_root || !out || cap < 8) return NULL;
    snprintf(out, (size_t)cap, "%s/aosp/framework.jar", java_root);
    if (readable(out)) return out;
    snprintf(out, (size_t)cap, "%s/framework.jar", java_root);
    if (readable(out)) return out;
    out[0] = 0;
    return NULL;
}

bool husk_aosp_present(const char *java_root)
{
    char p[1200];
    return husk_aosp_framework_path(java_root, p, sizeof(p)) != NULL;
}

bool husk_aosp_using(void) { return g_using; }

void husk_aosp_set_session(const char *package, int width, int height)
{
    if (package && package[0]) snprintf(g_pkg, sizeof(g_pkg), "%s", package);
    if (width > 0) g_width = width;
    if (height > 0) g_height = height;
}

static int add_if(const char *path, char (*paths)[1200], int max, const char **boot, int nb)
{
    if (nb >= max || !readable(path)) return nb;
    snprintf(paths[nb], 1200, "%s", path);
    boot[nb] = paths[nb];
    return nb + 1;
}

int husk_aosp_boot_jars(const char *java_root, char (*paths)[1200], int max, const char **boot)
{
    int nb = 0;
    if (!java_root || !paths || !boot || max < 12) return 0;
    const char *r = java_root;
    char p[1200];
    static const char *const art[] = { "core-oj", "core-libart", "okhttp", "bouncycastle", "apache-xml" };
    for (int i = 0; i < 5; i++) {
        snprintf(p, sizeof(p), "%s/com.android.art/javalib/%s.jar", r, art[i]);
        nb = add_if(p, paths, max, boot, nb);
    }
    snprintf(p, sizeof(p), "%s/com.android.i18n/javalib/core-icu4j.jar", r);
    nb = add_if(p, paths, max, boot, nb);
    snprintf(p, sizeof(p), "%s/com.android.conscrypt/javalib/conscrypt.jar", r);
    nb = add_if(p, paths, max, boot, nb);

    /* Handwritten Activity/View/Resources first. If AOSP is present, use the
     * core dex (no generated empty stubs) so framework.jar supplies those APIs. */
    bool aosp = husk_aosp_present(r);
    bool core = false;
    snprintf(p, sizeof(p), "%s/husk-framework-core.dex", r);
    if (aosp && readable(p)) {
        nb = add_if(p, paths, max, boot, nb);
        core = true;
        tl_log_line("aosp: husk-framework-core.dex + framework.jar (generated stubs omitted)");
    } else {
        snprintf(p, sizeof(p), "%s/husk-framework.dex", r);
        nb = add_if(p, paths, max, boot, nb);
    }

    if (aosp) {
        static const char *const fw[] = {
            "aosp/framework.jar", "framework.jar",
            "aosp/framework-graphics.jar", "framework-graphics.jar",
            "aosp/ext.jar", "ext.jar",
            "aosp/telephony-common.jar", "telephony-common.jar",
            "aosp/voip-common.jar", "voip-common.jar",
            "aosp/ims-common.jar", "ims-common.jar",
            "aosp/services.jar", "services.jar",
            "aosp/framework-connectivity.jar", "framework-connectivity.jar",
            "aosp/tethering.jar", "tethering.jar",
        };
        for (int i = 0; i < (int)(sizeof(fw) / sizeof(fw[0])); i++) {
            snprintf(p, sizeof(p), "%s/%s", r, fw[i]);
            nb = add_if(p, paths, max, boot, nb);
        }
        g_using = true;
        if (!core) tl_log_line("aosp: framework.jar fills classes the full husk-framework.dex does not define");
    } else {
        g_using = false;
    }
    snprintf(p, sizeof(p), "%s/org.apache.http.legacy.jar", r);
    nb = add_if(p, paths, max, boot, nb);
    return nb;
}

typedef struct {
    uint8_t *p;
    size_t n, cap;
} pw;

static void pw_need(pw *w, size_t extra)
{
    if (w->n + extra <= w->cap) return;
    size_t c = w->cap ? w->cap * 2 : 128;
    while (c < w->n + extra) c *= 2;
    uint8_t *q = realloc(w->p, c);
    if (!q) return;
    memset(q + w->cap, 0, c - w->cap);
    w->p = q;
    w->cap = c;
}

static void pw_i32(pw *w, int32_t v)
{
    pw_need(w, 4);
    if (w->n + 4 > w->cap) return;
    memcpy(w->p + w->n, &v, 4);
    w->n += 4;
}

static void pw_str16(pw *w, const char *s)
{
    if (!s) {
        pw_i32(w, -1);
        return;
    }
    int32_t len = (int32_t)strlen(s);
    pw_i32(w, len);
    size_t bytes = (size_t)(len + 1) * 2;
    size_t pad = (bytes + 3) & ~(size_t)3;
    pw_need(w, pad);
    if (w->n + pad > w->cap) return;
    for (int32_t i = 0; i <= len; i++) {
        uint16_t c = (uint16_t)(unsigned char)s[i];
        memcpy(w->p + w->n + (size_t)i * 2, &c, 2);
    }
    memset(w->p + w->n + bytes, 0, pad - bytes);
    w->n += pad;
}

static void pw_noex(pw *w) { pw_i32(w, 0); }

static const char *iface_for(const char *name)
{
    if (!name) return "android.os.IBinder";
    if (!strcmp(name, "activity")) return "android.app.IActivityManager";
    if (!strcmp(name, "activity_task")) return "android.app.IActivityTaskManager";
    if (!strcmp(name, "package")) return "android.content.pm.IPackageManager";
    if (!strcmp(name, "window")) return "android.view.IWindowManager";
    if (!strcmp(name, "display")) return "android.hardware.display.IDisplayManager";
    if (!strcmp(name, "input")) return "android.hardware.input.IInputManager";
    if (!strcmp(name, "input_method")) return "com.android.internal.view.IInputMethodManager";
    if (!strcmp(name, "alarm")) return "android.app.IAlarmManager";
    if (!strcmp(name, "power")) return "android.os.IPowerManager";
    if (!strcmp(name, "clipboard")) return "android.content.IClipboard";
    if (!strcmp(name, "notification")) return "android.app.INotificationManager";
    if (!strcmp(name, "connectivity")) return "android.net.IConnectivityManager";
    if (!strcmp(name, "wifi")) return "android.net.wifi.IWifiManager";
    if (!strcmp(name, "audio")) return "android.media.IAudioService";
    if (!strcmp(name, "sensorservice")) return "android.gui.SensorServer";
    if (!strcmp(name, "SurfaceFlinger")) return "android.ui.ISurfaceComposer";
    if (!strcmp(name, "permission") || !strcmp(name, "permissionmgr")) return "android.permission.IPermissionManager";
    if (!strcmp(name, "mount")) return "android.os.storage.IStorageManager";
    if (!strcmp(name, "user")) return "android.os.IUserManager";
    if (!strcmp(name, "account")) return "android.accounts.IAccountManager";
    if (!strcmp(name, "appops")) return "com.android.internal.app.IAppOpsService";
    if (!strcmp(name, "jobscheduler")) return "android.app.job.IJobScheduler";
    if (!strcmp(name, "location")) return "android.location.ILocationManager";
    if (!strcmp(name, "phone")) return "com.android.internal.telephony.ITelephony";
    if (!strcmp(name, "content")) return "android.content.IContentService";
    if (!strcmp(name, "battery") || !strcmp(name, "batterystats")) return "android.os.IBatteryPropertiesRegistrar";
    if (!strcmp(name, "vibrator")) return "android.os.IVibratorService";
    if (!strcmp(name, "uimode")) return "android.app.IUiModeManager";
    if (!strcmp(name, "usagestats")) return "android.app.usage.IUsageStatsManager";
    if (!strcmp(name, "deviceidle")) return "android.os.IDeviceIdleController";
    if (!strcmp(name, "media.audio_flinger")) return "android.media.IAudioFlinger";
    if (!strcmp(name, "media.player")) return "android.media.IMediaPlayerService";
    return "android.os.IBinder";
}

static void fill_service(pw *w, const char *name, uint32_t code)
{
    pw_noex(w);
    if (!name) {
        pw_i32(w, 0);
        return;
    }
    /* Screen on; permission checks GRANTED (0); user 0; no parcelable payload. */
    if (!strcmp(name, "power")) {
        pw_i32(w, 1);
        return;
    }
    if (!strcmp(name, "package") || !strcmp(name, "permission") || !strcmp(name, "permissionmgr") ||
        !strcmp(name, "appops")) {
        pw_i32(w, 0);
        return;
    }
    if (!strcmp(name, "user")) {
        pw_i32(w, 0);
        return;
    }
    if (!strcmp(name, "window") || !strcmp(name, "display")) {
        pw_i32(w, 1); /* Point present / typed object */
        pw_i32(w, g_width);
        pw_i32(w, g_height);
        (void)code;
        return;
    }
    if (!strcmp(name, "activity") || !strcmp(name, "activity_task")) {
        pw_i32(w, 0);
        pw_str16(w, g_pkg[0] ? g_pkg : "android");
        return;
    }
    pw_i32(w, 0);
}

static void svc_txn(uint32_t code, uint32_t flags, const uint8_t *data, size_t data_size,
                    const uint64_t *offsets, size_t n_offsets, husk_binder_reply *reply, void *cookie)
{
    (void)flags;
    (void)data;
    (void)data_size;
    (void)offsets;
    (void)n_offsets;
    const char *name = cookie;
    pw w = {0};
    if (code == PING_TRANSACTION || code == DUMP_TRANSACTION || code == SHELL_COMMAND_TRANSACTION) {
        /* empty success: Binder.onTransact returns true with no payload */
    } else if (code == INTERFACE_TRANSACTION) {
        pw_str16(&w, iface_for(name));
    } else {
        fill_service(&w, name, code);
    }
    reply->data = w.p;
    reply->data_size = w.n;
    reply->offsets = NULL;
    reply->n_offsets = 0;
}

static void add_svc(const char *name)
{
    uint32_t h = husk_binder_add_local(svc_txn, (void *)name);
    if (h) husk_sm_add(name, h);
}

void husk_aosp_start_system(void)
{
    husk_sm_start();
    static const char *const names[] = {
        "activity", "activity_task", "package", "window", "display", "input",
        "input_method", "alarm", "power", "batterystats", "battery", "clipboard",
        "notification", "connectivity", "wifi", "audio", "sensorservice",
        "SurfaceFlinger", "permission", "permissionmgr", "mount", "user",
        "account", "device_policy", "lock_settings", "graphicsstats",
        "appops", "jobscheduler", "location", "phone", "telecom",
        "content", "settings", "deviceidle", "vibrator", "uimode",
        "usagestats", "backup", "media.audio_flinger", "media.player",
        "media.camera", "webviewupdate", "package_native", "overlay",
        "statusbar", "accessibility", "wallpaper", "search", "country_detector",
        "network_management", "netpolicy", "storagestats", "usb", "serial",
        NULL
    };
    for (int i = 0; names[i]; i++) add_svc(names[i]);
    tl_log_line("aosp: native system_server slice is up");
}
