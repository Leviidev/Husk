/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "husk-tl-ld.h"

#include <errno.h>
#include <libkern/OSCacheControl.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "husk-tl-internal.h"
#include "husk-tl-a64.h"
#include "husk-tl-xmem.h"

/* The project's log sink, and the bionic shim's surface. */
void  tl_log_line(const char *fmt, ...);
void *tl_bionic_find(const char *name);
bool  tl_bionic_is_system_lib(const char *soname);

/* ------------------------------------------------------------------ ELF */

typedef struct { uint8_t e_ident[16]; uint16_t e_type, e_machine; uint32_t e_version;
                 uint64_t e_entry, e_phoff, e_shoff; uint32_t e_flags;
                 uint16_t e_ehsize, e_phentsize, e_phnum, e_shentsize, e_shnum, e_shstrndx; } elf_ehdr;
typedef struct { uint32_t p_type, p_flags; uint64_t p_offset, p_vaddr, p_paddr, p_filesz, p_memsz, p_align; } elf_phdr;
typedef struct { uint32_t st_name; uint8_t st_info, st_other; uint16_t st_shndx; uint64_t st_value, st_size; } elf_sym;
typedef struct { uint64_t r_offset, r_info; int64_t r_addend; } elf_rela;
typedef struct { int64_t d_tag; uint64_t d_val; } elf_dyn;

enum {
    PT_LOAD_ = 1, PT_DYNAMIC_ = 2, PT_TLS_ = 7, PT_GNU_RELRO_ = 0x6474e552,
    DT_NULL_ = 0, DT_NEEDED_ = 1, DT_PLTRELSZ_ = 2, DT_HASH_ = 4, DT_STRTAB_ = 5, DT_SYMTAB_ = 6,
    DT_RELA_ = 7, DT_RELASZ_ = 8, DT_STRSZ_ = 10, DT_INIT_ = 12, DT_SONAME_ = 14, DT_JMPREL_ = 23,
    DT_INIT_ARRAY_ = 25, DT_INIT_ARRAYSZ_ = 27, DT_FLAGS_ = 30, DT_RELRSZ_ = 35, DT_RELR_ = 36,
    DT_GNU_HASH_ = 0x6ffffef5, DT_ANDROID_RELA_ = 0x60000011, DT_ANDROID_RELASZ_ = 0x60000012,
    DT_ANDROID_RELR_ = 0x6fffe000, DT_ANDROID_RELRSZ_ = 0x6fffe001,
    R_NONE = 0, R_ABS64 = 257, R_GLOB_DAT = 1025, R_JUMP_SLOT = 1026, R_RELATIVE = 1027,
    R_TLS_DTPMOD = 1028, R_TLS_DTPREL = 1029, R_TLS_TPREL = 1030, R_TLSDESC = 1031, R_IRELATIVE = 1032,
    STB_WEAK_ = 2, STT_TLS_ = 6, STT_GNU_IFUNC_ = 10, SHN_UNDEF_ = 0,
    PF_X_ = 1, PF_W_ = 2, PF_R_ = 4, EM_AARCH64_ = 183,
};

#define PAGE TL_XMEM_PAGE

/* ----------------------------------------------------------------- libs */

#define MAX_LIBS 96
#define MAX_DEPS 48

struct tl_lib {
    char name[96];                 /* as asked for */
    char soname[96];
    uint8_t *rx, *rw;              /* image: address of base_vaddr, in each view */
    uint64_t base_vaddr;
    size_t npages;
    uint8_t *pflags;               /* TL_PAGE_* per page */
    elf_phdr *phdr;                /* malloc'd copy, file addresses */
    unsigned phnum;

    /* dynamic section, as vaddrs */
    uint64_t strtab, strsz, symtab, gnu_hash, sysv_hash;
    uint64_t rela, relasz, jmprel, pltrelsz, arela, arelasz, relr, relrsz;
    uint64_t init, init_array, init_arraysz;
    uint32_t nsyms;                /* the symbols the hash table covers */
    uint32_t ncache;               /* an upper bound on the table: relocations name imports that the hash table does not cover */
    uint64_t *symcache;            /* resolved import per symbol index (ncache of them); 0 = not yet */

    uint64_t needed[MAX_DEPS];
    int nneeded;
    struct tl_lib *deps[MAX_DEPS]; /* the closure, breadth-first, excluding self */
    int ndeps;
    bool deps_ready;

    uint8_t *stub_rx, *stub_rw;    /* pages after the image: stubs for rewritten `svc` and x18 sites */
    size_t stub_used, stub_cap, nstub;
    uint8_t *isl_rx, *isl_rw;      /* a second pool of stubs inside the image: the dead tail of the relocation table, for sites too far from the stub pages */
    size_t isl_used, isl_cap;

    struct { uint64_t start, end; } code[16];   /* executable sections, as vaddrs: the only places instructions are patched */
    int ncode;
    int tls_id;                    /* 1-based number of this library's thread-local storage template, 0 if it has none */
    uint64_t *fde_start, *fde_end; size_t nfde;   /* the address ranges of the functions the unwind tables describe, sorted; none if the library has no tables */
    size_t n_x18_data;             /* words naming x18 that lie outside every function: constant tables inside .text, left alone */
    size_t n_ctr;                  /* reads of CTR_EL0 replaced by a constant */
    size_t n_svc_far, n_adr_failed;   /* svc sites with no stub in branch range (answered ENOSYS), adr sites that could not be rewritten */
    size_t n_x18, n_x18_failed;    /* sites rewritten for the reserved register, and sites that could not be */
    int state;                     /* 0 mapped, 1 relocating, 2 relocated, 3 initialising, 4 initialised */
    uint32_t n_unresolved;
};

static struct {
    pthread_mutex_t lock;
    tl_lib *libs[MAX_LIBS];
    int nlibs;
    tl_zip apks[TL_LD_MAX_APKS];
    int napks;
    int verbosity;
    size_t unresolved;
    uint8_t *tcb_rx, *tcb_rw;      /* the fake thread block every `mrs tpidr_el0` reads */
    char **argv, **envp;
    bool recursive_init;
} G = { .lock = PTHREAD_MUTEX_INITIALIZER, .verbosity = 1 };

/* Serialises load/init. Recursive, as a linker's lock has to be: a library's constructor may dlopen another (Geode's constructors
 * dlopen the game they hook), on the thread that is already inside the loader. */
static pthread_mutex_t g_big = PTHREAD_RECURSIVE_MUTEX_INITIALIZER;

void tl_ld_set_verbosity(int v) { G.verbosity = v; }
void tl_ld_set_environment(char **argv, char **envp) { G.argv = argv; G.envp = envp; }
size_t tl_ld_unresolved_count(void) { return G.unresolved; }

static inline const void *at(const tl_lib *L, uint64_t vaddr) { return L->rw + (vaddr - L->base_vaddr); }

/* ------------------------------------------------------------- the APKs */

/* The app's split APKs (its 64-bit libraries, its asset packs), given before the engine starts: they are added right after
 * the base, whichever engine adds that, as Android puts a split's libraries and assets beside the base's. */
static char g_splits[15][1024];
static int g_nsplits;
static char g_apk_paths[TL_LD_MAX_APKS][1024];

void tl_ld_queue_split(const char *path)
{
    if (path && path[0] && g_nsplits < 15) snprintf(g_splits[g_nsplits++], sizeof(g_splits[0]), "%s", path);
}

const char *tl_ld_queued_split(int i) { return i >= 0 && i < g_nsplits ? g_splits[i] : NULL; }

static bool add_one(const char *path)
{
    for (int i = 0; i < G.napks; i++) if (!strcmp(g_apk_paths[i], path)) return true;     /* already there */
    if (G.napks >= TL_LD_MAX_APKS) return false;
    char err[160];
    if (!tl_zip_open(&G.apks[G.napks], path, err, sizeof(err))) {
        tl_log_line("ld: cannot open %s: %s", path, err);
        return false;
    }
    snprintf(g_apk_paths[G.napks], sizeof(g_apk_paths[0]), "%s", path);
    G.napks++;
    return true;
}

bool tl_ld_add_apk(const char *path)
{
    bool first = G.napks == 0;
    if (!add_one(path)) return false;
    if (first)
        for (int i = 0; i < g_nsplits; i++) {
            if (add_one(g_splits[i])) tl_log_line("ld: split %s", strrchr(g_splits[i], '/') ? strrchr(g_splits[i], '/') + 1 : g_splits[i]);
        }
    return true;
}

const tl_zip *tl_ld_apk_at(int i) { return (i >= 0 && i < G.napks) ? &G.apks[i] : NULL; }

/* Every arm64 library the APKs carry, by file name and size: a game that links SDL into its own library names no SDL library to look for. */
int tl_ld_apk_libs(void (*cb)(const char *name, uint64_t size, void *user), void *user)
{
    int n = 0;
    for (int i = 0; i < G.napks; i++)
        for (size_t k = 0; k < G.apks[i].count; k++) {
            const char *nm = G.apks[i].entries[k].name;
            size_t l = strlen(nm);
            if (strncmp(nm, "lib/arm64-v8a/", 14) || l < 18 || strcmp(nm + l - 3, ".so") || strchr(nm + 14, '/')) continue;
            cb(nm + 14, G.apks[i].entries[k].usize, user); n++;
        }
    return n;
}

bool tl_ld_has_lib(const char *name)
{
    char path[160];
    snprintf(path, sizeof(path), "lib/arm64-v8a/%s", name);
    for (int i = 0; i < G.napks; i++) if (tl_zip_find(&G.apks[i], path)) return true;
    return false;
}

static bool fetch_from_apks(const char *name, uint8_t **out, size_t *len)
{
    char path[160];
    snprintf(path, sizeof(path), "lib/arm64-v8a/%s", name);
    for (int i = 0; i < G.napks; i++) {
        const tl_zip_entry *e = tl_zip_find(&G.apks[i], path);
        if (!e) continue;
        const uint8_t *data; size_t n; bool owned; char err[160];
        if (!tl_zip_data(&G.apks[i], e, (size_t)1 << 30, &data, &n, &owned, err, sizeof(err))) {
            tl_log_line("ld: %s: %s", name, err);
            return false;
        }
        if (!owned) {
            uint8_t *copy = malloc(n);
            if (!copy) return false;
            memcpy(copy, data, n);
            data = copy;
        }
        *out = (uint8_t *)data;
        *len = n;
        return true;
    }
    return false;
}

/* ------------------------------------------------------------- hashing */

static uint32_t gnu_hash(const char *s)
{
    uint32_t h = 5381;
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) h = h * 33 + *p;
    return h;
}

static uint32_t sysv_hash(const char *s)
{
    uint32_t h = 0, g;
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        h = (h << 4) + *p;
        if ((g = h & 0xf0000000u)) h ^= g >> 24;
        h &= ~g;
    }
    return h;
}

static const char *sym_name(const tl_lib *L, const elf_sym *s)
{
    return (const char *)at(L, L->strtab) + s->st_name;
}

static const elf_sym *sym_at(const tl_lib *L, uint32_t i)
{
    return (const elf_sym *)at(L, L->symtab) + i;
}

/* The address a defined symbol is known by: the writable view for data. */
static void *sym_value(const tl_lib *L, const elf_sym *s)
{
    uint64_t off = s->st_value - L->base_vaddr;
    size_t page = (size_t)(off / PAGE);
    bool is_func = (s->st_info & 0xf) == 2;
    if (!is_func && page < L->npages && (L->pflags[page] & TL_PAGE_W)) return L->rw + off;
    return L->rx + off;
}

static const elf_sym *lib_find(const tl_lib *L, const char *name)
{
    if (!L->symtab || !L->strtab) return NULL;
    if (L->gnu_hash) {
        const uint8_t *g = at(L, L->gnu_hash);
        uint32_t nb, symoff, bloom_n, bloom_shift;
        memcpy(&nb, g, 4); memcpy(&symoff, g + 4, 4); memcpy(&bloom_n, g + 8, 4); memcpy(&bloom_shift, g + 12, 4);
        if (!nb) return NULL;
        const uint64_t *bloom = (const uint64_t *)(g + 16);
        const uint32_t *buckets = (const uint32_t *)(g + 16 + (size_t)bloom_n * 8);
        const uint32_t *chains = buckets + nb;
        uint32_t h = gnu_hash(name);
        if (bloom_n) {
            uint64_t w = bloom[(h / 64) % bloom_n];
            if (!((w >> (h % 64)) & 1) || !((w >> ((h >> bloom_shift) % 64)) & 1)) return NULL;
        }
        uint32_t b = buckets[h % nb];
        if (b < symoff) return NULL;
        for (uint32_t i = b;; i++) {
            uint32_t c = chains[i - symoff];
            if ((h | 1) == (c | 1)) {
                const elf_sym *s = sym_at(L, i);
                if (s->st_shndx != SHN_UNDEF_ && !strcmp(sym_name(L, s), name)) return s;
            }
            if (c & 1) break;
        }
        return NULL;
    }
    if (L->sysv_hash) {
        const uint32_t *t = at(L, L->sysv_hash);
        uint32_t nb = t[0], nc = t[1];
        if (!nb) return NULL;
        for (uint32_t i = t[2 + sysv_hash(name) % nb]; i && i < nc; i = t[2 + nb + i]) {
            const elf_sym *s = sym_at(L, i);
            if (s->st_shndx != SHN_UNDEF_ && !strcmp(sym_name(L, s), name)) return s;
        }
    }
    return NULL;
}

