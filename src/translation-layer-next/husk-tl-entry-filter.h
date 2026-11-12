/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef HUSK_TL_ENTRY_FILTER_H
#define HUSK_TL_ENTRY_FILTER_H
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

enum { TL_ENTRY_FILTER_WORDS = 16, TL_ENTRY_FILTER_BYTES = 64 };

/* Verified non-PC-relative first instructions of supported Tru Tien text
 * entries. The installer also checks the exact symbol/prologue pairing. */
static inline bool tl_entry_filter_first_supported(uint32_t first)
{
    return first == 0x39488008u /* ldrb w8, [x0, #0x220] */
        || first == 0xA9BA7BFDu /* stp x29, x30, [sp, #-96]! */
        || first == 0x6DB82BEBu /* stp d11, d10, [sp, #-128]! */
        || first == 0xA9BC7BFDu /* stp x29, x30, [sp, #-64]! */
        || first == 0xD10303FFu /* sub sp, sp, #192 */
        || first == 0xD10383FFu /* sub sp, sp, #224 */
        || first == 0xD10503FFu /* sub sp, sp, #320 */
        || first == 0xF9400408u /* ldr x8, [x0, #8] */
        || first == 0xF9400808u /* ldr x8, [x0, #16] */
        || first == 0xA9BB7BFDu; /* stp x29, x30, [sp, #-80]! */
}

static inline bool tl_entry_filter_branch(uintptr_t from, uintptr_t to, uint32_t *out)
{
    if ((from | to) & 3u || from > INT64_MAX || to > INT64_MAX) return false;
    int64_t bytes = (int64_t)to - (int64_t)from;
    if (bytes < -(INT64_C(1) << 27) || bytes >= (INT64_C(1) << 27)) return false;
    *out = 0x14000000u | ((uint32_t)(bytes / 4) & 0x03FFFFFFu);
    return true;
}

static inline bool tl_entry_filter_load(uintptr_t pc, uintptr_t slot, unsigned reg, uint32_t out[2])
{
    if ((pc & 3u) || (slot & 7u) || pc > INT64_MAX || slot > INT64_MAX) return false;
    int64_t pages = ((int64_t)(slot & ~(uintptr_t)4095) - (int64_t)(pc & ~(uintptr_t)4095)) / 4096;
    if (pages < -(INT64_C(1) << 20) || pages >= (INT64_C(1) << 20)) return false;
    uint32_t imm = (uint32_t)pages & 0x1FFFFFu;
    out[0] = 0x90000000u | ((imm & 3u) << 29) | ((imm >> 2) << 5) | reg;
    out[1] = 0xF9400000u | ((uint32_t)((slot & 4095u) / 8) << 10) | (reg << 5) | reg;
    return true;
}

/* Build an entry pre-filter without writing either destination until every
 * address is validated. The stub preserves the original SP/LR, calls through
 * two RW slots, executes the one verified original instruction, then resumes.
 * Code contains no absolute host addresses and is safe to sign ahead of time. */
static inline bool tl_entry_filter_encode(uint32_t out[TL_ENTRY_FILTER_WORDS], uint32_t *entry_branch,
                                          uintptr_t entry, uintptr_t stub, uintptr_t common_slot,
                                          uintptr_t callback_slot, uint32_t first)
{
    if (!out || !entry_branch || !tl_entry_filter_first_supported(first)
        || entry > UINTPTR_MAX - 4 || stub > UINTPTR_MAX - TL_ENTRY_FILTER_BYTES) return false;
    uint32_t branch, code[TL_ENTRY_FILTER_WORDS];
    for (unsigned i = 0; i < TL_ENTRY_FILTER_WORDS; i++) code[i] = 0xD503201Fu;
    if (!tl_entry_filter_branch(entry, stub, &branch)
        || !tl_entry_filter_load(stub + 8, common_slot, 16, code + 2)
        || !tl_entry_filter_load(stub + 16, callback_slot, 17, code + 4)
        || !tl_entry_filter_branch(stub + 40, entry + 4, code + 10)) return false;
    code[0] = 0xA9BF7BFDu; /* stp x29, x30, [sp, #-16]! */
    code[1] = 0xA9BF47F0u; /* stp x16, x17, [sp, #-16]! */
    code[6] = 0xD63F0200u; /* blr x16 (common, callback in x17) */
    code[7] = 0xA8C147F0u; /* ldp x16, x17, [sp], #16 */
    code[8] = 0xA8C17BFDu; /* ldp x29, x30, [sp], #16 */
    code[9] = first;
    memcpy(out, code, sizeof(code));
    *entry_branch = branch;
    return true;
}
#endif
