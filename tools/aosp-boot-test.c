/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Boot-path construction and ServiceManager slice for Husk 3.3 AOSP. */
#include "husk-tl-aosp.h"
#include "husk-tl-binder.h"
#include "husk-tl-servicemanager.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdarg.h>
#include <unistd.h>
#include <sys/stat.h>

void tl_log_line(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    fputc('\n', stderr);
    va_end(ap);
}

static void touch(const char *p)
{
    char dir[1200];
    snprintf(dir, sizeof(dir), "%s", p);
    char *slash = strrchr(dir, '/');
    if (slash) { *slash = 0; mkdir(dir, 0755); }
    FILE *f = fopen(p, "w");
    if (f) fclose(f);
}

int main(void)
{
    char tmp[] = "/tmp/husk-aosp-boot-XXXXXX";
    if (!mkdtemp(tmp)) { perror("mkdtemp"); return 1; }

    char paths[20][1200];
    const char *boot[20];
    int nb = husk_aosp_boot_jars(tmp, paths, 20, boot);
    if (nb != 0) { fprintf(stderr, "empty root should add nothing, got %d\n", nb); return 1; }
    if (husk_aosp_present(tmp)) { fprintf(stderr, "present on empty root\n"); return 1; }

    char fw[1400], husk[1400];
    snprintf(husk, sizeof(husk), "%s/husk-framework.dex", tmp);
    touch(husk);
    nb = husk_aosp_boot_jars(tmp, paths, 20, boot);
    int saw_husk = 0, saw_fw = 0, husk_i = -1, fw_i = -1;
    for (int i = 0; i < nb; i++) {
        if (strstr(boot[i], "husk-framework.dex")) { saw_husk = 1; husk_i = i; }
        if (strstr(boot[i], "framework.jar")) { saw_fw = 1; fw_i = i; }
    }
    if (!saw_husk || saw_fw) { fprintf(stderr, "fallback path wrong (husk=%d fw=%d)\n", saw_husk, saw_fw); return 1; }
    if (husk_aosp_using()) { fprintf(stderr, "using AOSP without framework.jar\n"); return 1; }

    snprintf(fw, sizeof(fw), "%s/aosp/framework.jar", tmp);
    { char d[1400]; snprintf(d, sizeof(d), "%s/aosp", tmp); mkdir(d, 0755); }
    touch(fw);
    nb = husk_aosp_boot_jars(tmp, paths, 20, boot);
    saw_husk = saw_fw = 0;
    husk_i = fw_i = -1;
    for (int i = 0; i < nb; i++) {
        if (strstr(boot[i], "husk-framework.dex")) { saw_husk = 1; husk_i = i; }
        if (strstr(boot[i], "framework.jar")) { saw_fw = 1; fw_i = i; }
    }
    if (!saw_husk || !saw_fw || husk_i > fw_i || !husk_aosp_using()) {
        fprintf(stderr, "AOSP fill path wrong (husk=%d@%d fw=%d@%d using=%d)\n",
                saw_husk, husk_i, saw_fw, fw_i, husk_aosp_using());
        return 1;
    }

    char corep[1400];
    snprintf(corep, sizeof(corep), "%s/husk-framework-core.dex", tmp);
    touch(corep);
    nb = husk_aosp_boot_jars(tmp, paths, 20, boot);
    int saw_core = 0, saw_fat = 0;
    for (int i = 0; i < nb; i++) {
        if (strstr(boot[i], "husk-framework-core.dex")) saw_core = 1;
        else if (strstr(boot[i], "husk-framework.dex")) saw_fat = 1;
        if (strstr(boot[i], "framework.jar")) saw_fw = 1;
    }
    if (!saw_core || saw_fat || !saw_fw) {
        fprintf(stderr, "core+AOSP path wrong (core=%d fat=%d fw=%d)\n", saw_core, saw_fat, saw_fw);
        return 1;
    }

    husk_aosp_set_session("com.example.app", 720, 1280);
    husk_aosp_start_system();
    if (!husk_sm_get("activity") || !husk_sm_get("package") || !husk_sm_get("window")) {
        fprintf(stderr, "system_server slice missing activity/package/window\n");
        return 1;
    }

    uint32_t act = husk_sm_get("activity");
    husk_binder_reply r = {0};
    if (husk_binder_transact(act, 0x5f4e5446u /* INTERFACE */, 0, NULL, 0, NULL, 0, &r) != 0 || r.data_size < 8) {
        fprintf(stderr, "INTERFACE transact failed size=%zu\n", r.data_size);
        return 1;
    }
    int32_t nchars = 0;
    memcpy(&nchars, r.data, 4);
    char desc[80];
    int i;
    for (i = 0; i < nchars && i < 79 && 4 + i * 2 + 1 < (int)r.data_size; i++) {
        uint16_t c;
        memcpy(&c, r.data + 4 + i * 2, 2);
        desc[i] = (char)c;
    }
    desc[i] = 0;
    husk_binder_reply_free(&r);
    if (strcmp(desc, "android.app.IActivityManager") != 0) {
        fprintf(stderr, "INTERFACE descriptor '%s'\n", desc);
        return 1;
    }
    if (husk_binder_transact(act, 0x5f504e47u /* PING */, 0, NULL, 0, NULL, 0, &r) != 0) {
        fprintf(stderr, "PING transact failed\n");
        return 1;
    }
    husk_binder_reply_free(&r);

    uint32_t win = husk_sm_get("window");
    if (husk_binder_transact(win, 1, 0, NULL, 0, NULL, 0, &r) != 0 || r.data_size < 16) {
        fprintf(stderr, "window size reply short %zu\n", r.data_size);
        return 1;
    }
    int32_t ex = 0, present = 0, w = 0, h = 0;
    memcpy(&ex, r.data, 4);
    memcpy(&present, r.data + 4, 4);
    memcpy(&w, r.data + 8, 4);
    memcpy(&h, r.data + 12, 4);
    husk_binder_reply_free(&r);
    if (ex != 0 || present != 1 || w != 720 || h != 1280) {
        fprintf(stderr, "window size %d %d %dx%d\n", ex, present, w, h);
        return 1;
    }

    const char *real = getenv("TL_AOSP_FRAMEWORK");
    if (!real) real = "/Volumes/GTAV/husk2/root/system/framework/framework.jar";
    if (access(real, R_OK) == 0) {
        printf("aosp: framework.jar present at %s (Activity load needs the Dalvik harness)\n", real);
    }

    printf("aosp: boot path + system_server slice ok\n");
    return 0;
}