static uint32_t count_dynsyms(const tl_lib *L)
{
    if (L->sysv_hash) return ((const uint32_t *)at(L, L->sysv_hash))[1];
    if (!L->gnu_hash) return 0;
    const uint8_t *g = at(L, L->gnu_hash);
    uint32_t nb, symoff, bloom_n;
    memcpy(&nb, g, 4); memcpy(&symoff, g + 4, 4); memcpy(&bloom_n, g + 8, 4);
    const uint32_t *buckets = (const uint32_t *)(g + 16 + (size_t)bloom_n * 8);
    const uint32_t *chains = buckets + nb;
    uint32_t last = 0;
    for (uint32_t i = 0; i < nb; i++) if (buckets[i] > last) last = buckets[i];
    if (last < symoff) return symoff;
    while (!(chains[last - symoff] & 1)) last++;
    return last + 1;
}

/*
 * How long the dynamic symbol table can be. The hash tables cover the symbols that are defined here, and a linker may put the imports after them
 * (libEOSSDK does: its relocations name symbols 813 and up of a table the hash counts as 813 long), so the count is the larger of that and the
 * distance to whichever table follows the symbols in the file.
 */
static uint32_t dynsym_bound(const tl_lib *L, uint32_t hashed)
{
    uint64_t next = ~0ull;
    const uint64_t after[] = { L->strtab, L->gnu_hash, L->sysv_hash, L->rela, L->jmprel, L->arela, L->relr };
    for (size_t i = 0; i < sizeof(after) / sizeof(after[0]); i++) if (after[i] > L->symtab && after[i] < next) next = after[i];
    uint32_t n = hashed;
    if (L->symtab && next != ~0ull) { uint64_t m = (next - L->symtab) / sizeof(elf_sym); if (m > n && m < (1u << 24)) n = (uint32_t)m; }
    return n;
}

/* ----------------------------------------------------- lookup by scope */

const char *tl_path_resolve(const char *path, char *buf, size_t n);
static const char *base_name(const char *path) { const char *b = strrchr(path, '/'); return b ? b + 1 : path; }

/* By the name it was asked for, its soname, or -- for one loaded from a path (a mod's library) -- its file name. */
static tl_lib *find_loaded(const char *name)
{
    const char *b = base_name(name);
    for (int i = 0; i < G.nlibs; i++) {
        if (!strcmp(G.libs[i]->name, name) || !strcmp(G.libs[i]->soname, name)) return G.libs[i];
        if (!strcmp(base_name(G.libs[i]->name), b) || !strcmp(G.libs[i]->soname, b)) return G.libs[i];
    }
    return NULL;
}

/* A library that is a file of its own rather than an APK entry: Geode, and the mods it loads from where it unpacked them. */
static bool fetch_from_file(const char *path, uint8_t **out, size_t *len)
{
    if (!strchr(path, '/')) return false;
    char real[1024];
    const char *p = tl_path_resolve(path, real, sizeof(real));
    FILE *f = fopen(p, "rb");
    if (!f) return false;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *data = n > 0 ? malloc((size_t)n) : NULL;
    bool ok = data && fread(data, 1, (size_t)n, f) == (size_t)n;
    fclose(f);
    if (!ok) { free(data); return false; }
    *out = data; *len = (size_t)n;
    return true;
}

tl_lib *tl_ld_find_lib(const char *name) { return find_loaded(name); }

static void build_scope(tl_lib *L)
{
    if (L->deps_ready) return;
    L->ndeps = 0;
    tl_lib *queue[MAX_DEPS + 1];
    int qh = 0, qt = 0;
    queue[qt++] = L;
    while (qh < qt && L->ndeps < MAX_DEPS) {
        tl_lib *cur = queue[qh++];
        for (int i = 0; i < cur->nneeded; i++) {
            const char *n = (const char *)at(cur, cur->strtab) + cur->needed[i];
            tl_lib *d = find_loaded(n);
            if (!d || d == L) continue;
            bool seen = false;
            for (int k = 0; k < L->ndeps; k++) if (L->deps[k] == d) seen = true;
            if (seen) continue;
            L->deps[L->ndeps++] = d;
            if (qt < MAX_DEPS + 1) queue[qt++] = d;
        }
    }
    L->deps_ready = true;
}

/* Symbol lookup the way a library sees it: its own scope, then the system. */
/* Functions a driver puts in front of whatever the libraries define: an import of one of these names is bound to the replacement, which can still call the original. */
#define MAX_INTERPOSE 8
static struct { char name[48]; void *fn; } g_interpose[MAX_INTERPOSE];
static int g_ninterpose;
void tl_ld_interpose(const char *name, void *fn)
{
    if (g_ninterpose < MAX_INTERPOSE) { snprintf(g_interpose[g_ninterpose].name, sizeof(g_interpose[0].name), "%s", name); g_interpose[g_ninterpose++].fn = fn; }
}

/*
 * The C++ runtime's exception machinery. Geometry Dash carries its own copy and exports it; Geode and its mods are built
 * against libc++_shared, and an exception thrown by one runtime cannot be caught by the other. So everything but the game
 * itself takes these from libc++_shared, as Geode's own Android launcher arranges by renaming the game's copies.
 */
static bool is_cxx_runtime_symbol(const char *name)
{
    static const char *const names[] = { "__gxx_personality_v0", "__cxa_throw", "__cxa_rethrow", "__cxa_allocate_exception",
        "__cxa_free_exception", "__cxa_begin_catch", "__cxa_end_catch", "__cxa_guard_acquire", "__cxa_guard_release",
        "__cxa_guard_abort", NULL };
    for (int i = 0; names[i]; i++) if (!strcmp(name, names[i])) return true;
    return false;
}

static void *lookup_for(tl_lib *L, const char *name, bool *weak_hit)
{
    (void)weak_hit;
    for (int i = 0; i < g_ninterpose; i++) if (!strcmp(g_interpose[i].name, name)) return g_interpose[i].fn;
    build_scope(L);
    const elf_sym *s = lib_find(L, name);
    if (s && (s->st_info & 0xf) != STT_GNU_IFUNC_) return sym_value(L, s);
    if (strcmp(L->soname, "libcocos2dcpp.so") != 0 && is_cxx_runtime_symbol(name)) {
        for (int i = 0; i < L->ndeps; i++) {
            if (strcmp(L->deps[i]->soname, "libc++_shared.so") != 0) continue;
            s = lib_find(L->deps[i], name);
            if (s) return sym_value(L->deps[i], s);
        }
    }
    for (int i = 0; i < L->ndeps; i++) {
        s = lib_find(L->deps[i], name);
        if (s && (s->st_info & 0xf) != STT_GNU_IFUNC_) return sym_value(L->deps[i], s);
    }
    return tl_bionic_find(name);
}

void *tl_ld_sym(tl_lib *lib, const char *name)
{
    if (lib) {
        const elf_sym *s = lib_find(lib, name);
        return s ? sym_value(lib, s) : NULL;
    }
    for (int i = 0; i < G.nlibs; i++) {
        const elf_sym *s = lib_find(G.libs[i], name);
        if (s) return sym_value(G.libs[i], s);
    }
    return NULL;
}

/* Where the library's vaddr 0 lies in the executable view: what dladdr calls its base. */
void *tl_ld_lib_base(const tl_lib *L) { return L ? (void *)(L->rx - L->base_vaddr) : NULL; }

tl_lib *tl_ld_lib_of(const void *addr)
{
    const uint8_t *a = addr;
    for (int i = 0; i < G.nlibs; i++) {
        tl_lib *L = G.libs[i];
        size_t span = L->npages * PAGE;
        if ((a >= L->rx && a < L->rx + span) || (a >= L->rw && a < L->rw + span)) return L;
    }
    return NULL;
}

const char *tl_ld_lib_name(const tl_lib *lib) { return lib ? lib->name : NULL; }

const char *tl_ld_symbol_at(const void *addr, const char **lib_name, const void **sym_addr)
{
    tl_lib *L = tl_ld_lib_of(addr);
    if (!L) return NULL;
    if (lib_name) *lib_name = L->name;
    uint64_t off = (const uint8_t *)addr >= L->rx && (const uint8_t *)addr < L->rx + L->npages * PAGE
                 ? (uint64_t)((const uint8_t *)addr - L->rx) : (uint64_t)((const uint8_t *)addr - L->rw);
    const elf_sym *best = NULL;
    uint32_t n = L->nsyms;
    for (uint32_t i = 1; i < n; i++) {
        const elf_sym *s = sym_at(L, i);
        if (s->st_shndx == SHN_UNDEF_ || !s->st_value) continue;
        uint64_t so = s->st_value - L->base_vaddr;
        if (so <= off && (!best || so > best->st_value - L->base_vaddr)) best = s;
    }
    if (!best) return NULL;
    if (sym_addr) *sym_addr = L->rx + (best->st_value - L->base_vaddr);
    return sym_name(L, best);
}

int tl_ld_iterate(tl_ld_phdr_cb cb, void *user)
{
    int r = 0;
    for (int i = 0; i < G.nlibs && !r; i++) {
        tl_lib *L = G.libs[i];
        if (L->state < 2) continue;
        r = cb((uintptr_t)(L->rx - L->base_vaddr), L->name, L->phdr, L->phnum, user);
    }
    return r;
}

/* ------------------------------------------------- unresolved-import stubs */

/*
 * An import nothing provides is bound to a 32-byte stub in the executable region
 * that loads its own name and jumps to the logger, so the first call announces
 * exactly what was missing. Failing the whole load would hide every import after
 * the first one; most of these are never called.
 */
static void tl_unresolved_called(const char *name, void *lr)
{
    static pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;
    static const char *seen[512];
    static int nseen;
    pthread_mutex_lock(&m);
    bool again = false;
    for (int i = 0; i < nseen; i++) if (seen[i] == name) again = true;
    if (!again && nseen < 512) seen[nseen++] = name;
    pthread_mutex_unlock(&m);
    if (!again) {
        const char *ln = NULL; const void *sa = NULL;
        const char *caller = tl_ld_symbol_at(lr, &ln, &sa);
        tl_log_line("ld: CALLED an unresolved import: %s (from %s %s+%#lx)", name, ln ? ln : "?",
                    caller ? caller : "?", sa ? (unsigned long)((const char *)lr - (const char *)sa) : 0ul);
    }
}

__attribute__((naked, used)) static void tl_unresolved_entry(void)
{
#if defined(__aarch64__)
    __asm__ volatile(
        "mov x0, x17\n"
        "mov x1, x30\n"
        "stp x29, x30, [sp, #-16]!\n"
        "bl _tl_unresolved_called_c\n"
        "ldp x29, x30, [sp], #16\n"
        "mov x0, #0\n"
        "ret\n");
#endif
}
void tl_unresolved_called_c(const char *name, void *lr);
void tl_unresolved_called_c(const char *name, void *lr) { tl_unresolved_called(name, lr); }

static uint8_t *g_stub_rx, *g_stub_rw;
static size_t g_stub_left;

static void *make_stub(const char *name)
{
    if (g_stub_left < 32) {
        if (!tl_xmem_alloc(PAGE, &g_stub_rx, &g_stub_rw)) return NULL;
        g_stub_left = PAGE;
    }
    uint8_t *rw = g_stub_rw, *rx = g_stub_rx;
    uint32_t code[4] = {
        0x58000090u,        /* ldr x16, #16  (the entry) */
        0x580000B1u,        /* ldr x17, #20  (the name)  */
        0xD61F0200u,        /* br  x16                   */
        0xD503201Fu,        /* nop                       */
    };
    memcpy(rw, code, 16);
    uint64_t entry = (uint64_t)(uintptr_t)tl_unresolved_entry, nm = (uint64_t)(uintptr_t)name;
    memcpy(rw + 16, &entry, 8);
    memcpy(rw + 24, &nm, 8);
    tl_xmem_flush(rx, 32);
    g_stub_rx += 32; g_stub_rw += 32; g_stub_left -= 32;
    return rx;
}

/* ------------------------------------------------------- raw system calls */

/*
 * Some libraries make Linux system calls themselves: `mov x8, #nr; svc #0`. On Darwin
 * that traps into a different kernel's table. Each such site is rewritten to branch to
 * a small stub in the library's own stub page, which saves the two scratch registers
 * the host might clobber, calls tl_svc_common, and branches back to the instruction
 * after the site. tl_svc_common saves everything else the C handler may disturb --
 * the kernel preserves all registers but x0 across a system call, so code around a
 * raw `svc` relies on that -- and calls tl_linux_syscall with the arguments and number.
 */
long tl_linux_syscall(long a0, long a1, long a2, long a3, long a4, long a5, long nr);

__attribute__((naked, used)) void tl_svc_common(void)
{
#if defined(__aarch64__)
    __asm__ volatile(
        "stp x29, x30, [sp, #-16]!\n"
        "mov x29, sp\n"
        "sub sp, sp, #512\n"
        "stp x1, x2, [sp, #0]\n"
        "stp x3, x4, [sp, #16]\n"
        "stp x5, x6, [sp, #32]\n"
        "stp x7, x8, [sp, #48]\n"
        "stp x9, x10, [sp, #64]\n"
        "stp x11, x12, [sp, #80]\n"
        "stp x13, x14, [sp, #96]\n"
        "str x15, [sp, #112]\n"
        "stp q0, q1, [sp, #128]\n"
        "stp q2, q3, [sp, #160]\n"
        "stp q4, q5, [sp, #192]\n"
        "stp q6, q7, [sp, #224]\n"
        "stp q16, q17, [sp, #256]\n"
        "stp q18, q19, [sp, #288]\n"
        "stp q20, q21, [sp, #320]\n"
        "stp q22, q23, [sp, #352]\n"
        "stp q24, q25, [sp, #384]\n"
        "stp q26, q27, [sp, #416]\n"
        "stp q28, q29, [sp, #448]\n"
        "stp q30, q31, [sp, #480]\n"
        "mov x6, x8\n"
        "bl _tl_linux_syscall\n"
        "ldp x1, x2, [sp, #0]\n"
        "ldp x3, x4, [sp, #16]\n"
        "ldp x5, x6, [sp, #32]\n"
        "ldp x7, x8, [sp, #48]\n"
        "ldp x9, x10, [sp, #64]\n"
        "ldp x11, x12, [sp, #80]\n"
        "ldp x13, x14, [sp, #96]\n"
        "ldr x15, [sp, #112]\n"
        "ldp q0, q1, [sp, #128]\n"
        "ldp q2, q3, [sp, #160]\n"
        "ldp q4, q5, [sp, #192]\n"
        "ldp q6, q7, [sp, #224]\n"
        "ldp q16, q17, [sp, #256]\n"
        "ldp q18, q19, [sp, #288]\n"
        "ldp q20, q21, [sp, #320]\n"
        "ldp q22, q23, [sp, #352]\n"
        "ldp q24, q25, [sp, #384]\n"
        "ldp q26, q27, [sp, #416]\n"
        "ldp q28, q29, [sp, #448]\n"
        "ldp q30, q31, [sp, #480]\n"
        "mov sp, x29\n"
        "ldp x29, x30, [sp], #16\n"
        "ret\n");
#endif
}

