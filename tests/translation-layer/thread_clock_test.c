/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "husk-tl-thread-clock.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    bool per_thread = false;
    int tid = 0, which = 0;

    assert(!tl_decode_cpu_clock_id(0, &per_thread, &tid, &which));
    assert(!tl_decode_cpu_clock_id(3, &per_thread, &tid, &which));

    int clk = tl_thread_cpu_clock_id(1000);
    assert(clk < 0);
    assert(tl_decode_cpu_clock_id(clk, &per_thread, &tid, &which));
    assert(per_thread);
    assert(tid == 1000);
    assert(which == 2);

    int max28 = tl_thread_cpu_clock_id(0x0fffffff);
    assert(max28 < 0);
    assert(tl_decode_cpu_clock_id(max28, &per_thread, &tid, &which));
    assert(per_thread && tid == 0x0fffffff && which == 2);

    assert(tl_parse_task_schedstat_tid("/proc/self/task/12345/schedstat", &tid) && tid == 12345);
    assert(tl_parse_task_schedstat_tid("/proc/42/task/99/schedstat", &tid) && tid == 99);
    assert(!tl_parse_task_schedstat_tid("/proc/self/status", &tid));
    assert(!tl_parse_task_schedstat_tid("/proc/self/task/0/schedstat", &tid));
    assert(!tl_parse_task_schedstat_tid("/proc/self/task/12/stat", &tid));
    assert(!tl_parse_task_schedstat_tid("/proc/self/task/12abc/schedstat", &tid));

    char buf[64];
    tl_format_schedstat(buf, sizeof(buf), 123456000ULL, 0ULL, 1ULL);
    assert(strcmp(buf, "123456000 0 1\n") == 0);

    puts("PASS: Linux per-thread CPU clock ids and task schedstat formatting");
    return 0;
}
