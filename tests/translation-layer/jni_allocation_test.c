/* SPDX-License-Identifier: GPL-2.0-or-later */
/* The production constructors/unref path, with only class lookup stubbed. */
#include "husk-tl-jni.h"
#include "husk-tl-jni-allocation.h"
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct tl_jclass { const char *name; };
static tl_jni_allocation_ledger g_allocations;
static const void *tl_dvm;
static unsigned calloc_call, fail_calloc_call;
static void *test_calloc(size_t n, size_t size)
{
    if (fail_calloc_call && ++calloc_call == fail_calloc_call) return NULL;
    return calloc(n, size);
}
tl_jclass *tl_jni_class(const char *name) { (void)name; return (tl_jclass *)(uintptr_t)1; }
#define calloc test_calloc
#include "jni_allocation_under_test.h"
#undef calloc

static tl_jni_allocation_sample sample(void)
{ return tl_jni_allocation_snapshot(&g_allocations, sizeof(jobj)); }
static void *worker(void *unused)
{
    (void)unused;
    for (unsigned i = 0; i < 10000; i++) {
        jobj *a = tl_jni_new_prim_array('B', 16);
        tl_jni_unref(a);
    }
    return NULL;
}
int main(void)
{
    _Static_assert(sizeof(((jobj *)0)->arr) <= sizeof(((jobj *)0)->str), "Array accounting must not enlarge jobj");
    _Static_assert(sizeof(((jobj *)0)->oarr) <= sizeof(((jobj *)0)->str), "Object-array accounting must not enlarge jobj");
    jobj *p = tl_jni_new_prim_array('I', 3);
    jobj *o = tl_jni_new_obj_array(NULL, 2);
    tl_jni_allocation_sample s = sample();
    assert(s.created == 2 && s.unfreed == 2 && s.headers == 2 * sizeof(jobj));
    assert(s.array_payload == 12 + 2 * sizeof(jobj *));
    tl_jni_ref(p); tl_jni_unref(p);
    assert(sample().unfreed == 2); /* Still referenced: no release is counted. */
    tl_jni_unref(p); tl_jni_unref(o);
    assert(!sample().unfreed && !sample().array_payload);

    p = tl_jni_new_prim_array('I', 10);
    o = tl_jni_new_obj_array(NULL, 10);
    p->arr.len = 2; o->oarr.len = 1; /* Logical shrink retains the allocation. */
    assert(sample().array_payload == 40 + 10 * sizeof(jobj *));
    tl_jni_unref(p); tl_jni_unref(o);
    assert(!sample().unfreed && !sample().array_payload);

    p = tl_jni_new_prim_array('D', 0);
    o = tl_jni_new_obj_array(NULL, 0);
    assert(sample().array_payload == sizeof(double) + sizeof(jobj *));
    tl_jni_unref(p); tl_jni_unref(o);
    assert(!sample().array_payload);

    tl_dvm = (const void *)(uintptr_t)1;
    p = tl_jni_new_prim_array('B', 13);
    tl_jni_unref(p); tl_jni_unref(p);
    s = sample();
    assert(s.unfreed == 1 && s.array_payload == 13 && s.deferred == 2 && p->refs == 1);
    /* Accounting must not change Java object lifetime or reinterpret deferred
     * unrefs as distinct unreachable objects. */
    ((uint8_t *)p->arr.data)[0] = 42;
    tl_dvm = NULL; tl_jni_unref(p);
    assert(!sample().unfreed && !sample().array_payload);

    calloc_call = 0; fail_calloc_call = 2; /* Object succeeds, payload fails. */
    p = tl_jni_new_prim_array('J', 4);
    assert(p && !p->arr.data && sample().unfreed == 1 && !sample().array_payload);
    fail_calloc_call = 0; tl_jni_unref(p);
    assert(!sample().unfreed);

    uint64_t created = sample().created;
    pthread_t threads[4];
    for (unsigned i = 0; i < 4; i++) assert(!pthread_create(&threads[i], NULL, worker, NULL));
    for (unsigned i = 0; i < 4; i++) assert(!pthread_join(threads[i], NULL));
    s = sample();
    assert(s.created == created + 40000 && !s.unfreed && !s.array_payload && s.deferred == 2);
    puts("PASS: actual JNI object/array allocation and non-DVM frees balance requested-storage accounting, preserve DVM retention, handle zero/failed payloads and concurrent allocation without heap walks");
    return 0;
}
