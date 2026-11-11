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
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

static void tl_log_line(const char *format, ...) { (void)format; }

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

    /* 4. Narrow scanners obey the same argument boundary. A color tag needs
     * only two register arguments (%n consumes a pointer but is not a match).
     * The old implementation unconditionally read 96 slots, reaching NULL. */
    assert(tl_count_scan_args(NULL) == 0);
    assert(tl_count_scan_args("plain %% literal") == 0);
    assert(tl_count_scan_args("%*d %% %6x %n") == 2);
    assert(tl_count_scan_args("%[^]a-z] %[]0-9] %*[^,] %ls") == 3);
    assert(tl_count_scan_args("%3$d %1$d") == 3);
    assert(tl_count_scan_args("%m[a-z] %mls %*ms") == 2);
    assert(tl_count_scan_args("%hhd %hd %ld %lld %jd %zd %td %Lf") == 8);
    uint32_t color = 0;
    int consumed = -1;
    gp[2] = (uintptr_t)&color;
    gp[3] = (uintptr_t)&consumed;
    ap = (tl_va_list){ .stack = NULL, .gr_top = &gp[8], .gr_offs = -6 * 8 };
    assert(tl_scan_string("^FFCB4ATh\xE1\xBA\xA5t L\xC3\xBD \xC4\x91\xE1\xBB\x99ng", "^%6x%n", &ap) == 1);
    assert(color == 0xFFCB4Au && consumed == 7);
    assert(ap.gr_offs == -4 * 8 && ap.stack == NULL);
    ap = (tl_va_list){ .stack = NULL, .gr_offs = 0 };
    assert(tl_scan_string("99 %", "%*d %%", &ap) == 0);
    assert(ap.stack == NULL && ap.gr_offs == 0);
    char lower[5] = {0, 0, 0, 0, 0x7f}, upper[5] = {0, 0, 0, 0, 0x7f}, close[5] = {0, 0, 0, 0, 0x7f};
    gp[2] = (uintptr_t)lower; gp[3] = (uintptr_t)upper; gp[4] = (uintptr_t)close;
    ap = (tl_va_list){ .stack = NULL, .gr_top = &gp[8], .gr_offs = -6 * 8 };
    assert(tl_scan_string("123 abcDEF ]]]", "%*u %3[a-z]%3[A-Z] %3[]]", &ap) == 3);
    assert(strcmp(lower, "abc") == 0 && strcmp(upper, "DEF") == 0 && strcmp(close, "]]]") == 0);
    assert(lower[4] == 0x7f && upper[4] == 0x7f && close[4] == 0x7f);
    assert(ap.gr_offs == -3 * 8 && ap.stack == NULL);
#ifndef _WIN32
    int first = 0, second = 0;
    gp[2] = (uintptr_t)&first; gp[3] = (uintptr_t)&second;
    ap = (tl_va_list){ .stack = NULL, .gr_top = &gp[8], .gr_offs = -6 * 8 };
    assert(tl_scan_string("17 29", "%2$d %1$d", &ap) == 2);
    assert(first == 29 && second == 17 && ap.gr_offs == -4 * 8);
#endif

    /* 5. File scans cross the six remaining GP slots into exactly the needed
     * stack arguments, including files with more than the old 16-field cap. */
    FILE *input = tmpfile();
    assert(input);
    assert(fputs("^FFCB4A", input) >= 0);
    rewind(input);
    gp[2] = (uintptr_t)&color; gp[3] = (uintptr_t)&consumed;
    ap = (tl_va_list){ .stack = NULL, .gr_top = &gp[8], .gr_offs = -6 * 8 };
    color = 0; consumed = -1;
    assert(tl_scan_file(input, "^%6x%n", &ap) == 1);
    assert(color == 0xFFCB4Au && consumed == 7 && ap.gr_offs == -4 * 8 && ap.stack == NULL);
    fclose(input);
    input = tmpfile();
    assert(input);
    int values[18] = {0};
    uint64_t stack_args[12];
    char many_format[18 * 3 + 1] = {0};
    for (int i = 0; i < 18; i++) {
        fprintf(input, "%d ", i + 1);
        memcpy(many_format + i * 3, "%d ", 3);
        if (i < 6) gp[2 + i] = (uintptr_t)&values[i];
        else stack_args[i - 6] = (uintptr_t)&values[i];
    }
    rewind(input);
    ap = (tl_va_list){ .stack = stack_args, .gr_top = &gp[8], .gr_offs = -6 * 8 };
    assert(tl_scan_file(input, many_format, &ap) == 18);
    for (int i = 0; i < 18; i++) assert(values[i] == i + 1);
    assert(ap.gr_offs == 0 && ap.stack == &stack_args[12]);
    fclose(input);
    char too_many[97 * 2 + 1] = {0};
    for (int i = 0; i < 97; i++) memcpy(too_many + 2 * i, "%d", 2);
    ap = (tl_va_list){ .stack = NULL, .gr_offs = 0 };
    errno = 0;
    assert(tl_scan_string("", too_many, &ap) == EOF && errno == E2BIG && ap.stack == NULL);
    input = tmpfile();
    assert(input);
    errno = 0;
    assert(tl_scan_file(input, too_many, &ap) == EOF && errno == E2BIG && ftell(input) == 0);
    fclose(input);
    wchar_t too_many_wide[65 * 2 + 1] = {0};
    for (int i = 0; i < 65; i++) { too_many_wide[2 * i] = L'%'; too_many_wide[2 * i + 1] = L'd'; }
    errno = 0;
    assert(tl_scan_wide(L"", too_many_wide, &ap) == EOF && errno == E2BIG && ap.stack == NULL);

    /* 6. b_tmpnam fits inside Bionic's 20-byte L_tmpnam and maps under /data/local/tmp/. */
    char buf1[20], buf2[20];
    memset(buf1, 0x5a, sizeof(buf1));
    memset(buf2, 0x5a, sizeof(buf2));
    assert(b_tmpnam(buf1) == buf1);
    assert(b_tmpnam(buf2) == buf2);
    assert(strlen(buf1) < 20 && strncmp(buf1, "/data/local/tmp/", 16) == 0);
    assert(strlen(buf2) < 20 && strcmp(buf1, buf2) != 0);
    char *tls = b_tmpnam(NULL);
    assert(tls != NULL && strlen(tls) < 20 && strncmp(tls, "/data/local/tmp/", 16) == 0);

    puts("PASS: compressed heap, bounded AAPCS64 string/file/wide scans and Bionic tmpnam contracts verified");
    return 0;
}
