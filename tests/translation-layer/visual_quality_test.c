/* SPDX-License-Identifier: GPL-2.0-or-later */
#include <assert.h>
#include <math.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "husk-tl-visual.h"

#include "visual_quality_under_test.h"

static unsigned param_i_calls, param_f_calls;
static unsigned last_pname_i, last_pname_f;
static unsigned last_obj_i, last_obj_f;
static int last_val_i;
static float last_val_f;

static void fake_set_i(unsigned obj, unsigned pname, int param)
{
    last_obj_i = obj;
    param_i_calls++;
    last_pname_i = pname;
    last_val_i = param;
}

static void fake_set_f(unsigned obj, unsigned pname, float param)
{
    last_obj_f = obj;
    param_f_calls++;
    last_pname_f = pname;
    last_val_f = param;
}

static void reset_calls(void)
{
    param_i_calls = param_f_calls = 0;
    last_pname_i = last_pname_f = 0;
    last_obj_i = last_obj_f = 0;
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
    assert(stdp.sharpness == 0.0f && stdp.contrast == 0.0f && stdp.vibrance == 0.0f);

    tl_visual_params crisp = tl_visual_compute_params(TL_VISUAL_CRISP, true, 2796, 1290, 2796, 1290);
    assert(fabsf(crisp.sharpness - 0.34f) < 1e-5f && fabsf(crisp.contrast - 0.16f) < 1e-5f
           && fabsf(crisp.vibrance - 0.12f) < 1e-5f);

    tl_visual_params pro_native = tl_visual_compute_params(TL_VISUAL_PRO_MAX, true, 2796, 1290, 2796, 1290);
    tl_visual_params pro_upscaled = tl_visual_compute_params(TL_VISUAL_PRO_MAX, true, 1568, 724, 2796, 1290);
    tl_visual_params pro_srgb = tl_visual_compute_params(TL_VISUAL_PRO_MAX, false, 2796, 1290, 2796, 1290);
    assert(fabsf(pro_native.sharpness - 0.56f) < 1e-5f && fabsf(pro_native.contrast - 0.30f) < 1e-5f
           && fabsf(pro_native.vibrance - 0.26f) < 1e-5f);
    assert(pro_upscaled.sharpness > pro_native.sharpness && pro_upscaled.sharpness <= 0.78f);
    assert(pro_native.sharpness > crisp.sharpness && crisp.sharpness > stdp.sharpness);
    assert(pro_native.contrast > crisp.contrast && crisp.contrast > stdp.contrast);
    assert(pro_native.vibrance > crisp.vibrance && crisp.vibrance > stdp.vibrance);
    assert(pro_srgb.vibrance > pro_native.vibrance);

    /* Every display preset preserves the game's filtering decisions. In
     * particular, selecting sharpening must not silently enable AF when the
     * game reports Aniso=0/1x or replace its mipmap interpolation. */
    const int filters[] = { 0x2600, 0x2601, 0x2700, 0x2701, 0x2702, 0x2703 };
    const unsigned targets[] = { 0x0DE1, 0x8C1A, 0x8513 };
    const float af_values[] = { 1.0f, 2.0f, 4.0f, 16.0f };
    for (int preset = TL_VISUAL_STANDARD; preset <= TL_VISUAL_PRO_MAX; preset++) {
        tl_host_set_visual_quality(preset);
        for (unsigned target = 0; target < sizeof(targets) / sizeof(targets[0]); target++) {
            bool eligible = is_aniso_tex_target(targets[target]);
            for (unsigned f = 0; f < sizeof(filters) / sizeof(filters[0]); f++) {
                reset_calls();
                apply_filter_param_i(targets[target], eligible, 0x2801, filters[f], fake_set_i, fake_set_f);
                assert(param_i_calls == 1 && last_pname_i == 0x2801 && last_val_i == filters[f]);
                assert(last_obj_i == targets[target]);
                assert(param_f_calls == 0); /* no hidden AF call, even without its extension */
                reset_calls();
                apply_filter_param_f(targets[target], eligible, 0x2801, (float)filters[f], fake_set_f);
                assert(param_i_calls == 0 && param_f_calls == 1 && last_pname_f == 0x2801);
                assert(last_val_f == (float)filters[f]);
                assert(last_obj_f == targets[target]);
            }
            for (unsigned v = 0; v < sizeof(af_values) / sizeof(af_values[0]); v++) {
                reset_calls();
                apply_filter_param_f(targets[target], eligible, 0x84FE, af_values[v], fake_set_f);
                assert(param_f_calls == 1 && last_pname_f == 0x84FE && last_val_f == af_values[v]);
                assert(last_obj_f == targets[target]);
                reset_calls();
                apply_filter_param_i(targets[target], eligible, 0x84FE, (int)af_values[v], fake_set_i, fake_set_f);
                assert(param_i_calls == 1 && last_pname_i == 0x84FE && last_val_i == (int)af_values[v]);
                assert(last_obj_i == targets[target]);
                assert(param_f_calls == 0);
            }
        }
    }

    /* Switching presets must not inject filtering that can remain stuck on
     * textures/samplers after returning to Standard. */
    const int preset_sequence[] = { TL_VISUAL_CRISP, TL_VISUAL_PRO_MAX, TL_VISUAL_STANDARD };
    for (unsigned i = 0; i < sizeof(preset_sequence) / sizeof(preset_sequence[0]); i++) {
        tl_host_set_visual_quality(preset_sequence[i]);
        reset_calls();
        apply_filter_param_i(0x0DE1, true, 0x2801, 0x2701, fake_set_i, fake_set_f);
        assert(last_val_i == 0x2701 && param_f_calls == 0);
    }

    /* Leave validation to GL: invalid AF/filter requests must be forwarded,
     * rather than replaced by an apparently-valid preset value. */
    tl_host_set_visual_quality(TL_VISUAL_PRO_MAX);
    reset_calls();
    apply_filter_param_f(0x0DE1, true, 0x84FE, 0.0f, fake_set_f);
    assert(param_f_calls == 1 && last_val_f == 0.0f);
    reset_calls();
    apply_filter_param_i(0x0DE1, true, 0x2801, -1, fake_set_i, fake_set_f);
    assert(param_i_calls == 1 && last_val_i == -1 && param_f_calls == 0);
    reset_calls();
    apply_filter_param_f(17, true, 0x84FE, 2.0f, fake_set_f); /* sampler object, not texture target */
    assert(param_f_calls == 1 && last_obj_f == 17 && last_pname_f == 0x84FE && last_val_f == 2.0f);

    puts("PASS: display enhancement presets preserve original guest texture/sampler filtering, AF off/2x/16x, preset transitions and GL validation without injecting GPU work");
    return 0;
}
