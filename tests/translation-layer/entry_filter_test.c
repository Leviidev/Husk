/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "husk-tl-entry-filter.h"
#include <assert.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>

static uintptr_t branch_target(uint32_t word, uintptr_t pc)
{
    assert((word & 0xFC000000u) == 0x14000000u);
    int64_t offset = word & 0x03FFFFFFu;
    if (offset & 0x02000000) offset -= 0x04000000;
    return (uintptr_t)((int64_t)pc + offset * 4);
}
static uintptr_t loaded_slot(const uint32_t *code, uintptr_t pc, unsigned reg)
{
    assert((code[0] & 0x9F00001Fu) == (0x90000000u | reg));
    assert((code[1] & 0xFFC003FFu) == (0xF9400000u | (reg << 5) | reg));
    int64_t pages = ((code[0] >> 29) & 3u) | (((code[0] >> 5) & 0x7FFFFu) << 2);
    if (pages & 0x100000) pages -= 0x200000;
    return (uintptr_t)((int64_t)(pc & ~(uintptr_t)4095) + pages * 4096)
        + ((code[1] >> 10) & 4095u) * 8;
}

#define PAGE 16384u
#define TL_PAGE_X 1u
#define SLOT_ENTRY_COMMON 16u
#define SLOT_TEXT_FONT 24u
#define SLOT_TEXT_EXTENT 32u
#define SLOT_TEXT_HUD 40u
#define SLOT_TEXT_PLAIN 48u
#define SLOT_TEXT_FLOAT 56u
#define SLOT_TEXT_CLIPPED 64u
#define SLOT_TEXT_LEGACY_EXTENT 72u
#define SLOT_TEXT_PARAMS 80u
#define PREFIX 128u
#define ENTRIES 13u
typedef struct { uint64_t st_value, st_size; uint8_t st_info; uint16_t st_shndx; } elf_sym;
typedef struct {
    const char *name;
    bool aot;
    uint8_t *rx, *rw, *slots, *pflags, *stub_rx, *stub_rw;
    size_t npages, stub_used, stub_cap;
    uint64_t base_vaddr;
} tl_lib;
static elf_sym symbols[ENTRIES];
static bool marker = true;
static int missing = -1;
static const char *const names[] = {
    "_ZN9A3DFTFont8TextOutWEiiPKwjbjfifbjf",
    "_ZN9A3DFTFont13GetTextExtentEPKwib",
    "_ZN12CHudDrawList7AddTextEPKwRK7HUDRECTdjiiji",
    "_Z17AUI_TextOutFormatP9A3DFTFontiiPKwP5ARectIiEiPiibS6_P9A2DSpritePN5abase6vectorI20_AUITEXTAREA_EMOTIONNS9_13default_allocEEES6_biibP15EditBoxItemsSetPNSA_IS8_SC_EEfbPNSA_I16FONT_WITH_SHADOWSC_EEiP15TEXTITEM_RCINFOb",
    "_Z15AUI_GetTextRectP9A3DFTFontPKwRiS3_S3_iP15EditBoxItemsSetiiPN5abase6vectorIP9A2DSpriteNS6_13default_allocEEEfPNS7_I16FONT_WITH_SHADOWSA_EEbi",
    "_ZN9A3DFTFont11TextOutVertEiiPKwjbjfifbjfj",
    "_ZN9A3DFTFont8TextOutFEffPKwjbjfif",
    "_ZN9A3DFTFont9DrawTextWEiiPKwjP5ARectIiEbjfifbjf",
    "_ZN7A3DFont8TextOutWEiiPKwjifb",
    "_ZN7A3DFont9DrawTextWEiiPKwjP5ARectIiEif",
    "_ZN7A3DFont13GetTextExtentEPKwiPiS2_",
    "_ZN10A3DFontMan13GetTextExtentEjPKwiPiS2_",
    "_Z17AUI_TextOutFormatR16TAG_OP_IN_PARAMSR17TAG_OP_OUT_PARAMS",
};
static const uint32_t firsts[] = { 0x39488008u, 0xA9BA7BFDu, 0x6DB82BEBu, 0xA9BC7BFDu, 0xD10303FFu, 0x39488008u, 0xD10503FFu, 0x39488008u, 0xF9400408u, 0xF9400408u, 0xF9400808u, 0xA9BB7BFDu, 0xD10383FFu };
static const unsigned slots[] = { SLOT_TEXT_FONT, SLOT_TEXT_EXTENT, SLOT_TEXT_HUD, SLOT_TEXT_PLAIN, SLOT_TEXT_HUD, SLOT_TEXT_FONT, SLOT_TEXT_FLOAT, SLOT_TEXT_CLIPPED, SLOT_TEXT_PLAIN, SLOT_TEXT_PLAIN, SLOT_TEXT_EXTENT, SLOT_TEXT_LEGACY_EXTENT, SLOT_TEXT_PARAMS };
static const char *base_name(const char *path) { const char *p = strrchr(path, '/'); return p ? p + 1 : path; }
static const elf_sym *lib_find(const tl_lib *lib, const char *name)
{
    (void)lib;
    if (!strcmp(name, "_ZN18AUIRichTextPrinter14ms_TextPrinterE")) return marker ? &symbols[0] : NULL;
    for (unsigned i = 0; i < ENTRIES; i++) if (!strcmp(name, names[i])) return missing == (int)i ? NULL : &symbols[i];
    return NULL;
}
static void tl_log_line(const char *fmt, ...) { (void)fmt; }
#include "entry_filter_install_under_test.h"

