/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * The guest dynamic linker.
 *
 * Takes an Android app's arm64 shared objects out of its APK and makes them run
 * inside this process: it maps them into the executable region (husk-tl-xmem),
 * relocates them, binds their imports to the bionic shims (husk-tl-bionic) or to
 * each other, and runs their constructors.
 *
 * It follows Android's linker where that decides whether a library works:
 *
 *   - an import is looked up first in the importing library's own scope -- itself,
 *     then its DT_NEEDED closure in breadth-first order -- and only then in the
 *     system. Unity's libraries each carry a private C++ runtime, and binding one
 *     library's exceptions to another's would corrupt both;
 *   - a library's dependencies are mapped, relocated and initialised before it;
 *   - initialisers receive (argc, argv, envp), as bionic passes them.
 *
 * It differs where the platform forces it. Code and data are two views of the same
 * pages (see husk-tl-xmem.h), so an address handed out for a data object is its
 * writable view and one for code is its executable view; the loader rewrites
 * code that would otherwise reach data through the wrong one.
 */
#ifndef HUSK_TL_LD_H
#define HUSK_TL_LD_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct tl_lib tl_lib;

/* Where libraries come from: the APKs' lib/arm64-v8a/ directories. */
#define TL_LD_MAX_APKS 48   /* a big game ships a dozen asset-pack splits besides its config ones (Disney Speedstorm: 16) */
bool tl_ld_add_apk(const char *path);
/* Forget the current app's APKs and libraries so another app can load. Runtime (system) libraries stay. */
void tl_ld_reset_session(void);
/* A split APK of the app about to start; added right after its base APK, whichever engine adds that. */
void tl_ld_queue_split(const char *path);
const char *tl_ld_queued_split(int index);              /* NULL past the last */

/* The APKs added, in order, for asset access; NULL past the last. */
struct tl_zip;
const struct tl_zip *tl_ld_apk_at(int index);

/* Bind every later import of `name` to `fn` instead of the library that defines it (the original is still reachable with tl_ld_sym). Call before the libraries load. */
void tl_ld_interpose(const char *name, void *fn);

/* Whether one of the APKs carries this arm64 library (an engine is told by the libraries it ships). */
bool tl_ld_has_lib(const char *name);
bool tl_ld_is_arm32(const tl_lib *lib);
int tl_ld_apk_libs(void (*cb)(const char *name, uint64_t size, void *user), void *user);

/*
 * Load a library by file name or soname (already-loaded ones are returned as
 * they are), with everything it needs. Returns NULL and logs why on failure.
 * `name` may also be a system library ("libc.so"): that returns NULL without
 * logging an error, because the system is not a library this linker owns.
 */
tl_lib *tl_ld_load(const char *name);
/* Another folder to look for libraries in, by file name, after the APKs. */
void tl_ld_add_search_dir(const char *dir);

/* Run constructors for a library and anything it needs that has not run. */
bool tl_ld_init(tl_lib *lib);
/* After that library's constructors: Unity installs IL2CPP's terminate handler here, because
 * NativeLoader.load often returns before libil2cpp.so exists. */
void tl_ld_on_init(void (*fn)(tl_lib *lib));

/* A library already loaded, by file name or soname. */
tl_lib *tl_ld_find_lib(const char *name);

/* A symbol exported by `lib`, or (lib == NULL) by any loaded library, in load order. */
void *tl_ld_sym(tl_lib *lib, const char *name);
void *tl_ld_sym_deep(tl_lib *lib, const char *name, tl_lib **found_in);   /* the library, then its dependencies, as bionic's dlsym(handle) */

/* The library an address belongs to (either view), and the nearest exported symbol. */
tl_lib *tl_ld_lib_of(const void *addr);
void *tl_ld_lib_base(const tl_lib *lib);
const char *tl_ld_lib_name(const tl_lib *lib);
const char *tl_ld_symbol_at(const void *addr, const char **lib_name, const void **sym_addr);

/* An imported AOT dylib maps the ELF image r-x (AMFI execSeg). Stores that land there belong on the writable copy of that
 * page, if the loader made one. NULL if `addr` is not in such a page. Safe to call from a fault handler after libraries
 * have loaded. */
void *tl_ld_aot_writable(const void *addr, size_t bytes);
/* True when `addr` is inside an imported AOT image (including its stub pages). mprotect there cannot change permissions. */
bool tl_ld_aot_contains(const void *addr);
/* True when any byte of [addr, addr+bytes) is a loaded library (AOT dylib or JIT image, either view).
 * mmap(MAP_FIXED)/munmap/madvise must not treat that as anonymous heap. */
bool tl_ld_covers(const void *addr, size_t bytes);
/* If `addr` is a fetch from the writable copy of a function (relative C++ vtables
 * add a slot offset to a vtable that lives in the copy, which lands in the copy of
 * .text, which is not executable), the executable view of the same offset; otherwise NULL. */
void *tl_ld_aot_exec(const void *addr);

