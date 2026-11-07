/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifdef _WIN32
#define _CRT_STDIO_ISO_WIDE_SPECIFIERS 1
#endif
#include "husk-tl-va.h"
#include <assert.h>
#include <errno.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

/* Extracted directly from production sources by tests/run_portable.py. */
#include "trutien_under_test.h"

int main(void)
{
    /* 1. Compressed heap reservation is kept only for V8/Dart/Flutter engines, freed for Tru Tien / Angelica / SDL. */
    assert(!lib_uses_compressed_heap(NULL));
    assert(!lib_uses_compressed_heap("assets/libflutter.so"));
    assert(!lib_uses_compressed_heap("lib/arm64-v8a/libmain.so"));
    assert(!lib_uses_compressed_heap("lib/arm64-v8a/libSDL3.so"));
    assert(!lib_uses_compressed_heap("lib/arm64-v8a/libfmod.so"));
    assert(lib_uses_compressed_heap("lib/arm64-v8a/libflutter.so"));
    assert(lib_uses_compressed_heap("lib/arm64-v8a/libcocos2djs.so"));
    assert(lib_uses_compressed_heap("lib/arm64-v8a/libminecraftpe.so"));
    assert(lib_uses_compressed_heap("lib/arm64-v8a/libpuerts.so"));
    assert(lib_uses_compressed_heap("lib/arm64-v8a/libv8.cr.so"));
    assert(lib_uses_compressed_heap("lib/arm64-v8a/libnode.so"));

    /* 2. Wide scanf argument counter handles %%, %* suppression, character classes, and positional %n$. */
    assert(tl_count_wide_scan_args(NULL) == 0);
    assert(tl_count_wide_scan_args(L"plain %% literal") == 0);
    assert(tl_count_wide_scan_args(L"%*d %% %d %f") == 2);
    assert(tl_count_wide_scan_args(L"%[^]a-z] %[]0-9] %*[^,] %ls") == 3);
    assert(tl_count_wide_scan_args(L"%3$d %1$d") == 3);

    /* 3. tl_scan_wide pops ONLY the required arguments from AAPCS64 va_list (ap.stack = NULL proves no stack over-read). */
    int a = 0, b = 0;
    wchar_t word[16] = {0};
    uint64_t gp[8] = { 0, 0, (uintptr_t)&a, (uintptr_t)&b, (uintptr_t)word, 0, 0, 0 };
    tl_va_list ap = {
        .stack = NULL,
        .gr_top = &gp[8],
        .vr_top = NULL,
        .gr_offs = -6 * 8,
        .vr_offs = 0,
    };
    int matched = tl_scan_wide(L"999 42 -77 trutien", L"%*d %d %d %15ls", &ap);
    assert(matched == 3);
    assert(a == 42 && b == -77 && wcscmp(word, L"trutien") == 0);
    assert(ap.gr_offs == -3 * 8); /* consumed exactly 3 register slots */

    /* 4. b_tmpnam fits inside Bionic's 20-byte L_tmpnam and maps under /data/local/tmp/. */
    char buf1[20], buf2[20];
    memset(buf1, 0x5a, sizeof(buf1));
    memset(buf2, 0x5a, sizeof(buf2));
    assert(b_tmpnam(buf1) == buf1);
    assert(b_tmpnam(buf2) == buf2);
    assert(strlen(buf1) < 20 && strncmp(buf1, "/data/local/tmp/", 16) == 0);
    assert(strlen(buf2) < 20 && strcmp(buf1, buf2) != 0);
    char *tls = b_tmpnam(NULL);
    assert(tls != NULL && strlen(tls) < 20 && strncmp(tls, "/data/local/tmp/", 16) == 0);

    puts("PASS: Tru Tien VA compressed-heap release, AAPCS64 vswscanf/swscanf and Bionic tmpnam contracts verified");
    return 0;
}