/* ------------------------------------------------------------------ probes */

__attribute__((naked, used)) void tl_probe_common(void)
{
#if defined(__aarch64__)
    __asm__ volatile(
        "stp x29, x30, [sp, #-16]!\n"
        "mov x29, sp\n"
        "sub sp, sp, #624\n"
        "stp x0, x1, [sp, #0]\n"   "stp x2, x3, [sp, #16]\n"   "stp x4, x5, [sp, #32]\n"   "stp x6, x7, [sp, #48]\n"
        "stp x8, x9, [sp, #64]\n"  "stp x10, x11, [sp, #80]\n" "stp x12, x13, [sp, #96]\n" "stp x14, x15, [sp, #112]\n"
        "stp x16, x17, [sp, #128]\n" "stp x18, x19, [sp, #144]\n" "stp x20, x21, [sp, #160]\n" "stp x22, x23, [sp, #176]\n"
        "stp x24, x25, [sp, #192]\n" "stp x26, x27, [sp, #208]\n" "str x28, [sp, #224]\n"
        "stp q0, q1, [sp, #240]\n"  "stp q2, q3, [sp, #272]\n"  "stp q4, q5, [sp, #304]\n"  "stp q6, q7, [sp, #336]\n"
        "stp q16, q17, [sp, #368]\n" "stp q18, q19, [sp, #400]\n" "stp q20, q21, [sp, #432]\n" "stp q22, q23, [sp, #464]\n"
        "stp q24, q25, [sp, #496]\n" "stp q26, q27, [sp, #528]\n" "stp q28, q29, [sp, #560]\n" "stp q30, q31, [sp, #592]\n"
        "mov x0, sp\n"
        "blr x17\n"
        "ldp x0, x1, [sp, #0]\n"   "ldp x2, x3, [sp, #16]\n"   "ldp x4, x5, [sp, #32]\n"   "ldp x6, x7, [sp, #48]\n"
        "ldp x8, x9, [sp, #64]\n"  "ldp x10, x11, [sp, #80]\n" "ldp x12, x13, [sp, #96]\n" "ldp x14, x15, [sp, #112]\n"
        "ldp q0, q1, [sp, #240]\n"  "ldp q2, q3, [sp, #272]\n"  "ldp q4, q5, [sp, #304]\n"  "ldp q6, q7, [sp, #336]\n"
        "ldp q16, q17, [sp, #368]\n" "ldp q18, q19, [sp, #400]\n" "ldp q20, q21, [sp, #432]\n" "ldp q22, q23, [sp, #464]\n"
        "ldp q24, q25, [sp, #496]\n" "ldp q26, q27, [sp, #528]\n" "ldp q28, q29, [sp, #560]\n" "ldp q30, q31, [sp, #592]\n"
        "mov sp, x29\n"
        "ldp x29, x30, [sp], #16\n"
        "ret\n");
#endif
}

bool tl_ld_probe(tl_lib *L, uint64_t vaddr, void (*cb)(uint64_t *regs))
{
    uint64_t off = vaddr - L->base_vaddr;
    if (off + 4 > L->npages * PAGE || L->stub_used + 64 > L->stub_cap) return false;
    uint32_t *site_rw = (uint32_t *)(L->rw + off);
    const uint8_t *site_rx = L->rx + off;
    uint8_t *rx = L->stub_rx + L->stub_used, *rw = L->stub_rw + L->stub_used;
    L->stub_used += 64;
    uint64_t common = (uint64_t)(uintptr_t)tl_probe_common, cbv = (uint64_t)(uintptr_t)cb;
    memcpy(rw + 40, &cbv, 8);
    memcpy(rw + 48, &common, 8);
    uint32_t ldr_common = 0x58000010u | ((uint32_t)(((48 - 8) / 4) & 0x7FFFF) << 5);   /* ldr x16, [stub+48] */
    uint32_t ldr_cb     = 0x58000011u | ((uint32_t)(((40 - 12) / 4) & 0x7FFFF) << 5);  /* ldr x17, [stub+40] */
    int64_t back = ((int64_t)(site_rx + 4) - (int64_t)(rx + 32)) / 4;
    uint32_t code[10] = {
        0xA9BF7BFDu, 0xA9BF47F0u, ldr_common, ldr_cb, 0xD63F0200u /* blr x16 */,
        0xA8C147F0u, 0xA8C17BFDu, *site_rw /* the original instruction */,
        0x14000000u | ((uint32_t)back & 0x3FFFFFFu), 0xD503201Fu,
    };
    memcpy(rw, code, 40);
    int64_t to = ((int64_t)rx - (int64_t)site_rx) / 4;
    if (to <= -(1 << 25) || to >= (1 << 25)) return false;
    *site_rw = 0x14000000u | ((uint32_t)to & 0x3FFFFFFu);
    tl_xmem_flush(rx, 64);
    tl_xmem_flush(site_rx, 4);
    return true;
}

static bool stub_in_range(const uint8_t *site, const uint8_t *stub)
{
    int64_t to = ((int64_t)stub - (int64_t)site) / 4;
    return to > -(1 << 25) && to < (1 << 25);
}

/*
 * A 32-byte slot for a site, as writable and executable addresses, from whichever of the library's two stub pools is
 * within branch range of the site (a branch reaches 128 MiB, and Minecraft's code spans 220 MiB). `*lit` is the
 * pool's literal slot, which holds the address of the shared handler.
 */
static bool stub_slot(tl_lib *L, const uint8_t *site_rx, uint8_t **rx, uint8_t **rw, const uint8_t **lit)
{
    if (L->stub_used + 32 <= L->stub_cap && stub_in_range(site_rx, L->stub_rx + L->stub_used)) {
        *rx = L->stub_rx + L->stub_used; *rw = L->stub_rw + L->stub_used; *lit = L->stub_rx;
        L->stub_used += 32;
        return true;
    }
    if (L->isl_used + 32 <= L->isl_cap && stub_in_range(site_rx, L->isl_rx + L->isl_used)) {
        *rx = L->isl_rx + L->isl_used; *rw = L->isl_rw + L->isl_used; *lit = L->isl_rx;
        L->isl_used += 32;
        return true;
    }
    return false;
}

/*
 * `adr Xd, label` where the label is writable data. adr reaches only a megabyte, and the writable view of the
 * image is somewhere else entirely, so the instruction is replaced by a branch to a stub that builds the writable
 * view's address in Xd (four moves) and branches back. Linkers turn adrp+add pairs into adr when the target is
 * close, which is why small libraries have these and Unity's did not.
 */
static bool adr_stub(tl_lib *L, const uint8_t *site_rx, uint32_t rd, uint64_t target, uint32_t *branch)
{
    uint8_t *rx, *rw; const uint8_t *lit;
    if (!stub_slot(L, site_rx, &rx, &rw, &lit)) return false;
    int64_t to = ((int64_t)rx - (int64_t)site_rx) / 4;
    int64_t back = ((int64_t)(site_rx + 4) - (int64_t)(rx + 16)) / 4;
    uint32_t code[8] = {
        0xD2800000u | ((uint32_t)(target & 0xFFFF) << 5) | rd,                  /* movz Xd, #bits 0..15 */
        0xF2A00000u | ((uint32_t)((target >> 16) & 0xFFFF) << 5) | rd,          /* movk Xd, #bits 16..31, lsl 16 */
        0xF2C00000u | ((uint32_t)((target >> 32) & 0xFFFF) << 5) | rd,          /* movk Xd, #bits 32..47, lsl 32 */
        0xF2E00000u | ((uint32_t)((target >> 48) & 0xFFFF) << 5) | rd,          /* movk Xd, #bits 48..63, lsl 48 */
        0x14000000u | ((uint32_t)back & 0x3FFFFFFu),                            /* b site+4 */
        0xD503201Fu, 0xD503201Fu, 0xD503201Fu,
    };
    memcpy(rw, code, 32);
    *branch = 0x14000000u | ((uint32_t)to & 0x3FFFFFFu);
    return true;
}

/* A 32-byte stub for the `svc` at site_rx; returns its executable address, or NULL when no pool has room within range. */
static uint8_t *svc_stub(tl_lib *L, const uint8_t *site_rx)
{
    uint8_t *rx, *rw; const uint8_t *lit;
    if (!stub_slot(L, site_rx, &rx, &rw, &lit)) return NULL;
    uint32_t imm19 = (uint32_t)(((int64_t)lit - (int64_t)(rx + 8)) / 4) & 0x7FFFFu;
    int64_t back = ((int64_t)(site_rx + 4) - (int64_t)(rx + 24)) / 4;
    uint32_t code[8] = {
        0xA9BF7BFDu,                    /* stp x29, x30, [sp, #-16]! */
        0xA9BF47F0u,                    /* stp x16, x17, [sp, #-16]! */
        0x58000010u | (imm19 << 5),     /* ldr x16, <the tl_svc_common literal at the page start> */
        0xD63F0200u,                    /* blr x16 */
        0xA8C147F0u,                    /* ldp x16, x17, [sp], #16 */
        0xA8C17BFDu,                    /* ldp x29, x30, [sp], #16 */
        0x14000000u | ((uint32_t)back & 0x3FFFFFFu),   /* b site+4 */
        0xD503201Fu,                    /* nop */
    };
    memcpy(rw, code, 32);
    return rx;
}

/* --------------------------------------------------------------------- x18 */

/*
 * Apple reserves x18, and the kernel zeroes it on every exception return -- every
 * page fault, every interrupt -- while Android compilers use it as one more scratch
 * register and keep values in it across instructions that can fault. Left alone,
 * guest code that does that computes with a zero at some random point and dies
 * with an address of 0xfffffffffffffff0.
 *
 * So no guest instruction ever holds a live value in the real x18. Each one that
 * names it is replaced by a branch to a small stub that works on a "virtual x18"
 * kept in the thread's own TSD array, which the kernel does not touch: the stub
 * loads the virtual value into a scratch register, runs the original instruction
 * with that register in place of x18, writes the (possibly changed) value back, and
 * branches to the following instruction. The scratch registers are saved in the
 * 128 bytes below sp that Apple's ABI keeps free of signal frames. Nothing the
 * kernel does can land between two instructions of a stub and be seen: only x18
 * is ever lost, and x18 is not used.
 *
 * Instructions that read their operand's value as part of control flow or that are
 * pc-relative cannot be copied into a stub unchanged, so they get their own
 * shapes: adrp/adr/ldr-literal into x18 become the constant they compute, cbz and
 * tbz become a test of the loaded value that branches on to the original target,
 * and `br x18` jumps through a scratch register.
 */
#define X18_STUB_BYTES 64

static int64_t g_vx18_off = -1;     /* byte offset of the virtual-x18 slot from the TSD base */

static bool vx18_init(void)
{
    if (g_vx18_off >= 0) return true;
#if defined(__aarch64__)
    pthread_key_t key;
    if (pthread_key_create(&key, NULL)) return false;
    const uintptr_t sentinel = (uintptr_t)0x5a5a1234deadbeefull;
    pthread_setspecific(key, (void *)sentinel);
    uintptr_t base;
    __asm__ volatile("mrs %0, tpidrro_el0" : "=r"(base));
    base &= ~(uintptr_t)7;
    const volatile uintptr_t *tsd = (const volatile uintptr_t *)base;
    int64_t off = -1;
    for (int i = 0; i < 520; i++) if (tsd[i] == sentinel) { off = (int64_t)i * 8; break; }
    pthread_setspecific(key, NULL);
    if (off < 0 || off > 32760) { pthread_key_delete(key); return false; }
    g_vx18_off = off;
    return true;
#else
    return false;
#endif
}

/* The value of the calling thread's virtual x18, for a signal handler to save and restore around guest handlers. */
uint64_t tl_vx18_get(void)
{
#if defined(__aarch64__)
    if (g_vx18_off < 0) return 0;
    uintptr_t base;
    __asm__ volatile("mrs %0, tpidrro_el0" : "=r"(base));
    return *(const volatile uint64_t *)((base & ~(uintptr_t)7) + (uintptr_t)g_vx18_off);
#else
    return 0;
#endif
}
void tl_vx18_set(uint64_t v)
{
#if defined(__aarch64__)
    if (g_vx18_off < 0) return;
    uintptr_t base;
    __asm__ volatile("mrs %0, tpidrro_el0" : "=r"(base));
    *(volatile uint64_t *)((base & ~(uintptr_t)7) + (uintptr_t)g_vx18_off) = v;
#else
    (void)v;
#endif
}

