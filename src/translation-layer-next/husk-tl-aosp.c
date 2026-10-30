/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Optional AOSP framework.jar on the boot class path, plus a native slice of
 * system_server (ServiceManager + the named services ActivityThread looks up).
 */
#include "husk-tl-aosp.h"
#include "husk-tl-binder.h"
#include "husk-tl-servicemanager.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void tl_log_line(const char *fmt, ...);

static bool g_using;

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

static void svc_txn(uint32_t code, uint32_t flags, const uint8_t *data, size_t data_size,
                    const uint64_t *offsets, size_t n_offsets, husk_binder_reply *reply, void *cookie)
{
    (void)code; (void)flags; (void)data; (void)data_size; (void)offsets; (void)n_offsets; (void)cookie;
    /* writeNoException + a run of zeros: readException succeeds; typed nulls are writeInt(0). */
    size_t n = 64;
    uint8_t *buf = calloc(1, n);
    reply->data = buf;
    reply->data_size = n;
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
        "input_method", "alarm", "power", "batterystats", "clipboard",
        "notification", "connectivity", "wifi", "audio", "sensorservice",
        "SurfaceFlinger", "permission", "permissionmgr", "mount", "user",
        "account", "device_policy", "lock_settings", "graphicsstats",
        "appops", "jobscheduler", "location", "phone", "telecom",
        "content", "settings", "deviceidle", "vibrator", "uimode",
        "usagestats", "backup", "media.audio_flinger", "media.player",
        NULL
    };
    for (int i = 0; names[i]; i++) add_svc(names[i]);
    tl_log_line("aosp: native system_server slice is up");
}
