/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef HUSK_TL_MEMORY_H
#define HUSK_TL_MEMORY_H
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
typedef struct tl_memory_info {
    uint64_t total, available, resident, peak_resident, footprint, virtual_bytes;
    unsigned threads;
} tl_memory_info;
void tl_host_memory_info(tl_memory_info *info);
void tl_host_log_memory_breakdown(const char *tag);

static inline void tl_memory_proc_meminfo(char *out, size_t size, const tl_memory_info *info)
{
    uint64_t available = info->available < info->total ? info->available : info->total;
    snprintf(out, size, "MemTotal: %llu kB\nMemFree: %llu kB\nMemAvailable: %llu kB\n"
             "Buffers: 0 kB\nCached: 0 kB\nSwapTotal: 0 kB\nSwapFree: 0 kB\n",
             (unsigned long long)(info->total / 1024), (unsigned long long)(available / 1024),
             (unsigned long long)(available / 1024));
}
static inline void tl_memory_proc_status(char *out, size_t size, const tl_memory_info *info, int pid)
{
    snprintf(out, size, "Name:\tapp_process64\nState:\tR (running)\nTgid:\t%d\nPid:\t%d\nPPid:\t1\n"
             "Uid:\t10001\t10001\t10001\t10001\nThreads:\t%u\nVmRSS:\t%llu kB\nVmHWM:\t%llu kB\nVmSize:\t%llu kB\n",
             pid, pid, info->threads, (unsigned long long)(info->resident / 1024),
             (unsigned long long)(info->peak_resident / 1024), (unsigned long long)(info->virtual_bytes / 1024));
}
#endif
