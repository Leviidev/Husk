/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * What an Android native library will ask of Husk's loader, read from the
 * library itself.
 *
 * Android's .so files are ELF, built for Android's own C library, and laid out
 * for Android's page size. None of that is native to iOS, so each library has
 * to be loaded by Husk rather than by dyld, into executable memory that only a
 * debugger (or TrollStore) can grant. Whether that can work for a given
 * library is decided by a handful of facts that are all in the file: how its
 * segments fall on 16 KiB pages, which relocation types it uses, whether its
 * code makes Linux system calls directly. This reads them.
 *
 * Written against the ELF specification and the AArch64 ELF ABI rather than a
 * system header: the iOS SDK has no <elf.h>. All reads are little-endian and
 * byte-wise, so nothing depends on the host or on the file's alignment.
 */
#include "husk-tl-internal.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define EI_CLASS        4
#define EI_DATA         5
#define ELFCLASS64      2
#define ELFDATA2LSB     1
#define ET_DYN          3
#define EM_AARCH64      183

#define PT_LOAD         1
#define PT_DYNAMIC      2
#define PT_TLS          7
#define PT_GNU_RELRO    0x6474e552u

#define PF_X            1u
#define PF_W            2u

#define SHT_NOBITS      8u
#define SHF_EXECINSTR   4u

#define DT_NULL         0
#define DT_NEEDED       1
#define DT_PLTRELSZ     2
#define DT_HASH         4
#define DT_STRTAB       5
#define DT_SYMTAB       6
#define DT_RELA         7
#define DT_RELASZ       8
#define DT_STRSZ        10
#define DT_SYMENT       11
#define DT_SONAME       14
#define DT_REL          17
#define DT_RELSZ        18
#define DT_PLTREL       20
#define DT_TEXTREL      22
#define DT_JMPREL       23
#define DT_FLAGS        30
#define DT_RELRSZ       35
#define DT_RELR         36
#define DT_GNU_HASH         0x6ffffef5
#define DT_ANDROID_REL      0x6000000f
#define DT_ANDROID_RELSZ    0x60000010
#define DT_ANDROID_RELA     0x60000011
#define DT_ANDROID_RELASZ   0x60000012
#define DT_ANDROID_RELR     0x6fffe000
#define DT_ANDROID_RELRSZ   0x6fffe001
#define DF_TEXTREL      4u

/* AArch64 dynamic relocations the loader will implement. */
#define R_AARCH64_NONE          0
#define R_AARCH64_ABS64         257
#define R_AARCH64_GLOB_DAT      1025
#define R_AARCH64_JUMP_SLOT     1026
#define R_AARCH64_RELATIVE      1027
#define R_AARCH64_TLS_DTPMOD64  1028
#define R_AARCH64_TLS_DTPREL64  1029
#define R_AARCH64_TLS_TPREL64   1030
#define R_AARCH64_TLSDESC       1031
#define R_AARCH64_IRELATIVE     1032

#define TL_PAGE         16384u
#define MAX_LOADS       64
#define MAX_PAGES       (1u << 20)      /* 16 GiB of image */
#define MAX_SYMBOLS     (1u << 22)
#define MAX_PACKED      (1u << 26)

static uint16_t rd16(const uint8_t *p) { return (uint16_t)(p[0] | (p[1] << 8)); }
static uint32_t rd32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16)
         | ((uint32_t)p[3] << 24);
}
static uint64_t rd64(const uint8_t *p) { return rd32(p) | ((uint64_t)rd32(p + 4) << 32); }

typedef struct elf {
    const uint8_t *d;
    size_t         n;
    tl_segment     loads[MAX_LOADS];
    uint64_t       load_off[MAX_LOADS];
    uint64_t       load_filesz[MAX_LOADS];
    size_t         nloads;
    bool           has_relro;
    tl_segment     relro;
    bool           has_dyn;
    uint64_t       dyn_off, dyn_size;
} elf;

