/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "husk-tl-frame-perf.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    tl_frame_perf_metric m = {0};
    for (unsigned i = 0; i < 95; i++) tl_frame_perf_metric_add(&m, UINT64_C(10000000));
    for (unsigned i = 0; i < 5; i++) tl_frame_perf_metric_add(&m, UINT64_C(30000000));
    assert(m.count == 100 && m.over20 == 5 && m.max_ns == UINT64_C(30000000));
    assert(tl_frame_perf_mean_ms(&m) == 11.0 && tl_frame_perf_p95_bound_ms(&m) == 10.0);
    memset(&m, 0, sizeof(m));
    tl_frame_perf_metric_add(&m, UINT64_C(20000000));
    tl_frame_perf_metric_add(&m, UINT64_C(20000001));
    assert(m.over20 == 1 && tl_frame_perf_p95_bound_ms(&m) == 21.0);
    memset(&m, 0, sizeof(m));
    tl_frame_perf_metric_add(&m, UINT64_C(541123456));
    assert(tl_frame_perf_p95_bound_ms(&m) > 541.12 && tl_frame_perf_p95_bound_ms(&m) < 541.13);

    tl_frame_perf_window w = {0};
    uint64_t start = UINT64_C(1000000000);
    assert(tl_frame_perf_note(&w, start, 1000, 2000, 3000));
    assert(!tl_frame_perf_note(&w, start, 999, 999, 999));
    assert(!tl_frame_perf_note(&w, start - 1, 999, 999, 999));
    assert(w.frames == 1 && !w.interval.count && !tl_frame_perf_due(&w));
    for (unsigned i = 1; i <= 1800; i++)
        assert(tl_frame_perf_note(&w, start + UINT64_C(30000000000) * i / 1800, 1000, 2000, 3000));
    assert(w.frames == 1801 && w.interval.count == 1800 && tl_frame_perf_fps(&w) == 60.0 && tl_frame_perf_due(&w));
    assert(w.interval.over20 == 0);
    char text[1024];
    tl_frame_perf_format(text, sizeof(text), "producer", "copy-publish", "pace-wait", "handoff", &w);
    assert(strstr(text, "fps=60.00") && strstr(text, "p95<=17.00") && strstr(text, "copy-publish_ms"));
    tl_frame_perf_roll(&w);
    assert(w.frames == 0 && w.work.count == 0 && !tl_frame_perf_due(&w));
    assert(tl_frame_perf_note(&w, start + UINT64_C(30500000000), 1, 2, 3));
    assert(w.interval.max_ns == UINT64_C(500000000) && w.interval.over20 == 1); /* no lost rollover stall */
    tl_frame_perf_reset(&w); /* stop/pause: exclude idle and previous-session gaps */
    assert(tl_frame_perf_note(&w, UINT64_C(9000000000000), 1, 2, 3));
    assert(!w.interval.count && w.frames == 1 && !tl_frame_perf_due(&w));
    tl_frame_perf_epoch epoch = {0};
    assert(tl_frame_perf_epoch_token(&epoch) == 0);
    tl_frame_perf_epoch_activate(&epoch, true);
    uint64_t token = tl_frame_perf_epoch_token(&epoch);
    assert(token && tl_frame_perf_epoch_note(&epoch, token, start, 1, 2, 3));
    tl_frame_perf_epoch_activate(&epoch, false);
    assert(!tl_frame_perf_epoch_note(&epoch, token, start + 1, 1, 2, 3));
    tl_frame_perf_epoch_activate(&epoch, true);
    assert(tl_frame_perf_epoch_token(&epoch) != token);
    assert(!tl_frame_perf_epoch_note(&epoch, token, start + 2, 1, 2, 3));
    assert(tl_frame_perf_epoch_note(&epoch, tl_frame_perf_epoch_token(&epoch), start + 3, 1, 2, 3));
    assert(epoch.window.frames == 1 && !epoch.window.interval.count); /* no old session/idle gap */
    puts("PASS: bounded frame telemetry separates actual intervals and stage costs, preserves strict20ms counts, conservative p95 bounds, rollover stalls and reset/duplicate semantics without allocations");
    return 0;
}
