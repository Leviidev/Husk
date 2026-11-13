/* SPDX-License-Identifier: GPL-2.0-or-later */
#define _DARWIN_C_SOURCE
#include "husk-tl-host-device.h"
#include <pthread.h>
#include <stdatomic.h>
#include <sys/sysctl.h>

static tl_host_device device;
static pthread_once_t once = PTHREAD_ONCE_INIT;
static atomic_int display_density;

void tl_host_set_display_density(float pixels_per_dp)
{
    /* Ignore malformed samples without discarding a previously valid screen
     * density. The comparisons also reject NaN/infinity before conversion. */
    if (!(pixels_per_dp >= 0.1f && pixels_per_dp <= 16.0f)) return;
    atomic_store(&display_density, (int)(pixels_per_dp * 160.0f + 0.5f));
}
float tl_host_display_density(void)
{
    int dpi = atomic_load(&display_density);
    return dpi > 0 ? (float)dpi / 160.0f : 3.0f;
}
static const char *display_density_property(void)
{
    int dpi = atomic_load(&display_density);
    if (!dpi) return NULL;
    static _Thread_local char value[16];
    snprintf(value, sizeof(value), "%d", dpi);
    return value;
}

static void read_string(const char *key, char *value, size_t capacity)
{
    size_t bytes = capacity;
    value[0] = 0;
    if (sysctlbyname(key, value, &bytes, NULL, 0) != 0) value[0] = 0;
    value[capacity - 1] = 0;
}
static void read_device(void)
{
    char machine[64], board[64], cpu_brand[92];
    read_string("hw.machine", machine, sizeof(machine));
    read_string("hw.model", board, sizeof(board));
    read_string("machdep.cpu.brand_string", cpu_brand, sizeof(cpu_brand));
    uint32_t family = 0;
    size_t bytes = sizeof(family);
    if (sysctlbyname("hw.cpufamily", &family, &bytes, NULL, 0) != 0 || bytes != sizeof(family)) family = 0;
    tl_host_device_describe(&device, machine, board, cpu_brand, family);
}
static atomic_int display_max_refresh_x100;
static atomic_int display_wide_color;

void tl_host_set_display_max_refresh(float refresh_rate)
{
    if (!(refresh_rate >= 1.0f && refresh_rate <= 1000.0f)) return;
    atomic_store(&display_max_refresh_x100, (int)(refresh_rate * 100.0f + 0.5f));
}
float tl_host_display_max_refresh(void)
{
    int hz_x100 = atomic_load(&display_max_refresh_x100);
    return hz_x100 > 0 ? (float)hz_x100 / 100.0f : 60.0f;
}
void tl_host_set_display_wide_color(int wide_color)
{
    atomic_store(&display_wide_color, wide_color > 0 ? 1 : -1);
}
bool tl_host_display_wide_color(void)
{
    return atomic_load(&display_wide_color) > 0;
}

const char *tl_host_device_property(const char *name)
{
    if (name && !strcmp(name, "ro.sf.lcd_density")) return display_density_property();
    pthread_once(&once, read_device);
    return tl_host_device_lookup(&device, name);
}