static inline uint32_t e_stur(unsigned rt, int imm)  { return 0xF8000000u | (((uint32_t)imm & 0x1FFu) << 12) | (31u << 5) | rt; }
static inline uint32_t e_ldur(unsigned rt, int imm)  { return 0xF8400000u | (((uint32_t)imm & 0x1FFu) << 12) | (31u << 5) | rt; }
static inline uint32_t e_mrs_tsd(unsigned rt)        { return 0xD53BD060u | rt; }
static inline uint32_t e_and_tsd(unsigned r)         { return 0x927DF000u | (r << 5) | r; }          /* and r, r, #~7 */
static inline uint32_t e_ldr_slot(unsigned rt, unsigned rn) { return 0xF9400000u | ((uint32_t)(g_vx18_off / 8) << 10) | (rn << 5) | rt; }
static inline uint32_t e_str_slot(unsigned rt, unsigned rn) { return 0xF9000000u | ((uint32_t)(g_vx18_off / 8) << 10) | (rn << 5) | rt; }

static int e_mov64(uint32_t *out, unsigned rd, uint64_t v)
{
    int n = 0;
    out[n++] = 0xD2800000u | (uint32_t)((v & 0xFFFF) << 5) | rd;                       /* movz rd, #lo */
    for (int sh = 1; sh < 4; sh++) {
        uint64_t part = (v >> (16 * sh)) & 0xFFFF;
        if (part) out[n++] = 0xF2800000u | ((uint32_t)sh << 21) | ((uint32_t)part << 5) | rd;   /* movk */
    }
    return n;
}

static bool in_range_b(const uint8_t *from, const uint8_t *to)
{
    int64_t o = ((int64_t)to - (int64_t)from) / 4;
    return o > -(1 << 25) && o < (1 << 25);
}
static inline uint32_t e_b(const uint8_t *from, const uint8_t *to)
{
    return 0x14000000u | ((uint32_t)(((int64_t)to - (int64_t)from) / 4) & 0x3FFFFFFu);
}

static unsigned pick_scratch(uint32_t used, unsigned avoid)
{
    static const unsigned order[] = { 16, 17, 15, 14, 13, 12, 11, 10, 9 };
    for (size_t i = 0; i < sizeof(order) / sizeof(order[0]); i++)
        if (!(used & (1u << order[i])) && order[i] != avoid) return order[i];
    return 0;
}

enum { X18_NONE = 0, X18_DONE = 1, X18_FAILED = 2 };

/* Rewrite the instruction at site_rw (executable address pc) if it names x18. */
static int x18_rewrite(tl_lib *L, uint32_t *site_rw, const uint8_t *pc, ptrdiff_t delta)
{
    uint32_t insn = *site_rw;
    if ((insn & 31u) != 18 && ((insn >> 5) & 31u) != 18 && ((insn >> 10) & 31u) != 18 && ((insn >> 16) & 31u) != 18)
        return X18_NONE;
    bool known;
    if (!a64_uses_gpr(insn, 18, &known)) return X18_NONE;
    if (!known) {
        if (G.verbosity >= 2) tl_log_line("ld: %s: unrecognised instruction %08x at +%#llx looks like it uses x18", L->name, insn, (unsigned long long)(pc - L->rx));
        return X18_FAILED;
    }
    if (g_vx18_off < 0 || L->stub_used + X18_STUB_BYTES > L->stub_cap) return X18_FAILED;

    uint8_t *rx = L->stub_rx + L->stub_used, *rw = L->stub_rw + L->stub_used;
    uint32_t c[X18_STUB_BYTES / 4];
    int n = 0;
    const uint8_t *back = pc + 4;
    uint32_t used = a64_gpr_mask(insn);
#define EMIT(word) do { uint32_t w_ = (word); c[n++] = w_; } while (0)
#define EMIT_B(to) do { uint32_t w_ = e_b(rx + n * 4, (to)); c[n++] = w_; } while (0)

    if ((insn & 0x9F000000u) == 0x90000000u || (insn & 0x9F000000u) == 0x10000000u || (insn & 0xBF000000u) == 0x18000000u) {
        /* pc-relative into x18: the value is a constant of the site, known now */
        uint64_t value;
        if ((insn & 0x9F000000u) == 0x90000000u || (insn & 0x9F000000u) == 0x10000000u) {
            bool page = (insn & 0x80000000u) != 0;
            int64_t imm = (int64_t)((((insn >> 5) & 0x7FFFFu) << 2) | ((insn >> 29) & 3u));
            if (imm & 0x100000) imm -= 0x200000;
            uintptr_t tp = page ? ((uintptr_t)pc & ~(uintptr_t)0xFFF) + (uintptr_t)(imm * 4096) : (uintptr_t)pc + (uintptr_t)imm;
            if (tp >= (uintptr_t)L->rx && tp < (uintptr_t)L->rx + L->npages * PAGE && (L->pflags[(tp - (uintptr_t)L->rx) / PAGE] & TL_PAGE_W))
                tp += (uintptr_t)delta;
            value = tp;
        } else {
            int64_t imm = (int64_t)((insn >> 5) & 0x7FFFFu);
            if (imm & 0x40000) imm -= 0x80000;
            const uint8_t *src = (const uint8_t *)((uintptr_t)pc + (uintptr_t)(imm * 4) + (uintptr_t)delta);
            value = (insn & 0x40000000u) ? *(const uint64_t *)src : (uint64_t)*(const uint32_t *)src;
        }
        unsigned S = 16, T = 17;
        EMIT(e_stur(S, -16)); EMIT(e_stur(T, -8));
        n += e_mov64(c + n, S, value);
        EMIT(e_mrs_tsd(T)); EMIT(e_and_tsd(T)); EMIT(e_str_slot(S, T));
        EMIT(e_ldur(T, -8)); EMIT(e_ldur(S, -16));
        EMIT_B(back);
    } else if ((insn & 0xFFFFFC1Fu) == 0xD61F0000u) {                       /* br x18 */
        EMIT(e_mrs_tsd(17)); EMIT(e_and_tsd(17)); EMIT(e_ldr_slot(17, 17));
        EMIT(0xD61F0000u | (17u << 5));
    } else if ((insn & 0x7E000000u) == 0x34000000u || (insn & 0x7E000000u) == 0x36000000u) {   /* cbz, cbnz, tbz, tbnz */
        bool is_tb = (insn & 0x7E000000u) == 0x36000000u;
        int64_t imm = is_tb ? (int64_t)((insn >> 5) & 0x3FFFu) : (int64_t)((insn >> 5) & 0x7FFFFu);
        int64_t sign = is_tb ? 0x2000 : 0x40000;
        if (imm & sign) imm -= sign * 2;
        const uint8_t *target = pc + imm * 4;
        unsigned S = 16;
        EMIT(e_stur(S, -16));
        EMIT(e_mrs_tsd(S)); EMIT(e_and_tsd(S)); EMIT(e_ldr_slot(S, S));
        /* the test at index 4 branches to index 7 when taken: three instructions on */
        uint32_t field_mask = is_tb ? (0x3FFFu << 5) : (0x7FFFFu << 5);
        EMIT((insn & ~(field_mask | 0x1Fu)) | (3u << 5) | S);
        EMIT(e_ldur(S, -16));
        EMIT_B(back);
        EMIT(e_ldur(S, -16));
        if (!in_range_b(rx + n * 4, target)) return X18_FAILED;
        EMIT_B(target);
    } else {
        /* a base register of sp with writeback would collide with the saved scratch registers */
        bool pair = (insn & 0x3A000000u) == 0x28000000u, single = (insn & 0x3B000000u) == 0x38000000u;
        unsigned mode = pair ? (insn >> 23) & 3u : (insn >> 10) & 3u;
        if ((pair || (single && !((insn >> 21) & 1u))) && ((insn >> 5) & 31u) == 31 && (mode == 1 || mode == 3)) {
            tl_log_line("ld: %s: x18 instruction %08x at +%#llx writes back sp", L->name, insn, (unsigned long long)(pc - L->rx));
            return X18_FAILED;
        }
        unsigned S = pick_scratch(used, 0), T = pick_scratch(used, S);
        if (!S || !T) return X18_FAILED;
        EMIT(e_stur(S, -16)); EMIT(e_stur(T, -8));
        EMIT(e_mrs_tsd(S)); EMIT(e_and_tsd(S)); EMIT(e_ldr_slot(S, S));
        EMIT(a64_subst_gpr(insn, 18, S));
        EMIT(e_mrs_tsd(T)); EMIT(e_and_tsd(T)); EMIT(e_str_slot(S, T));
        EMIT(e_ldur(T, -8)); EMIT(e_ldur(S, -16));
        EMIT_B(back);
    }
#undef EMIT
#undef EMIT_B
    if (!in_range_b(pc, rx)) return X18_FAILED;
    memcpy(rw, c, (size_t)n * 4);
    for (int i = n; i < X18_STUB_BYTES / 4; i++) ((uint32_t *)rw)[i] = 0xD503201Fu;
    L->stub_used += X18_STUB_BYTES;
    *site_rw = e_b(pc, rx);
    return X18_DONE;
}

static uint8_t *build_data_map(const tl_lib *L, const uint32_t *w, size_t n, uint64_t vstart, size_t *n_data);   /* below, with the unwind tables it reads */

/* The cheap test x18_rewrite starts with, and what it would go on to decide: whether this word is an instruction that names x18. */
static bool x18_rewrite_would_apply(uint32_t insn)
{
    if ((insn & 31u) != 18 && ((insn >> 5) & 31u) != 18 && ((insn >> 10) & 31u) != 18 && ((insn >> 16) & 31u) != 18) return false;
    bool known;
    return a64_uses_gpr(insn, 18, &known);
}

/* --------------------------------------------------------------- patching */

#if defined(__aarch64__)
static uint32_t encode_adrp(uint32_t rt, const void *pc, const void *target)
{
    int64_t delta = ((int64_t)((uintptr_t)target & ~(uintptr_t)0xFFF)
                   - (int64_t)((uintptr_t)pc & ~(uintptr_t)0xFFF)) >> 12;
    uint32_t imm = (uint32_t)delta & 0x1FFFFF;
    return 0x90000000u | ((imm & 3u) << 29) | ((imm >> 2) << 5) | (rt & 0x1Fu);
}

/*
 * Two rewrites in executable pages, both on the writable view:
 *
 *  - `mrs Xt, tpidr_el0` becomes `adrp Xt, <fake thread block>`. Android code
 *    reads its stack-protector cookie from [tpidr_el0 + 0x28]; Darwin keeps its
 *    own thread pointer elsewhere and leaves this register for nothing in
 *    particular. Every thread sharing one cookie is harmless: the cookie only has
 *    to be the same at a function's entry and exit.
 *  - an `adrp` that points into this image's writable pages is retargeted at the
 *    writable view, because code reaches its globals pc-relatively and the page
 *    the executable view shows is not writable.
 */
/* Whether a vaddr is in one of the library's executable sections. */
static bool in_code(const tl_lib *L, uint64_t vaddr)
{
    for (int r = 0; r < L->ncode; r++) if (vaddr >= L->code[r].start && vaddr < L->code[r].end) return true;
    return false;
}

/* An adrp into a page that holds code and data both (a small library built for 4 KiB pages): what the next instructions make of it.
 * "add Xd, Xn, #imm" with Xn the adrp's register forms an address; when that address is in code (a callback handed to someone),
 * the adrp must keep naming the executable view. Loads and stores through it are data, and get the writable view. */
static bool adrp_targets_code(const tl_lib *L, const uint32_t *w, size_t i, size_t nwords, uint32_t rd, uintptr_t tp)
{
    if (!(L->pflags[(tp - (uintptr_t)L->rx) / PAGE] & TL_PAGE_X)) return false;
    for (size_t k = i + 1; k < nwords && k <= i + 4; k++) {
        uint32_t n = w[k];
        if ((n & 0xFF800000u) == 0x91000000u && ((n >> 5) & 0x1Fu) == rd) {        /* add Xd, Xn, #imm{, lsl #12} */
            uint64_t imm = (n >> 10) & 0xFFFu;
            if (n & (1u << 22)) imm <<= 12;
            return in_code(L, (uint64_t)(tp - (uintptr_t)L->rx) + imm + L->base_vaddr);
        }
        if (((n >> 5) & 0x1Fu) == rd) return false;                              /* any other use of it: data */
    }
    return false;
}

