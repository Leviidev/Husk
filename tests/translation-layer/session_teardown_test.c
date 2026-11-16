/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Production session closing/force-retirement state machine and JNI/DVM
 * session object sweep + class retirement contracts across Game A -> B -> A. */
#include <assert.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define TL_SESSION_IMPLEMENTATION
#include "husk-tl-session.h"
#include "husk-tl-jni-internal.h"
#include "husk-tl-jni-allocation.h"

#define NBUCKETS 512
static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;
static tl_jclass *g_classes[NBUCKETS], *g_allocation_classes;
static tl_jni_allocation_ledger g_allocations;
static tl_jni_class_allocation g_class_mirrors, g_class_unknown;
static tl_jni_class_allocation_window g_class_allocation_window = { .epoch = 1 };
static const tl_dvm_hooks g_mock_dvm = {0};
const tl_dvm_hooks *tl_dvm = &g_mock_dvm;
static __thread jobj *t_pending;

static tl_jclass *find_class_locked(const char *name)
{
    for (int b = 0; b < NBUCKETS; b++) {
        for (tl_jclass *c = g_classes[b]; c; c = c->next)
            if (c->name && !strcmp(c->name, name)) return c;
    }
    return NULL;
}

static tl_jclass *test_classes[8];
tl_jclass *tl_jni_class(const char *name)
{
    for (int i = 0; i < 8; i++) {
        if (test_classes[i] && !strcmp(test_classes[i]->name, name)) return test_classes[i];
    }
    return test_classes[0];
}

#include "jni_allocation_under_test.h"
#include "session_teardown_under_test.h"

static atomic_int g_wake_calls;
static atomic_int g_retire_calls;
static atomic_bool g_guest_dtor_ran;
static atomic_bool g_worker_entered_wait;
static atomic_llong g_second_wait_ns;

static void on_wake(void) { atomic_fetch_add(&g_wake_calls, 1); }
static void on_retire(void) { atomic_fetch_add(&g_retire_calls, 1); }

static int64_t mono_ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000000000ll + ts.tv_nsec;
}

typedef struct {
    bool tracked;
    bool exit_via_retire;
} sleeper_cfg;

static void *lingering_sleeper(void *arg)
{
    sleeper_cfg cfg = *(sleeper_cfg *)arg;
    free(arg);
    pthread_cleanup_push(tl_guest_thread_release, (void *)(uintptr_t)cfg.tracked);
    tl_guest_thread_set_retirable(true);
    atomic_store(&g_worker_entered_wait, true);

    /* First wait wakes immediately once on transition to CLOSING (state 2). */
    bool woke = tl_guest_session_wait_close(2000000000ll);
    assert(woke && tl_guest_session_closing());

    /* Second wait in CLOSING must stay bounded (<=20 ms) rather than busy-spinning in 0 ns. */
    int64_t t0 = mono_ns();
    (void)tl_guest_session_wait_close(200000000ll);
    atomic_store(&g_second_wait_ns, mono_ns() - t0);

    while (cfg.exit_via_retire) {
        (void)tl_guest_session_wait_close(200000000ll);
        if (tl_guest_session_force_stopping() && tl_guest_thread_is_retirable()) {
            /* Force-retired guest threads must not run guest C++/TLS destructors. */
            tl_guest_thread_retire();
        }
    }
    if (!tl_guest_thread_is_retiring()) atomic_store(&g_guest_dtor_ran, true);
    tl_guest_thread_set_retirable(false);
    pthread_cleanup_pop(1);
    return NULL;
}

