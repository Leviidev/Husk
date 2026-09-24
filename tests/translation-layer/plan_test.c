/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * The page planner on hand-made layouts, where the right answer can be
 * worked out on paper -- the scanner tests only see what a linker produced.
 */
#include "husk-tl-internal.h"

#include <stdio.h>
#include <string.h>

#define R 4u
#define W 2u
#define X 1u

static int failures;

static void expect(const char *what, const tl_segment *loads, size_t n,
                   const tl_segment *relro, uint64_t page, const char *want)
{
    uint8_t flags[64];
    uint64_t base = 0;
    size_t pages = tl_page_plan(loads, n, relro, page, flags, sizeof(flags), &base);
    char got[65] = "";
    for (size_t i = 0; i < pages && i < 64; i++) {
        uint8_t f = flags[i];
        got[i] = (f & TL_PAGE_X) && (f & TL_PAGE_W) ? '!' : (f & TL_PAGE_X) ? 'X'
               : (f & TL_PAGE_W) ? 'W' : (f & TL_PAGE_R) ? 'R' : '-';
    }
    if (strcmp(got, want) != 0) {
        printf("FAIL %s: got \"%s\", want \"%s\"\n", what, got, want);
        failures++;
    } else {
        printf("ok   %s: %s\n", what, got);
    }
}

int main(void)
{
    /* lld's layout at 4 KiB: headers, code, RELRO, data, one 4 KiB page each. */
    const tl_segment small[] = {
        { 0x0000, 0x0800, R },
        { 0x1000, 0x0400, R | X },
        { 0x2000, 0x0300, R | W },          /* RELRO: .dynamic, .got */
        { 0x3000, 0x0100, R | W },          /* .data, .bss */
    };
    const tl_segment small_relro = { 0x2000, 0x0300, R };
    expect("4k layout on 4k pages", small, 4, &small_relro, 4096, "RXRW");
    /* All four land in one 16 KiB page: code and writable data together. */
    expect("4k layout on 16k pages", small, 4, &small_relro, 16384, "!");

    /* The same library built for 16 KiB pages. */
    const tl_segment wide[] = {
        { 0x00000, 0x0800, R },
        { 0x04000, 0x0400, R | X },
        { 0x08000, 0x0300, R | W },
        { 0x0C000, 0x0100, R | W },
    };
    const tl_segment wide_relro = { 0x8000, 0x0300, R };
    expect("16k layout on 16k pages", wide, 4, &wide_relro, 16384, "RXRW");

    /* RELRO shares a page with code, but RELRO is only written while
     * relocating, so the page can stay executable. */
    const tl_segment shared[] = {
        { 0x0000, 0x3000, R | X },
        { 0x3000, 0x0800, R | W },          /* all of it RELRO */
        { 0x4000, 0x1000, R | W },
    };
    const tl_segment shared_relro = { 0x3000, 0x0800, R };
    expect("code with RELRO on one page", shared, 3, &shared_relro, 16384, "XW");
    expect("same, without RELRO", shared, 3, NULL, 16384, "!W");

    /* One writable segment, RELRO in its middle: both ends stay writable. */
    const tl_segment middle[] = {
        { 0x0000, 0x4000, R | X },
        { 0x4000, 0xC000, R | W },
    };
    const tl_segment middle_relro = { 0x8000, 0x4000, R };
    expect("RELRO in the middle", middle, 2, &middle_relro, 16384, "XWRW");

    /* .bss past the end of the file still has to be writable. */
    const tl_segment bss[] = {
        { 0x0000, 0x4000, R | X },
        { 0x4000, 0x9000, R | W },
    };
    expect("bss spans pages", bss, 2, NULL, 16384, "XWWW");

    /* A hole between segments is a page nothing is loaded into. */
    const tl_segment hole[] = {
        { 0x0000, 0x4000, R | X },
        { 0xC000, 0x4000, R | W },
    };
    expect("hole", hole, 2, NULL, 16384, "X--W");

    /* Not a power of two, or an address that overflows: refused. */
    uint8_t flags[4];
    uint64_t base;
    const tl_segment bad[] = { { UINT64_MAX - 10, 100, R } };
    if (tl_page_plan(small, 4, NULL, 12288, flags, 4, &base) != 0
        || tl_page_plan(bad, 1, NULL, 16384, flags, 4, &base) != 0) {
        printf("FAIL bad layouts were accepted\n");
        failures++;
    } else {
        printf("ok   bad layouts refused\n");
    }

    printf("%s\n", failures ? "FAILED" : "all passed");
    return failures ? 1 : 0;
}