static void set_error(tl_elf_report *r, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(r->error, sizeof(r->error), fmt, ap);
    va_end(ap);
    r->ok = false;
}

static bool in_file(const elf *e, uint64_t off, uint64_t len)
{
    return off <= e->n && len <= e->n - off;
}

/* Where the bytes at a virtual address are in the file, if they are there at
 * all -- .bss, for one, has an address and no bytes. */
static bool vaddr_to_off(const elf *e, uint64_t vaddr, uint64_t len, uint64_t *off)
{
    for (size_t i = 0; i < e->nloads; i++) {
        uint64_t lv = e->loads[i].vaddr, fs = e->load_filesz[i];
        if (vaddr >= lv && vaddr - lv <= fs && len <= fs - (vaddr - lv)) {
            uint64_t o = e->load_off[i] + (vaddr - lv);
            if (!in_file(e, o, len)) {
                return false;
            }
            *off = o;
            return true;
        }
    }
    return false;
}

static bool u32_at(const elf *e, uint64_t vaddr, uint32_t *v)
{
    uint64_t off;
    if (!vaddr_to_off(e, vaddr, 4, &off)) {
        return false;
    }
    *v = rd32(e->d + off);
    return true;
}

/* ------------------------------------------------------------- page plan */

static void mark(uint8_t *flags, uint64_t start, uint64_t page,
                 uint64_t lo, uint64_t hi, uint8_t bit)
{
    if (hi <= lo) {
        return;
    }
    uint64_t first = (lo - start) / page, last = (hi - 1 - start) / page;
    for (uint64_t i = first; i <= last; i++) {
        flags[i] |= bit;
    }
}

size_t tl_page_plan(const tl_segment *loads, size_t nloads,
                    const tl_segment *relro, uint64_t page,
                    uint8_t *flags, size_t max, uint64_t *base)
{
    if (page == 0 || (page & (page - 1)) != 0) {
        return 0;
    }
    uint64_t lo = UINT64_MAX, hi = 0;
    for (size_t i = 0; i < nloads; i++) {
        if (loads[i].memsz == 0) {
            continue;
        }
        if (loads[i].vaddr > UINT64_MAX - loads[i].memsz) {
            return 0;
        }
        if (loads[i].vaddr < lo) lo = loads[i].vaddr;
        if (loads[i].vaddr + loads[i].memsz > hi) hi = loads[i].vaddr + loads[i].memsz;
    }
    if (lo == UINT64_MAX || hi > UINT64_MAX - (page - 1)) {
        return 0;
    }
    uint64_t start = lo & ~(page - 1);
    uint64_t end = (hi + page - 1) & ~(page - 1);
    uint64_t n = (end - start) / page;
    if (base) {
        *base = start;
    }
    /* Without somewhere to write, this is a question about size. */
    if (!flags) {
        return (size_t)n;
    }
    if (n == 0 || n > max) {
        return 0;
    }
    memset(flags, 0, (size_t)n);

    uint64_t r0 = relro ? relro->vaddr : 0;
    uint64_t r1 = relro ? relro->vaddr + relro->memsz : 0;
    for (size_t i = 0; i < nloads; i++) {
        const tl_segment *s = &loads[i];
        if (s->memsz == 0) {
            continue;
        }
        uint64_t a = s->vaddr, b = s->vaddr + s->memsz;
        mark(flags, start, page, a, b, TL_PAGE_R);
        if (s->flags & PF_X) {
            mark(flags, start, page, a, b, TL_PAGE_X);
        }
        if (s->flags & PF_W) {
            if (!relro || r1 <= a || r0 >= b) {
                mark(flags, start, page, a, b, TL_PAGE_W);
            } else {
                /* What is left of [a, b) either side of RELRO. */
                mark(flags, start, page, a, r0 > a ? r0 : a, TL_PAGE_W);
                mark(flags, start, page, r1 < b ? r1 : b, b, TL_PAGE_W);
            }
        }
    }
    return (size_t)n;
}

