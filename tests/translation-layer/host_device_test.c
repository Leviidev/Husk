/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "husk-tl-host-device.h"
#include <assert.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <math.h>
#include "host_density_under_test.h"

int main(void)
{
    assert(!display_density_property());
    tl_host_set_display_density(3.0f); assert(!strcmp(display_density_property(), "480"));
    tl_host_set_display_density(2.5f); assert(!strcmp(display_density_property(), "400"));
    tl_host_set_display_density(2.0f); assert(!strcmp(display_density_property(), "320"));
    tl_host_set_display_density(NAN); tl_host_set_display_density(INFINITY);
    tl_host_set_display_density(-1); tl_host_set_display_density(1000);
    assert(!strcmp(display_density_property(), "320"));
    tl_host_device info;
    tl_host_device_describe(&info, "iPhone16,2", "D84AP", "", 0xe81e7ef6u);
    assert(!strcmp(tl_host_device_lookup(&info, "ro.product.model"), "iPhone16,2"));
    assert(!strcmp(tl_host_device_lookup(&info, "ro.product.device"), info.model));
    assert(!strcmp(tl_host_device_lookup(&info, "ro.hardware"), "D84AP"));
    assert(!strcmp(tl_host_device_lookup(&info, "ro.soc.model"), "Apple ARM64 family 0xe81e7ef6"));
    assert(!strcmp(tl_host_device_lookup(&info, "ro.soc.manufacturer"), "Apple"));
    assert(!strcmp(tl_host_device_lookup(&info, "ro.build.id"), "HUSK"));
    assert(!strcmp(tl_host_device_lookup(&info, "ro.build.fingerprint"),
                   tl_host_device_lookup(&info, "ro.vendor.build.fingerprint")));
    assert(strstr(info.fingerprint, "apple/iPhone16,2/iPhone16,2:14/HUSK/"));
    assert(!tl_host_device_lookup(&info, "ro.build.version.sdk"));
    assert(!tl_host_device_lookup(&info, "serial") && !tl_host_device_lookup(&info, NULL));

    /* Query failures stay neutral, and a different host is not described as
     * this user's iPhone or assigned a guessed marketing/performance tier. */
    tl_host_device_describe(&info, "iPad99,1", "", NULL, 0);
    assert(!strcmp(info.model, "iPad99,1") && !strcmp(info.board, info.model));
    assert(!strcmp(info.soc, "Apple ARM64"));
    tl_host_device_describe(&info, "arm64", "Mac14,2", "Apple M2", 0);
    assert(!strcmp(info.model, "Mac14,2") && !strcmp(info.soc, "Apple M2"));
    tl_host_device_describe(&info, NULL, NULL, NULL, 0);
    assert(!strcmp(info.model, "Apple device") && !strcmp(info.soc, "Apple ARM64"));

    char long_name[300]; memset(long_name, 'x', sizeof(long_name)); long_name[299] = 0;
    tl_host_device_describe(&info, long_name, long_name, long_name, 0);
    assert(strlen(info.model) == sizeof(info.model) - 1 && strlen(info.board) == sizeof(info.board) - 1);
    assert(strlen(info.soc) == sizeof(info.soc) - 1 && strlen(info.fingerprint) < 92 && strlen(info.display) < 92);
    puts("PASS: real host model/board/CPU identity stays coherent, bounded and neutral on unknown devices without changing Android API or collecting unique identifiers");
    return 0;
}
