/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "husk-tl-internal.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned char image[32768];
static void w16(size_t at, unsigned n) { image[at] = (unsigned char)n; image[at+1] = (unsigned char)(n >> 8); }
static void w32(size_t at, unsigned n) { for (int i = 0; i < 4; i++) image[at+i] = (unsigned char)(n >> (8*i)); }
static void w64(size_t at, unsigned long long n) { for (int i = 0; i < 8; i++) image[at+i] = (unsigned char)(n >> (8*i)); }
static void fixture(unsigned imports)
{
    memset(image, 0, sizeof(image));
    memcpy(image, "\177ELF\2\1\1", 7); w16(16, 3); w16(18, 183); w32(20, 1);
    w64(32, 64); w16(52, 64); w16(54, 56); w16(56, 2);
    w32(64, 1); w32(68, 4); w64(96, sizeof(image)); w64(104, sizeof(image)); w64(112, 16384);
    w32(120, 2); w64(128, 0x200); w64(136, 0x200); w64(152, 7*16); w64(160, 7*16);
    const unsigned tags[] = {5, 10, 6, 11, 4, 1, 0};
    const unsigned vals[] = {0x4000, 0x2000, 0x1000, 24, 0x400, 1, 0};
    for (unsigned i = 0; i < 7; i++) { w64(0x200+i*16, tags[i]); w64(0x208+i*16, vals[i]); }
    w32(0x400, 1); w32(0x404, imports + 3);  /* null + imports + two exports */
    strcpy((char *)image + 0x4001, "libc.so");
    unsigned off = 9;
    for (unsigned i = 1; i <= imports + 2; i++) {
        char name[64];
        snprintf(name, sizeof(name), "symbol%03u", i);
        if (i == 1) strcpy(name, "iconv_open");
        if (i == 2) strcpy(name, "iconv");
        if (i == 3) strcpy(name, "iconv_close");
        if (i == imports + 1) strcpy(name, "JNI_OnLoad");
        if (i == imports + 2) strcpy(name, "SDL_main");
        w32(0x1000+i*24, off); image[0x1004+i*24] = i == 2 ? 0x22 : 0x12;
        if (i > imports) w16(0x1006+i*24, 1);
        strcpy((char *)image + 0x4000 + off, name); off += (unsigned)strlen(name) + 1;
    }
}
int main(void)
{
    tl_elf_report *r = malloc(sizeof(*r)); assert(r);
    fixture(3); tl_elf_analyze(image, sizeof(image), r);
    assert(r->ok && r->imports == 3 && r->imports_weak == 1 && r->import_name_count == 3);
    assert(r->imports_scanned && !r->import_names_truncated && !strcmp(r->import_names[0], "iconv_open"));
    assert(!strcmp(r->import_names[1], "iconv") && !strcmp(r->import_names[2], "iconv_close"));
    assert(r->exports_jni_onload && r->exports_sdl_main && r->needed_count == 1 && !strcmp(r->needed[0], "libc.so"));
    puts("PASS: ELF dynamic imports, weak binding, dependency and JNI/SDL exports");
    fixture(TL_MAX_IMPORT_NAMES + 4); tl_elf_analyze(image, sizeof(image), r);
    assert(r->ok && r->imports == TL_MAX_IMPORT_NAMES + 4 && r->import_name_count == TL_MAX_IMPORT_NAMES && r->import_names_truncated);
    puts("PASS: bounded import report records truncation without losing total count");
    fixture(3); w32(0x1000+24, 0x2000); tl_elf_analyze(image, sizeof(image), r);
    assert(r->ok && r->imports == 3 && r->import_name_count == 2 && r->import_names_truncated);
    fixture(3); tl_elf_analyze(image, 80, r); assert(!r->ok);
    fixture(3); w64(0x200+4*16, 0); tl_elf_analyze(image, sizeof(image), r);
    assert(r->ok && !r->imports_scanned); /* no hash/count: cannot claim a complete empty import list */
    puts("PASS: malformed string offset and truncated ELF handled without overread");
    free(r);
    return 0;
}