static void patch_image(tl_lib *L, size_t *n_tpidr, size_t *n_adrp, size_t *n_adr, size_t *n_svc)
{
    ptrdiff_t delta = L->rw - L->rx;
    *n_tpidr = *n_adrp = *n_adr = *n_svc = 0;
    /* Only instructions are patched: many libraries put .rodata and .eh_frame in the same
     * executable segment as .text, and a data word that happens to look like an adrp or a
     * load must be left alone. */
    for (int r = 0; r < L->ncode; r++) {
        uint32_t *w = (uint32_t *)(L->rw + (L->code[r].start - L->base_vaddr));
        const uint8_t *x = L->rx + (L->code[r].start - L->base_vaddr);
        size_t nwords = (size_t)((L->code[r].end - L->code[r].start) / 4);
        size_t ndata;
        uint8_t *dmap = build_data_map(L, w, nwords, L->code[r].start, &ndata);
        for (size_t i = 0; i < nwords; i++) {
            uint32_t insn = w[i];
            const uint8_t *pc = x + i * 4;
            /* a constant in the code section is not an instruction that happens to name x18 */
            /* Only the x18 rewrite is held back: it matches a word on a handful of ordinary bit fields, and a quarter of a constant table does. The
             * others (adrp, adr, mrs, svc) match fixed patterns that a constant almost never has, and the code beside a table needs them. */
            bool is_data = dmap && ((dmap[i >> 3] >> (i & 7)) & 1);
            /* TL_X18_RANGE=<lo>-<hi> (hex vaddrs) or TL_X18_OFF: rewrite fewer sites, to find one that is mishandled. A Mac does not clear x18, so leaving it is safe to test with. */
            static int dbg = -1; static uint64_t dbg_lo, dbg_hi;
            if (dbg < 0) { const char *e = getenv("TL_X18_RANGE"); dbg = getenv("TL_X18_OFF") ? 1 : 0; if (e) { dbg = 2; sscanf(e, "%llx-%llx", (unsigned long long *)&dbg_lo, (unsigned long long *)&dbg_hi); } }
            uint64_t va = L->code[r].start + (uint64_t)i * 4;
            bool skip_dbg = dbg == 1 || (dbg == 2 && !(va >= dbg_lo && va < dbg_hi));
            int xr = (is_data || skip_dbg) ? X18_NONE : x18_rewrite(L, &w[i], pc, delta);
            if (is_data && x18_rewrite_would_apply(insn)) {
                L->n_x18_data++;
                if (G.verbosity >= 3) tl_log_line("ld: %s: x18 word %08x at +%#llx left alone", L->name, insn, (unsigned long long)(L->code[r].start + (uint64_t)i * 4));
            }
            if (xr == X18_DONE) { L->n_x18++; continue; }
            if (xr == X18_FAILED) { L->n_x18_failed++; continue; }
            if ((insn & 0xFFFFFFE0u) == 0xD53B0020u) {            /* mrs Xt, ctr_el0: privileged for user code on Apple silicon */
                /* The cache type register: the smallest cache lines the program may assume. Code that flushes the
                 * instruction cache (V8) reads it to step by line; 64 bytes is right for Apple's and safe for any. */
                uint32_t branch;
                if (adr_stub(L, pc, insn & 0x1Fu, 0x8444c004ull, &branch)) { w[i] = branch; L->n_ctr++; }
                else L->n_adr_failed++;
            } else if ((insn & 0xFFFFFFE0u) == 0xD53BD040u) {            /* mrs Xt, tpidr_el0 */
                w[i] = encode_adrp(insn & 0x1Fu, pc, G.tcb_rw);
                (*n_tpidr)++;
            } else if ((insn & 0x9F000000u) == 0x90000000u) {     /* adrp */
                int64_t imm = (int64_t)((((insn >> 5) & 0x7FFFFu) << 2) | ((insn >> 29) & 3u));
                if (imm & 0x100000) imm -= 0x200000;
                uintptr_t tp = ((uintptr_t)pc & ~(uintptr_t)0xFFF) + (uintptr_t)(imm << 12);
                if (tp >= (uintptr_t)L->rx && tp < (uintptr_t)L->rx + L->npages * PAGE) {
                    size_t tpg = (tp - (uintptr_t)L->rx) / PAGE;
                    if ((L->pflags[tpg] & TL_PAGE_W) && !adrp_targets_code(L, w, i, nwords, insn & 0x1Fu, tp)) {
                        w[i] = encode_adrp(insn & 0x1Fu, pc, (const void *)(tp + (uintptr_t)delta));
                        (*n_adrp)++;
                    }
                }
            } else if (insn == 0xD4000001u) {                       /* svc #0 */
                uint8_t *stub = svc_stub(L, pc);
                int64_t off = stub ? ((int64_t)stub - (int64_t)pc) / 4 : 0;
                if (stub && off > -(1 << 25) && off < (1 << 25)) {
                    w[i] = 0x14000000u | ((uint32_t)off & 0x3FFFFFFu);
                    (*n_svc)++;
                } else {
                    /* A branch reaches 128 MiB, and a big library's stubs are at its far end. Such a site gets the
                     * answer a kernel without the call gives -- ENOSYS -- which is what the code around it (a crash
                     * reporter's raw-syscall wrappers, in Minecraft's case) is written to cope with. */
                    w[i] = 0x92800000u | (37u << 5);                /* movn x0, #37  (x0 = -ENOSYS) */
                    L->n_svc_far++;
                }
            } else if ((insn & 0x9F000000u) == 0x10000000u) {     /* adr */
                int64_t imm = (int64_t)((((insn >> 5) & 0x7FFFFu) << 2) | ((insn >> 29) & 3u));
                if (imm & 0x100000) imm -= 0x200000;
                uintptr_t tp = (uintptr_t)pc + (uintptr_t)imm;
                if (tp >= (uintptr_t)L->rx && tp < (uintptr_t)L->rx + L->npages * PAGE
                    && (L->pflags[(tp - (uintptr_t)L->rx) / PAGE] & TL_PAGE_W)
                    && !in_code(L, (uint64_t)(tp - (uintptr_t)L->rx) + L->base_vaddr)) {
                    uint32_t branch;
                    if (adr_stub(L, pc, insn & 0x1Fu, (uint64_t)(tp + (uintptr_t)delta), &branch)) { w[i] = branch; (*n_adr)++; }
                    else L->n_adr_failed++;
                }
            }
        }
        free(dmap);
    }
}
#else
static void patch_image(tl_lib *L, size_t *a, size_t *b, size_t *c, size_t *d) { (void)L; *a = *b = *c = *d = 0; }
#endif


/* ------------------------------------------------------- thread-local storage */

/*
 * A library's PT_TLS segment is a template for a block every thread has its own copy of. Android code reaches its thread-local variables in
 * one of two ways that matter here: through a "TLS descriptor" (the newer, and what libUE4 uses) or through __tls_get_addr(module, offset). Both end
 * in the same place -- a per-thread block for the library, allocated and filled from the template the first time a thread asks.
 *
 * A descriptor is a pair in the library's GOT: a function to call and an argument. The code calls the function with the descriptor's address and
 * expects the variable's offset from the thread pointer, which it adds to the thread pointer (`mrs x1, tpidr_el0`) itself. Here that register is read as a
 * fixed address (see the tpidr patch), so the function answers with the distance from that fixed address to this thread's copy of the variable. It is called
 * where the compiler expects an ordinary instruction, so it must leave every register but x0 and x1 as it found them -- hence the assembly.
 */
#define MAX_TLS_MODULES 16
static struct { tl_lib *lib; uint64_t init_vaddr, filesz, memsz, align; } g_tlsmod[MAX_TLS_MODULES + 1];
static int g_ntls;
static pthread_key_t g_tls_key;
static pthread_once_t g_tls_once = PTHREAD_ONCE_INIT;

typedef struct { void *blk[MAX_TLS_MODULES + 1]; } tls_thread;
static void tls_thread_free(void *p) { tls_thread *t = p; for (int i = 0; i <= MAX_TLS_MODULES; i++) free(t->blk[i]); free(t); }
static void tls_key_init(void) { pthread_key_create(&g_tls_key, tls_thread_free); }

static void *tls_block(unsigned module)
{
    pthread_once(&g_tls_once, tls_key_init);
    if (module == 0 || module > (unsigned)g_ntls) return NULL;
    tls_thread *t = pthread_getspecific(g_tls_key);
    if (!t) { t = calloc(1, sizeof(*t)); pthread_setspecific(g_tls_key, t); }
    if (!t->blk[module]) {
        uint64_t al = g_tlsmod[module].align < 16 ? 16 : g_tlsmod[module].align;
        void *b = NULL;
        if (posix_memalign(&b, (size_t)al, (size_t)(g_tlsmod[module].memsz + 16)) != 0) return NULL;
        memset(b, 0, (size_t)g_tlsmod[module].memsz);
        const tl_lib *L = g_tlsmod[module].lib;
        if (g_tlsmod[module].filesz) memcpy(b, L->rx + (g_tlsmod[module].init_vaddr - L->base_vaddr), (size_t)g_tlsmod[module].filesz);
        t->blk[module] = b;
    }
    return t->blk[module];
}

void *tl_ld_tls_get_addr(uint64_t module, uint64_t offset)
{
    uint8_t *b = tls_block((unsigned)module);
    return b ? b + offset : NULL;
}

/* arg is the descriptor's second word: the module in the high half, the variable's offset in its template in the low. */
__attribute__((used)) uint64_t tl_tls_offset(uint64_t arg)
{
    uint8_t *b = tls_block((unsigned)(arg >> 32));
    uintptr_t tp = (uintptr_t)G.tcb_rw & ~(uintptr_t)0xFFF;                 /* what `mrs xN, tpidr_el0` was turned into */
    return (uint64_t)((uintptr_t)(b ? b : (uint8_t *)tp) + (uint32_t)arg - tp);
}

#if defined(__aarch64__)
__attribute__((naked, used)) static void tl_tlsdesc_entry(void)
{
    __asm__ volatile(
        "stp x29, x30, [sp, #-16]!\n"
        "sub sp, sp, #512\n"
        "stp x2, x3,   [sp, #0]\n"   "stp x4, x5,   [sp, #16]\n"  "stp x6, x7,   [sp, #32]\n"  "stp x8, x9,   [sp, #48]\n"
        "stp x10, x11, [sp, #64]\n"  "stp x12, x13, [sp, #80]\n"  "stp x14, x15, [sp, #96]\n"  "stp x16, x17, [sp, #112]\n"
        "stp q0, q1,   [sp, #128]\n" "stp q2, q3,   [sp, #160]\n" "stp q4, q5,   [sp, #192]\n" "stp q6, q7,   [sp, #224]\n"
        "stp q16, q17, [sp, #256]\n" "stp q18, q19, [sp, #288]\n" "stp q20, q21, [sp, #320]\n" "stp q22, q23, [sp, #352]\n"
        "stp q24, q25, [sp, #384]\n" "stp q26, q27, [sp, #416]\n" "stp q28, q29, [sp, #448]\n" "stp q30, q31, [sp, #480]\n"
        "ldr x0, [x0, #8]\n"
        "bl _tl_tls_offset\n"
        "ldp x2, x3,   [sp, #0]\n"   "ldp x4, x5,   [sp, #16]\n"  "ldp x6, x7,   [sp, #32]\n"  "ldp x8, x9,   [sp, #48]\n"
        "ldp x10, x11, [sp, #64]\n"  "ldp x12, x13, [sp, #80]\n"  "ldp x14, x15, [sp, #96]\n"  "ldp x16, x17, [sp, #112]\n"
        "ldp q0, q1,   [sp, #128]\n" "ldp q2, q3,   [sp, #160]\n" "ldp q4, q5,   [sp, #192]\n" "ldp q6, q7,   [sp, #224]\n"
        "ldp q16, q17, [sp, #256]\n" "ldp q18, q19, [sp, #288]\n" "ldp q20, q21, [sp, #320]\n" "ldp q22, q23, [sp, #352]\n"
        "ldp q24, q25, [sp, #384]\n" "ldp q26, q27, [sp, #416]\n" "ldp q28, q29, [sp, #448]\n" "ldp q30, q31, [sp, #480]\n"
        "add sp, sp, #512\n"
        "ldp x29, x30, [sp], #16\n"
        "ret\n");
}
#else
static void tl_tlsdesc_entry(void) {}
#endif

/* -------------------------------------------------------------- relocation */

static inline uint64_t rd64(const uint8_t *p) { uint64_t v; memcpy(&v, p, 8); return v; }

/* An address inside the image as pointer data records it: writable view for data, executable view for code. A small library built
 * for 4 KiB pages has code and data on one 16 KiB page, which is then writable; a pointer into its executable sections (a Python
 * module's exec slot, a vtable entry) is still code and must be called through the executable view. */
static uint64_t image_addr(const tl_lib *L, uint64_t vaddr)
{
    uint64_t off = vaddr - L->base_vaddr;
    size_t page = (size_t)(off / PAGE);
    if (page < L->npages && (L->pflags[page] & TL_PAGE_W)) {
        if (L->pflags[page] & TL_PAGE_X)
            for (int r = 0; r < L->ncode; r++)
                if (vaddr >= L->code[r].start && vaddr < L->code[r].end) return (uint64_t)(uintptr_t)(L->rx + off);
        return (uint64_t)(uintptr_t)(L->rw + off);
    }
    return (uint64_t)(uintptr_t)(L->rx + off);
}

static uint64_t bind_symbol(tl_lib *L, uint32_t symidx, bool *failed)
{
    bool cached = L->symcache && symidx < L->ncache;
    if (cached && L->symcache[symidx]) return L->symcache[symidx];
    const elf_sym *s = sym_at(L, symidx);
    const char *name = sym_name(L, s);
    uint64_t val = 0;
    if (s->st_shndx != SHN_UNDEF_ && (s->st_info >> 4) != STB_WEAK_) {
        /* Defined here. Search the scope anyway so an earlier library's definition
         * wins, as it does under ELF interposition... except that a library's own
         * definition is what Android's linker uses first for its own symbols. */
        if ((s->st_info & 0xf) == STT_GNU_IFUNC_) { *failed = true; return 0; }
        val = (uint64_t)(uintptr_t)sym_value(L, s);
    } else {
        void *a = lookup_for(L, name, NULL);
        if (a) {
            val = (uint64_t)(uintptr_t)a;
        } else if ((s->st_info >> 4) == STB_WEAK_) {
            val = 0;
        } else {
            void *stub = make_stub(name);
            if (!stub) { *failed = true; return 0; }
            val = (uint64_t)(uintptr_t)stub;
            L->n_unresolved++;
            G.unresolved++;
            if (G.verbosity >= 2 || L->n_unresolved <= 32) tl_log_line("ld: %s: unresolved import %s", L->name, name);
        }
    }
    if (cached) L->symcache[symidx] = val ? val : 1;   /* 1 marks a resolved NULL */
    return val;
}