static uint8_t image[2 * PAGE], stubs[PAGE], runtime_slots[PREFIX], flags[2];
static tl_lib fixture(bool separate_slots)
{
    memset(image, 0xAA, sizeof(image)); memset(stubs, 0xCC, sizeof(stubs));
    memset(runtime_slots, 0xDD, sizeof(runtime_slots)); memset(flags, TL_PAGE_X, sizeof(flags));
    marker = true; missing = -1;
    for (unsigned i = 0; i < ENTRIES; i++) {
        symbols[i] = (elf_sym){ .st_value = 0x1000 + 0x100 * (i + 1), .st_size = 100, .st_info = 2, .st_shndx = 1 };
        memcpy(image + 0x100 * (i + 1), firsts + i, 4);
    }
    return (tl_lib){ .name = "/native/libmain.so", .rx = image, .rw = image,
        .slots = separate_slots ? runtime_slots : stubs, .pflags = flags, .stub_rx = stubs,
        .stub_rw = stubs, .npages = 2, .base_vaddr = 0x1000, .stub_used = PREFIX, .stub_cap = sizeof(stubs) };
}
static void unchanged(const tl_lib *lib)
{
    assert(lib->stub_used == PREFIX);
    for (unsigned i = 0; i < ENTRIES; i++) {
        uint32_t word; memcpy(&word, image + 0x100 * (i + 1), 4); assert(word == firsts[i]);
    }
    for (unsigned i = 0; i < sizeof(stubs); i++) assert(stubs[i] == 0xCC);
}
static void installed(bool separate_slots)
{
    tl_lib lib = fixture(separate_slots);
    assert(patch_trutien_text_entries(&lib) && lib.stub_used == PREFIX + ENTRIES * TL_ENTRY_FILTER_BYTES);
    for (unsigned i = 0; i < PREFIX; i++) assert(stubs[i] == 0xCC); /* RW JIT slots never overlap code. */
    for (unsigned i = 0; i < ENTRIES; i++) {
        uintptr_t entry = (uintptr_t)(image + 0x100 * (i + 1));
        uintptr_t stub = (uintptr_t)(stubs + PREFIX + i * TL_ENTRY_FILTER_BYTES);
        uint32_t branch, code[TL_ENTRY_FILTER_WORDS]; memcpy(&branch, (void *)entry, 4); memcpy(code, (void *)stub, sizeof(code));
        assert(branch_target(branch, entry) == stub);
        assert(loaded_slot(code + 2, stub + 8, 16) == (uintptr_t)lib.slots + SLOT_ENTRY_COMMON);
        assert(loaded_slot(code + 4, stub + 16, 17) == (uintptr_t)lib.slots + slots[i]);
        assert(code[0] == 0xA9BF7BFDu && code[1] == 0xA9BF47F0u && code[6] == 0xD63F0200u);
        assert(code[7] == 0xA8C147F0u && code[8] == 0xA8C17BFDu && code[9] == firsts[i]);
        assert(branch_target(code[10], stub + 40) == entry + 4);
    }
}

