/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * elf2macho-test: an arm64 Android library (.so) as a Mach-O dylib, for running Android code with no JIT (Husk 3.2; see
 * Husk_3.2.MD and 3.2-progress.md). The work is in the translation layer (husk-tl-aot.c, tl_ld_aot_build in husk-tl-ld.c), which
 * the app uses too; this is the Mac harness for it.
 *
 *   tools/regress/build.sh tools/elf2macho-test.c <out>           (it links the translation layer)
 *   elf2macho-test <lib.so> <out.dylib> [--ios]       write it (macOS: also ad-hoc signed; --ios marks it for iOS, unsigned);
 *                                                     an output ending in / names it aot-<hash>-<lib>.dylib there, as the app wants
 *   elf2macho-test --prepare <cache dir> <apk>...      what the app does before a JIT-less launch (husk_aot_prepare), ad-hoc signed
 *                                                     (or with TL_P12=<file> TL_P12_PASSWORD=<password>)
 *   elf2macho-test --load <out.dylib> <lib.so> [fn [arg]]   dlopen it, check it against a fresh build, call fn(arg) from the
 *                                                     dylib's code (a leaf that needs no relocation: mpg123_plain_strerror -1)
 */
#include <dlfcn.h>
#include <errno.h>
#include <limits.h>
#include <mach-o/dyld.h>
#include <mach-o/fixup-chains.h>
#include <mach-o/getsect.h>
#include <mach-o/loader.h>
#include <mach/mach.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "husk-tl-aot.h"
#include "husk-tl-codesign.h"

#define PAGE ((uint64_t)16384)
#define ROUND_DOWN(x) ((x) & ~(PAGE - 1))
#define ROUND_UP(x) (((x) + PAGE - 1) & ~(PAGE - 1))

void tl_log_line(const char *fmt, ...)
{
    va_list ap; va_start(ap, fmt);
    vfprintf(stderr, fmt, ap); fputc('\n', stderr);
    va_end(ap);
}

/* ------------------------------------------------------------------ ELF */

typedef struct { uint8_t e_ident[16]; uint16_t e_type, e_machine; uint32_t e_version;
                 uint64_t e_entry, e_phoff, e_shoff; uint32_t e_flags;
                 uint16_t e_ehsize, e_phentsize, e_phnum, e_shentsize, e_shnum, e_shstrndx; } elf_ehdr;
typedef struct { uint32_t p_type, p_flags; uint64_t p_offset, p_vaddr, p_paddr, p_filesz, p_memsz, p_align; } elf_phdr;
typedef struct { uint32_t st_name; uint8_t st_info, st_other; uint16_t st_shndx; uint64_t st_value, st_size; } elf_sym;
typedef struct { int64_t d_tag; uint64_t d_val; } elf_dyn;
enum { PT_LOAD_ = 1, PT_DYNAMIC_ = 2, EM_AARCH64_ = 183, DT_NULL_ = 0, DT_STRTAB_ = 5, DT_SYMTAB_ = 6, DT_GNU_HASH_ = 0x6ffffef5,
       DT_HASH_ = 4 };

typedef struct {
    uint8_t *file; size_t flen;
    const elf_ehdr *eh; const elf_phdr *ph;
    uint64_t lo, hi;                  /* the image: lowest PT_LOAD rounded down, highest end rounded up (16 KiB) */
    uint64_t max_align;
} elf;

static uint8_t *slurp(const char *path, size_t *len)
{
    FILE *f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "%s: %s\n", path, strerror(errno)); return NULL; }
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    uint8_t *b = malloc((size_t)n ? (size_t)n : 1);
    if (!b || fread(b, 1, (size_t)n, f) != (size_t)n) { fclose(f); free(b); return NULL; }
    fclose(f); *len = (size_t)n; return b;
}