/*
 * Walk loaded libraries the way dl_iterate_phdr does. The callback gets the load
 * bias (the executable view's address of vaddr 0), the library's name, its program
 * headers and their count; a non-zero return stops the walk.
 */
typedef int (*tl_ld_phdr_cb)(uintptr_t bias, const char *name, const void *phdr,
                             unsigned phnum, void *user);
int tl_ld_iterate(tl_ld_phdr_cb cb, void *user);

/*
 * Diagnostic probe: before the instruction at `vaddr` runs, call cb(regs) with the integer registers
 * x0..x28 as they stand (regs[i] is xi); then the instruction runs as usual. The instruction must not be
 * pc-relative. For finding out what guest code is doing when no debugger can follow it.
 */
bool tl_ld_probe(tl_lib *lib, uint64_t vaddr, void (*cb)(uint64_t *regs));

/*
 * The calling thread's virtual x18. Guest code never holds a live value in the real
 * register (the kernel zeroes it); its x18 lives in a TSD slot instead, and anything
 * that runs guest code on a signal frame saves it before and restores it after.
 */
uint64_t tl_vx18_get(void);
void tl_vx18_set(uint64_t v);

/* Imports that nothing provided, bound to a stub that logs on call. */
size_t tl_ld_unresolved_count(void);

/* How loud loading is: 0 quiet, 1 per-library summary, 2 every unresolved import. */
void tl_ld_set_verbosity(int v);

/* The program's own argv/envp for initialisers. Optional. */
void tl_ld_set_environment(char **argv, char **envp);

/*
 * Ahead of time (Husk 3.2): an arm64 library mapped and patched the way the loader does it, for a Mach-O dylib that runs with no
 * executable memory made at run time. `text` is the image with every code rewrite applied, then its data-slot page and its
 * thread-block page: `size` bytes, laid out so the dylib can map every page at the address the code computes for it (executable
 * pages in a __TEXT segment, data pages in a __DATA one, no page both). A library built for 4 KiB pages has pages that hold code
 * and writable data at once; its data pages then also get a writable copy at `size` + their page index, the copies keep the
 * image's page numbering (so an address that crosses a page boundary stays in the copies), and the rewritten code reads its data
 * there (see mixed_page, husk-tl-ld.c). `text` points into a scratch region that tl_ld_aot_free releases. husk-tl-aot.h writes
 * the dylib.
 */
typedef struct tl_ld_aot_image {
    const uint8_t *text;
    size_t size;
    uint64_t base_vaddr;               /* the vaddr at the start of the image */
    size_t npages, nstub;              /* 16 KiB pages of image and of stubs (a data-slot page and a thread-block page follow the stubs) */
    uint8_t *page_class;               /* one per page of `size`, bit flags: 1 executable, 2 a writable copy of its data at `size` + page * 16384; neither: a writable data page; freed with the image */
    size_t page_count;
    size_t n_mixed;                    /* page_class bit 2: the pages copied (0 in a library built for 16 KiB pages) */
    size_t n_x18, n_x18_failed, n_svc, n_svc_far, n_adrp, n_adr, n_adr_failed, n_tpidr;
    void *region; size_t region_size;
    char err[160];
} tl_ld_aot_image;
bool tl_ld_aot_build(const char *name, const uint8_t *file, size_t flen, tl_ld_aot_image *out);

/* What a dylib made from a library records about it (section __TEXT,__husk_manifest), so the loader only imports a dylib that was
 * made from this exact file by a compatible loader. */
#define TL_AOT_MAGIC   "HUSKAOT1"
#define TL_AOT_VERSION 9u          /* bump whenever the rewrites or the dylib's layout change: an old dylib must not be imported */
typedef struct tl_aot_manifest {
    char magic[8];
    uint32_t version, page;            /* TL_AOT_VERSION; 16384 */
    uint64_t so_hash, so_size;         /* tl_aot_hash of the .so, and its size */
    uint64_t base_vaddr, npages, nstub, size;   /* as tl_ld_aot_image */
    uint64_t n_mixed;                  /* pages with a writable copy at `size` + page * 16384 (0 in a library built for 16 KiB) */
} tl_aot_manifest;
uint64_t tl_aot_hash(const uint8_t *p, size_t n);

/* The name a dylib made from this library has: aot-v<version>-<hash of the .so>-<library name>.dylib. The version is in it so a
 * dylib made by another version of the loader is converted again rather than found, rejected and left in place. */
void tl_aot_file_name(char *out, size_t n, uint64_t hash, const char *lib_name);

/* Where the loader looks for <library>.dylib made ahead of time (NULL: nowhere; several folders separated by ':', searched in
 * order). A matching one is used instead of mapping the library into executable memory. */
void tl_ld_set_aot_dir(const char *dir);
void tl_ld_aot_free(tl_ld_aot_image *img);

#ifdef __cplusplus
}
#endif

#endif /* HUSK_TL_LD_H */
