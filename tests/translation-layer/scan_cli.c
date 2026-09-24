/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Drives the translation layer's C side from the command line, so the tests
 * can exercise exactly what the app calls:
 *
 *   scan_cli scan APK...            husk_tl_scan()
 *   scan_cli entry APK NAME LIMIT   husk_tl_read_entry(), bytes to stdout
 *   scan_cli checks [noexec]        husk_tl_run_checks()
 */
#include "husk-tl.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv)
{
    if (argc >= 2 && !strcmp(argv[1], "scan")) {
        char *json = husk_tl_scan((const char *const *)(argv + 2), argc - 2);
        puts(json ? json : "null");
        husk_tl_free(json);
        return 0;
    }
    if (argc == 5 && !strcmp(argv[1], "entry")) {
        size_t len = 0;
        void *data = husk_tl_read_entry(argv[2], argv[3], strtoull(argv[4], NULL, 10), &len);
        if (!data) {
            return 1;
        }
        fwrite(data, 1, len, stdout);
        husk_tl_free(data);
        return 0;
    }
    if (argc >= 2 && !strcmp(argv[1], "checks")) {
        char *json = husk_tl_run_checks(!(argc == 3 && !strcmp(argv[2], "noexec")));
        puts(json ? json : "null");
        husk_tl_free(json);
        return 0;
    }
    fprintf(stderr, "usage: scan_cli scan APK... | entry APK NAME LIMIT | checks [noexec]\n");
    return 2;
}
