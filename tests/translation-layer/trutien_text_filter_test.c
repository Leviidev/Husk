/* SPDX-License-Identifier: GPL-2.0-or-later */
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

static bool fail_resize;
static unsigned resizes;
static void *test_realloc(void *p, size_t bytes)
{ resizes++; return fail_resize ? NULL : realloc(p, bytes); }
#define realloc test_realloc
#include "../../src/translation-layer-next/husk-tl-trutien-text.c"
#undef realloc

/* Link the same complete production helper; these loader symbols are unused
 * by the entry callbacks being tested here. */
const char *tl_ld_lib_name(const tl_lib *lib) { (void)lib; return NULL; }
void *tl_ld_sym(tl_lib *lib, const char *name) { (void)lib; (void)name; return NULL; }
void tl_log_line(const char *format, ...) { (void)format; }
void tl_entry_filter_return(void) {}

static size_t ulen(const uint32_t *text) { size_t n = 0; while (text[n]) n++; return n; }
static void expect(const uint32_t *actual, const uint32_t *wanted)
{ assert(!memcmp(actual, wanted, (ulen(wanted) + 1) * sizeof(*wanted))); }
static void regs_init(uint64_t regs[29])
{ for (unsigned i = 0; i < 29; i++) regs[i] = 0xa0000000u + i; }
static void unchanged(const uint64_t *before, const uint64_t *after, int text, int count)
{
    for (int i = 0; i < 29; i++) if (i != text && i != count) assert(before[i] == after[i]);
}
static void cleanup(void)
{
    text_scratch *scratch = pthread_getspecific(text_key);
    pthread_setspecific(text_key, NULL);
    text_scratch_free(scratch);
}

