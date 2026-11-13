/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Guest thread ID, per-thread Linux POSIX CPU clock ((~tid << 3) | 6),
 * and /proc/self/task/<tid>/schedstat helpers.
 */
#ifndef HUSK_TL_THREAD_CLOCK_H
#define HUSK_TL_THREAD_CLOCK_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Linux kernel include/linux/posix-timers.h:
 * per-thread SCHED clock id is ((~tid << 3) | 6). */
static inline int tl_thread_cpu_clock_id(int tid)
{
    return (int)((~(uint32_t)tid << 3) | 6u);
}

static inline bool tl_decode_cpu_clock_id(int clock_id, bool *per_thread, int *tid_or_pid, int *clock_which)
{
    if (clock_id >= 0) return false;
    uint32_t u = (uint32_t)clock_id;
    if (per_thread) *per_thread = (u & 4u) != 0;
    if (clock_which) *clock_which = (int)(u & 3u);
    if (tid_or_pid) *tid_or_pid = (int)((~u) >> 3);
    return true;
}

static inline bool tl_parse_task_schedstat_tid(const char *path, int *tid_out)
{
    if (!path || strncmp(path, "/proc/", 6) != 0) return false;
    const char *task = strstr(path + 5, "/task/");
    if (!task) return false;
    const char *p = task + 6;
    if (*p < '0' || *p > '9') return false;
    char *end = NULL;
    unsigned long tid = strtoul(p, &end, 10);
    if (tid == 0 || tid > 0x1fffffffUL || !end || strcmp(end, "/schedstat") != 0) return false;
    if (tid_out) *tid_out = (int)tid;
    return true;
}

static inline void tl_format_schedstat(char *buf, size_t n, uint64_t exec_ns, uint64_t delay_ns, uint64_t pcount)
{
    if (!buf || n == 0) return;
    snprintf(buf, n, "%llu %llu %llu\n",
             (unsigned long long)exec_ns,
             (unsigned long long)delay_ns,
             (unsigned long long)pcount);
}

#ifndef _WIN32
#include <pthread.h>

int tl_thread_clock_tid(void);
int tl_thread_clock_tid_of(pthread_t t);
bool tl_thread_clock_gettime(int clock_id, struct timespec *ts);
bool tl_thread_clock_getres(int clock_id, struct timespec *ts);
const char *tl_thread_schedstat(const char *path, char *buf, size_t n);
#endif

#endif
