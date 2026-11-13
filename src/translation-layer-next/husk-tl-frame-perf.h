/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef HUSK_TL_FRAME_PERF_H
#define HUSK_TL_FRAME_PERF_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* Bounded wall-clock diagnostics, not GPU timers. Quantiles are conservative
 * upper bounds in 1 ms bins; overflow uses the observed maximum. No samples,
 * allocations, clocks or locks are hidden in this pure helper. */
#define TL_FRAME_PERF_BINS 256u
#define TL_FRAME_PERF_REPORT_NS UINT64_C(30000000000)
typedef struct {
    uint64_t count, total_ns, max_ns, over20;
    uint64_t bins[TL_FRAME_PERF_BINS];
} tl_frame_perf_metric;
typedef struct {
    uint64_t start_ns, last_ns, frames;
    bool started;
    tl_frame_perf_metric interval, work, wait, handoff;
} tl_frame_perf_window;

static inline void tl_frame_perf_metric_add(tl_frame_perf_metric *m, uint64_t ns)
{
    uint64_t bin = ns ? (ns - 1) / UINT64_C(1000000) : 0;
    if (bin >= TL_FRAME_PERF_BINS) bin = TL_FRAME_PERF_BINS - 1;
    m->bins[bin]++; m->count++; m->total_ns += ns;
    if (ns > m->max_ns) m->max_ns = ns;
    if (ns > UINT64_C(20000000)) m->over20++;
}
static inline double tl_frame_perf_mean_ms(const tl_frame_perf_metric *m)
{ return m->count ? (double)m->total_ns / (double)m->count / 1e6 : 0; }
static inline double tl_frame_perf_p95_bound_ms(const tl_frame_perf_metric *m)
{
    if (!m->count) return 0;
    uint64_t rank = (m->count / 100) * 95 + ((m->count % 100) * 95 + 99) / 100, seen = 0;
    for (unsigned i = 0; i < TL_FRAME_PERF_BINS; i++) {
        seen += m->bins[i];
        if (seen >= rank) return i == TL_FRAME_PERF_BINS - 1 ? (double)m->max_ns / 1e6 : i + 1.0;
    }
    return (double)m->max_ns / 1e6;
}
static inline void tl_frame_perf_reset(tl_frame_perf_window *w) { memset(w, 0, sizeof(*w)); }
static inline bool tl_frame_perf_note(tl_frame_perf_window *w, uint64_t now_ns,
    uint64_t work_ns, uint64_t wait_ns, uint64_t handoff_ns)
{
    if (w->started && now_ns <= w->last_ns) return false; /* duplicate/out-of-order publication */
    if (!w->started) { w->start_ns = now_ns; w->started = true; }
    else tl_frame_perf_metric_add(&w->interval, now_ns - w->last_ns);
    w->last_ns = now_ns; w->frames++;
    tl_frame_perf_metric_add(&w->work, work_ns);
    tl_frame_perf_metric_add(&w->wait, wait_ns);
    tl_frame_perf_metric_add(&w->handoff, handoff_ns);
    return true;
}
static inline bool tl_frame_perf_due(const tl_frame_perf_window *w)
{ return w->started && w->frames && w->last_ns - w->start_ns >= TL_FRAME_PERF_REPORT_NS; }
static inline double tl_frame_perf_fps(const tl_frame_perf_window *w)
{
    uint64_t ns = w->last_ns - w->start_ns;
    return ns ? (double)w->interval.count * 1e9 / (double)ns : 0;
}
static inline void tl_frame_perf_roll(tl_frame_perf_window *w)
{
    uint64_t last = w->last_ns; bool started = w->started;
    tl_frame_perf_reset(w);
    w->last_ns = w->start_ns = last; w->started = started;
}
typedef struct {
    tl_frame_perf_window window;
    uint64_t generation;
    bool active;
} tl_frame_perf_epoch;
static inline void tl_frame_perf_epoch_activate(tl_frame_perf_epoch *e, bool active)
{
    tl_frame_perf_reset(&e->window);
    if (++e->generation == 0) e->generation = 1;
    e->active = active;
}
static inline uint64_t tl_frame_perf_epoch_token(const tl_frame_perf_epoch *e)
{ return e->active ? e->generation : 0; }
static inline bool tl_frame_perf_epoch_note(tl_frame_perf_epoch *e, uint64_t token,
    uint64_t now_ns, uint64_t work_ns, uint64_t wait_ns, uint64_t handoff_ns)
{
    return token && e->active && token == e->generation
        && tl_frame_perf_note(&e->window, now_ns, work_ns, wait_ns, handoff_ns);
}
static inline void tl_frame_perf_format(char *out, size_t size, const char *role,
    const char *work, const char *wait, const char *handoff, const tl_frame_perf_window *w)
{
    const tl_frame_perf_metric *i = &w->interval, *a = &w->work, *b = &w->wait, *c = &w->handoff;
    snprintf(out, size,
        "frame-perf[%s]: frames=%llu elapsed=%.3fs fps=%.2f; "
        "interval_ms(mean=%.2f p95<=%.2f max=%.2f over20=%llu); "
        "%s_ms(mean=%.2f p95<=%.2f max=%.2f over20=%llu); "
        "%s_ms(mean=%.2f p95<=%.2f max=%.2f over20=%llu); "
        "%s_ms(mean=%.2f p95<=%.2f max=%.2f over20=%llu)",
        role, (unsigned long long)w->frames, (double)(w->last_ns - w->start_ns) / 1e9, tl_frame_perf_fps(w),
        tl_frame_perf_mean_ms(i), tl_frame_perf_p95_bound_ms(i), (double)i->max_ns / 1e6, (unsigned long long)i->over20,
        work, tl_frame_perf_mean_ms(a), tl_frame_perf_p95_bound_ms(a), (double)a->max_ns / 1e6, (unsigned long long)a->over20,
        wait, tl_frame_perf_mean_ms(b), tl_frame_perf_p95_bound_ms(b), (double)b->max_ns / 1e6, (unsigned long long)b->over20,
        handoff, tl_frame_perf_mean_ms(c), tl_frame_perf_p95_bound_ms(c), (double)c->max_ns / 1e6, (unsigned long long)c->over20);
}
#endif