static char page_class(uint8_t f)
{
    if ((f & TL_PAGE_X) && (f & TL_PAGE_W)) return '!';
    if (f & TL_PAGE_X) return 'X';
    if (f & TL_PAGE_W) return 'W';
    if (f & TL_PAGE_R) return 'R';
    return '-';
}

/* "R1 X4 R1 W2": a class and how many pages of it in a row. */
static void summarise_layout(const uint8_t *flags, size_t n, char *out, size_t cap)
{
    size_t len = 0;
    out[0] = '\0';
    for (size_t i = 0; i < n;) {
        char c = page_class(flags[i]);
        size_t run = 1;
        while (i + run < n && page_class(flags[i + run]) == c) {
            run++;
        }
        char piece[32];
        int w = snprintf(piece, sizeof(piece), "%s%c%zu", len ? " " : "", c, run);
        if (w < 0 || len + (size_t)w + 4 >= cap) {
            if (len + 4 < cap) {
                memcpy(out + len, " ...", 5);
            }
            return;
        }
        memcpy(out + len, piece, (size_t)w + 1);
        len += (size_t)w;
        i += run;
    }
}

/* ------------------------------------------------------------------ code */

/*
 * Instructions that matter to an iOS host, counted by pattern. A word in a
 * code section that matches is almost always the instruction -- AArch64
 * compilers put constants in .rodata, not in the instruction stream -- but
 * these are counts to judge by, not a disassembly.
 */
static void scan_code(const uint8_t *p, uint64_t len, tl_elf_report *r)
{
    for (uint64_t i = 0; i + 4 <= len; i += 4) {
        uint32_t insn = rd32(p + i);
        if ((insn & 0xFFE0001Fu) == 0xD4000001u) {
            r->svc++;               /* svc #imm: a Linux system call */
        } else if ((insn & 0xFFFFFFE0u) == 0xD53BD040u) {
            r->tpidr_reads++;       /* mrs Xt, tpidr_el0 */
        } else if ((insn & 0xFFFFFFE0u) == 0xD51BD040u) {
            r->tpidr_writes++;      /* msr tpidr_el0, Xt */
        }
    }
}

static void scan_all_code(const elf *e, tl_elf_report *r)
{
    const uint8_t *h = e->d;
    uint64_t shoff = rd64(h + 40);
    uint16_t shentsize = rd16(h + 58), shnum = rd16(h + 60);
    bool scanned = false;

    /* Executable sections when there are section headers; they exclude the
     * read-only data that some linkers put in the executable segment. */
    if (shoff && shnum && shentsize == 64 && in_file(e, shoff, (uint64_t)shnum * 64)) {
        for (uint16_t i = 0; i < shnum; i++) {
            const uint8_t *s = h + shoff + (uint64_t)i * 64;
            uint32_t type = rd32(s + 4);
            uint64_t flags = rd64(s + 8), off = rd64(s + 24), size = rd64(s + 32);
            if (!(flags & SHF_EXECINSTR) || type == SHT_NOBITS || !in_file(e, off, size)) {
                continue;
            }
            scan_code(h + off, size, r);
            scanned = true;
        }
    }
    if (scanned) {
        return;
    }
    for (size_t i = 0; i < e->nloads; i++) {
        if ((e->loads[i].flags & PF_X) && in_file(e, e->load_off[i], e->load_filesz[i])) {
            scan_code(h + e->load_off[i], e->load_filesz[i], r);
        }
    }
}

/* ----------------------------------------------------------- relocations */

static void count_reloc(tl_elf_report *r, uint32_t type)
{
    r->relocs++;
    switch (type) {
    case R_AARCH64_NONE:
    case R_AARCH64_ABS64:
    case R_AARCH64_GLOB_DAT:
    case R_AARCH64_JUMP_SLOT:
    case R_AARCH64_RELATIVE:
    case R_AARCH64_IRELATIVE:
        break;
    case R_AARCH64_TLS_DTPMOD64:
    case R_AARCH64_TLS_DTPREL64:
    case R_AARCH64_TLS_TPREL64:
    case R_AARCH64_TLSDESC:
        r->relocs_tls++;
        break;
    default:
        if (r->relocs_unsupported++ == 0) {
            r->first_unsupported = type;
        }
        break;
    }
}

