/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef HUSK_TL_VISUAL_H
#define HUSK_TL_VISUAL_H

#include <stdbool.h>
#include <stdint.h>

enum {
    TL_VISUAL_STANDARD = 0,
    TL_VISUAL_CRISP    = 1,
    TL_VISUAL_PRO_MAX  = 2,
};

typedef struct {
    float sharpness;
    float contrast;
    float vibrance;
} tl_visual_params;

static inline int tl_visual_clamp_preset(int preset)
{
    if (preset <= TL_VISUAL_STANDARD) return TL_VISUAL_STANDARD;
    if (preset >= TL_VISUAL_PRO_MAX) return TL_VISUAL_PRO_MAX;
    return preset;
}

/* Compute display-only enhancement. Texture filtering remains the game's
 * decision, independent of these compositor sharpening and color presets. */
static inline tl_visual_params tl_visual_compute_params(int preset, bool wide_color,
                                                        int src_w, int src_h, int dst_w, int dst_h)
{
    tl_visual_params out = { 0.0f, 0.0f, 0.0f };
    int mode = tl_visual_clamp_preset(preset);
    if (mode == TL_VISUAL_STANDARD) return out;

    float upscale = 1.0f;
    if (src_w > 0 && src_h > 0 && dst_w > 0 && dst_h > 0) {
        float sx = (float)dst_w / (float)src_w;
        float sy = (float)dst_h / (float)src_h;
        upscale = sx > sy ? sx : sy;
        if (upscale < 1.0f) upscale = 1.0f;
    }
    float extra_upscale = upscale > 1.02f ? (upscale - 1.0f) : 0.0f;

    if (mode == TL_VISUAL_CRISP) {
        float sharp = 0.34f + extra_upscale * 0.16f;
        out.sharpness = sharp > 0.50f ? 0.50f : sharp;
        out.contrast = 0.16f;
        out.vibrance = wide_color ? 0.12f : 0.14f;
        return out;
    }

    float sharp = 0.56f + extra_upscale * 0.22f;
    out.sharpness = sharp > 0.78f ? 0.78f : sharp;
    out.contrast = 0.30f;
    out.vibrance = wide_color ? 0.26f : 0.30f;
    return out;
}

/* Step back fragment shader cost proactively under thermal pressure so GPU power
 * drops before iOS .serious throttling dims the display and cuts CPU clocks. */
static inline tl_visual_params tl_visual_compute_params_thermal(int preset, int thermal_state,
                                                                bool wide_color, int src_w, int src_h,
                                                                int dst_w, int dst_h)
{
    tl_visual_params out = tl_visual_compute_params(preset, wide_color, src_w, src_h, dst_w, dst_h);
    if (thermal_state <= 0) return out;
    if (thermal_state == 1) {
        out.sharpness *= 0.55f;
        if (out.sharpness > 0.40f) out.sharpness = 0.40f;
        out.contrast *= 0.70f;
        out.vibrance *= 0.75f;
        return out;
    }
    out.sharpness = 0.0f;
    out.contrast *= 0.35f;
    out.vibrance *= 0.45f;
    return out;
}

#endif
