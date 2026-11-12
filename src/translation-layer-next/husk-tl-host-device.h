/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef HUSK_TL_HOST_DEVICE_H
#define HUSK_TL_HOST_DEVICE_H
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* Device-class facts only. Never collect a serial number, UDID, account or
 * advertising identifier. Android SDK/ABI values still describe the runtime. */
typedef struct {
    char model[64], board[64], soc[92], fingerprint[92], display[92];
} tl_host_device;

static inline void tl_host_device_describe(tl_host_device *out, const char *machine,
                                         const char *board, const char *cpu_brand,
                                         uint32_t cpu_family)
{
    memset(out, 0, sizeof(*out));
    const char *model = machine && *machine ? machine : "Apple device";
    /* hw.machine is the architecture on macOS, where hw.model names the Mac. */
    if ((!strcmp(model, "arm64") || !strcmp(model, "x86_64")) && board && *board) model = board;
    snprintf(out->model, sizeof(out->model), "%s", model);
    snprintf(out->board, sizeof(out->board), "%s", board && *board ? board : out->model);
    if (cpu_brand && *cpu_brand) snprintf(out->soc, sizeof(out->soc), "%s", cpu_brand);
    else if (cpu_family) snprintf(out->soc, sizeof(out->soc), "Apple ARM64 family 0x%08x", (unsigned)cpu_family);
    else snprintf(out->soc, sizeof(out->soc), "Apple ARM64");
    /* This is a Husk Android-API compatibility build, running on real Apple
     * hardware. It is not an Android manufacturer's firmware identity. */
    snprintf(out->fingerprint, sizeof(out->fingerprint), "apple/%.18s/%.18s:14/HUSK/11000000:user/release-keys", out->model, out->model);
    snprintf(out->display, sizeof(out->display), "Husk on %s", out->model);
}

static inline const char *tl_host_device_lookup(const tl_host_device *info, const char *name)
{
    if (!name) return NULL;
    if (!strcmp(name, "ro.product.manufacturer") || !strcmp(name, "ro.soc.manufacturer")) return "Apple";
    if (!strcmp(name, "ro.product.brand")) return "apple";
    if (!strcmp(name, "ro.product.model") || !strcmp(name, "ro.product.name") || !strcmp(name, "ro.product.device")) return info->model;
    if (!strcmp(name, "ro.product.board") || !strcmp(name, "ro.hardware") || !strcmp(name, "ro.board.platform")
        || !strcmp(name, "ro.boot.hardware.sku") || !strcmp(name, "ro.boot.product.hardware.sku")) return info->board;
    if (!strcmp(name, "ro.soc.model")) return info->soc;
    if (!strcmp(name, "ro.build.fingerprint") || !strcmp(name, "ro.vendor.build.fingerprint")) return info->fingerprint;
    if (!strcmp(name, "ro.build.id")) return "HUSK";
    if (!strcmp(name, "ro.build.display.id")) return info->display;
    return NULL;
}

const char *tl_host_device_property(const char *name);
/* Actual host pixels per logical point/dp, set before launching the game. */
void tl_host_set_display_density(float pixels_per_dp);
#endif