int main(void)
{
    uint32_t code[TL_ENTRY_FILTER_WORDS], previous[TL_ENTRY_FILTER_WORDS], branch = 0x12345678;
    memset(code, 0xEE, sizeof(code)); memcpy(previous, code, sizeof(code));
    uintptr_t entry = 0x10001000, stub = 0x10102000, common = 0x10105010, callback = 0x10105018;
    assert(tl_entry_filter_encode(code, &branch, entry, stub, common, callback, firsts[0]));
    assert(branch_target(branch, entry) == stub && branch_target(code[10], stub + 40) == entry + 4);
    assert(loaded_slot(code + 2, stub + 8, 16) == common && loaded_slot(code + 4, stub + 16, 17) == callback);
    assert(tl_entry_filter_encode(code, &branch, stub, entry, common, callback, firsts[1])); /* negative branch */
    memcpy(code, previous, sizeof(code)); branch = 0x12345678;
    assert(!tl_entry_filter_encode(code, &branch, entry + 1, stub, common, callback, firsts[0]));
    assert(!tl_entry_filter_encode(code, &branch, entry, entry + ((uintptr_t)1 << 27), common, callback, firsts[0]));
    assert(!tl_entry_filter_encode(code, &branch, entry, stub, common + 1, callback, firsts[0]));
    assert(!tl_entry_filter_encode(code, &branch, entry, stub, stub + ((uintptr_t)1 << 32), callback, firsts[0]));
    assert(!tl_entry_filter_encode(code, &branch, entry, stub, common, callback, 0x90000000u)); /* PC-relative ADRP rejected */
    assert(!tl_entry_filter_encode(code, &branch, entry, UINTPTR_MAX - 3, common, callback, firsts[0]));
    assert(branch == 0x12345678 && !memcmp(code, previous, sizeof(code)));
    installed(false); installed(true);
    tl_lib lib = fixture(true); lib.aot = true;
    assert(!patch_trutien_text_entries(&lib)); unchanged(&lib); /* imported signed code stays untouched */
    lib = fixture(true); missing = 1;
    assert(!patch_trutien_text_entries(&lib)); unchanged(&lib);
    lib = fixture(true); lib.stub_cap = PREFIX + ENTRIES * TL_ENTRY_FILTER_BYTES - 1;
    assert(!patch_trutien_text_entries(&lib)); unchanged(&lib);
    lib = fixture(true); marker = false;
    assert(!patch_trutien_text_entries(&lib)); unchanged(&lib);
    lib = fixture(true); lib.name = "other.so";
    assert(!patch_trutien_text_entries(&lib)); unchanged(&lib);
    lib = fixture(true); symbols[2].st_info = 1;
    assert(!patch_trutien_text_entries(&lib)); unchanged(&lib);
    lib = fixture(true); uint32_t bad = 0; memcpy(image + 0x200, &bad, 4);
    assert(!patch_trutien_text_entries(&lib) && lib.stub_used == PREFIX);
    uint32_t word; memcpy(&word, image + 0x100, 4); assert(word == firsts[0]);
    for (unsigned i = 0; i < sizeof(stubs); i++) assert(stubs[i] == 0xCC); /* no first-entry partial patch */
    puts("PASS: actual entry encoder and install preflight preserve signed-code immutability, exact prologues, stack/slot layout, branch targets and all-or-none installation");
    return 0;
}