static bool reloc_one(tl_lib *L, uint64_t r_offset, uint32_t type, uint32_t symidx, int64_t addend)
{
    uint64_t off = r_offset - L->base_vaddr;
    if (off + 8 > L->npages * PAGE) return false;
    uint64_t *place = (uint64_t *)(L->rw + off);
    bool failed = false;
    switch (type) {
    case R_NONE:
        return true;
    case R_RELATIVE:
        *place = image_addr(L, (uint64_t)addend);
        return true;
    case R_ABS64: case R_GLOB_DAT: case R_JUMP_SLOT: {
        if (symidx == 0) { *place = image_addr(L, (uint64_t)addend); return true; }
        uint64_t v = bind_symbol(L, symidx, &failed);
        if (failed) return false;
        if (v == 1 && L->symcache) v = 0;
        *place = v + (type == R_ABS64 ? (uint64_t)addend : 0);
        return true;
    }
    case R_IRELATIVE: {
        /* The resolver is guest code: run it, store what it returns. */
        uint64_t (*resolver)(void) = (uint64_t (*)(void))(uintptr_t)(L->rx + ((uint64_t)addend - L->base_vaddr));
        *place = resolver();
        return true;
    }
    case R_TLSDESC: case R_TLS_DTPMOD: case R_TLS_DTPREL: {
        uint64_t symoff = 0;
        if (symidx) {
            const elf_sym *ts = sym_at(L, symidx);
            if (ts->st_shndx == SHN_UNDEF_) { tl_log_line("ld: %s: a thread-local variable of another library (%s) is not supported", L->name, sym_name(L, ts)); return false; }
            symoff = ts->st_value;
        }
        if (!L->tls_id) { tl_log_line("ld: %s: a TLS relocation but no TLS segment", L->name); return false; }
        if (type == R_TLSDESC) {
            if (off + 16 > L->npages * PAGE) return false;
            place[0] = (uint64_t)(uintptr_t)&tl_tlsdesc_entry;
            place[1] = ((uint64_t)L->tls_id << 32) | (uint32_t)(symoff + (uint64_t)addend);
        } else if (type == R_TLS_DTPMOD) {
            *place = (uint64_t)L->tls_id;
        } else {
            *place = symoff + (uint64_t)addend;
        }
        return true;
    }
    case R_TLS_TPREL:
        tl_log_line("ld: %s: an initial-exec TLS relocation (type %u) is not supported", L->name, type);
        return false;
    default:
        tl_log_line("ld: %s: unsupported relocation type %u", L->name, type);
        return false;
    }
}

static bool do_relas(tl_lib *L, uint64_t addr, uint64_t size, size_t *count)
{
    if (!addr || !size) return true;
    const elf_rela *r = at(L, addr);
    for (size_t i = 0; i < size / sizeof(elf_rela); i++) {
        if (!reloc_one(L, r[i].r_offset, (uint32_t)(r[i].r_info & 0xffffffffu),
                       (uint32_t)(r[i].r_info >> 32), r[i].r_addend)) return false;
        (*count)++;
    }
    return true;
}

typedef struct { const uint8_t *p, *end; bool bad; } sleb;
static int64_t rd_sleb(sleb *s)
{
    uint64_t v = 0; unsigned shift = 0; uint8_t b;
    do {
        if (s->p >= s->end || shift >= 64) { s->bad = true; return 0; }
        b = *s->p++;
        v |= (uint64_t)(b & 0x7f) << shift;
        shift += 7;
    } while (b & 0x80);
    if (shift < 64 && (b & 0x40)) v |= ~0ull << shift;
    return (int64_t)v;
}

static bool do_packed(tl_lib *L, size_t *count)
{
    /*
     * Android's APS2 encoding, read the way bionic's linker reads it. Offsets and
     * addends are running totals, not absolute values: each entry adds a signed
     * delta to the previous one. Within a group the order is: offset delta (if the
     * group shares one), info (if shared), shared-addend delta, then per entry its
     * own offset delta, info and addend delta as the flags leave them unshared.
     */
    if (!L->arela || !L->arelasz) return true;
    const uint8_t *d = at(L, L->arela);
    if (memcmp(d, "APS2", 4) != 0) { tl_log_line("ld: %s: packed relocations lack APS2 magic", L->name); return false; }
    sleb s = { d + 4, d + L->arelasz, false };
    int64_t total = rd_sleb(&s);
    uint64_t r_offset = (uint64_t)rd_sleb(&s), r_info = 0;
    int64_t r_addend = 0;
    if (s.bad || total < 0) return false;
    enum { BY_INFO = 1, BY_DELTA = 2, BY_ADDEND = 4, HAS_ADDEND = 8 };
    for (int64_t idx = 0; idx < total;) {
        int64_t group = rd_sleb(&s), flags = rd_sleb(&s);
        if (s.bad || group <= 0 || idx + group > total) return false;
        uint64_t group_delta = 0;
        if (flags & BY_DELTA) group_delta = (uint64_t)rd_sleb(&s);
        if (flags & BY_INFO) r_info = (uint64_t)rd_sleb(&s);
        int addend_mode = (int)(flags & (HAS_ADDEND | BY_ADDEND));
        if (addend_mode == (HAS_ADDEND | BY_ADDEND)) r_addend += rd_sleb(&s);
        else if (addend_mode != HAS_ADDEND) r_addend = 0;
        for (int64_t i = 0; i < group; i++) {
            r_offset += (flags & BY_DELTA) ? group_delta : (uint64_t)rd_sleb(&s);
            if (!(flags & BY_INFO)) r_info = (uint64_t)rd_sleb(&s);
            if (addend_mode == HAS_ADDEND) r_addend += rd_sleb(&s);
            if (s.bad) return false;
            if (!reloc_one(L, r_offset, (uint32_t)(r_info & 0xffffffffu), (uint32_t)(r_info >> 32), r_addend)) return false;
            (*count)++;
        }
        idx += group;
    }
    return true;
}

static bool do_relr(tl_lib *L, size_t *count)
{
    if (!L->relr || !L->relrsz) return true;
    const uint8_t *w = at(L, L->relr);
    uint64_t where = 0;
    for (size_t i = 0; i < L->relrsz / 8; i++) {
        uint64_t word = rd64(w + i * 8);
        if (!(word & 1)) {
            if (!reloc_one(L, word, R_RELATIVE, 0, (int64_t)rd64(L->rw + (word - L->base_vaddr)))) return false;
            (*count)++;
            where = word + 8;
        } else {
            for (unsigned b = 1; b < 64; b++) {
                if (word & (1ull << b)) {
                    uint64_t a = where + (uint64_t)(b - 1) * 8;
                    if (!reloc_one(L, a, R_RELATIVE, 0, (int64_t)rd64(L->rw + (a - L->base_vaddr)))) return false;
                    (*count)++;
                }
            }
            where += 63 * 8;
        }
    }
    return true;
}

/* ----------------------------------------------------------------- mapping */

static void parse_dynamic(tl_lib *L, uint64_t dyn_vaddr, uint64_t dyn_size)
{
    const elf_dyn *d = at(L, dyn_vaddr);
    for (size_t i = 0; i < dyn_size / sizeof(elf_dyn); i++) {
        switch (d[i].d_tag) {
        case DT_NULL_: return;
        case DT_NEEDED_: if (L->nneeded < MAX_DEPS) L->needed[L->nneeded++] = d[i].d_val; break;
        case DT_STRTAB_: L->strtab = d[i].d_val; break;
        case DT_STRSZ_: L->strsz = d[i].d_val; break;
        case DT_SYMTAB_: L->symtab = d[i].d_val; break;
        case DT_GNU_HASH_: L->gnu_hash = d[i].d_val; break;
        case DT_HASH_: L->sysv_hash = d[i].d_val; break;
        case DT_RELA_: L->rela = d[i].d_val; break;
        case DT_RELASZ_: L->relasz = d[i].d_val; break;
        case DT_JMPREL_: L->jmprel = d[i].d_val; break;
        case DT_PLTRELSZ_: L->pltrelsz = d[i].d_val; break;
        case DT_ANDROID_RELA_: L->arela = d[i].d_val; break;
        case DT_ANDROID_RELASZ_: L->arelasz = d[i].d_val; break;
        case DT_RELR_: case DT_ANDROID_RELR_: L->relr = d[i].d_val; break;
        case DT_RELRSZ_: case DT_ANDROID_RELRSZ_: L->relrsz = d[i].d_val; break;
        case DT_INIT_: L->init = d[i].d_val; break;
        case DT_INIT_ARRAY_: L->init_array = d[i].d_val; break;
        case DT_INIT_ARRAYSZ_: L->init_arraysz = d[i].d_val; break;
        case DT_SONAME_: break;     /* read once the string table is known */
        default: break;
        }
    }
}

static bool ensure_tcb(void)
{
    if (G.tcb_rw) return true;
    if (!tl_xmem_alloc(PAGE, &G.tcb_rx, &G.tcb_rw)) return false;
    uint64_t *t = (uint64_t *)G.tcb_rw;
    t[0] = (uint64_t)(uintptr_t)G.tcb_rw;      /* self */
    t[1] = 1000; t[2] = 1000;
    t[5] = 0xdeadbeefcafebabeull;             /* [tpidr_el0 + 0x28]: the stack cookie */
    return true;
}


/* ------------------------------------------------------------------ unwind ranges */

/*
 * Which words of the executable sections are code. Assembly files put their constant tables in .text (OpenSSL's SHA-256 and SHA-512 keep
 * theirs right after the function), and a table word that happens to name x18 -- one constant in 40 does -- would be "rewritten" into a branch to
 * a stub, which turns the constant into garbage and the hash into a wrong answer. The unwind tables list the extent of every function the
 * compiler emitted, which is exactly the code; a word outside all of them is data, and the rewrites that only code deserves leave it alone.
 * A library with no unwind tables, or tables this cannot read, is treated as code throughout, as before.
 */
static bool read_enc(const uint8_t *p, const uint8_t *end, uint8_t enc, uint64_t field_vaddr, int64_t *out, size_t *size)
{
    int64_t v = 0; size_t n;
    switch (enc & 0x0F) {
    case 0x00: case 0x04: case 0x0C: n = 8; if (p + n > end) return false; memcpy(&v, p, 8); break;
    case 0x02: n = 2; if (p + n > end) return false; { uint16_t t; memcpy(&t, p, 2); v = t; } break;
    case 0x0A: n = 2; if (p + n > end) return false; { int16_t t; memcpy(&t, p, 2); v = t; } break;
    case 0x03: n = 4; if (p + n > end) return false; { uint32_t t; memcpy(&t, p, 4); v = t; } break;
    case 0x0B: n = 4; if (p + n > end) return false; { int32_t t; memcpy(&t, p, 4); v = t; } break;
    default: return false;
    }
    if ((enc & 0x70) == 0x10) v += (int64_t)field_vaddr;          /* pc-relative; the others (absolute, or relative to the table) do not occur for an FDE's range */
    else if ((enc & 0x70) != 0x00) return false;
    *out = v; *size = n;
    return true;
}
static bool uleb(const uint8_t **p, const uint8_t *end, uint64_t *v)
{
    uint64_t r = 0; int sh = 0;
    while (*p < end) { uint8_t b = *(*p)++; r |= (uint64_t)(b & 0x7F) << sh; sh += 7; if (!(b & 0x80)) { *v = r; return true; } if (sh > 63) return false; }
    return false;
}
typedef struct { uint64_t s, e; } fde_range;
static int cmp_range(const void *a, const void *b) { const fde_range *x = a, *y = b; return x->s < y->s ? -1 : x->s > y->s; }

/* The encoding a CIE says its FDEs use for the address of the function (the 'R' augmentation); false if the record cannot be read. */
static bool cie_fde_encoding(const uint8_t *cie, const uint8_t *end, uint8_t *enc)
{
    if (cie + 12 > end) return false;
    uint32_t len; memcpy(&len, cie, 4);
    const uint8_t *rec_end = cie + 4 + len;
    if (len == 0 || len == 0xFFFFFFFFu || rec_end > end) return false;
    const uint8_t *q = cie + 8;                               /* past the length and the zero id */
    uint8_t version = *q++;
    const char *aug = (const char *)q;
    while (q < rec_end && *q) q++;
    if (q >= rec_end) return false;
    q++;
    uint64_t t;
    if (!uleb(&q, rec_end, &t) || !uleb(&q, rec_end, &t)) return false;          /* code and data alignment */
    if (version == 1) q++; else if (!uleb(&q, rec_end, &t)) return false;         /* the return address register */
    uint8_t fenc = 0;
    if (aug[0] == 'z') {
        uint64_t alen; if (!uleb(&q, rec_end, &alen)) return false;
        const uint8_t *aend = q + alen;
        for (const char *a = aug + 1; *a && q < aend; a++) {
            if (*a == 'R') fenc = *q++;
            else if (*a == 'L') q++;
            else if (*a == 'P') { uint8_t penc = *q++; int64_t dummy; size_t psz; if (!read_enc(q, aend, (uint8_t)(penc & 0x0F), 0, &dummy, &psz)) return false; q += psz; }
            else if (*a == 'S' || *a == 'B') { /* no data */ }
            else break;
        }
    }
    *enc = fenc;
    return true;
}