static bool elf_open(elf *E, const char *path)
{
    memset(E, 0, sizeof(*E));
    if (!(E->file = slurp(path, &E->flen))) return false;
    if (E->flen < sizeof(elf_ehdr) || memcmp(E->file, "\x7f" "ELF", 4) || E->file[4] != 2) {
        fprintf(stderr, "%s: not a 64-bit ELF file\n", path); return false;
    }
    E->eh = (const elf_ehdr *)E->file;
    if (E->eh->e_machine != EM_AARCH64_) { fprintf(stderr, "%s: not arm64 (machine %u)\n", path, E->eh->e_machine); return false; }
    if (E->eh->e_phoff + (uint64_t)E->eh->e_phnum * sizeof(elf_phdr) > E->flen) { fprintf(stderr, "%s: bad program headers\n", path); return false; }
    E->ph = (const elf_phdr *)(E->file + E->eh->e_phoff);
    E->lo = UINT64_MAX;
    for (int i = 0; i < E->eh->e_phnum; i++) {
        const elf_phdr *p = &E->ph[i];
        if (p->p_type != PT_LOAD_) continue;
        if (p->p_offset + p->p_filesz > E->flen) { fprintf(stderr, "%s: PT_LOAD %d runs past the file\n", path, i); return false; }
        if (p->p_vaddr < E->lo) E->lo = p->p_vaddr;
        if (p->p_vaddr + p->p_memsz > E->hi) E->hi = p->p_vaddr + p->p_memsz;
        if (p->p_align > E->max_align) E->max_align = p->p_align;
    }
    if (E->lo == UINT64_MAX) { fprintf(stderr, "%s: no PT_LOAD\n", path); return false; }
    E->lo = ROUND_DOWN(E->lo); E->hi = ROUND_UP(E->hi);
    return true;
}

/* The data copy as the dylib must hold it: the PT_LOADs' file bytes at their vaddrs from base, the rest zero. */
static void elf_fill(const elf *E, uint64_t base, uint8_t *img, size_t size)
{
    memset(img, 0, size);
    for (int i = 0; i < E->eh->e_phnum; i++) {
        const elf_phdr *p = &E->ph[i];
        if (p->p_type == PT_LOAD_) memcpy(img + (p->p_vaddr - base), E->file + p->p_offset, p->p_filesz);
    }
}

/* A defined dynamic symbol's vaddr, by a linear walk of .dynsym (the count from the hash table). */
static uint64_t elf_lookup(const elf *E, const char *name)
{
    const elf_dyn *dyn = NULL; size_t ndyn = 0;
    for (int i = 0; i < E->eh->e_phnum; i++)
        if (E->ph[i].p_type == PT_DYNAMIC_) { dyn = (const elf_dyn *)(E->file + E->ph[i].p_offset); ndyn = E->ph[i].p_filesz / sizeof(elf_dyn); }
    if (!dyn) return 0;
    uint64_t strtab = 0, symtab = 0, hash = 0, gnu = 0;
    for (size_t i = 0; i < ndyn && dyn[i].d_tag != DT_NULL_; i++) {
        if (dyn[i].d_tag == DT_STRTAB_) strtab = dyn[i].d_val;
        else if (dyn[i].d_tag == DT_SYMTAB_) symtab = dyn[i].d_val;
        else if (dyn[i].d_tag == DT_HASH_) hash = dyn[i].d_val;
        else if (dyn[i].d_tag == DT_GNU_HASH_) gnu = dyn[i].d_val;
    }
    /* vaddr -> file offset */
    #define OFF(v) ({ uint64_t _v = (v), _o = 0; for (int _i = 0; _i < E->eh->e_phnum; _i++) { const elf_phdr *_p = &E->ph[_i]; \
        if (_p->p_type == PT_LOAD_ && _v >= _p->p_vaddr && _v < _p->p_vaddr + _p->p_filesz) _o = _p->p_offset + (_v - _p->p_vaddr); } _o; })
    if (!strtab || !symtab) return 0;
    uint32_t n = 0;
    if (hash) n = ((const uint32_t *)(E->file + OFF(hash)))[1];
    else if (gnu) {
        const uint32_t *g = (const uint32_t *)(E->file + OFF(gnu));
        uint32_t nb = g[0], base = g[1], bloom = g[2];
        const uint32_t *buckets = g + 4 + bloom * 2, *chain = buckets + nb;
        uint32_t last = 0;
        for (uint32_t b = 0; b < nb; b++) if (buckets[b] > last) last = buckets[b];
        if (last) { while (!(chain[last - base] & 1)) last++; n = last + 1; }
    }
    const elf_sym *syms = (const elf_sym *)(E->file + OFF(symtab));
    const char *strs = (const char *)(E->file + OFF(strtab));
    for (uint32_t i = 1; i < n; i++)
        if (syms[i].st_shndx && syms[i].st_value && !strcmp(strs + syms[i].st_name, name)) return syms[i].st_value;
    #undef OFF
    return 0;
}

