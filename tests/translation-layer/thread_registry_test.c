/* SPDX-License-Identifier: GPL-2.0-or-later */
#define _DEFAULT_SOURCE
#define TL_SESSION_IMPLEMENTATION
#include "husk-tl-session.h"
#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <sched.h>
#include <stdlib.h>
#include <stdatomic.h>
#include <time.h>
#include <unistd.h>
#include <stdio.h>
#include "thread_registry_under_test.h"

static pthread_key_t key;
static atomic_bool destructor_entered, destructor_allowed;
static void destructor(void *unused)
{
    (void)unused;
    atomic_store(&destructor_entered, true);
    while (!atomic_load(&destructor_allowed)) usleep(1000);
}
typedef struct { atomic_bool ready; bool use_destructor; } context;
static void *worker(void *argument)
{
    context *ctx = argument;
    while (!atomic_load(&ctx->ready)) sched_yield();
    pthread_cleanup_push(tl_guest_thread_release, (void *)1);
    if (ctx->use_destructor) assert(!pthread_setspecific(key, (void *)1));
    pthread_cleanup_pop(1);
    return (void *)42;
}
static pthread_t start(context *ctx)
{
    bool tracked;
    assert(tl_guest_thread_reserve(&tracked) && tracked);
    pthread_t thread;
    assert(!pthread_create(&thread, NULL, worker, ctx));
    tl_guest_thread_register((uintptr_t)thread, true);
    return thread;
}
int main(void)
{
    assert(!pthread_key_create(&key, destructor));
    for (int session = 0; session < 100; session++) {
        assert(tl_guest_session_begin());
        context detached = {0}, joinable = {0};
        pthread_t first = start(&detached), second = start(&joinable);
        assert(session_thread_detach(first) == 0);
        assert(session_thread_detach(first) == EINVAL);
        assert(tl_guest_pthread_join(first, NULL) == EINVAL);
        assert(!tl_guest_session_reap(0)); /* neither entry has returned */
        atomic_store(&detached.ready, true);
        atomic_store(&joinable.ready, true);
        void *result = NULL;
        assert(!tl_guest_pthread_join(second, &result) && result == (void *)42);
        tl_guest_session_close();
        while (tl_guest_session_threads()) usleep(1000);
        assert(tl_guest_session_reap(2000));
        assert(tl_guest_session_finish());
    }
    assert(tl_guest_session_begin());
    context blocked = { .use_destructor = true };
    (void)start(&blocked);
    atomic_store(&blocked.ready, true);
    while (!atomic_load(&destructor_entered)) usleep(1000);
    assert(tl_guest_session_threads() == 0);
    tl_guest_session_close();
    assert(!tl_guest_session_reap(10)); /* counter zero does not mean TLS is done */
    assert(!tl_guest_session_threads());
    atomic_store(&destructor_allowed, true);
    assert(tl_guest_session_reap(2000));
    assert(tl_guest_session_finish());
    assert(!pthread_key_delete(key));
    puts("PASS: production thread registry joins detached/guest-joined threads once and retains blocked TLS teardown for retry");
    return 0;
}
