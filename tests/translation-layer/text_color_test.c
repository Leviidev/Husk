/* SPDX-License-Identifier: GPL-2.0-or-later */
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fail_allocation;
static unsigned allocations;
static void *test_malloc(size_t bytes)
{
    allocations++;
    if (fail_allocation && --fail_allocation == 0) return NULL;
    return malloc(bytes);
}
#define malloc test_malloc
#include "husk-tl-text-color.h"
#undef malloc

#define U16(s) ((const uint16_t *)u##s)
static size_t len16(const uint16_t *s) { size_t n = 0; while (s[n]) n++; return n; }
static void equal16(const tl_text_color_view16 *v, const uint16_t *wanted)
{
    assert(v->count == len16(wanted));
    assert(!memcmp(v->text, wanted, v->count * sizeof(*wanted)));
}
static void check(const uint16_t *source, const uint16_t *wanted)
{
    size_t n = len16(source);
    uint16_t *original = malloc((n + 1) * sizeof(*source));
    memcpy(original, source, (n + 1) * sizeof(*source));
    tl_text_color_view16 v = tl_text_color_view16_make(source, n, 0, n, true, true);
    equal16(&v, wanted);
    assert(v.source_count == n && tl_text_color_visible16(&v, n) == v.count);
    assert(!memcmp(original, source, (n + 1) * sizeof(*source)));
    size_t last = 0;
    for (size_t i = 0; i <= n; i++) {
        size_t at = tl_text_color_visible16(&v, i);
        assert(at >= last && at <= last + 1);
        if (at != last) assert(v.text[at - 1] == source[i - 1]);
        last = at;
    }
    for (size_t i = 0; i <= v.count; i++) {
        size_t at = tl_text_color_source16(&v, i);
        assert(at <= n && tl_text_color_visible16(&v, at) == i);
        assert(at == n || tl_text_color_visible16(&v, at + 1) > i);
    }
    tl_text_color_view16_dispose(&v);
    free(original);
}
int main(void)
{
    check(U16("^FFFFLogin ^DDDDmenu ^ffcb4aWorld ^^ABCDEFName ^8080ffBan ^00ff00Cang ^11ff11100 HP"),
          U16("Login menu World Name Ban Cang 100 HP"));
    check(U16("^ffff ^aBcDe ^AABBCC ^1234567 ^123456789 ^FFFFFFFF"), U16("   7 789 FF"));
    check(U16("^FFCB4ATh\u1ea5t L\u00fd \u0111\u1ed9ng ^DDDDTi\u1ebfng Vi\u1ec7t A\u0302 \U0001f642 \uf1ff"),
          U16("Th\u1ea5t L\u00fd \u0111\u1ed9ng Ti\u1ebfng Vi\u1ec7t A\u0302 \U0001f642 \uf1ff"));
    check(U16("x^2 ^ABC ^^ ^GHIJKL ^\uff26\uff26\uff26\uff26 ^FFFFFFG"),
          U16("x^2 ^ABC ^^ ^GHIJKL ^\uff26\uff26\uff26\uff26 G"));
    check(U16("t\u0103ng ^ ffcb4a%d^ffffff x^ 1234 ^\nffffff ^  ffffff"),
          U16("t\u0103ng %d x^ 1234 ^\nffffff ^  ffffff"));

    const uint16_t *source = U16("ab^FFFFLogin^112233!");
    size_t n = len16(source);
    /* Styled runs and drawPosText can start/end in the middle of a control. */
    for (size_t i = 2; i < 7; i++) {
        tl_text_color_view16 v = tl_text_color_view16_make(source, n, i, i + 1, true, true);
        equal16(&v, U16("")); assert(v.source_count == 1);
        assert(tl_text_color_source16(&v, 0) == 1);
        tl_text_color_view16_dispose(&v);
    }
    tl_text_color_view16 v = tl_text_color_view16_make(source, n, 4, 10, true, true);
    equal16(&v, U16("Log"));
    assert(tl_text_color_source16(&v, 0) == 3);
    assert(tl_text_color_source16(&v, 1) == 4);
    tl_text_color_view16_dispose(&v);
    v = tl_text_color_view16_make(source, n, 0, n, false, true);
    assert(v.text == source && v.count == n && !v.owned && !v.visible_at);
    tl_text_color_view16_dispose(&v);
    v = tl_text_color_view16_make(source, n, n + 10, n + 20, true, true);
    assert(!v.count && !v.source_count && !v.owned);
    tl_text_color_view16_dispose(&v);

    /* Both failed buffer allocation and failed map allocation retain a safe
     * unmodified view; normal strings allocate nothing in the hot path. */
    for (int i = 1; i <= 2; i++) {
        fail_allocation = i;
        v = tl_text_color_view16_make(source, n, 0, n, true, true);
        assert(v.text == source && v.count == n && !v.owned && !v.visible_at);
        tl_text_color_view16_dispose(&v);
    }
    fail_allocation = 0;
    unsigned before = allocations;
    v = tl_text_color_view16_make(U16("Plain ^2 \U0001f642"), 11, 0, 11, true, true);
    assert(allocations == before && !v.owned && !v.visible_at);
    tl_text_color_view16_dispose(&v);

    /* Native UTF-32 and Java UTF-16 use the same exact token grammar. */
    const uint32_t *wide = U"^FFFF x ^^12345678 x ^123456789 x ^123 ^ ffcb4a";
    const uint16_t *narrow = U16("^FFFF x ^^12345678 x ^123456789 x ^123 ^ ffcb4a");
    n = len16(narrow);
    for (size_t i = 0; i <= n; i++)
        assert(tl_text_color_end32(wide, i, n) == tl_text_color_end16(narrow, i, n));
    uint16_t *carets = malloc(65536 * sizeof(*carets));
    for (size_t i = 0; i < 65536; i++) carets[i] = '^';
    v = tl_text_color_view16_make(carets, 65536, 0, 65536, true, true);
    assert(!v.owned && v.count == 65536);
    tl_text_color_view16_dispose(&v); free(carets);
    puts("PASS: production UTF-16/UTF-32 color parser, copied Unicode ranges, partial-run suppression, original-index mapping, no-op fast path and allocation failure");
    return 0;
}