static void count_table(const elf *e, uint64_t vaddr, uint64_t size, uint64_t entsize,
                        tl_elf_report *r)
{
    uint64_t off;
    if (!vaddr || !size || !vaddr_to_off(e, vaddr, size, &off)) {
        return;
    }
    for (uint64_t i = 0; i + entsize <= size; i += entsize) {
        count_reloc(r, (uint32_t)rd64(e->d + off + i + 8));
    }
}

typedef struct sleb_reader {
    const uint8_t *p, *end;
    bool bad;
} sleb_reader;

static int64_t sleb(sleb_reader *s)
{
    uint64_t v = 0;
    unsigned shift = 0;
    uint8_t b;
    do {
        if (s->p >= s->end || shift >= 64) {
            s->bad = true;
            return 0;
        }
        b = *s->p++;
        v |= (uint64_t)(b & 0x7f) << shift;
        shift += 7;
    } while (b & 0x80);
    if (shift < 64 && (b & 0x40)) {
        v |= ~0ull << shift;
    }
    return (int64_t)v;
}

/*
 * Android's packed relocations ("APS2", from lld's --pack-dyn-relocs=android):
 * a count, a starting offset, then groups of relocations that share their
 * offset step, their info word or their addend, all SLEB128.
 */
#define GROUPED_BY_INFO         1
#define GROUPED_BY_OFFSET_DELTA 2
#define GROUPED_BY_ADDEND       4
#define GROUP_HAS_ADDEND        8

static bool count_packed(const elf *e, uint64_t vaddr, uint64_t size, bool rela,
                         tl_elf_report *r)
{
    uint64_t off;
    if (!vaddr_to_off(e, vaddr, size, &off) || size < 4
        || memcmp(e->d + off, "APS2", 4) != 0) {
        return false;
    }
    sleb_reader s = { e->d + off + 4, e->d + off + size, false };
    int64_t count = sleb(&s);
    (void)sleb(&s);                 /* initial offset */
    if (s.bad || count < 0 || count > MAX_PACKED) {
        return false;
    }
    int64_t done = 0;
    uint64_t info = 0;
    while (done < count) {
        int64_t group = sleb(&s), gflags = sleb(&s);
        if (s.bad || group <= 0 || group > count - done) {
            return false;
        }
        if (gflags & GROUPED_BY_OFFSET_DELTA) {
            (void)sleb(&s);
        }
        if (gflags & GROUPED_BY_INFO) {
            info = (uint64_t)sleb(&s);
        }
        if (rela) {
            if ((gflags & GROUP_HAS_ADDEND) && (gflags & GROUPED_BY_ADDEND)) {
                (void)sleb(&s);
            } else if (!(gflags & GROUP_HAS_ADDEND) && (gflags & GROUPED_BY_ADDEND)) {
                return false;
            }
        }
        for (int64_t i = 0; i < group; i++) {
            if (!(gflags & GROUPED_BY_OFFSET_DELTA)) {
                (void)sleb(&s);
            }
            if (!(gflags & GROUPED_BY_INFO)) {
                info = (uint64_t)sleb(&s);
            }
            if (rela && (gflags & GROUP_HAS_ADDEND) && !(gflags & GROUPED_BY_ADDEND)) {
                (void)sleb(&s);
            }
            if (s.bad) {
                return false;
            }
            count_reloc(r, (uint32_t)(info & 0xffffffffu));
        }
        done += group;
    }
    return true;
}

/* RELR: an address word relocates one slot, a bitmap word (low bit set)
 * relocates one slot per other set bit. All of them are RELATIVE. */
