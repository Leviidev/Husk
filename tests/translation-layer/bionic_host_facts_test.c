/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "husk-tl-host-device.h"
#include "husk-tl-memory.h"
#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <time.h>

static tl_host_device device;
static tl_memory_info memory;
static int guest_errno;
static unsigned callbacks;
static const char *expected_property;
const char *tl_host_device_property(const char *name) { return tl_host_device_lookup(&device, name); }
void tl_host_memory_info(tl_memory_info *out) { *out = memory; }
static const char *host_prop(const char *name) { (void)name; return NULL; }
static void tl_set_guest_errno(int value) { guest_errno = value; }
static int tl_dns_servers(char out[][64], int max) { (void)out; (void)max; return 0; }
static const char *test_getenv(const char *key) { (void)key; return NULL; }
#define getenv test_getenv
#ifndef _SC_NPROCESSORS_ONLN
#define _SC_NPROCESSORS_ONLN 123
#endif
static long test_sysconf(int key) { assert(key == _SC_NPROCESSORS_ONLN); return 6; }
#define sysconf test_sysconf
#ifndef CLOCK_MONOTONIC
#define CLOCK_MONOTONIC 1
#endif
static int test_clock_gettime(int clock, struct timespec *time)
{ assert(clock == CLOCK_MONOTONIC); time->tv_sec = 12; time->tv_nsec = 0; return 0; }
#define clock_gettime test_clock_gettime
#include "bionic_host_facts_under_test.h"
#undef getenv
#undef sysconf
#undef clock_gettime

static void property_callback(void *cookie, const char *name, const char *value, uint32_t serial)
{
    assert(cookie == &device && !strcmp(name, expected_property) && !serial);
    assert(!strcmp(value, tl_sysprop(name))); callbacks++;
}
int main(void)
{
    tl_host_device_describe(&device, "iPhone16,2", "D84AP", NULL, 0xe81e7ef6u);
    const char *keys[] = { "ro.product.model", "ro.product.name", "ro.product.device", "ro.product.board",
        "ro.product.manufacturer", "ro.soc.manufacturer", "ro.soc.model", "ro.hardware", "ro.board.platform",
        "ro.build.id", "ro.build.display.id", "ro.build.fingerprint", "ro.vendor.build.fingerprint",
        "ro.boot.hardware.sku", "ro.boot.product.hardware.sku" };
    for (unsigned i = 0; i < sizeof(keys) / sizeof(*keys); i++) {
        const void *handle = bionic___system_property_find(keys[i]); assert(handle);
        char name[32], value[92], direct[92];
        int bytes = bionic___system_property_read(handle, name, value);
        assert(!strcmp(name, keys[i]) && bytes == (int)strlen(value));
        assert(!strcmp(value, tl_sysprop(keys[i])));
        assert(bionic___system_property_get(keys[i], direct) == bytes && !strcmp(direct, value));
        expected_property = keys[i];
        bionic___system_property_read_callback(handle, property_callback, &device);
    }
    assert(callbacks == sizeof(keys) / sizeof(*keys));
    assert(!tl_sysprop(NULL) && !tl_sysprop("missing.property"));
    assert(!strcmp(tl_sysprop("ro.build.version.sdk"), "34"));
    assert(!strcmp(tl_sysprop("ro.product.cpu.abi"), "arm64-v8a"));

    guest_sysinfo info;
    memory.total = 2ull << 30; memory.available = 45ull << 20;
    assert(!bionic_sysinfo(&info));
    assert(info.uptime == 12 && info.totalram == memory.total && info.freeram == memory.available && info.mem_unit == 1);
    assert(bionic_sysconf(98) == (long)(memory.total / 16384));
    assert(bionic_sysconf(99) == (long)(memory.available / 16384));
    memory.available = 7ull << 20;
    assert(!bionic_sysinfo(&info) && info.freeram == memory.available);
    assert(bionic_sysconf(99) == (long)(memory.available / 16384));
    memory.available = memory.total * 2;
    assert(!bionic_sysinfo(&info) && info.freeram == memory.total);
    assert(bionic_sysconf(99) == (long)(memory.total / 16384));
    memory.total = 8ull << 30; memory.available = 100ull << 20;
    assert(bionic_sysconf(98) == 524288 && bionic_sysconf(99) == 6400);
    assert(bionic_sysconf(96) == 6 && bionic_sysconf(97) == 6);
    assert(bionic_sysconf(-1) == -1 && guest_errno == 22);
    puts("PASS: real host identity agrees across property get/find/read/callback; sysinfo/sysconf free memory follows changing process headroom and CPU count stays real");
    return 0;
}
