/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "husk-tl-jni-class-allocation.h"
#include <assert.h>
#include <pthread.h>
#include <stdio.h>

typedef struct {
    const char *name;
    tl_jni_class_allocation allocations;
} synthetic_class;
static synthetic_class classes[12];
static atomic_uint workers_done;
static void *allocate(void *argument)
{
    synthetic_class *cls = argument;
    for (unsigned i = 0; i < 25000; i++) {
        tl_jni_class_allocation_object(&cls->allocations);
        tl_jni_class_allocation_array(&cls->allocations, 16);
    }
    atomic_fetch_add(&workers_done, 1);
    return NULL;
}
static tl_jni_class_allocation_report report(tl_jni_class_allocation_window *window, uint64_t now)
{
    tl_jni_class_allocation_report r = tl_jni_class_allocation_begin(window, now);
    for (size_t i = 0; i < 12; i++)
        tl_jni_class_allocation_add(&r, i + 3, classes[i].name,
            tl_jni_class_allocation_snapshot(&classes[i].allocations, r.baseline));
    return r;
}
int main(void)
{
    _Static_assert(sizeof(tl_jni_class_allocation) <= 32, "Per-class counter/baseline storage stays bounded");
    _Static_assert(sizeof(tl_jni_class_allocation_report) <= 2048, "Snapshot uses fixed, small storage");
    static const char *names[] = { "zero", "one", "two", "three", "four", "five",
        "six", "seven", "eight", "nine", "ten", "eleven" };
    for (size_t i = 0; i < 12; i++) {
        classes[i].name = names[i];
        tl_jni_class_allocation_init(&classes[i].allocations);
        for (size_t n = 0; n < i; n++) tl_jni_class_allocation_object(&classes[i].allocations);
        tl_jni_class_allocation_array(&classes[i].allocations, i * 16);
    }
    tl_jni_class_allocation_window window = { .epoch = 1 };
    tl_jni_class_allocation_report r = report(&window, 100);
    assert(r.baseline && !r.interval_ns && r.count == 8 && r.groups == 12);
    assert(r.totals.created == 66 && r.totals.array_bytes == 66 * 16);
    assert(!r.totals.created_delta && !r.totals.array_bytes_delta);
    for (size_t i = 0; i < 8; i++) assert(r.top[i].counts.created == 11 - i);
    tl_jni_class_allocation_sample other = tl_jni_class_allocation_other(&r);
    assert(other.created == 6 && other.array_bytes == 6 * 16);

    /* The least-created class becomes first by interval traffic, rather than
     * keeping lifetime leaders at the top forever. Payload-only growth counts. */
    for (unsigned i = 0; i < 20; i++) tl_jni_class_allocation_object(&classes[0].allocations);
    tl_jni_class_allocation_array(&classes[1].allocations, 4096);
    r = report(&window, 200);
    assert(!r.baseline && r.interval_ns == 100 && r.count == 2);
    assert(r.top[0].declaration == 3 && r.top[0].counts.created_delta == 20);
    assert(r.top[1].declaration == 4 && !r.top[1].counts.created_delta && r.top[1].counts.array_bytes_delta == 4096);
    assert(r.totals.created == 86 && r.totals.created_delta == 20);
    other = tl_jni_class_allocation_other(&r);
    assert(other.created == 65 && !other.created_delta && !other.array_bytes_delta);

    /* A retirement/new-session boundary baselines shared boot classes too;
     * retained old declarations and a same-named new declaration stay distinct. */
    tl_jni_class_allocation_reset(&window);
    classes[11].name = classes[0].name;
    tl_jni_class_allocation_object(&classes[0].allocations);
    tl_jni_class_allocation_object(&classes[11].allocations);
    r = report(&window, 300);
    assert(r.epoch == 2 && r.baseline && !r.interval_ns && !r.totals.created_delta);
    tl_jni_class_allocation_object(&classes[11].allocations);
    r = report(&window, 400);
    assert(!r.baseline && r.totals.created_delta == 1 && r.count == 1 && r.top[0].declaration == 14);
    r = report(&window, 400); /* Duplicate/nonadvancing clocks never imply a rate. */
    assert(r.baseline && !r.interval_ns && !r.totals.created_delta);
    r = report(&window, 0); /* Clock failure re-baselines instead of underflowing. */
    assert(r.baseline && !r.interval_ns);

    static const char long_name[] =
        "some/really/long/generated/ClassNameThatIsLongerThanTheDiagnosticNameBuffer/"
        "some/really/long/generated/ClassNameThatIsLongerThanTheDiagnosticNameBuffer/"
        "andEvenMoreCharactersToRequireTruncation";
    r = (tl_jni_class_allocation_report){ .baseline = true };
    tl_jni_class_allocation_add(&r, 99, long_name, (tl_jni_class_allocation_sample){ .created = 1 });
    assert(r.top[0].name[159] == 0 && !strcmp(r.top[0].name + 156, "..."));

    /* Workers update the same production atomic counters while snapshots read
     * them. Only the snapshot caller owns baselines, as under JNI's g_lock. */
    tl_jni_class_allocation_reset(&window);
    report(&window, 1000);
    uint64_t before_created = 0, before_bytes = 0;
    for (size_t i = 0; i < 12; i++) {
        before_created += atomic_load(&classes[i].allocations.created);
        before_bytes += atomic_load(&classes[i].allocations.array_bytes_created);
    }
    pthread_t threads[4];
    for (unsigned i = 0; i < 4; i++) assert(!pthread_create(&threads[i], NULL, allocate, &classes[i % 2]));
    uint64_t now = 1001, previous = before_created, deltas = 0, byte_deltas = 0;
    while (atomic_load(&workers_done) != 4) {
        r = report(&window, now++);
        assert(r.totals.created >= previous);
        previous = r.totals.created;
        deltas += r.totals.created_delta;
        byte_deltas += r.totals.array_bytes_delta;
    }
    for (unsigned i = 0; i < 4; i++) assert(!pthread_join(threads[i], NULL));
    r = report(&window, now++);
    deltas += r.totals.created_delta;
    byte_deltas += r.totals.array_bytes_delta;
    assert(r.totals.created == before_created + 100000 && deltas == 100000);
    assert(r.totals.array_bytes == before_bytes + 1600000 && byte_deltas == 1600000);
    for (unsigned session = 0; session < 50; session++) {
        tl_jni_class_allocation_reset(&window);
        tl_jni_class_allocation_object(&classes[0].allocations);
        r = report(&window, now++);
        assert(r.baseline && !r.totals.created_delta);
        tl_jni_class_allocation_object(&classes[0].allocations);
        r = report(&window, now++);
        assert(!r.baseline && r.totals.created_delta == 1);
    }
    puts("PASS: production per-class allocation counters, fixed top selection/remainders, copied names, concurrent snapshots, clock failure and repeated-session baselines");
    return 0;
}