static bool count_relr(const elf *e, uint64_t vaddr, uint64_t size, tl_elf_report *r)
{
    uint64_t off;
    if (!vaddr_to_off(e, vaddr, size, &off)) {
        return false;
    }
    for (uint64_t i = 0; i + 8 <= size; i += 8) {
        uint64_t w = rd64(e->d + off + i);
        if ((w & 1) == 0) {
            count_reloc(r, R_AARCH64_RELATIVE);
        } else {
            for (w >>= 1; w; w &= w - 1) {
                count_reloc(r, R_AARCH64_RELATIVE);
            }
        }
    }
    return true;
}

/* -------------------------------------------------------------- symbols */

/* How many dynamic symbols there are. ELF does not record it; the hash table
 * implies it. */
static uint64_t symbol_count(const elf *e, uint64_t hash, uint64_t gnu_hash)
{
    uint32_t v;
    if (hash && u32_at(e, hash + 4, &v)) {
        return v;           /* nchain */
    }
    if (!gnu_hash) {
        return 0;
    }
    uint32_t nbuckets, symoffset, bloom_size;
    if (!u32_at(e, gnu_hash, &nbuckets) || !u32_at(e, gnu_hash + 4, &symoffset)
        || !u32_at(e, gnu_hash + 8, &bloom_size) || nbuckets > MAX_SYMBOLS
        || bloom_size > MAX_SYMBOLS) {
        return 0;
    }
    uint64_t buckets = gnu_hash + 16 + (uint64_t)bloom_size * 8;
    uint64_t chains = buckets + (uint64_t)nbuckets * 4;
    uint32_t last = 0;
    for (uint32_t i = 0; i < nbuckets; i++) {
        uint32_t b;
        if (!u32_at(e, buckets + (uint64_t)i * 4, &b)) {
            return 0;
        }
        if (b > last) last = b;
    }
    if (last < symoffset) {
        return symoffset;
    }
    /* Walk the last bucket's chain to its end marker. */
    for (uint64_t i = last; i < MAX_SYMBOLS; i++) {
        uint32_t c;
        if (!u32_at(e, chains + (i - symoffset) * 4, &c)) {
            return 0;
        }
        if (c & 1) {
            return i + 1;
        }
    }
    return 0;
}

static void copy_string(const elf *e, uint64_t strtab_off, uint64_t strsz,
                        uint64_t index, char *out, size_t cap)
{
    out[0] = '\0';
    if (index >= strsz) {
        return;
    }
    size_t i = 0;
    for (; i + 1 < cap && index + i < strsz; i++) {
        char c = (char)e->d[strtab_off + index + i];
        if (!c) break;
        out[i] = c;
    }
    out[i] = '\0';
}

/* --------------------------------------------------------------- dynamic */

typedef struct dyninfo {
    uint64_t strtab, strsz, symtab, syment, hash, gnu_hash;
    uint64_t rela, relasz, rel, relsz, jmprel, pltrelsz, pltrel;
    uint64_t arela, arelasz, arel, arelsz, relr, relrsz, arelr, arelrsz;
    uint64_t soname, flags;
    bool     has_soname, textrel;
    uint64_t needed[TL_MAX_NEEDED];
    int      nneeded;
} dyninfo;

