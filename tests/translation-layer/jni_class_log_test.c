/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Actual JNI logger and registry retirement, with only clock/log sinks mocked. */
#include "husk-tl-jni-internal.h"
#include "husk-tl-jni-allocation.h"
#include <assert.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define NBUCKETS 512
static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;
static tl_jclass *g_classes[NBUCKETS], *g_allocation_classes;
static tl_jni_allocation_ledger g_allocations;
static tl_jni_class_allocation g_class_mirrors, g_class_unknown;
static tl_jni_class_allocation_window g_class_allocation_window = { .epoch = 1 };
static uint64_t clock_ns = UINT64_C(10000000000);
static char captured[64][2048];
static size_t line_count;
static tl_jclass *rename_after_snapshot;

static int test_clock_gettime(int clock_id, struct timespec *out)
{
    assert(clock_id == CLOCK_MONOTONIC);
    out->tv_sec = (time_t)(clock_ns / UINT64_C(1000000000));
    out->tv_nsec = (long)(clock_ns % UINT64_C(1000000000));
    clock_ns += UINT64_C(30000000000);
    return 0;
}

__attribute__((format(printf, 1, 2))) static void tl_log_line(const char *format, ...)
{
    /* Every production log call, including summary/rows and legacy totals,
     * must run outside the class registry lock. */
    assert(!pthread_mutex_trylock(&g_lock));
    assert(line_count < sizeof(captured) / sizeof(captured[0]));
    va_list args;
    va_start(args, format);
    int n = vsnprintf(captured[line_count], sizeof(captured[0]), format, args);
    va_end(args);
    assert(n >= 0 && (size_t)n < sizeof(captured[0]));
    bool summary = strstr(captured[line_count], "jvm-class-allocations-partial[") != NULL;
    line_count++;
    if (summary && rename_after_snapshot) {
        /* The logger already copied its report. Replacing and freeing the
         * source name now must not change or invalidate the following rows. */
        free(rename_after_snapshot->name);
        rename_after_snapshot->name = strdup("renamed/Class");
        assert(rename_after_snapshot->name);
        rename_after_snapshot = NULL;
    }
    pthread_mutex_unlock(&g_lock);
}

#define clock_gettime test_clock_gettime
#include "jni_class_log_under_test.h"
#undef clock_gettime

static const char *find_line(size_t from, const char *text)
{
    for (size_t i = from; i < line_count; i++)
        if (strstr(captured[i], text)) return captured[i];
    assert(!"Expected production log line was not emitted");
    return NULL;
}
static uint64_t field(const char *line, const char *name)
{
    const char *at = strstr(line, name);
    assert(at);
    char *end;
    unsigned long long value = strtoull(at + strlen(name), &end, 10);
    assert(end != at + strlen(name));
    return (uint64_t)value;
}
static void init_class(tl_jclass *cls, const char *name, uint64_t declaration)
{
    cls->name = strdup(name);
    assert(cls->name);
    cls->allocation_declaration = declaration;
    tl_jni_class_allocation_init(&cls->allocations);
}
static void objects(tl_jclass *cls, unsigned n)
{
    for (unsigned i = 0; i < n; i++) tl_jni_class_allocation_object(&cls->allocations);
}
static bool drop_class(tl_jclass *cls, void *user)
{
    return cls == user;
}