static void load_unwind_ranges(tl_lib *L, const uint8_t *file, size_t flen, const elf_phdr *phs, unsigned phnum)
{
    const uint32_t PT_EH = 0x6474e550u;
    size_t hdr_off = (size_t)-1; uint64_t hdr_vaddr = 0;
    for (unsigned i = 0; i < phnum; i++) if (phs[i].p_type == PT_EH) { hdr_off = (size_t)phs[i].p_offset; hdr_vaddr = phs[i].p_vaddr; }
    if (hdr_off == (size_t)-1 || hdr_off + 12 > flen) return;
    const uint8_t *h = file + hdr_off, *fend = file + flen;
    const char *why = "unsupported .eh_frame_hdr";
    fde_range *r = NULL;
    if (h[0] != 1 || h[2] == 0xFF || h[3] != 0x3B) goto fail;                   /* version 1, a count, a table of datarel sdata4 pairs */
    {
        int64_t ehf, count; size_t s1, s2;
        if (!read_enc(h + 4, fend, h[1], hdr_vaddr + 4, &ehf, &s1) || !read_enc(h + 4 + s1, fend, h[2], 0, &count, &s2)) goto fail;
        const uint8_t *tbl = h + 4 + s1 + s2;
        if (count <= 0 || tbl + (size_t)count * 8 > fend) { why = "the FDE table does not fit the file"; goto fail; }
        r = malloc((size_t)count * sizeof(*r));
        size_t n = 0;
        uint64_t last_cie = (uint64_t)-1; uint8_t last_enc = 0;
        for (int64_t i = 0; i < count; i++) {
            int32_t start_rel, fde_rel;
            memcpy(&start_rel, tbl + i * 8, 4); memcpy(&fde_rel, tbl + i * 8 + 4, 4);
            uint64_t fde_vaddr = hdr_vaddr + (uint64_t)(int64_t)fde_rel;
            size_t fde_off = (size_t)-1;
            for (unsigned q = 0; q < phnum; q++)
                if (phs[q].p_type == PT_LOAD_ && fde_vaddr >= phs[q].p_vaddr && fde_vaddr < phs[q].p_vaddr + phs[q].p_filesz) { fde_off = (size_t)(phs[q].p_offset + (fde_vaddr - phs[q].p_vaddr)); break; }
            if (fde_off == (size_t)-1 || fde_off + 12 > flen) { why = "an FDE is outside the file"; goto fail; }
            const uint8_t *fde = file + fde_off;
            uint32_t len, cie_ptr; memcpy(&len, fde, 4); memcpy(&cie_ptr, fde + 4, 4);
            if (len == 0 || len == 0xFFFFFFFFu || fde_off + 4 + len > flen || cie_ptr == 0 || fde_off + 4 < cie_ptr) { why = "an FDE record is malformed"; goto fail; }
            size_t cie_off = fde_off + 4 - cie_ptr;
            if (cie_off != last_cie) {
                if (!cie_fde_encoding(file + cie_off, fend, &last_enc)) { why = "a CIE record cannot be read"; goto fail; }
                last_cie = cie_off;
            }
            int64_t pc_begin, range; size_t sz1, sz2;
            const uint8_t *fld = fde + 8;
            if (!read_enc(fld, fde + 4 + len, last_enc, fde_vaddr + 8, &pc_begin, &sz1)) { why = "an FDE address encoding is not supported"; goto fail; }
            if (!read_enc(fld + sz1, fde + 4 + len, (uint8_t)(last_enc & 0x0F), 0, &range, &sz2)) { why = "an FDE range encoding is not supported"; goto fail; }
            r[n].s = (uint64_t)pc_begin; r[n].e = (uint64_t)pc_begin + (uint64_t)range; n++;
        }
        qsort(r, n, sizeof(*r), cmp_range);
        L->fde_start = malloc(n * sizeof(uint64_t)); L->fde_end = malloc(n * sizeof(uint64_t));
        for (size_t i = 0; i < n; i++) { L->fde_start[i] = r[i].s; L->fde_end[i] = r[i].e; }
        L->nfde = n;
        free(r);
        return;
    }
fail:
    if (G.verbosity >= 1) tl_log_line("ld:   %s: unwind tables not used (%s)", L->name, why);
    free(r);
}

/*
 * Which words of an executable range are data. A word inside a function the unwind tables know of is code, always. Elsewhere (a library built without
 * unwind tables has long stretches of code there, as well as the constant tables assembly files keep in .text) it is data when it lies in a cluster
 * of words that no instruction can have: a table of 32-bit constants is a quarter unallocated encodings, so any 17 words of it hold three or more such
 * words, while compiled code holds none. A word within 8 of such a cluster is counted in it, which takes in the edges of the table. Returns a bitmap, one bit
 * per word, or NULL when no word is data.
 */
static bool plausible_word(uint32_t w)
{
    if ((w >> 16) == 0) return true;                 /* udf: the zero padding between functions, and traps */
    unsigned g = (w >> 25) & 0xF;                    /* op0 of the A64 encoding space: 0000 reserved, 0001 and 0011 unallocated, 0010 SVE */
    return g > 3;
}

static uint8_t *build_data_map(const tl_lib *L, const uint32_t *w, size_t n, uint64_t vstart, size_t *n_data)
{
    *n_data = 0;
    if (n < 32) return NULL;
    uint8_t *bad = calloc((n + 7) / 8, 1), *hot = calloc((n + 7) / 8, 1), *data = calloc((n + 7) / 8, 1);
    size_t j = 0; uint64_t max_end = 0;
#define BIT(m, i) (((m)[(i) >> 3] >> ((i) & 7)) & 1)
#define SETBIT(m, i) ((m)[(i) >> 3] |= (uint8_t)(1u << ((i) & 7)))
    for (size_t i = 0; i < n; i++) {                 /* words that are neither inside a function nor an instruction */
        uint64_t a = vstart + (uint64_t)i * 4;
        while (j < L->nfde && L->fde_start[j] <= a) { if (L->fde_end[j] > max_end) max_end = L->fde_end[j]; j++; }
        if (a < max_end) continue;
        if (!plausible_word(w[i])) SETBIT(bad, i);
    }
    int sum = 0;                                     /* the window of 17 words around i */
    for (size_t i = 0; i < 8 && i < n; i++) sum += BIT(bad, i);
    for (size_t i = 0; i < n; i++) {
        if (i + 8 < n) sum += BIT(bad, i + 8);
        if (i >= 9) sum -= BIT(bad, i - 9);
        if (sum >= 3 && BIT(bad, i)) SETBIT(hot, i);
    }
    /* A table is the run of words between the first and last such word, as long as they come within 64 of each other (in a table of constants three
     * in four words are fine on their own, but a gap of 64 without a bad one has odds of one in 20 million); and 32 words either side, which is as far
     * as the first words of a table can be from its first bad one with any likelihood. */
    long last = -1;
    for (size_t i = 0; i < n; i++) {
        if (!BIT(hot, i)) continue;
        size_t from = (last >= 0 && (long)i - last <= 64) ? (size_t)last : (i >= 32 ? i - 32 : 0);
        for (size_t k = from; k <= i; k++) SETBIT(data, k);
        for (size_t k = i + 1; k <= i + 32 && k < n; k++) SETBIT(data, k);
        last = (long)i;
    }
    /* function bodies are never clusters of data, whatever sits beside them; and the words a PC-relative load reads are data wherever they are */
    j = 0; max_end = 0;
    uint8_t *incode = hot;                       /* reused: the cluster test is done with it */
    memset(incode, 0, (n + 7) / 8);
    for (size_t i = 0; i < n; i++) {
        uint64_t a = vstart + (uint64_t)i * 4;
        while (j < L->nfde && L->fde_start[j] <= a) { if (L->fde_end[j] > max_end) max_end = L->fde_end[j]; j++; }
        if (a < max_end) { SETBIT(incode, i); data[i >> 3] &= (uint8_t)~(1u << (i & 7)); }
    }
    for (size_t i = 0; i < n; i++) {
        uint32_t insn = w[i];
        if ((insn & 0x3B000000u) != 0x18000000u) continue;                 /* ldr Rt, <literal> */
        if (BIT(data, i) && !BIT(incode, i)) continue;                       /* a lookalike inside a table is not a load */
        int64_t imm = (int64_t)((insn >> 5) & 0x7FFFFu); if (imm & 0x40000) imm -= 0x80000;
        int64_t t = (int64_t)i + imm;
        unsigned opc = insn >> 30, v = (insn >> 26) & 1;
        unsigned nw = v ? (opc == 0 ? 1 : opc == 1 ? 2 : opc == 2 ? 4 : 0) : (opc == 1 ? 2 : opc == 3 ? 0 : 1);     /* words read: s/w and ldrsw 1, d/x 2, q 4; prfm none */
        for (unsigned k = 0; k < nw; k++) if (t + k >= 0 && (size_t)(t + k) < n) SETBIT(data, (size_t)(t + k));
    }
    for (size_t i = 0; i < n; i++) if (BIT(data, i)) (*n_data)++;
#undef BIT
#undef SETBIT
    free(bad); free(hot);
    if (!*n_data) { free(data); return NULL; }
    return data;
}

static tl_lib *map_library(const char *name, uint8_t *file, size_t flen)
{
    if (G.nlibs >= MAX_LIBS) { tl_log_line("ld: too many libraries"); return NULL; }
    const elf_ehdr *eh = (const elf_ehdr *)file;
    if (flen < sizeof(*eh) || memcmp(file, "\x7f""ELF", 4) != 0 || eh->e_ident[4] != 2 || eh->e_ident[5] != 1
        || eh->e_machine != EM_AARCH64_) {
        tl_log_line("ld: %s is not a 64-bit little-endian arm64 ELF image", name);
        return NULL;
    }
    if (eh->e_phentsize < sizeof(elf_phdr) || eh->e_phoff + (uint64_t)eh->e_phnum * eh->e_phentsize > flen) {
        tl_log_line("ld: %s: program headers run off the file", name);
        return NULL;
    }
    tl_segment loads[16]; int nloads = 0; tl_segment relro = {0}; bool has_relro = false;
    elf_phdr tls_seg = {0}; bool has_tls = false;
    uint64_t dyn_v = 0, dyn_n = 0;
    elf_phdr *phs = malloc((size_t)eh->e_phnum * sizeof(elf_phdr));
    if (!phs) return NULL;
    for (unsigned i = 0; i < eh->e_phnum; i++) {
        memcpy(&phs[i], file + eh->e_phoff + (size_t)i * eh->e_phentsize, sizeof(elf_phdr));
        const elf_phdr *p = &phs[i];
        if (p->p_type == PT_LOAD_ && nloads < 16) {
            loads[nloads].vaddr = p->p_vaddr; loads[nloads].memsz = p->p_memsz; loads[nloads].flags = p->p_flags;
            nloads++;
        } else if (p->p_type == PT_GNU_RELRO_) {
            relro.vaddr = p->p_vaddr; relro.memsz = p->p_memsz; has_relro = true;
        } else if (p->p_type == PT_DYNAMIC_) {
            dyn_v = p->p_vaddr; dyn_n = p->p_filesz;
        } else if (p->p_type == PT_TLS_) {
            tls_seg = *p; has_tls = true;
        }
    }
    if (!nloads || !dyn_n) { tl_log_line("ld: %s has no loadable or dynamic segments", name); free(phs); return NULL; }

    uint64_t base_vaddr = 0;
    size_t npages = tl_page_plan(loads, (size_t)nloads, has_relro ? &relro : NULL, PAGE, NULL, 0, &base_vaddr);
    if (!npages) { tl_log_line("ld: %s: unusable page layout", name); free(phs); return NULL; }
    uint8_t *flags = malloc(npages);
    tl_page_plan(loads, (size_t)nloads, has_relro ? &relro : NULL, PAGE, flags, npages, &base_vaddr);

    /* The executable sections, from the section headers when the file has them (it almost always does);
     * otherwise whole executable segments, which is correct for a library with nothing but code in them. */
    struct { uint64_t vaddr, size, foff; } code[16]; int ncode = 0;
    if (eh->e_shoff && eh->e_shentsize >= 64 && eh->e_shnum && eh->e_shoff + (uint64_t)eh->e_shnum * eh->e_shentsize <= flen) {
        for (unsigned i = 0; i < eh->e_shnum && ncode < 16; i++) {
            const uint8_t *sh = file + eh->e_shoff + (size_t)i * eh->e_shentsize;
            uint32_t type; uint64_t flags, addr, off, size;
            memcpy(&type, sh + 4, 4); memcpy(&flags, sh + 8, 8); memcpy(&addr, sh + 16, 8); memcpy(&off, sh + 24, 8); memcpy(&size, sh + 32, 8);
            if (type == 1 /* PROGBITS */ && (flags & 4 /* EXECINSTR */) && size && off + size <= flen) {
                code[ncode].vaddr = addr; code[ncode].size = size; code[ncode].foff = off; ncode++;
            }
        }
    }
    /* Bytes of an executable segment that no section claims: protectors (app shields, packers) keep their unpacking code there,
     * out of the section headers, and it reads tpidr_el0 like any other code. Every allocated section counts as claimed, so
     * .rodata and .eh_frame stay data. */
    if (ncode && eh->e_shoff && eh->e_shentsize >= 64) {
        struct { uint64_t lo, hi; } claimed[64]; int nclaimed = 0;
        for (unsigned i = 0; i < eh->e_shnum && nclaimed < 64; i++) {
            const uint8_t *sh = file + eh->e_shoff + (size_t)i * eh->e_shentsize;
            uint64_t flags, addr, size; uint32_t type;
            memcpy(&type, sh + 4, 4); memcpy(&flags, sh + 8, 8); memcpy(&addr, sh + 16, 8); memcpy(&size, sh + 32, 8);
            if ((flags & 2 /* ALLOC */) && size) { claimed[nclaimed].lo = addr; claimed[nclaimed].hi = addr + (type == 8 /* NOBITS */ ? 0 : size); nclaimed++; }
        }
        for (unsigned i = 0; i < eh->e_phnum && ncode < 16; i++) {
            const elf_phdr *p = &phs[i];
            if (p->p_type != PT_LOAD_ || !(p->p_flags & PF_X_) || p->p_offset + p->p_filesz > flen) continue;
            uint64_t a = p->p_vaddr, end = p->p_vaddr + p->p_filesz;
            while (a < end && ncode < 16) {
                /* the next claimed byte at or after a */
                uint64_t next = end; bool inside = false;
                for (int k = 0; k < nclaimed; k++) {
                    if (a >= claimed[k].lo && a < claimed[k].hi) { inside = true; a = claimed[k].hi; break; }
                    if (claimed[k].lo > a && claimed[k].lo < next) next = claimed[k].lo;
                }
                if (inside) continue;
                uint64_t lo = (a + 3) & ~3ull, hi = next & ~3ull;
                if (hi > lo && hi - lo >= 256 && lo > (uint64_t)eh->e_phoff + (uint64_t)eh->e_phnum * eh->e_phentsize + 0x1000) {   /* not the headers at the start */
                    code[ncode].vaddr = lo; code[ncode].size = hi - lo; code[ncode].foff = p->p_offset + (lo - p->p_vaddr); ncode++;
                    tl_log_line("ld: %s: %#llx-%#llx is executable but in no section: treated as code", name, (unsigned long long)lo, (unsigned long long)hi);
                }
                a = next;
            }
        }
    }
    if (!ncode) {
        tl_log_line("ld: %s has no section headers; treating every executable segment as code", name);
        for (unsigned i = 0; i < eh->e_phnum && ncode < 16; i++) {
            const elf_phdr *p = &phs[i];
            if (p->p_type == PT_LOAD_ && (p->p_flags & PF_X_) && p->p_offset + p->p_filesz <= flen) {
                code[ncode].vaddr = p->p_vaddr; code[ncode].size = p->p_filesz; code[ncode].foff = p->p_offset; ncode++;
            }
        }
    }

    /* Stub pages after the image: one literal slot, a few probes, a stub for every raw
     * system call, and one for every instruction that names the reserved register x18. */
    size_t stub_bytes = 16 + 8192;
    for (int r = 0; r < ncode; r++) {
        const uint32_t *wv = (const uint32_t *)(file + code[r].foff);
        for (size_t k = 0, cnt = (size_t)(code[r].size / 4); k < cnt; k++) {
            uint32_t v = wv[k];
            if (v == 0xD4000001u) stub_bytes += 32;
            else if ((v & 0xFFFFFFE0u) == 0xD53B0020u) stub_bytes += 32;                 /* mrs Xt, CTR_EL0 */
            else if ((v & 0x9F000000u) == 0x10000000u) {                 /* adr: a stub if it reaches writable data */
                int64_t imm = (int64_t)((((v >> 5) & 0x7FFFFu) << 2) | ((v >> 29) & 3u));
                if (imm & 0x100000) imm -= 0x200000;
                int64_t tv = (int64_t)(code[r].vaddr + k * 4) + imm - (int64_t)base_vaddr;
                if (tv >= 0 && (size_t)tv < npages * PAGE && (flags[(size_t)tv / PAGE] & TL_PAGE_W)) stub_bytes += 32;
            }
            else if (((v & 31u) == 18 || ((v >> 5) & 31u) == 18 || ((v >> 10) & 31u) == 18 || ((v >> 16) & 31u) == 18) && a64_uses_gpr(v, 18, NULL))
                stub_bytes += X18_STUB_BYTES;
        }
    }
    size_t nstub = (stub_bytes + PAGE - 1) / PAGE;
    if (!vx18_init()) tl_log_line("ld: no thread-specific slot for the virtual x18; instructions using x18 will not be rewritten");

    uint8_t *rx, *rw;
    if (!tl_xmem_alloc((npages + nstub) * PAGE, &rx, &rw)) {
        tl_log_line("ld: %s needs %zu MiB of executable memory and the region has %zu MiB left", name,
                    npages * PAGE >> 20, (tl_xmem_size() - tl_xmem_used()) >> 20);
        free(flags); free(phs);
        return NULL;
    }
    /* The region's pages are not guaranteed zero (StikDebug writes a byte into each),
     * and .bss has to be. */
    memset(rw, 0, (npages + nstub) * PAGE);
    for (unsigned i = 0; i < eh->e_phnum; i++) {
        const elf_phdr *p = &phs[i];
        if (p->p_type != PT_LOAD_ || !p->p_filesz) continue;
        if (p->p_offset + p->p_filesz > flen || p->p_vaddr < base_vaddr
            || p->p_vaddr - base_vaddr + p->p_filesz > npages * PAGE) {
            tl_log_line("ld: %s: segment %u is outside the file or the image", name, i);
            free(flags); free(phs);
            return NULL;
        }
        memcpy(rw + (p->p_vaddr - base_vaddr), file + p->p_offset, p->p_filesz);
    }

    tl_lib *L = calloc(1, sizeof(*L));
    snprintf(L->name, sizeof(L->name), "%s", name);
    L->rx = rx; L->rw = rw; L->base_vaddr = base_vaddr; L->npages = npages; L->pflags = flags;
    L->phdr = phs; L->phnum = eh->e_phnum;
    load_unwind_ranges(L, file, flen, phs, eh->e_phnum);
    if (has_tls) {
        if (g_ntls >= MAX_TLS_MODULES) { tl_log_line("ld: %s: too many libraries with thread-local storage", name); return NULL; }
        L->tls_id = ++g_ntls;
        g_tlsmod[L->tls_id].lib = L; g_tlsmod[L->tls_id].init_vaddr = tls_seg.p_vaddr; g_tlsmod[L->tls_id].filesz = tls_seg.p_filesz;
        g_tlsmod[L->tls_id].memsz = tls_seg.p_memsz; g_tlsmod[L->tls_id].align = tls_seg.p_align;
    }
    for (int r = 0; r < ncode; r++) { L->code[r].start = code[r].vaddr; L->code[r].end = code[r].vaddr + code[r].size; }
    L->ncode = ncode;
    L->stub_rx = rx + npages * PAGE; L->stub_rw = rw + npages * PAGE; L->stub_used = 16; L->stub_cap = nstub * PAGE; L->nstub = nstub;
    { uint64_t h = (uint64_t)(uintptr_t)tl_svc_common; memcpy(L->stub_rw, &h, 8); }
    parse_dynamic(L, dyn_v, dyn_n);
    if (L->strtab) {
        const elf_dyn *d = at(L, dyn_v);
        for (size_t i = 0; i < dyn_n / sizeof(elf_dyn) && d[i].d_tag != DT_NULL_; i++) {
            if (d[i].d_tag == DT_SONAME_) snprintf(L->soname, sizeof(L->soname), "%s", (const char *)at(L, L->strtab) + d[i].d_val);
        }
    }
    if (!L->soname[0]) snprintf(L->soname, sizeof(L->soname), "%s", name);
    L->nsyms = count_dynsyms(L);
    L->ncache = dynsym_bound(L, L->nsyms);
    if (L->ncache) L->symcache = calloc(L->ncache, sizeof(uint64_t));
    G.libs[G.nlibs++] = L;
    return L;
}

