/* SPDX-License-Identifier: GPL-2.0-or-later */
#include <assert.h>
#include <math.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "husk-tl-visual.h"

static float fake_max_af = 16.0f;
static float probe_max_anisotropy(void) { return fake_max_af; }

#include "visual_quality_under_test.h"

static unsigned param_i_calls, param_f_calls;
static unsigned last_pname_i, last_pname_f;
static int last_val_i;
static float last_val_f;

static void fake_set_i(unsigned obj, unsigned pname, int param)
{
    (void)obj;
    param_i_calls++;
    last_pname_i = pname;
    last_val_i = param;
}

static void fake_set_f(unsigned obj, unsigned pname, float param)
{
    (void)obj;
    param_f_calls++;
    last_pname_f = pname;
    last_val_f = param;
}

static void reset_calls(void)
{
    param_i_calls = param_f_calls = 0;
    last_pname_i = last_pname_f = 0;
    last_val_i = 0;
    last_val_f = 0.0f;
}

int main(void)
{
    /* Host visual preset storage defaults to Pro Max and clamps invalid values. */
    assert(tl_host_visual_quality() == TL_VISUAL_PRO_MAX);
    tl_host_set_visual_quality(-99);
    assert(tl_host_visual_quality() == TL_VISUAL_STANDARD);
    tl_host_set_visual_quality(99);
    assert(tl_host_visual_quality() == TL_VISUAL_PRO_MAX);

    /* Preset parameter scaling across 1:1 native vs upscaled and sRGB vs Display P3. */
    tl_visual_params stdp = tl_visual_compute_params(TL_VISUAL_STANDARD, true, 1568, 724, 2796, 1290);
    assert(stdp.sharpness == 0.0f && stdp.contrast == 0.0f && stdp.vibrance == 0.0f && stdp.anisotropy == 1.0f && !stdp.trilinear);

    tl_visual_params crisp = tl_visual_compute_params(TL_VISUAL_CRISP, true, 2796, 1290, 2796, 1290);
    assert(fabsf(crisp.sharpness - 0.38f) < 1e-5f && crisp.anisotropy == 4.0f && crisp.trilinear);

    tl_visual_params pro_native = tl_visual_compute_params(TL_VISUAL_PRO_MAX, true, 2796, 1290, 2796, 1290);
    tl_visual_params pro_upscaled = tl_visual_compute_params(TL_VISUAL_PRO_MAX, true, 1568, 724, 2796, 1290);
    tl_visual_params pro_srgb = tl_visual_compute_params(TL_VISUAL_PRO_MAX, false, 2796, 1290, 2796, 1290);
    assert(pro_upscaled.sharpness > pro_native.sharpness && pro_upscaled.sharpness <= 0.88f);
    assert(pro_native.contrast > crisp.contrast && pro_native.anisotropy == 8.0f);
    assert(pro_srgb.vibrance > pro_native.vibrance);

    /* Pro Max upgrades bilinear mipmap filter to trilinear and applies capped 8x AF on mipmapped textures. */
    tl_host_set_visual_quality(TL_VISUAL_PRO_MAX);
    fake_max_af = 16.0f;
    reset_calls();
    apply_filter_param_i(0x0DE1, is_aniso_tex_target(0x0DE1), 0x2801, 0x2701, fake_set_i, fake_set_f);
    assert(param_i_calls == 1 && last_pname_i == 0x2801 && last_val_i == 0x2703);
    assert(param_f_calls == 1 && last_pname_f == 0x84FE && last_val_f == 8.0f);

    /* Hardware AF ceiling (4x) clamps 8x cleanly without exceeding limit. */
    fake_max_af = 4.0f;
    reset_calls();
    apply_filter_param_i(0x0DE1, is_aniso_tex_target(0x0DE1), 0x2801, 0x2703, fake_set_i, fake_set_f);
    assert(last_val_i == 0x2703 && param_f_calls == 1 && last_val_f == 4.0f);

    /* Absent AF extension (1.0x) never emits 0x84FE calls. */
    fake_max_af = 1.0f;
    reset_calls();
    apply_filter_param_i(0x0DE1, is_aniso_tex_target(0x0DE1), 0x2801, 0x2701, fake_set_i, fake_set_f);
    assert(last_val_i == 0x2703 && param_f_calls == 0);

    /* Non-mipmap filters (GL_NEAREST, GL_LINEAR) and non-eligible targets never gain AF. */
    fake_max_af = 16.0f;
    reset_calls();
    apply_filter_param_i(0x0DE1, is_aniso_tex_target(0x0DE1), 0x2801, 0x2601, fake_set_i, fake_set_f);
    assert(last_val_i == 0x2601 && param_f_calls == 0);
    reset_calls();
    apply_filter_param_i(0x8513, is_aniso_tex_target(0x8513), 0x2801, 0x2701, fake_set_i, fake_set_f);
    assert(last_val_i == 0x2701 && param_f_calls == 0);

    /* Explicit low AF from guest is raised to target, higher AF is preserved. */
    reset_calls();
    apply_filter_param_f(0x0DE1, true, 0x84FE, 2.0f, fake_set_f);
    assert(param_f_calls == 1 && last_val_f == 8.0f);
    reset_calls();
    apply_filter_param_f(0x0DE1, true, 0x84FE, 16.0f, fake_set_f);
    assert(param_f_calls == 1 && last_val_f == 16.0f);

    /* Standard preset passes original min filter and AF untouched. */
    tl_host_set_visual_quality(TL_VISUAL_STANDARD);
    reset_calls();
    apply_filter_param_i(0x0DE1, is_aniso_tex_target(0x0DE1), 0x2801, 0x2701, fake_set_i, fake_set_f);
    assert(last_val_i == 0x2701 && param_f_calls == 0);

    puts("PASS: Pro Max visual preset defaults, RCAS upscale response, trilinear promotion, and capped hardware AF contracts hold");
    return 0;
}
