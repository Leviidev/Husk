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
    float anisotropy;
    bool  trilinear;
} tl_visual_params;

static inline int tl_visual_clamp_preset(int preset)
{
    if (preset <= TL_VISUAL_STANDARD) return TL_VISUAL_STANDARD;
    if (preset >= TL_VISUAL_PRO_MAX) return TL_VISUAL_PRO_MAX;
    return preset;
}

/* Compute compositor sharpening, OLED micro-contrast, Display P3 adaptive
 * vibrance, and hardware anisotropic filtering parameters for a frame. */
static inline tl_visual_params tl_visual_compute_params(int preset, bool wide_color,
                                                        int src_w, int src_h, int dst_w, int dst_h)
{
    tl_visual_params out = { 0.0f, 0.0f, 0.0f, 1.0f, false };
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
        float sharp = 0.38f + extra_upscale * 0.40f;
        out.sharpness = sharp > 0.68f ? 0.68f : sharp;
        out.contrast = 0.14f;
        out.vibrance = wide_color ? 0.08f : 0.14f;
        out.anisotropy = 4.0f;
        out.trilinear = true;
        return out;
    }

    float sharp = 0.58f + extra_upscale * 0.45f;
    out.sharpness = sharp > 0.88f ? 0.88f : sharp;
    out.contrast = 0.24f;
    out.vibrance = wide_color ? 0.14f : 0.22f;
    out.anisotropy = 8.0f;
    out.trilinear = true;
    return out;
}

static inline float tl_visual_clamp_anisotropy(float requested, float max_supported)
{
    if (requested <= 1.0f || max_supported <= 1.0f) return 1.0f;
    return requested < max_supported ? requested : max_supported;
}

#endif
