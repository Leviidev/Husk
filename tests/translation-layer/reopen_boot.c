/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "husk-tl-dvm.h"
#include "husk-tl-jni.h"
#include "husk-tl-jni-internal.h"
#include "husk-tl-ld.h"
#include "husk-tl-session.h"
#include <assert.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

void tl_log_line(const char *fmt, ...) {
    va_list ap; va_start(ap, fmt); vfprintf(stderr, fmt, ap); fputc('\n', stderr); va_end(ap);
}
static atomic_bool done;
static bool ok;
static jvalue returned, marker;
static char database[1024];
static pthread_mutex_t probe_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t probe_changed = PTHREAD_COND_INITIALIZER;
static unsigned observed;
static void probe_entered(void *env, void *cls, int32_t kind) {
    (void)env; (void)cls;
    pthread_mutex_lock(&probe_lock); observed |= 1u << kind;
    pthread_cond_broadcast(&probe_changed); pthread_mutex_unlock(&probe_lock);
}
static void await_probe(unsigned bits) {
    struct timespec at; clock_gettime(CLOCK_REALTIME, &at); at.tv_sec += 10;
    pthread_mutex_lock(&probe_lock);
    while ((observed & bits) != bits) assert(pthread_cond_timedwait(&probe_changed, &probe_lock, &at) == 0);
    pthread_mutex_unlock(&probe_lock);
}
typedef struct probe_call { const char *method, *sig; bool ok; jvalue value; } probe_call;
static void probe_done(void *unused) { (void)unused; tl_session_worker_done(); }
static void exit_probe_thread(void *env, void *cls) { (void)env; (void)cls; pthread_exit(NULL); }
static void *call_probe(void *arg) {
    probe_call *call = arg;
    pthread_cleanup_push(probe_done, NULL);
    call->ok = tl_dvm_call_static("ReopenBootProbe", call->method, call->sig, NULL, &call->value);
    pthread_cleanup_pop(1);
    return NULL;
}
static pthread_t start_probe(probe_call *call) {
    pthread_attr_t attr; pthread_attr_init(&attr); pthread_attr_setstacksize(&attr, 64u << 20);
    pthread_t thread; assert(tl_session_worker_add());
    assert(pthread_create(&thread, &attr, call_probe, call) == 0); pthread_attr_destroy(&attr);
    return thread;
}
static void *run(void *unused) {
    (void)unused;
    jvalue argument = { .l = tl_jni_new_string(database) };
    ok = tl_dvm_call_static("ReopenBootProbe", "marker", "()Ljava/lang/Object;", NULL, &marker)
      && tl_dvm_call_static("ReopenBootProbe", "run", "(Ljava/lang/String;)I", &argument, &returned);
    atomic_store(&done, true);
    return NULL;
}
int main(int argc, char **argv) {
    assert(argc == 4);
    const char *tmp = getenv("TMPDIR"); assert(tmp && tmp[0] == '/');
    int length = snprintf(database, sizeof(database), "%s/husk-reopen-%d.sqlite", tmp, getpid());
    assert(length > 0 && (size_t)length < sizeof(database)); unlink(database);
    static const char *names[] = { "com.android.art/javalib/core-oj.jar", "com.android.art/javalib/core-libart.jar",
        "com.android.art/javalib/okhttp.jar", "com.android.art/javalib/bouncycastle.jar", "com.android.art/javalib/apache-xml.jar",
        "com.android.i18n/javalib/core-icu4j.jar", "com.android.conscrypt/javalib/conscrypt.jar", "husk-framework.dex" };
    char paths[8][1024], dir[1024]; const char *boot[9];
    for (int i = 0; i < 8; i++) { snprintf(paths[i], sizeof(paths[i]), "%s/%s", argv[1], names[i]); boot[i] = paths[i]; }
    boot[7] = argv[3]; /* Framework compiled from this branch, not a prebuilt fork. */
    boot[8] = argv[2];
    snprintf(dir, sizeof(dir), "%s/com.android.art", argv[1]); setenv("ANDROID_ART_ROOT", dir, 1);
    snprintf(dir, sizeof(dir), "%s/com.android.i18n", argv[1]); setenv("ANDROID_I18N_ROOT", dir, 1);
    snprintf(dir, sizeof(dir), "%s/com.android.tzdata", argv[1]); setenv("ANDROID_TZDATA_ROOT", dir, 1);
    snprintf(dir, sizeof(dir), "%s/com.android.i18n/lib64", argv[1]); tl_ld_add_search_dir(dir);
    snprintf(dir, sizeof(dir), "%s/com.android.conscrypt/lib64", argv[1]); tl_ld_add_search_dir(dir);
    tl_jni_init(); assert(tl_dvm_start(boot, 9));
    snprintf(dir, sizeof(dir), "%s/com.android.art/lib64", argv[1]); tl_dvm_load_natives(dir);
    assert(tl_jni_register_one(tl_jni_class_object("ReopenBootProbe"), "entered", "(I)V", probe_entered) == 0);
    assert(tl_jni_register_one(tl_jni_class_object("ReopenBootProbe"), "exitThread", "()V", exit_probe_thread) == 0);
    void *old_marker = NULL;
    for (int cycle = 0; cycle < 3; cycle++) {
        assert(tl_session_restart()); assert(tl_dvm_initialize_session());
        atomic_store(&done, false); returned.j = 0; marker.j = 0;
        pthread_attr_t attr; pthread_attr_init(&attr); pthread_attr_setstacksize(&attr, 64u << 20);
        pthread_t thread; assert(pthread_create(&thread, &attr, run, NULL) == 0); pthread_attr_destroy(&attr);
        for (int i = 0; i < 3000 && !atomic_load(&done); i++) usleep(1000);
        if (!atomic_load(&done)) { fputs("FAIL: frame callback was stranded on an old looper\n", stderr); tl_session_request_stop(); }
        pthread_join(thread, NULL);
        assert(ok && returned.i == 1 && marker.l && marker.l != old_marker);
        old_marker = marker.l;
        observed = 0;
        probe_call normal = { .method = "monitorWait", .sig = "()I" };
        pthread_t normal_thread = start_probe(&normal); await_probe(1);
        jvalue result;
        assert(tl_dvm_call_static("ReopenBootProbe", "notifyWaiter", "()V", NULL, &result));
        pthread_join(normal_thread, NULL); assert(normal.ok && normal.value.i == 1);
        assert(tl_dvm_call_static("ReopenBootProbe", "timedWait", "()I", NULL, &result) && result.i == 1);
        probe_call abnormal = { .method = "abnormalExit", .sig = "()V" };
        pthread_t abnormal_thread = start_probe(&abnormal);
        assert(tl_session_wait_drained(3000)); pthread_join(abnormal_thread, NULL);
        assert(!abnormal.ok);
        probe_call java_exit = { .method = "javaThreadExit", .sig = "()V" };
        pthread_t java_exit_thread = start_probe(&java_exit);
        assert(tl_session_wait_drained(5000)); pthread_join(java_exit_thread, NULL);
        assert(java_exit.ok);
        probe_call abnormal_init = { .method = "exitInInit", .sig = "()I" };
        abnormal_thread = start_probe(&abnormal_init);
        assert(tl_session_wait_drained(3000)); pthread_join(abnormal_thread, NULL);
        assert(!abnormal_init.ok);
        probe_call failed_init = { .method = "exitInInit", .sig = "()I" };
        abnormal_thread = start_probe(&failed_init);
        assert(tl_session_wait_drained(3000)); pthread_join(abnormal_thread, NULL);
        assert(!failed_init.ok);
        observed = 0;
        probe_call canceled[] = {
            { .method = "monitorWait", .sig = "()I" }, { .method = "sleepUntilStop", .sig = "()V" },
            { .method = "holdUntilStop", .sig = "()V" }, { .method = "contendUntilStop", .sig = "()V" },
            { .method = "slowInit", .sig = "()I" }, { .method = "waitForInit", .sig = "()I" }
        };
        pthread_t blocked[6];
        for (int i = 0; i < 3; i++) blocked[i] = start_probe(&canceled[i]);
        await_probe(7); blocked[3] = start_probe(&canceled[3]); await_probe(15);
        blocked[4] = start_probe(&canceled[4]); await_probe(31);
        blocked[5] = start_probe(&canceled[5]); await_probe(63);
        tl_session_request_stop(); assert(tl_session_wait_drained(5000));
        for (int i = 0; i < 6; i++) { pthread_join(blocked[i], NULL); assert(!canceled[i].ok); }
        assert(tl_dvm_end_session());
        fprintf(stderr, "PASS: session %d created fresh boot singletons/properties, executed its frame callback and released abandoned database locks\n", cycle + 1);
        fprintf(stderr, "PASS: wait/notify, recursive monitor ownership and original timeout; stop wakes sleep/wait/contended entry\n");
    }
    unlink(database);
}
