/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef HUSK_TL_JNI_ALLOCATION_H
#define HUSK_TL_JNI_ALLOCATION_H
#include "husk-tl-jni.h"
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

/* Requested storage owned by the JNI object constructors, not a Java heap
 * census. In particular strings, field slots, native resources and allocator
 * overhead are absent. A deferred unref is a call, not a dead-object count. */
typedef struct {
    atomic_uint_fast64_t created, freed, array_allocated, array_freed, deferred;
    atomic_bool gc_lock;
    jobj *gc_head, *gc_tail;
} tl_jni_allocation_ledger;

static inline void tl_jni_gc_lock(tl_jni_allocation_ledger *s)
{
    while (atomic_exchange_explicit(&s->gc_lock, true, memory_order_acquire)) {}
}
static inline void tl_jni_gc_unlock(tl_jni_allocation_ledger *s)
{
    atomic_store_explicit(&s->gc_lock, false, memory_order_release);
}
static inline void tl_jni_allocation_track(tl_jni_allocation_ledger *s, jobj *o)
{
    if (!o) return;
    tl_jni_gc_lock(s);
    o->gc_prev = s->gc_tail;
    o->gc_next = NULL;
    if (s->gc_tail) s->gc_tail->gc_next = o;
    else s->gc_head = o;
    s->gc_tail = o;
    tl_jni_gc_unlock(s);
}
static inline void tl_jni_allocation_untrack(tl_jni_allocation_ledger *s, jobj *o)
{
    if (!o) return;
    tl_jni_gc_lock(s);
    if (o->gc_prev || o->gc_next || s->gc_head == o) {
        if (o->gc_prev) o->gc_prev->gc_next = o->gc_next;
        else s->gc_head = o->gc_next;
        if (o->gc_next) o->gc_next->gc_prev = o->gc_prev;
        else s->gc_tail = o->gc_prev;
        o->gc_prev = o->gc_next = NULL;
    }
    tl_jni_gc_unlock(s);
}
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