static void *worker(void *unused)
{
    (void)unused;
    static const uint32_t source[] = U"^FFCB4AThất Lý động ^DDDD^^ffff00Tiếng Việt^FFFF";
    static const uint32_t wanted[] = U"Thất Lý động Tiếng Việt";
    uint64_t regs[29], before[29];
    regs_init(regs); regs[1] = (uintptr_t)source;
    memcpy(before, regs, sizeof(regs));
    tl_trutien_text_set_package("another.game");
    tl_trutien_hud_filter(regs, NULL); assert(!memcmp(before, regs, sizeof(regs)));
    tl_trutien_text_set_package(NULL);
    tl_trutien_hud_filter(regs, NULL); assert(!memcmp(before, regs, sizeof(regs)));
    tl_trutien_text_set_package("vn.trutien.client");
    regs_init(regs); regs[3] = (uintptr_t)source; regs[7] = UINT32_MAX;
    memcpy(before, regs, sizeof(regs));
    tl_trutien_font_filter(regs, NULL);
    expect((const uint32_t *)(uintptr_t)regs[3], wanted);
    assert(regs[7] == ulen(wanted)); unchanged(before, regs, 3, 7);
    expect(source, U"^FFCB4AThất Lý động ^DDDD^^ffff00Tiếng Việt^FFFF");

    regs_init(regs); regs[1] = (uintptr_t)source; regs[2] = ulen(source);
    memcpy(before, regs, sizeof(regs));
    tl_trutien_extent_filter(regs, NULL);
    expect((const uint32_t *)(uintptr_t)regs[1], wanted);
    assert(regs[2] == ulen(wanted)); unchanged(before, regs, 1, 2);

    regs_init(regs); regs[1] = (uintptr_t)source;
    memcpy(before, regs, sizeof(regs));
    tl_trutien_hud_filter(regs, NULL);
    expect((const uint32_t *)(uintptr_t)regs[1], wanted);
    unchanged(before, regs, 1, -1);

    regs_init(regs); regs[3] = (uintptr_t)source;
    memcpy(before, regs, sizeof(regs));
    tl_trutien_plain_filter(regs, NULL);
    expect((const uint32_t *)(uintptr_t)regs[3], wanted);
    unchanged(before, regs, 3, -1); /* legacy font height is preserved */
    regs_init(regs); regs[1] = (uintptr_t)source; regs[5] = ulen(source);
    tl_trutien_float_filter(regs, NULL);
    expect((const uint32_t *)(uintptr_t)regs[1], wanted); assert(regs[5] == ulen(wanted));
    regs_init(regs); regs[1] = (uintptr_t)source; regs[7] = ulen(source);
    memcpy(before, regs, sizeof(regs));
    tl_trutien_hud_counted_filter(regs, NULL);
    expect((const uint32_t *)(uintptr_t)regs[1], wanted); assert(regs[7] == ulen(wanted));
    unchanged(before, regs, 1, 7);
    /* HUD chunks can be bounded without a NUL: never inspect the next chunk. */
    uint32_t hud_chunk[] = { '^','F','F','F','F','X','^','D','D','D','D' };
    regs_init(regs); regs[1] = (uintptr_t)hud_chunk; regs[7] = 6;
    tl_trutien_hud_counted_filter(regs, NULL);
    expect((const uint32_t *)(uintptr_t)regs[1], U"X"); assert(regs[7] == 1);
    regs_init(regs); regs[2] = (uintptr_t)source; regs[3] = ulen(source);
    tl_trutien_legacy_extent_filter(regs, NULL);
    expect((const uint32_t *)(uintptr_t)regs[2], wanted); assert(regs[3] == ulen(wanted));

    /* DrawTextW consumes its count in the first Android stack slot. Only the
     * low four bytes change while drawing, and all caller bytes are restored. */
    uint64_t stack_args[3] = { 0xfedcba9800000000ull | ulen(source), 0x123456789abcdef0ull, 0x1122334455667788ull };
    uint64_t stack_before[3]; memcpy(stack_before, stack_args, sizeof(stack_args));
    uint64_t clipped_link = 0x45678;
    regs_init(regs); regs[3] = (uintptr_t)source; memcpy(before, regs, sizeof(regs));
    tl_trutien_text_enter(regs, &clipped_link, tl_trutien_clipped_filter, stack_args);
    assert(clipped_link == (uintptr_t)tl_entry_filter_return);
    expect((const uint32_t *)(uintptr_t)regs[3], wanted); unchanged(before, regs, 3, -1);
    assert(stack_args[0] == (0xfedcba9800000000ull | ulen(wanted)));
    assert(stack_args[1] == stack_before[1] && stack_args[2] == stack_before[2]);
    assert(tl_trutien_text_leave() == 0x45678 && !memcmp(stack_args, stack_before, sizeof(stack_args)));

    uint64_t input[14], input_before[14];
    for (unsigned i = 0; i < 14; i++) input[i] = 0xaa000000u + i;
    input[1] = (uintptr_t)source; memcpy(input_before, input, sizeof(input));
    regs_init(regs); regs[0] = (uintptr_t)input; memcpy(before, regs, sizeof(regs));
    uint64_t params_link = 0x56789;
    tl_trutien_text_enter(regs, &params_link, tl_trutien_params_filter, NULL);
    assert(params_link == (uintptr_t)tl_entry_filter_return);
    const uint64_t *copy = (const uint64_t *)(uintptr_t)regs[0];
    assert(copy != input); expect((const uint32_t *)(uintptr_t)copy[1], wanted);
    for (unsigned i = 0; i < 14; i++) if (i != 1) assert(copy[i] == input[i]);
    unchanged(before, regs, 0, -1); assert(!memcmp(input, input_before, sizeof(input)));
    assert(tl_trutien_text_leave() == 0x56789);

    /* HUD layout uses count-one measures and later draws split chunks. The
     * outer scope retains raw source indices; inner calls hide partial tags. */
    static const uint32_t scoped[] = U"^FFCB4ABan ^FFFFViệt";
    uint64_t scope_link = 0x67890;
    regs_init(regs); regs[1] = (uintptr_t)scoped; memcpy(before, regs, sizeof(regs));
    tl_trutien_text_enter(regs, &scope_link, tl_trutien_hud_context_filter, NULL);
    assert(scope_link == (uintptr_t)tl_entry_filter_return && !memcmp(before, regs, sizeof(regs)));
    for (size_t i = 0; i < ulen(scoped); i++) {
        bool hidden = i < 7 || (i >= 11 && i < 16);
        uint64_t inner_link = 0x78900 + i;
        regs_init(regs); regs[1] = (uintptr_t)(scoped + i); regs[2] = 1;
        tl_trutien_text_enter(regs, &inner_link, tl_trutien_extent_filter, NULL);
        assert(regs[2] == (hidden ? 0 : 1));
        if (hidden) {
            expect((const uint32_t *)(uintptr_t)regs[1], U"");
            assert(tl_trutien_text_leave() == 0x78900 + i);
        } else assert(inner_link == 0x78900 + i && regs[1] == (uintptr_t)(scoped + i));
    }
    uint64_t chunk_link = 0x89000;
    regs_init(regs); regs[1] = (uintptr_t)(scoped + 3); regs[7] = 2;
    tl_trutien_text_enter(regs, &chunk_link, tl_trutien_hud_counted_filter, NULL);
    assert(regs[7] == 0); expect((const uint32_t *)(uintptr_t)regs[1], U"");
    assert(tl_trutien_text_leave() == 0x89000);
    assert(tl_trutien_text_leave() == 0x67890);
    regs_init(regs); regs[1] = (uintptr_t)(scoped + 3); regs[7] = 2;
    memcpy(before, regs, sizeof(regs));
    tl_trutien_hud_counted_filter(regs, NULL);
    assert(!memcmp(before, regs, sizeof(regs))); /* no stale scope */

    static const uint32_t ordinary[] = U"x^2 ^^ ^ABC ^GHIJKL Việt A\u0302 \U0001f642 \uf1ff";
    regs_init(regs); regs[1] = (uintptr_t)ordinary;
    memcpy(before, regs, sizeof(regs));
    tl_trutien_hud_filter(regs, NULL);
    assert(!memcmp(before, regs, sizeof(regs)));

    /* Count-limited strings need no trailing NUL, and a token outside the
     * caller's count must neither be read nor removed. */
    uint32_t limited[] = { '^','F','F','F','F','F','F','X' };
    regs_init(regs); regs[3] = (uintptr_t)limited; regs[7] = 7;
    tl_trutien_font_filter(regs, NULL);
    expect((const uint32_t *)(uintptr_t)regs[3], U""); assert(regs[7] == 0);
    regs_init(regs); regs[3] = (uintptr_t)limited; regs[7] = 3;
    memcpy(before, regs, sizeof(regs));
    tl_trutien_font_filter(regs, NULL); assert(!memcmp(before, regs, sizeof(regs)));

    /* Scratch storage is bounded and reused. Failed growth leaves the raw
     * arguments unchanged, rather than publishing a truncated/dangling copy. */
    cleanup();
    fail_resize = true;
    regs_init(regs); regs[1] = (uintptr_t)source;
    memcpy(before, regs, sizeof(regs));
    tl_trutien_hud_filter(regs, NULL); assert(!memcmp(before, regs, sizeof(regs)));
    fail_resize = false;
    for (unsigned i = 0; i < TEXT_SCRATCH_SLOTS; i++) {
        regs_init(regs); regs[1] = (uintptr_t)source;
        tl_trutien_hud_filter(regs, NULL);
    }
    unsigned warmed = resizes;
    for (unsigned i = 0; i < 10000; i++) {
        regs_init(regs); regs[1] = (uintptr_t)source;
        tl_trutien_hud_filter(regs, NULL); expect((const uint32_t *)(uintptr_t)regs[1], wanted);
    }
    assert(resizes == warmed);

    /* More than the former four rotating slots remain alive concurrently.
     * Each original function owns its scratch until its return bridge runs. */
    const uint32_t *nested[TEXT_SCRATCH_SLOTS];
    uint64_t link;
    for (unsigned i = 0; i < TEXT_SCRATCH_SLOTS; i++) {
        regs_init(regs); regs[1] = (uintptr_t)source; link = 0x10000u + i;
        tl_trutien_text_enter(regs, &link, tl_trutien_hud_filter, NULL);
        assert(link == (uintptr_t)tl_entry_filter_return);
        nested[i] = (const uint32_t *)(uintptr_t)regs[1];
        for (unsigned j = 0; j <= i; j++) expect(nested[j], wanted);
    }
    regs_init(regs); regs[1] = (uintptr_t)source; link = 0x20000;
    memcpy(before, regs, sizeof(regs));
    tl_trutien_text_enter(regs, &link, tl_trutien_hud_filter, NULL);
    assert(link == 0x20000 && !memcmp(before, regs, sizeof(regs)));
    for (unsigned i = TEXT_SCRATCH_SLOTS; i > 0; i--) {
        expect(nested[i-1], wanted);
        assert(tl_trutien_text_leave() == 0x10000u + i - 1);
    }
    assert(!((text_scratch *)pthread_getspecific(text_key))->depth);
    regs_init(regs); regs[1] = (uintptr_t)ordinary; link = 0x30000;
    tl_trutien_text_enter(regs, &link, tl_trutien_hud_filter, NULL);
    assert(link == 0x30000 && !((text_scratch *)pthread_getspecific(text_key))->depth);

    /* The original positive count remains intact when it exceeds the fixed
     * inspection bound. Null text and count zero are also no-ops. */
    regs_init(regs); regs[3] = (uintptr_t)source; regs[7] = TEXT_LIMIT + 1;
    memcpy(before, regs, sizeof(regs));
    tl_trutien_font_filter(regs, NULL); assert(!memcmp(before, regs, sizeof(regs)));
    regs[3] = 0; regs[7] = 100; memcpy(before, regs, sizeof(regs));
    tl_trutien_font_filter(regs, NULL); assert(!memcmp(before, regs, sizeof(regs)));
    cleanup();
    return NULL;
}
int main(void)
{
    pthread_t thread;
    assert(!pthread_create(&thread, NULL, worker, NULL));
    assert(!pthread_join(thread, NULL));
    puts("PASS: native font/measure/HUD callbacks hide full/fragmented color controls, preserve Vietnamese/emoji/plain carets and original strings/registers, honor counts, reuse storage and survive allocation failure");
    return 0;
}
