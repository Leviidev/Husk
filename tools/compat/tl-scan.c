// SPDX-License-Identifier: GPL-2.0-or-later
/* Prints the app scan Husk makes of an APK and its splits, as JSON (tools/compat/playsweep.py reads the engine from it).
 *   clang -I src/translation-layer tools/compat/tl-scan.c src/translation-layer/husk-tl-{scan,elf,zip,json,load}.c -lz \
 *         -Wl,-undefined,dynamic_lookup -o tl-scan */
#include <stdio.h>
#include <stdlib.h>
#include "husk-tl.h"

int main(int argc, char **argv)
{
    if (argc < 2) { fprintf(stderr, "usage: %s <base.apk> [split.apk ...]\n", argv[0]); return 2; }
    char *json = husk_tl_scan((const char *const *)argv + 1, argc - 1);
    if (!json) return 1;
    puts(json);
    free(json);
    return 0;
}
