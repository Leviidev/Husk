/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "husk-tl-trutien-text.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* Real ARM64 execution of the production register/return bridges. The static
 * entry has the same outer stub and supported original STP instruction as
 * the signed image encoder. No executable allocation or JIT is needed. */
#include "entry_filter_bridges_under_test.h"
void tl_entry_filter_common(void);

uint64_t text_entry_capture[28];
uint64_t text_return_capture[3];
void *text_entry_slots[] = { tl_entry_filter_common, tl_trutien_hud_filter };
static unsigned calls;
static const uint32_t text[] = U"^FFCB4AThất Lý động ^FFFF";
static const uint32_t clean[] = U"Thất Lý động ";

const char *tl_ld_lib_name(const tl_lib *lib) { (void)lib; return NULL; }
void *tl_ld_sym(tl_lib *lib, const char *name) { (void)lib; (void)name; return NULL; }
void tl_log_line(const char *format, ...) { (void)format; }

uint64_t call_text_entry(uint64_t, const uint32_t *, uint64_t, uint64_t, uint64_t,
                         uint64_t, uint64_t, uint64_t, uint64_t, uint64_t,
                         double, double, double, double, double, double, double, double);
static void invoke(unsigned depth)
{
    uint64_t value = call_text_entry(depth, text, 102, 103, 104, 105, 106, 107, 0xa0a0, 0xb0b0,
                                    1.25, 2.25, 3.25, 4.25, 5.25, 6.25, 7.25, 8.25);
    assert(value == 0x1234 && text_return_capture[0] == 0x1234);
    double fp;
    memcpy(&fp, &text_return_capture[1], sizeof(fp));
    assert(fp == 2.5 && text_return_capture[2] == 0x60000000);
}
void text_entry_body(uint64_t depth, const uint32_t *filtered)
{
    calls++;
    assert(!memcmp(filtered, clean, sizeof(clean)));
    assert(text_entry_capture[0] == depth && text_entry_capture[1] == (uintptr_t)filtered);
    for (unsigned i = 2; i < 8; i++) assert(text_entry_capture[i] == 100 + i);
    assert(text_entry_capture[8] == 0xa0a0 && text_entry_capture[9] == 0xb0b0);
    for (unsigned i = 0; i < 8; i++) {
        double fp;
        memcpy(&fp, &text_entry_capture[12 + i * 2], sizeof(fp));
        assert(fp == i + 1.25);
    }
    if (depth) invoke((unsigned)depth - 1);
    /* All nested entries have returned; the outer text is still intact. */
    assert(!memcmp(filtered, clean, sizeof(clean)));
}

__asm__(
    ".text\n.p2align 2\n"
    ".globl _call_text_entry\n_call_text_entry:\n"
    "sub sp, sp, #32\n stp x29, x30, [sp, #16]\n"
    "ldp x9, x10, [sp, #32]\n stp x9, x10, [sp]\n"
    "bl _test_text_entry\n"
    "adrp x9, _text_return_capture@PAGE\n add x9, x9, _text_return_capture@PAGEOFF\n"
    "str x0, [x9]\n str d0, [x9, #8]\n mrs x10, nzcv\n str x10, [x9, #16]\n"
    "ldp x29, x30, [sp, #16]\n add sp, sp, #32\n ret\n"
    ".p2align 2\n_test_text_entry:\n b _test_text_stub\n"
    "_test_text_resume:\n"
    "adrp x9, _text_entry_capture@PAGE\n add x9, x9, _text_entry_capture@PAGEOFF\n"
    "stp x0, x1, [x9]\n stp x2, x3, [x9, #16]\n"
    "stp x4, x5, [x9, #32]\n stp x6, x7, [x9, #48]\n"
    "ldp x10, x11, [sp, #96]\n stp x10, x11, [x9, #64]\n"
    "stp q0, q1, [x9, #96]\n stp q2, q3, [x9, #128]\n"
    "stp q4, q5, [x9, #160]\n stp q6, q7, [x9, #192]\n"
    "bl _text_entry_body\n"
    "mov x0, #0x1234\n fmov d0, #2.5\n cmp x0, x0\n"
    "ldp x29, x30, [sp], #96\n ret\n"
    ".p2align 2\n_test_text_stub:\n"
    "stp x29, x30, [sp, #-16]!\n stp x16, x17, [sp, #-16]!\n"
    "adrp x16, _text_entry_slots@PAGE\n ldr x16, [x16, _text_entry_slots@PAGEOFF]\n"
    "adrp x17, _text_entry_slots@PAGE\n add x17, x17, _text_entry_slots@PAGEOFF\n ldr x17, [x17, #8]\n"
    "blr x16\n ldp x16, x17, [sp], #16\n ldp x29, x30, [sp], #16\n"
    "stp x29, x30, [sp, #-96]!\n b _test_text_resume\n");

int main(void)
{
    tl_trutien_text_set_package("vn.trutien.client");
    invoke(9);
    assert(calls == 10 && !memcmp(text, U"^FFCB4AThất Lý động ^FFFF", sizeof(text)));
    puts("PASS: real Apple ARM64 entry/return bridges preserve integer/FP/stack arguments, return values, flags and ten nested text lifetimes without JIT");
    return 0;
}
