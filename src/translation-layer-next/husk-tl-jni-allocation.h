/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef HUSK_TL_JNI_ALLOCATION_H
#define HUSK_TL_JNI_ALLOCATION_H
#include <stdatomic.h>
#include <stdint.h>
#include <stddef.h>

/* Requested storage owned by the JNI object constructors, not a Java heap
 * census. In particular strings, field slots, native resources and allocator
 * overhead are absent. A deferred unref is a call, not a dead-object count. */
typedef struct {
    atomic_uint_fast64_t created, freed, array_allocated, array_freed, deferred;
} tl_jni_allocation_ledger;
typedef struct {
    uint64_t created, unfreed, headers, array_payload, deferred;
} tl_jni_allocation_sample;

static inline void tl_jni_allocation_object(tl_jni_allocation_ledger *s)
{ atomic_fetch_add_explicit(&s->created, 1, memory_order_relaxed); }
static inline void tl_jni_allocation_array(tl_jni_allocation_ledger *s, size_t bytes)
{ atomic_fetch_add_explicit(&s->array_allocated, bytes, memory_order_relaxed); }
static inline void tl_jni_allocation_release(tl_jni_allocation_ledger *s, size_t array_bytes)
{
    atomic_fetch_add_explicit(&s->array_freed, array_bytes, memory_order_relaxed);
    atomic_fetch_add_explicit(&s->freed, 1, memory_order_relaxed);
}
static inline void tl_jni_allocation_defer(tl_jni_allocation_ledger *s)
{ atomic_fetch_add_explicit(&s->deferred, 1, memory_order_relaxed); }
static inline tl_jni_allocation_sample tl_jni_allocation_snapshot(const tl_jni_allocation_ledger *s, size_t object_size)
{
    /* These are independent atomic observations while allocation continues.
     * Clamp cross-counter skew; do not use a sample to decide object liveness. */
    uint64_t freed = atomic_load_explicit(&s->freed, memory_order_relaxed);
    uint64_t created = atomic_load_explicit(&s->created, memory_order_relaxed);
    uint64_t array_freed = atomic_load_explicit(&s->array_freed, memory_order_relaxed);
    uint64_t array_allocated = atomic_load_explicit(&s->array_allocated, memory_order_relaxed);
    uint64_t unfreed = created >= freed ? created - freed : 0;
    return (tl_jni_allocation_sample){ created, unfreed, unfreed * object_size,
        array_allocated >= array_freed ? array_allocated - array_freed : 0,
        atomic_load_explicit(&s->deferred, memory_order_relaxed) };
}
#endif
