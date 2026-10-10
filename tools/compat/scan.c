/* SPDX-License-Identifier: GPL-2.0-or-later */
/* scan <apk> [split ...] -- what the app's scan says about an APK (husk_tl_scan), as JSON on stdout. */
#include <stdio.h>
#include <stdlib.h>
#include "husk-tl.h"

int main(int argc, char **argv)
{
    if (argc < 2) { fprintf(stderr, "usage: %s <apk> [split ...]\n", argv[0]); return 2; }
    char *json = husk_tl_scan((const char *const *)(argv + 1), argc - 1);
    if (!json) return 1;
    puts(json);
    husk_tl_free(json);
    return 0;
}
