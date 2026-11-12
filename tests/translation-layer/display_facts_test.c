/* SPDX-License-Identifier: GPL-2.0-or-later */
#include <assert.h>
#include <stdbool.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "husk-tl-display-timing.h"

typedef union { float f; } jvalue;
static struct { struct { float max_refresh_rate; } cfg; } A;
static float g_java_max_refresh_rate = 60.0f;
typedef struct { char language[2], country[2]; int orientation, density, screen_long, screen_size, ui_mode_type, ui_mode_night; } tl_aconfig;
static const char *density_property;
static const char *tl_host_device_property(const char *name)
{ assert(!strcmp(name, "ro.sf.lcd_density")); return density_property; }
#define NAT(name) static bool name(void *self, const jvalue *a, jvalue *ret)
#define F(value) ((jvalue){ .f = (value) })
#include "display_facts_under_test.h"

int main(void)
{
    jvalue value;
    A.cfg.max_refresh_rate = 120.0f;
    husk_java_set_display_max_refresh(120.0f);
    assert(g_java_max_refresh_rate == 120.0f);
    assert(N_refreshRate(NULL, NULL, &value) && value.f == TL_JAVA_DISPLAY_HZ);
    assert(N_maxRefreshRate(NULL, NULL, &value) && value.f == 120.0f);
    float invalid[] = { 0, -1, NAN, INFINITY, 1001 };
    for (unsigned i = 0; i < sizeof(invalid)/sizeof(*invalid); i++) {
        husk_java_set_display_max_refresh(120.0f);
        husk_java_set_display_max_refresh(invalid[i]);
        assert(g_java_max_refresh_rate == 60.0f);
        A.cfg.max_refresh_rate = invalid[i];
        assert(N_maxRefreshRate(NULL, NULL, &value) && value.f == TL_JAVA_DISPLAY_HZ);
        assert(N_refreshRate(NULL, NULL, &value) && value.f == 60.0f);
    }
    tl_aconfig *config = b_AConfiguration_new();
    assert(config && config->density == 480); /* Unknown host before configuration. */
    density_property = "320";
    b_AConfiguration_fromAssetManager(config, NULL);
    assert(config->density == 320);
    density_property = NULL;
    b_AConfiguration_fromAssetManager(config, NULL);
    assert(config->density == 320);
    density_property = "480";
    tl_aconfig *native = b_AConfiguration_new();
    assert(native && native->density == 480);
    b_AConfiguration_fromAssetManager(NULL, NULL);
    free(config); free(native);
    puts("PASS: virtual current refresh stays distinct from physical 120 Hz capability, invalid facts reset, and NDK density follows the real host scale");
    return 0;
}