static void run_thread_teardown_cycle(unsigned expected_epoch)
{
    assert(tl_guest_session_begin());
    assert(tl_guest_session_epoch() == expected_epoch);
    assert(!tl_guest_session_closing() && !tl_guest_session_force_stopping());

    atomic_store(&g_worker_entered_wait, false);
    atomic_store(&g_second_wait_ns, 0);
    atomic_store(&g_guest_dtor_ran, false);
    int wake_before = atomic_load(&g_wake_calls);
    int retire_before = atomic_load(&g_retire_calls);

    sleeper_cfg *cfg = malloc(sizeof(*cfg));
    assert(cfg != NULL);
    cfg->tracked = true;
    cfg->exit_via_retire = true;
    bool tracked = false;
    assert(tl_guest_thread_reserve(&tracked));
    assert(tracked);
    cfg->tracked = tracked;

    pthread_t th;
    assert(pthread_create(&th, NULL, lingering_sleeper, cfg) == 0);
    while (!atomic_load(&g_worker_entered_wait)) {
        struct timespec ts = { 0, 1000000 };
        nanosleep(&ts, NULL);
    }
    struct timespec settle = { 0, 10000000 };
    nanosleep(&settle, NULL);

    /* Phase 1: cooperative close wakes once and bounds re-waits (~20 ms) without 0-ns spin. */
    tl_guest_session_close();
    assert(tl_guest_session_closing() && !tl_guest_session_force_stopping());
    assert(atomic_load(&g_wake_calls) > wake_before);
    for (int i = 0; i < 50 && atomic_load(&g_second_wait_ns) == 0; i++) {
        struct timespec ts = { 0, 5000000 };
        nanosleep(&ts, NULL);
    }
    assert(atomic_load(&g_second_wait_ns) >= 10000000ll);
    assert(tl_guest_session_threads() == 1);

    /* Phase 2: force-abort retires the lingering guest thread cleanly and joins it. */
    tl_guest_session_abort();
    assert(tl_guest_session_force_stopping());
    assert(pthread_join(th, NULL) == 0);
    assert(atomic_load(&g_retire_calls) == retire_before + 1);
    assert(!atomic_load(&g_guest_dtor_ran));
    assert(tl_guest_session_threads() == 0);
    assert(tl_guest_session_finish());
}

static atomic_int g_monitors_destroyed;
static atomic_int g_intern_pruned;
static jobj *g_intern_keep, *g_intern_drop;

static void mock_destroy_monitor(void *m)
{
    atomic_fetch_add(&g_monitors_destroyed, 1);
    free(m);
}
static void mock_prune_interned(bool (*is_alive)(const jobj *o))
{
    if (g_intern_keep && !is_alive(g_intern_keep)) g_intern_keep = NULL;
    if (g_intern_drop && !is_alive(g_intern_drop)) {
        g_intern_drop = NULL;
        atomic_fetch_add(&g_intern_pruned, 1);
    }
}
static bool drop_app_cls(tl_jclass *jc, void *user)
{
    (void)user;
    if (!jc) return false;
    if (jc->dvm == (void *)(uintptr_t)2) return true;
    const char *n = jc->name;
    if (!n || n[0] != '[') return false;
    while (*n == '[') n++;
    if (*n != 'L') return false;
    size_t len = strlen(n);
    if (len < 3 || n[len - 1] != ';') return false;
    char base[512];
    if (len - 2 >= sizeof(base)) return false;
    memcpy(base, n + 1, len - 2);
    base[len - 2] = 0;
    /* Production is_app_array_class calls tl_jni_find_declared(base), which
     * acquires g_lock. Sweeping/forgetting classes must never hold g_lock
     * across drop_class or unlink base classes before array classes are checked. */
    pthread_mutex_lock(&g_lock);
    tl_jclass *elem = find_class_locked(base);
    pthread_mutex_unlock(&g_lock);
    return elem && elem != jc && elem->dvm == (void *)(uintptr_t)2;
}

static tl_jclass *register_class(int bucket, const char *name, tl_jclass *super, void *dvm_tag)
{
    tl_jclass *c = calloc(1, sizeof(*c));
    assert(c != NULL);
    c->name = strdup(name);
    c->super = super;
    c->dvm = dvm_tag;
    c->mirror = obj_alloc(TL_K_CLASS, NULL);
    c->mirror->klass.jc = c;
    c->next = g_classes[bucket];
    g_classes[bucket] = c;
    c->allocation_next = g_allocation_classes;
    g_allocation_classes = c;
    return c;
}

static void add_hle_field(tl_jclass *c, const char *name, const char *sig, bool is_static, uint32_t idx)
{
    tl_jfield *f = calloc(1, sizeof(*f));
    f->cls = c;
    f->name = strdup(name);
    f->sig = strdup(sig);
    f->is_static = is_static;
    f->index = idx;
    c->fields = realloc(c->fields, (size_t)(c->nfields + 1) * sizeof(*c->fields));
    c->fields[c->nfields++] = f;
}