/* ------------------------------------------------------------------ check */

static const struct mach_header_64 *header_of(const char *path)
{
    char want[PATH_MAX]; if (!realpath(path, want)) return NULL;
    for (uint32_t i = 0; i < _dyld_image_count(); i++) {
        char got[PATH_MAX];
        if (realpath(_dyld_get_image_name(i), got) && !strcmp(got, want)) return (const struct mach_header_64 *)_dyld_get_image_header(i);
    }
    return NULL;
}

static vm_prot_t prot_at(const void *p)
{
    mach_vm_address_t a = (mach_vm_address_t)(uintptr_t)p; mach_vm_size_t sz = 0;
    vm_region_basic_info_data_64_t info; mach_msg_type_number_t cnt = VM_REGION_BASIC_INFO_COUNT_64; mach_port_t obj;
    extern kern_return_t mach_vm_region(vm_map_t, mach_vm_address_t *, mach_vm_size_t *, vm_region_flavor_t, vm_region_info_t,
                                        mach_msg_type_number_t *, mach_port_t *);
    if (mach_vm_region(mach_task_self(), &a, &sz, VM_REGION_BASIC_INFO_64, (vm_region_info_t)&info, &cnt, &obj) != KERN_SUCCESS) return 0;
    return info.protection;
}

static bool build(const elf *E, const char *so, tl_ld_aot_image *A)
{
    const char *slash = strrchr(so, '/');
    if (!tl_ld_aot_build(slash ? slash + 1 : so, E->file, E->flen, A)) { fprintf(stderr, "%s: %s\n", so, A->err); return false; }
    return true;
}