static void read_dynamic(const elf *e, dyninfo *d)
{
    memset(d, 0, sizeof(*d));
    uint64_t count = e->dyn_size / 16;
    if (count > 4096) count = 4096;
    for (uint64_t i = 0; i < count; i++) {
        const uint8_t *p = e->d + e->dyn_off + i * 16;
        int64_t tag = (int64_t)rd64(p);
        uint64_t val = rd64(p + 8);
        switch (tag) {
        case DT_NULL:           return;
        case DT_NEEDED:
            if (d->nneeded < TL_MAX_NEEDED) d->needed[d->nneeded++] = val;
            break;
        case DT_PLTRELSZ:       d->pltrelsz = val; break;
        case DT_HASH:           d->hash = val; break;
        case DT_STRTAB:         d->strtab = val; break;
        case DT_SYMTAB:         d->symtab = val; break;
        case DT_RELA:           d->rela = val; break;
        case DT_RELASZ:         d->relasz = val; break;
        case DT_STRSZ:          d->strsz = val; break;
        case DT_SYMENT:         d->syment = val; break;
        case DT_SONAME:         d->soname = val; d->has_soname = true; break;
        case DT_REL:            d->rel = val; break;
        case DT_RELSZ:          d->relsz = val; break;
        case DT_PLTREL:         d->pltrel = val; break;
        case DT_TEXTREL:        d->textrel = true; break;
        case DT_JMPREL:         d->jmprel = val; break;
        case DT_FLAGS:          d->flags = val; break;
        case DT_RELRSZ:         d->relrsz = val; break;
        case DT_RELR:           d->relr = val; break;
        case DT_GNU_HASH:       d->gnu_hash = val; break;
        case DT_ANDROID_REL:    d->arel = val; break;
        case DT_ANDROID_RELSZ:  d->arelsz = val; break;
        case DT_ANDROID_RELA:   d->arela = val; break;
        case DT_ANDROID_RELASZ: d->arelasz = val; break;
        case DT_ANDROID_RELR:   d->arelr = val; break;
        case DT_ANDROID_RELRSZ: d->arelrsz = val; break;
        default:                break;
        }
    }
}

static void analyze_dynamic(const elf *e, tl_elf_report *r)
{
    dyninfo d;
    read_dynamic(e, &d);
    r->textrel = d.textrel || (d.flags & DF_TEXTREL);

    uint64_t stroff = 0;
    bool have_strings = d.strtab && d.strsz && vaddr_to_off(e, d.strtab, d.strsz, &stroff);
    if (have_strings) {
        for (int i = 0; i < d.nneeded; i++) {
            copy_string(e, stroff, d.strsz, d.needed[i], r->needed[r->needed_count],
                        sizeof(r->needed[0]));
            if (r->needed[r->needed_count][0]) {
                r->needed_count++;
            }
        }
        if (d.has_soname) {
            copy_string(e, stroff, d.strsz, d.soname, r->soname, sizeof(r->soname));
        }
    }

    count_table(e, d.rela, d.relasz, 24, r);
    count_table(e, d.rel, d.relsz, 16, r);
    count_table(e, d.jmprel, d.pltrelsz, d.pltrel == DT_REL ? 16 : 24, r);

    r->packing = "none";
    if ((d.arela && count_packed(e, d.arela, d.arelasz, true, r))
        || (d.arel && count_packed(e, d.arel, d.arelsz, false, r))) {
        r->packing = "android";
    }
    if ((d.relr && count_relr(e, d.relr, d.relrsz, r))
        || (d.arelr && count_relr(e, d.arelr, d.arelrsz, r))) {
        r->packing = strcmp(r->packing, "android") == 0 ? "android+relr" : "relr";
    }

    uint64_t nsyms = symbol_count(e, d.hash, d.gnu_hash);
    uint64_t symoff;
    if (d.symtab && nsyms && nsyms < MAX_SYMBOLS && (d.syment == 0 || d.syment == 24)
        && vaddr_to_off(e, d.symtab, nsyms * 24, &symoff)) {
        for (uint64_t i = 1; i < nsyms; i++) {
            const uint8_t *s = e->d + symoff + i * 24;
            if (rd16(s + 6) == 0 && rd32(s) != 0) {     /* SHN_UNDEF, named */
                r->imports++;
            }
        }
    }
}

/* ------------------------------------------------------------------ top */