int main(void)
{
    /* These are the real tl_jclass/jobj definitions, not layout substitutes. */
    tl_jclass retired = {0}, active = {0}, reloaded = {0};
    init_class(&retired, "retired/Class", 3);
    init_class(&active, "active/Class", 4);
    retired.next = retired.allocation_next = &active;
    retired.nmeths = 7; retired.nfields = 9; retired.dvm = &retired;
    g_classes[0] = g_allocation_classes = &retired;
    objects(&retired, 10); objects(&active, 2);
    tl_jni_class_allocation_array(&retired.allocations, 64);
    tl_jni_class_allocation_array(&active.allocations, 16);
    tl_jni_class_allocation_object(&g_class_mirrors);
    tl_jni_class_allocation_object(&g_class_unknown);
    rename_after_snapshot = &retired;
    tl_jni_log_allocation_stats("game-periodic");
    const char *summary = find_line(0, "jvm-class-allocations-partial[");
    assert(field(summary, " epoch=") == 1 && field(summary, " sample=") == 1);
    assert(strstr(summary, "baseline=1 interval=0.000 s"));
    assert(field(summary, " created-total=") == 14 && !field(summary, " created-delta="));
    assert(field(summary, " array-created-bytes-total=") == 80);
    assert(strstr(summary, "not live bytes") && strstr(summary, "excludes strings/fields/native resources/allocator overhead"));
    const char *row = find_line(0, "declaration=3 class=retired/Class");
    assert(field(row, " header-created-bytes-total=") == 10 * sizeof(jobj));
    assert(!strcmp(retired.name, "renamed/Class")); /* The emitted name was copied. */
    assert(find_line(0, "class=java/lang/Class (mirrors)"));
    assert(find_line(0, "class=<class-unavailable>"));

    objects(&retired, 3); objects(&active, 1);
    tl_jni_class_allocation_array(&retired.allocations, 32);
    size_t from = line_count;
    uint64_t last_ns = g_class_allocation_window.last_ns;
    tl_jni_log_allocation_stats("game-memory-warning");
    assert(line_count == from + 1); /* Warnings do not scan/reset class windows. */
    assert(g_class_allocation_window.sequence == 1 && g_class_allocation_window.last_ns == last_ns);
    from = line_count;
    tl_jni_log_allocation_stats("game-periodic");
    summary = find_line(from, "jvm-class-allocations-partial[");
    assert(strstr(summary, "baseline=0 interval=30.000 s"));
    assert(field(summary, " created-total=") == 18 && field(summary, " created-delta=") == 4);
    assert(field(summary, " array-created-bytes-delta=") == 32);
    assert(field(summary, " other-created-total=") == 2 && !field(summary, " other-created-delta="));
    row = find_line(from, "declaration=3 class=renamed/Class");
    assert(field(row, " created-delta=") == 3 && field(row, " header-created-bytes-delta=") == 3 * sizeof(jobj));

    tl_jni_forget_if(drop_class, &retired);
    assert(g_classes[0] == &active && !retired.next && !retired.dvm);
    assert(!retired.nmeths && !retired.nfields);
    assert(g_allocation_classes == &retired && retired.allocation_next == &active);
    objects(&retired, 1); objects(&active, 2);
    from = line_count;
    tl_jni_log_allocation_stats("game-periodic");
    summary = find_line(from, "jvm-class-allocations-partial[");
    assert(field(summary, " epoch=") == 2 && strstr(summary, "baseline=1 interval=0.000 s"));
    assert(field(summary, " created-total=") == 21 && !field(summary, " created-delta="));
    row = find_line(from, "declaration=3 class=renamed/Class");
    assert(field(row, " created-total=") == 14 && !field(row, " created-delta="));

    /* After retirement, late traffic on the old declaration remains visible;
     * new declarations contribute their entire creation since this baseline. */
    init_class(&reloaded, "retired/Class", 5);
    active.next = active.allocation_next = &reloaded;
    objects(&reloaded, 2); objects(&retired, 1);
    from = line_count;
    tl_jni_log_allocation_stats("game-periodic");
    summary = find_line(from, "jvm-class-allocations-partial[");
    assert(field(summary, " epoch=") == 2 && strstr(summary, "baseline=0 interval=30.000 s"));
    assert(field(summary, " created-total=") == 24 && field(summary, " created-delta=") == 3);
    assert(field(find_line(from, "declaration=3 class=renamed/Class"), " created-delta=") == 1);
    assert(field(find_line(from, "declaration=5 class=retired/Class"), " created-delta=") == 2);
    free(retired.name); free(active.name); free(reloaded.name);
    puts("PASS: production JNI class logs copy names, keep retired counters, reset session baselines, skip warning scans and log outside the registry lock");
    return 0;
}