static int check(const char *dylib, const char *so, const char *fn, long arg)
{
    elf E; if (!elf_open(&E, so)) return 1;
    tl_ld_aot_image A; if (!build(&E, so, &A)) return 1;
    void *h = dlopen(dylib, RTLD_NOW | RTLD_LOCAL);
    if (!h) { fprintf(stderr, "dlopen: %s\n", dlerror()); return 1; }
    const struct mach_header_64 *mh = header_of(dylib);
    if (!mh) { fprintf(stderr, "loaded, but dyld does not list it\n"); return 1; }
    size_t msz = 0;
    uint8_t *rx = (uint8_t *)(uintptr_t)mh + PAGE;      /* the image starts on the page after the header */
    uint8_t *rw = rx;                                   /* its data pages are writable there, at their own addresses */
    const tl_aot_manifest *M = (const tl_aot_manifest *)getsectiondata(mh, "__TEXT", "__husk_manifest", &msz);
    const uint64_t S = A.size;
    printf("dlopen ok: header %p, image %p (%zu pages, %llu KiB), %u segments\n", (void *)mh, (void *)rx, A.page_count,
           (unsigned long long)(S >> 10), mh->ncmds);
    if (!M || msz != sizeof(*M) || memcmp(M->magic, TL_AOT_MAGIC, 8) || M->version != TL_AOT_VERSION || M->so_hash != tl_aot_hash(E.file, E.flen)
        || M->size != S || M->base_vaddr != A.base_vaddr) { fprintf(stderr, "FAIL: the manifest does not describe this library\n"); return 1; }
    if ((uintptr_t)rx % PAGE) { fprintf(stderr, "FAIL: not 16 KiB aligned\n"); return 1; }
    /* The same library patched again, in other memory at another address, must give the same bytes: nothing in the image depends
     * on where it was built. */
    if (memcmp(rx, A.text, S)) {
        size_t k = 0; while (k < S && rx[k] == A.text[k]) k++;
        fprintf(stderr, "FAIL: the dylib differs from a fresh build at +%#zx\n", k); return 1;
    }
    /* Every page is mapped as its class says: executable pages read + execute, data pages read + write at the very address the code
     * uses for them (so data read at a code address is the relocated data, with no remapping at run time). */
    for (size_t p = 0; p < A.page_count; p++) {
        vm_prot_t prot = prot_at(rx + p * PAGE), want;
        if (p < A.npages + A.nstub) want = VM_PROT_EXECUTE; /* ld-style __TEXT: image data is r-x; copies are writable */
        else want = VM_PROT_WRITE;
        if ((prot & (VM_PROT_WRITE | VM_PROT_EXECUTE)) != want) {
            fprintf(stderr, "FAIL: page %zu is %c%c%c, expected %s\n", p, prot & 1 ? 'r' : '-', prot & 2 ? 'w' : '-', prot & 4 ? 'x' : '-',
                    want == VM_PROT_EXECUTE ? "r-x" : want == VM_PROT_WRITE ? "rw-" : "r--");
            return 1;
        }
    }
    printf("%zu pages mapped as laid out\n", A.page_count);
    /* A library built for 4 KiB pages has its data in writable copies after the image, at the image's size plus the page's index:
     * the code reads its data there. */
    size_t ncopy = 0;
    for (size_t p = 0; p < A.page_count; p++) {
        if (!(A.page_class[p] & 2)) continue;
        ncopy++;
        uint8_t *c = rx + S + p * PAGE;
        vm_prot_t prot = prot_at(c);
        if ((prot & (VM_PROT_WRITE | VM_PROT_EXECUTE)) != VM_PROT_WRITE) {
            fprintf(stderr, "FAIL: the copy of page %zu is %c%c%c, expected rw-\n", p, prot & 1 ? 'r' : '-', prot & 2 ? 'w' : '-',
                    prot & 4 ? 'x' : '-');
            return 1;
        }
        if (memcmp(c, A.text + p * PAGE, PAGE)) { fprintf(stderr, "FAIL: the copy of page %zu differs from the image\n", p); return 1; }
    }
    if (ncopy) printf("%zu data pages have writable copies after the image\n", ncopy);
    rw[S - 1] = 0x5a;                     /* the last data page takes writes */
    if (fn) {
        uint64_t v = elf_lookup(&E, fn);
        if (!v) { fprintf(stderr, "FAIL: %s is not a defined symbol of %s\n", fn, so); return 1; }
        const char *(*f)(long) = (const char *(*)(long))(rx + (v - A.base_vaddr));
        const char *r = f(arg);
        bool in = r && (uint8_t *)r >= rx && (uint8_t *)r < rx + S;
        printf("%s(%ld) from the dylib's code at %p returned %p: \"%s\"%s\n", fn, arg, (void *)f, (void *)r, in ? r : "(outside the image)",
               in ? "" : " -- WARNING");
    }
    tl_ld_aot_free(&A);
    printf("OK\n");
    return 0;
}

/* Section flags, independently of PT_LOAD permissions, define the metadata that
 * must stay in the relocated data view. A merged R-X load must not hide it. */
static int check_metadata(const char *path)
{
    elf E; if (!elf_open(&E, path)) return 1;
    tl_ld_aot_image A; if (!build(&E, path, &A)) return 1;
    if (E.eh->e_shentsize < 64 || E.eh->e_shoff + (uint64_t)E.eh->e_shentsize * E.eh->e_shnum > E.flen) return 1;
    size_t data_pages = 0, code_pages = 0;
    for (unsigned i = 0; i < E.eh->e_shnum; i++) {
        const uint8_t *sh = E.file + E.eh->e_shoff + (size_t)i * E.eh->e_shentsize;
        uint64_t flags, addr, size;
        memcpy(&flags, sh + 8, 8); memcpy(&addr, sh + 16, 8); memcpy(&size, sh + 32, 8);
        if (!(flags & 2) || !size) continue; /* SHF_ALLOC */
        if (addr < A.base_vaddr || addr - A.base_vaddr + size > A.npages * PAGE) return 1;
        uint64_t start = addr - A.base_vaddr, end = start + size;
        if (!(flags & 4)) { /* allocated data, including .eh_frame and .gcc_except_table */
            for (size_t p = start / PAGE; p <= (end - 1) / PAGE; p++) {
                if (!(A.page_class[p] & 2)) { fprintf(stderr, "FAIL: metadata section %u page %zu has no relocated copy\n", i, p); return 1; }
                data_pages++;
            }
        } else {
            for (size_t p = (start + PAGE - 1) / PAGE; p < end / PAGE; p++) {
                if ((A.page_class[p] & 3) != 1) { fprintf(stderr, "FAIL: pure code page %zu was copied as data\n", p); return 1; }
                code_pages++;
            }
        }
    }
    tl_ld_aot_free(&A);
    if (!data_pages || !code_pages) { fprintf(stderr, "FAIL: fixture must contain data and full code pages\n"); return 1; }
    printf("OK: %zu metadata page checks, %zu pure code page checks\n", data_pages, code_pages);
    return 0;
}

