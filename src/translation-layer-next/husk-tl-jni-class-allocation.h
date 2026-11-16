/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef HUSK_TL_JNI_CLASS_ALLOCATION_H
#define HUSK_TL_JNI_CLASS_ALLOCATION_H
#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

/* Creation traffic, not retained/live bytes. Recording needs only the already
 * known class and relaxed atomics; the previous samples belong to the caller's
 * snapshot lock. Payload counts only successful array backing allocations. */
typedef struct {
    atomic_uint_fast64_t created, array_bytes_created;
    uint64_t previous_created, previous_array_bytes_created;
} tl_jni_class_allocation;

static inline void tl_jni_class_allocation_init(tl_jni_class_allocation *s)
{
    atomic_init(&s->created, 0);
    atomic_init(&s->array_bytes_created, 0);
    s->previous_created = s->previous_array_bytes_created = 0;
}
static inline void tl_jni_class_allocation_object(tl_jni_class_allocation *s)
{ atomic_fetch_add_explicit(&s->created, 1, memory_order_relaxed); }
static inline void tl_jni_class_allocation_array(tl_jni_class_allocation *s, size_t bytes)
{ atomic_fetch_add_explicit(&s->array_bytes_created, bytes, memory_order_relaxed); }

typedef struct {
    uint64_t created, array_bytes, created_delta, array_bytes_delta;
} tl_jni_class_allocation_sample;

static inline tl_jni_class_allocation_sample tl_jni_class_allocation_snapshot(
    tl_jni_class_allocation *s, bool baseline)
{
    uint64_t created = atomic_load_explicit(&s->created, memory_order_relaxed);
    uint64_t bytes = atomic_load_explicit(&s->array_bytes_created, memory_order_relaxed);
    tl_jni_class_allocation_sample sample = { created, bytes, 0, 0 };
    if (!baseline) {
        sample.created_delta = created >= s->previous_created ? created - s->previous_created : 0;
        sample.array_bytes_delta = bytes >= s->previous_array_bytes_created ? bytes - s->previous_array_bytes_created : 0;
    }
    s->previous_created = created;
    s->previous_array_bytes_created = bytes;
    return sample;
}

enum { TL_JNI_CLASS_ALLOCATION_TOP = 8, TL_JNI_CLASS_ALLOCATION_NAME = 160 };
typedef struct {
    uint64_t declaration;
    char name[TL_JNI_CLASS_ALLOCATION_NAME];
    tl_jni_class_allocation_sample counts;
} tl_jni_class_allocation_row;
typedef struct {
    uint64_t epoch, sequence, last_ns;
} tl_jni_class_allocation_window;
typedef struct {
    uint64_t epoch, sequence, interval_ns, groups;
    bool baseline;
    size_t count;
    tl_jni_class_allocation_sample totals;
    tl_jni_class_allocation_row top[TL_JNI_CLASS_ALLOCATION_TOP];
} tl_jni_class_allocation_report;

/* Reset at the app-class retirement boundary. The next sample establishes new
 * baselines even for boot classes shared by successive app sessions. */
static inline void tl_jni_class_allocation_reset(tl_jni_class_allocation_window *w)
{ w->epoch++; w->last_ns = 0; }
static inline tl_jni_class_allocation_report tl_jni_class_allocation_begin(
    tl_jni_class_allocation_window *w, uint64_t now_ns)
{
    tl_jni_class_allocation_report r = {0};
    r.epoch = w->epoch;
    r.sequence = ++w->sequence;
    r.baseline = !w->last_ns || now_ns <= w->last_ns;
    r.interval_ns = r.baseline ? 0 : now_ns - w->last_ns;
    w->last_ns = now_ns;
    return r;
}
static inline void tl_jni_class_allocation_sum(tl_jni_class_allocation_sample *a,
    tl_jni_class_allocation_sample b)
{
    a->created += b.created; a->array_bytes += b.array_bytes;
    a->created_delta += b.created_delta; a->array_bytes_delta += b.array_bytes_delta;
}
static inline bool tl_jni_class_allocation_above(tl_jni_class_allocation_sample a,
    tl_jni_class_allocation_sample b, bool baseline)
{
    uint64_t ac = baseline ? a.created : a.created_delta;
    uint64_t bc = baseline ? b.created : b.created_delta;
    if (ac != bc) return ac > bc;
    uint64_t ab = baseline ? a.array_bytes : a.array_bytes_delta;
    uint64_t bb = baseline ? b.array_bytes : b.array_bytes_delta;
    return ab > bb;
}
static inline void tl_jni_class_allocation_add(tl_jni_class_allocation_report *r,
    uint64_t declaration, const char *name, tl_jni_class_allocation_sample s)
{
    r->groups++;
    tl_jni_class_allocation_sum(&r->totals, s);
    if (r->baseline ? (!s.created && !s.array_bytes) : (!s.created_delta && !s.array_bytes_delta)) return;
    size_t at = 0;
    while (at < r->count && !tl_jni_class_allocation_above(s, r->top[at].counts, r->baseline)) at++;
    if (at == TL_JNI_CLASS_ALLOCATION_TOP) return;
    size_t end = r->count < TL_JNI_CLASS_ALLOCATION_TOP ? r->count++ : TL_JNI_CLASS_ALLOCATION_TOP - 1;
    while (end > at) { r->top[end] = r->top[end - 1]; end--; }
    tl_jni_class_allocation_row *row = &r->top[at];
    row->declaration = declaration;
    row->counts = s;
    if (!name) name = "<class-unavailable>";
    size_t n = 0;
    while (n + 1 < sizeof(row->name) && name[n]) { row->name[n] = name[n]; n++; }
    row->name[n] = 0;
    if (name[n] && n >= 3) memcpy(row->name + n - 3, "...", 3);
}
static inline tl_jni_class_allocation_sample tl_jni_class_allocation_other(
    const tl_jni_class_allocation_report *r)
{
    tl_jni_class_allocation_sample other = r->totals;
    for (size_t i = 0; i < r->count; i++) {
        other.created -= r->top[i].counts.created;
        other.array_bytes -= r->top[i].counts.array_bytes;
        other.created_delta -= r->top[i].counts.created_delta;
        other.array_bytes_delta -= r->top[i].counts.array_bytes_delta;
    }
    return other;
}
#endif