int main(void)
{
    /* 1. Session state machine across A -> B -> A */
    tl_guest_register_wake_hook(on_wake);
    tl_guest_register_retire_hook(on_retire);
    run_thread_teardown_cycle(1);
    run_thread_teardown_cycle(2);
    run_thread_teardown_cycle(3);

    /* 2. Boot vs per-session JNI object sweep and class retirement across A -> B -> A */
    tl_jclass *cls_class = register_class(0, "java/lang/Class", NULL, NULL);
    tl_jclass *obj_class = register_class(1, "java/lang/Object", NULL, NULL);
    tl_jclass *str_class = register_class(2, "java/lang/String", obj_class, NULL);
    tl_jclass *boot_holder = register_class(3, "husk/BootHolder", obj_class, NULL);
    tl_jclass *boot_arr_class = register_class(12, "[Ljava/lang/Object;", obj_class, NULL);
    test_classes[0] = obj_class;
    test_classes[1] = str_class;
    test_classes[2] = cls_class;
    test_classes[3] = boot_arr_class;

    add_hle_field(boot_holder, "ref", "Ljava/lang/Object;", false, 0);
    boot_holder->statics = calloc(1, sizeof(jvalue));
    boot_holder->nstatics = 1;
    add_hle_field(boot_holder, "sref", "Ljava/lang/Object;", true, 0);

    jobj *boot_obj = obj_alloc(TL_K_OBJECT, boot_holder);
    boot_obj->fields = calloc(1, sizeof(jvalue));
    boot_obj->nfields = 1;
    g_intern_keep = obj_alloc(TL_K_STRING, str_class);
    g_intern_keep->str.utf8 = strdup("boot-interned");
    boot_holder->statics[0].l = boot_obj;
    tl_jni_mark_boot_objects();

    tl_jni_allocation_sample boot_sample = tl_jni_allocation_snapshot(&g_allocations, sizeof(jobj));
    assert(boot_sample.unfreed == 7);

    for (int sess = 0; sess < 3; sess++) {
        tl_jclass *app_cls = register_class(4 + sess, sess == 1 ? "com/game/B" : "com/game/A", obj_class, (void *)(uintptr_t)2);
        tl_jclass *app_arr_cls = register_class(20 + sess, sess == 1 ? "[Lcom/game/B;" : "[Lcom/game/A;", obj_class, NULL);
        app_cls->statics = calloc(1, sizeof(jvalue));
        app_cls->nstatics = 1;
        add_hle_field(app_cls, "appStatic", "Ljava/lang/Object;", true, 0);

        jobj *app_obj = obj_alloc(TL_K_OBJECT, app_cls);
        app_obj->monitor = malloc(16);
        jobj *app_arr = tl_jni_new_prim_array('B', 4096);
        jobj *app_oarr = tl_jni_new_obj_array(app_cls, 4);
        app_oarr->oarr.v[0] = app_obj;
        g_intern_drop = obj_alloc(TL_K_STRING, str_class);
        g_intern_drop->str.utf8 = strdup("ephemeral-interned");
        /* Boot holder temporarily points to dropped app object + string (must be cleared on sweep). */
        boot_obj->fields[0].l = app_obj;
        assert(app_arr != NULL);

        tl_jni_gc_ops ops = {
            .drop_class = drop_app_cls,
            .prune_interned = mock_prune_interned,
            .destroy_monitor = mock_destroy_monitor,
        };
        tl_jni_sweep_app_objects(&ops);
        tl_jni_forget_if(drop_app_cls, NULL);

        assert(boot_obj->fields[0].l == NULL);
        assert(g_intern_keep != NULL && g_intern_drop == NULL);
        assert(atomic_load(&g_intern_pruned) == sess + 1);
        assert(atomic_load(&g_monitors_destroyed) == sess + 1);
        assert(app_cls->dvm == NULL && app_cls->next == NULL && app_cls->mirror == NULL);
        assert(app_cls->fields == NULL && app_cls->statics == NULL);
        assert(app_arr_cls->next == NULL && app_arr_cls->mirror == NULL);
        assert(boot_arr_class->mirror != NULL);
        tl_jni_allocation_sample after = tl_jni_allocation_snapshot(&g_allocations, sizeof(jobj));
        assert(after.unfreed == boot_sample.unfreed);
        assert(after.array_payload == 0);
    }

    puts("PASS: session close/abort wakes once without 0-ns spin, force-retires lingering threads cleanly, and sweeps per-session JNI objects/arrays/monitors/classes across A -> B -> A");
    return 0;
}