int main(int argc, char **argv)
{
    tl_ld_set_verbosity(getenv("TL_VERBOSE") ? atoi(getenv("TL_VERBOSE")) : 0);
    if (argc == 3 && !strcmp(argv[1], "--check-metadata")) return check_metadata(argv[2]);
    if (argc >= 4 && !strcmp(argv[1], "--prepare")) {
        uint8_t *p12 = NULL; size_t p12len = 0;
        if (getenv("TL_P12") && !(p12 = slurp(getenv("TL_P12"), &p12len))) return 1;
        char err[300] = "";
        int n = husk_aot_prepare((const char *const *)argv + 3, argc - 3, argv[2], p12, p12len, getenv("TL_P12_PASSWORD") ? getenv("TL_P12_PASSWORD") : "",
                                 p12 ? "test-p12" : "adhoc", getenv("TL_SIGN_IDENT"), NULL, NULL, err, sizeof(err));
        if (n < 0) { fprintf(stderr, "prepare failed: %s\n", err); return 1; }
        printf("%d libraries ready in %s\n", n, argv[2]);
        return 0;
    }
    if (argc >= 4 && !strcmp(argv[1], "--load")) return check(argv[2], argv[3], argc > 4 ? argv[4] : NULL, argc > 5 ? strtol(argv[5], NULL, 0) : 0);
    if (argc < 3) {
        fprintf(stderr, "usage: %s <lib.so> <out.dylib> [--ios]\n       %s --load <out.dylib> <lib.so> [leaf function [argument]]\n", argv[0], argv[0]);
        return 2;
    }
    bool ios = argc > 3 && !strcmp(argv[3], "--ios");
    elf E; if (!elf_open(&E, argv[1])) return 1;
    const char *slash = strrchr(argv[1], '/');
    tl_ld_aot_image A; char err[200];
    /* an output ending in / is a folder: the dylib is named as the app looks for it, aot-v<version>-<hash of the .so>-<library>.dylib */
    char out[1200]; size_t ol = strlen(argv[2]);
    if (ol && argv[2][ol - 1] == '/') {
        char nm[200]; tl_aot_file_name(nm, sizeof(nm), tl_aot_hash(E.file, E.flen), slash ? slash + 1 : argv[1]);
        snprintf(out, sizeof(out), "%s%s", argv[2], nm);
    }
    else snprintf(out, sizeof(out), "%s", argv[2]);
    argv[2] = out;
    if (!tl_aot_make_dylib(slash ? slash + 1 : argv[1], E.file, E.flen, argv[2], ios, &A, err, sizeof(err))) { fprintf(stderr, "%s\n", err); return 1; }
    printf("%s: image at vaddr 0x%llx, %zu + %zu stub + 2 pages (%zu KiB, PT_LOAD align up to 0x%llx), %s\n"
           "  rewritten: %zu x18 (%zu not), %zu svc (%zu out of range), %zu tpidr, %zu adrp, %zu adr (%zu not); %zu data pages copied\n",
           argv[2], (unsigned long long)A.base_vaddr, A.npages, A.nstub, A.size >> 10, (unsigned long long)E.max_align, ios ? "iOS" : "macOS",
           A.n_x18, A.n_x18_failed, A.n_svc, A.n_svc_far, A.n_tpidr, A.n_adrp, A.n_adr, A.n_adr_failed, A.n_mixed);
    if (!ios) {
        char cmd[2048]; snprintf(cmd, sizeof(cmd), "codesign -s - -f '%s' 2>/dev/null", argv[2]);
        if (system(cmd) != 0) { fprintf(stderr, "%s: codesign failed\n", argv[2]); return 1; }
    }
    return 0;
}
