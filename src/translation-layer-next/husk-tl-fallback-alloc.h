/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * A last-resort allocator for guest malloc() when the host malloc refuses.
 *
 * Blocks are whole anonymous mappings recorded in a fixed registry. Ownership
 * is decided ONLY by looking the address up in that registry: the pointer is
 * never dereferenced, and no header is read from the bytes before it. A guest
 * free() receives pointers of every origin (host malloc, loaded images, memory
 * the guest got elsewhere); probing memory in front of an arbitrary pointer
 * faults when that pointer starts a mapping.
 */
#ifndef HUSK_TL_FALLBACK_ALLOC_H
#define HUSK_TL_FALLBACK_ALLOC_H

#include <stdbool.h>
#include <stddef.h>

/* Zero-filled, page-aligned block of at least n bytes (n > 0). NULL with errno
 * set (ENOMEM when the registry is full, or mmap's own error). */
void *tl_fb_alloc(size_t n);

/* True when p is exactly the start of a live block returned by tl_fb_alloc. */
bool tl_fb_owns(const void *p);

/* The size originally requested for a live block, else 0. */
size_t tl_fb_size(const void *p);

/* Unmap a live block. False (and nothing touched) when p is not one. Safe to
 * call for any pointer, including NULL, unmapped or foreign addresses. */
bool tl_fb_release(void *p);

/* Blocks currently live (diagnostics and tests). */
size_t tl_fb_live(void);

#endif