/* ------------------------------------------------------------------ loading */

static bool relocate(tl_lib *L)
{
    size_t count = 0;
    L->state = 1;
    bool ok = do_relas(L, L->rela, L->relasz, &count)
           && do_relas(L, L->jmprel, L->pltrelsz, &count)
           && do_packed(L, &count)
           && do_relr(L, &count);
    free(L->symcache);
    L->symcache = NULL;
    if (!ok) { tl_log_line("ld: %s: relocation failed", L->name); return false; }
    /* The relocation table is dead now that the image is relocated. Its tail becomes a second pool of stubs, for the
     * sites that are out of branch range of the stub pages after a very large image. */
    if (L->relasz >= (1u << 20) && L->rela >= L->base_vaddr) {
        uint64_t end = (L->rela + L->relasz) & ~(uint64_t)(PAGE - 1), cap = 256u << 10;
        if (end - L->rela > cap + PAGE) {
            size_t off = (size_t)(end - cap - L->base_vaddr);
            L->isl_rx = L->rx + off; L->isl_rw = L->rw + off; L->isl_cap = cap; L->isl_used = 16;
            memset(L->isl_rw, 0, cap);
            uint64_t h = (uint64_t)(uintptr_t)tl_svc_common; memcpy(L->isl_rw, &h, 8);
        }
    }
    size_t t = 0, a = 0, ad = 0, sv = 0;
    patch_image(L, &t, &a, &ad, &sv);
    tl_xmem_flush(L->rx, (L->npages + L->nstub) * PAGE);
    L->state = 2;
    if (G.verbosity >= 1) {
        tl_log_line("ld: %-36s %5.1f MiB  %7zu relocs, %4zu tpidr + %5zu adrp patched%s%s", L->name,
                    (double)(L->npages * PAGE) / 1048576.0, count, t, a,
                    L->n_unresolved ? ", unresolved imports: " : "", "");
        if (L->n_unresolved) tl_log_line("ld:   %s: %u imports bound to logging stubs", L->name, L->n_unresolved);
        if (ad) tl_log_line("ld:   %s: %zu 'adr' instructions that reach writable data rewritten", L->name, ad);
        if (L->n_adr_failed) tl_log_line("ld:   %s: %zu 'adr' instructions reach writable data and could not be rewritten", L->name, L->n_adr_failed);
        if (L->n_ctr) tl_log_line("ld:   %s: %zu reads of CTR_EL0 replaced by a constant", L->name, L->n_ctr);
        if (L->n_svc_far) tl_log_line("ld:   %s: %zu raw system-call sites are out of branch range of the stubs and answer ENOSYS", L->name, L->n_svc_far);
        if (sv) tl_log_line("ld:   %s: %zu raw system-call sites rewritten", L->name, sv);
        if (L->n_x18 || L->n_x18_failed) tl_log_line("ld:   %s: %zu instructions using x18 rewritten for the virtual register%s", L->name, L->n_x18,
                                                     L->n_x18_failed ? " (and some that could not be)" : "");
        if (L->n_x18_data) tl_log_line("ld:   %s: %zu words that look like x18 instructions left alone: they are constants outside every function", L->name, L->n_x18_data);
    }
    return true;
}

/* Folders a library named only by its file name is also looked for in (the Java runtime's: the ART APEX's lib64). */
static char *g_search[8];
static int g_nsearch;
void tl_ld_add_search_dir(const char *dir) { if (g_nsearch < 8) g_search[g_nsearch++] = strdup(dir); }

static tl_lib *load_locked(const char *name, int depth)
{
    tl_lib *L = find_loaded(name);
    if (L) return L;
    if (tl_bionic_is_system_lib(name)) return NULL;
    if (depth > 32) { tl_log_line("ld: dependency chain too deep at %s", name); return NULL; }

    uint8_t *file; size_t flen;
    bool got = fetch_from_apks(base_name(name), &file, &flen) || fetch_from_file(name, &file, &flen);
    for (int d = 0; !got && d < g_nsearch; d++) {
        char path[1024];
        snprintf(path, sizeof(path), "%s/%s", g_search[d], base_name(name));
        got = fetch_from_file(path, &file, &flen);
    }
    if (!got) {
        tl_log_line("ld: %s is not in the APK and is not a system library", name);
        return NULL;
    }
    { char e[160] = ""; if (!tl_xmem_open(768u << 20, e, sizeof(e))) { tl_log_line("ld: %s", e); free(file); return NULL; } }
    {   /* adrp, which the loader uses to retarget code at the writable view, reaches +-4 GiB. */
        ptrdiff_t d = tl_xmem_delta();
        if (d > ((ptrdiff_t)3 << 30) || d < -((ptrdiff_t)3 << 30)) { tl_log_line("ld: the writable and executable views are %td MiB apart: too far for adrp", d >> 20); free(file); return NULL; }
    }
    if (!ensure_tcb()) { free(file); return NULL; }
    L = map_library(name, file, flen);
    free(file);
    if (!L) return NULL;

    /* Dependencies first, so everything this library binds against exists. */
    for (int i = 0; i < L->nneeded; i++) {
        const char *dn = (const char *)at(L, L->strtab) + L->needed[i];
        if (find_loaded(dn) || tl_bionic_is_system_lib(dn)) continue;
        if (!load_locked(dn, depth + 1)) tl_log_line("ld: %s: needed library %s could not be loaded", name, dn);
    }
    build_scope(L);
    if (!relocate(L)) return NULL;
    return L;
}

tl_lib *tl_ld_load(const char *name)
{
    pthread_mutex_lock(&g_big);
    tl_lib *L = load_locked(name, 0);
    pthread_mutex_unlock(&g_big);
    return L;
}

static bool init_locked(tl_lib *L)
{
    if (!L || L->state >= 3) return true;
    if (L->state < 2) return false;
    L->state = 3;
    build_scope(L);
    /* Needed libraries initialise first, in the order the linker would run them: the
     * deepest dependency first. The BFS list is shallowest-first, so walk it backwards. */
    for (int i = L->ndeps - 1; i >= 0; i--) init_locked(L->deps[i]);

    char *fallback_argv[] = { (char *)"app_process64", NULL };
    char *fallback_envp[] = { NULL };
    char **argv = G.argv ? G.argv : fallback_argv, **envp = G.envp ? G.envp : fallback_envp;
    int argc = 1;
    typedef void (*init_fn)(int, char **, char **);
    /* BoringSSL's FIPS self-check hashes its own .text, which the loader has rewritten (svc,
     * x18, tpidr sites), and aborts on the mismatch. Skip it: the code itself is unchanged. */
    const elf_sym *fips = lib_find(L, "BORINGSSL_integrity_test");
    if (fips && fips->st_shndx) {
        uint8_t *x = (uint8_t *)sym_value(L, fips);
        if (x >= L->rx) {
            uint32_t ret = 0xd65f03c0u;
            memcpy(L->rw + (x - L->rx), &ret, 4);
            sys_icache_invalidate(x, 4);
            tl_log_line("ld: %s: skipped BORINGSSL_integrity_test", L->name);
        }
    }
    if (L->init) ((init_fn)(L->rx + (L->init - L->base_vaddr)))(argc, argv, envp);
    if (L->init_array) {
        const uint64_t *fns = at(L, L->init_array);
        size_t n = L->init_arraysz / 8;
        for (size_t i = 0; i < n; i++) {
            uint64_t f = fns[i];
            if (f && f != ~0ull) ((init_fn)(uintptr_t)f)(argc, argv, envp);
        }
    }
    L->state = 4;
    if (G.verbosity >= 1) tl_log_line("ld: %-36s initialised (%s%zu constructors)", L->name, L->init ? "DT_INIT + " : "", (size_t)(L->init_arraysz / 8));
    return true;
}

bool tl_ld_init(tl_lib *lib)
{
    pthread_mutex_lock(&g_big);
    bool ok = init_locked(lib);
    pthread_mutex_unlock(&g_big);
    return ok;
}
