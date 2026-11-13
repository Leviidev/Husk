/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "husk-tl-memory.h"
#include <assert.h>
#include <string.h>
int main(void)
{
    char text[1024];
    tl_memory_info info = { .total = 8ull << 30, .available = 2ull << 30,
        .resident = 1500ull << 20, .peak_resident = 1600ull << 20,
        .footprint = 2700ull << 20, .virtual_bytes = 9000ull << 20, .threads = 37 };
    tl_memory_proc_meminfo(text, sizeof(text), &info);
    assert(strstr(text, "MemAvailable: 2097152 kB"));
    info.available = 45ull << 20;
    tl_memory_proc_meminfo(text, sizeof(text), &info);
    assert(strstr(text, "MemAvailable: 46080 kB"));
    assert(!strstr(text, "5000000"));
    tl_memory_proc_status(text, sizeof(text), &info, 42);
    assert(strstr(text, "Pid:\t42\n"));
    assert(strstr(text, "Threads:\t37\n"));
    assert(strstr(text, "VmRSS:\t1536000 kB"));
    assert(strstr(text, "VmHWM:\t1638400 kB"));
    assert(strstr(text, "RssAnon:\t1536000 kB\n"));
    assert(strstr(text, "RssFile:\t0 kB\n"));
    assert(strstr(text, "RssShmem:\t0 kB\n"));
    assert(strstr(text, "VmSwap:\t1228800 kB\n"));
    tl_memory_info clean = { .resident = 500ull << 20, .footprint = 300ull << 20, .threads = 4 };
    tl_memory_proc_status(text, sizeof(text), &clean, 99);
    assert(strstr(text, "VmRSS:\t512000 kB\n"));
    assert(strstr(text, "RssAnon:\t307200 kB\n"));
    assert(strstr(text, "VmSwap:\t0 kB\n"));
    info.available = info.total * 2;
    tl_memory_proc_meminfo(text, sizeof(text), &info);
    assert(strstr(text, "MemAvailable: 8388608 kB"));
    char small[4] = {1, 1, 1, 1};
    tl_memory_proc_meminfo(small, sizeof(small), &info);
    assert(small[3] == 0);
    puts("PASS: Android proc memory reflects changing process headroom, RSS, high-water mark and real thread count");
    return 0;
}
