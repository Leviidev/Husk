/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Executable memory for guest code.
 *
 * Every byte of arm64 code an Android app brings -- its libraries, and the small
 * stubs the loader writes -- has to live in memory the CPU will execute, and on
 * the phone there is exactly one source of that: the region StikDebug grants once,
 * at launch, as two views of the same pages. Code is written through the
 * writable view and run through the executable one.
 *
 * On a Mac there is no such restriction, but the loader is written against the
 * same two-view shape so the code that relocates, patches and addresses guest
 * images is the code that runs on the phone. The Mac gets its two views from
 * mach_vm_remap of ordinary memory.
 *
 * Allocation is a bump pointer: nothing is ever returned. The phone's region is
 * one-shot, so a freed hole could not be refilled by asking for another.
 */
#ifndef HUSK_TL_XMEM_H
#define HUSK_TL_XMEM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TL_XMEM_PAGE 16384u

/*
 * Find the region: the phone's StikDebug region, or on a Mac one made here of
 * `host_bytes`. Safe to call repeatedly; the region is found once.
 * Returns false when no executable memory can be had, and says why in `err`.
 */
bool tl_xmem_open(size_t host_bytes, char *err, size_t errlen);

/*
 * Take `bytes` (rounded up to whole pages) from the region. `*rx` is where the
 * code will run and `*rw` is the same memory for writing; rw - rx is the same
 * constant for every allocation. Returns false when the region is exhausted.
 */
bool tl_xmem_alloc(size_t bytes, uint8_t **rx, uint8_t **rw);

/* rw - rx for the region, valid after tl_xmem_open succeeds. */
ptrdiff_t tl_xmem_delta(void);

/* Whether an address is inside the region, in either view. */
bool tl_xmem_contains(const void *p);

/* Whether an address is in the executable view specifically. */
bool tl_xmem_is_rx(const void *p);

/* Make bytes written through the RW view visible to instruction fetch. */
void tl_xmem_flush(const void *rx, size_t bytes);

size_t tl_xmem_used(void);
size_t tl_xmem_size(void);

/* Set before claiming the process executable region. */
void husk_jit_budget_set_target_bytes(size_t bytes);
/* Read-only query; never requests a debugger or memory mapping. */
size_t husk_jit_budget_region_bytes(void);

/* Whether p is within dist bytes of the executable region (a JIT asking for code memory near its own code). */
bool tl_xmem_near(const void *p, size_t dist);

#ifdef __cplusplus
}
#endif

#endif /* HUSK_TL_XMEM_H */
