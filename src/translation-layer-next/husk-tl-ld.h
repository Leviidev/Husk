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
#define TL_LD_MAX_APKS 16
bool tl_ld_add_apk(const char *path);
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

/* A library already loaded, by file name or soname. */
tl_lib *tl_ld_find_lib(const char *name);

/* A symbol exported by `lib`, or (lib == NULL) by any loaded library, in load order. */
void *tl_ld_sym(tl_lib *lib, const char *name);

/* The library an address belongs to (either view), and the nearest exported symbol. */
tl_lib *tl_ld_lib_of(const void *addr);
void *tl_ld_lib_base(const tl_lib *lib);
const char *tl_ld_lib_name(const tl_lib *lib);
const char *tl_ld_symbol_at(const void *addr, const char **lib_name, const void **sym_addr);

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

#ifdef __cplusplus
}
#endif

#endif /* HUSK_TL_LD_H */