void tl_elf_analyze(const uint8_t *data, size_t size, tl_elf_report *r)
{
    memset(r, 0, sizeof(*r));
    r->packing = "none";
    r->ok = true;

    if (size < 64 || memcmp(data, "\x7f" "ELF", 4) != 0) {
        set_error(r, "not an ELF file");
        return;
    }
    r->is64 = data[EI_CLASS] == ELFCLASS64;
    r->machine = rd16(data + 18);
    if (data[EI_DATA] != ELFDATA2LSB) {
        set_error(r, "big-endian ELF");
        return;
    }
    if (!r->is64 || r->machine != EM_AARCH64) {
        set_error(r, "not arm64 code (ELF machine %u, %s)", r->machine,
                  r->is64 ? "64-bit" : "32-bit");
        return;
    }
    if (rd16(data + 16) != ET_DYN) {
        set_error(r, "not a shared library");
        return;
    }

    elf e;
    memset(&e, 0, sizeof(e));
    e.d = data;
    e.n = size;
    uint64_t phoff = rd64(data + 32);
    uint16_t phentsize = rd16(data + 54), phnum = rd16(data + 56);
    if (phentsize != 56 || phnum == 0 || !in_file(&e, phoff, (uint64_t)phnum * 56)) {
        set_error(r, "damaged program headers");
        return;
    }

    for (uint16_t i = 0; i < phnum; i++) {
        const uint8_t *p = data + phoff + (uint64_t)i * 56;
        uint32_t type = rd32(p), flags = rd32(p + 4);
        uint64_t off = rd64(p + 8), vaddr = rd64(p + 16);
        uint64_t filesz = rd64(p + 32), memsz = rd64(p + 40), align = rd64(p + 48);
        if (type == PT_LOAD) {
            if (e.nloads == MAX_LOADS) {
                set_error(r, "more than %d loadable segments", MAX_LOADS);
                return;
            }
            if (filesz > memsz || !in_file(&e, off, filesz)) {
                set_error(r, "segment %u lies outside the file", i);
                return;
            }
            e.loads[e.nloads] = (tl_segment){ vaddr, memsz, flags };
            e.load_off[e.nloads] = off;
            e.load_filesz[e.nloads] = filesz;
            e.nloads++;
            if (!r->min_align || align < r->min_align) r->min_align = align;
            if (align > r->max_align) r->max_align = align;
        } else if (type == PT_DYNAMIC) {
            if (in_file(&e, off, filesz)) {
                e.has_dyn = true;
                e.dyn_off = off;
                e.dyn_size = filesz;
            }
        } else if (type == PT_TLS) {
            r->has_tls = true;
        } else if (type == PT_GNU_RELRO) {
            e.has_relro = true;
            e.relro = (tl_segment){ vaddr, memsz, flags };
        }
    }
    if (e.nloads == 0) {
        set_error(r, "no loadable segments");
        return;
    }

    size_t n = tl_page_plan(e.loads, e.nloads, e.has_relro ? &e.relro : NULL,
                            TL_PAGE, NULL, 0, NULL);
    if (n == 0 || n > MAX_PAGES) {
        set_error(r, "implausible image size");
        return;
    }
    uint8_t *flags = malloc(n);
    if (!flags) {
        set_error(r, "out of memory");
        return;
    }
    uint64_t base;
    n = tl_page_plan(e.loads, e.nloads, e.has_relro ? &e.relro : NULL, TL_PAGE,
                     flags, n, &base);
    if (n == 0) {
        free(flags);
        set_error(r, "segments do not lay out");
        return;
    }
    r->pages = (uint32_t)n;
    r->span = (uint64_t)n * TL_PAGE;
    for (size_t i = 0; i < n; i++) {
        if (flags[i] & TL_PAGE_X) r->pages_exec++;
        if (flags[i] & TL_PAGE_W) r->pages_write++;
        if ((flags[i] & TL_PAGE_X) && (flags[i] & TL_PAGE_W)) r->pages_conflict++;
    }
    summarise_layout(flags, n, r->layout, sizeof(r->layout));
    free(flags);

    scan_all_code(&e, r);
    if (e.has_dyn) {
        analyze_dynamic(&e, r);
    }
}
