/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Count actual blocking calls: idle waits must not wake on a periodic timer. */
#include <assert.h>
#include <errno.h>
#include <poll.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>

static pthread_mutex_t test_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t test_changed = PTHREAD_COND_INITIALIZER;
static unsigned poll_calls, timed_waits, drain_waits;
static _Thread_local bool measure_sleep, measure_drain;
static int counted_poll(struct pollfd *fds, nfds_t count, int timeout)
{
    pthread_mutex_lock(&test_lock); poll_calls++; pthread_cond_broadcast(&test_changed); pthread_mutex_unlock(&test_lock);
    return poll(fds, count, timeout);
}
static int counted_timedwait(pthread_cond_t *cv, pthread_mutex_t *mutex, const struct timespec *at)
{
    if (measure_sleep) {
        pthread_mutex_lock(&test_lock); timed_waits++; pthread_cond_broadcast(&test_changed); pthread_mutex_unlock(&test_lock);
    }
    if (measure_drain) {
        pthread_mutex_lock(&test_lock); drain_waits++; pthread_cond_broadcast(&test_changed); pthread_mutex_unlock(&test_lock);
    }
#if defined(__APPLE__)
    return pthread_cond_timedwait_relative_np(cv, mutex, at);
#else
    return pthread_cond_timedwait(cv, mutex, at);
#endif
}
#define poll counted_poll
#if defined(__APPLE__)
#define pthread_cond_timedwait_relative_np counted_timedwait
#else
#define pthread_cond_timedwait counted_timedwait
#endif
#include "../../src/translation-layer-next/husk-tl-session.c"
#undef poll
#undef pthread_cond_timedwait
#undef pthread_cond_timedwait_relative_np

static void await_count(unsigned *value, unsigned target)
{
    struct timespec at; clock_gettime(CLOCK_REALTIME, &at); at.tv_sec += 5;
    pthread_mutex_lock(&test_lock);
    while (*value < target) assert(pthread_cond_timedwait(&test_changed, &test_lock, &at) == 0);
    pthread_mutex_unlock(&test_lock);
}
static void *poll_wait(void *unused)
{
    (void)unused;
    assert(tl_session_poll(NULL, 0, -1) == -1 && errno == ECANCELED);
    tl_session_worker_done(); return NULL;
}
static void *sleep_wait(void *unused)
{
    (void)unused;
    measure_sleep = true;
    assert(!tl_session_sleep(60000, 0));
    tl_session_worker_done(); return NULL;
}
static void *drain_wait(void *unused)
{
    (void)unused; measure_drain = true;
    assert(tl_session_wait_drained(2000)); return NULL;
}
static struct { pthread_mutex_t lock; pthread_cond_t cv; bool canceled; unsigned callbacks; } request = {
    .lock = PTHREAD_MUTEX_INITIALIZER, .cv = PTHREAD_COND_INITIALIZER
};
static void cancel_wait(void *context)
{
    (void)context;
    pthread_mutex_lock(&request.lock); request.canceled = true; request.callbacks++;
    pthread_cond_signal(&request.cv); pthread_mutex_unlock(&request.lock);
}
static void *callback_wait(void *unused)
{
    (void)unused;
    tl_session_wait wait; assert(tl_session_wait_begin(&wait, cancel_wait, NULL));
    pthread_mutex_lock(&request.lock);
    pthread_mutex_lock(&test_lock); timed_waits++; pthread_cond_broadcast(&test_changed); pthread_mutex_unlock(&test_lock);
    while (!request.canceled) pthread_cond_wait(&request.cv, &request.lock);
    pthread_mutex_unlock(&request.lock);
    tl_session_wait_end(&wait); tl_session_worker_done(); return NULL;
}
int main(void)
{
    assert(tl_session_restart());
    /* The wake pipe must already exist before guest FDs can be opened/closed. */
    int closed[2]; assert(pipe(closed) == 0); close(closed[0]); close(closed[1]);
    struct pollfd invalid[2] = { { .fd = closed[0], .events = POLLIN }, { .fd = closed[1], .events = POLLIN } };
    assert(tl_session_poll(invalid, 2, 1) == 2);
    assert(invalid[0].revents == POLLNVAL && invalid[1].revents == POLLNVAL);
    poll_calls = 0;
    assert(tl_session_poll(NULL, 0, 1000) == 0 && poll_calls == 1);
    int fd[2]; assert(pipe(fd) == 0); assert(write(fd[1], "x", 1) == 1);
    struct pollfd ready[2] = { { .fd = fd[0], .events = POLLIN }, { .fd = -1, .events = POLLIN, .revents = POLLHUP } };
    assert(tl_session_poll(ready, 2, 0) == 1 && (ready[0].revents & POLLIN) && ready[1].revents == 0);
    close(fd[0]); close(fd[1]);
    enum { N = 8 }; pthread_t threads[N];
    for (int cycle = 0; cycle < 20; cycle++) {
        assert(tl_session_restart()); poll_calls = 0;
        for (int i = 0; i < N; i++) {
            assert(tl_session_worker_add()); assert(pthread_create(&threads[i], NULL, poll_wait, NULL) == 0);
        }
        await_count(&poll_calls, N);
        assert(!tl_session_restart());
        tl_session_request_stop(); tl_session_request_stop();
        assert(tl_session_wait_drained(2000));
        for (int i = 0; i < N; i++) pthread_join(threads[i], NULL);
        assert(poll_calls == N);
        assert(tl_session_poll(NULL, 0, -1) == -1 && errno == ECANCELED && poll_calls == N);
    }
    assert(tl_session_restart());
    assert(tl_session_poll(NULL, 0, 0) == 0); /* the old session's wake byte is gone */
    timed_waits = 0;
    measure_sleep = true;
    assert(tl_session_sleep(40, 0) && timed_waits == 1);
    measure_sleep = false;
    for (int cycle = 0; cycle < 20; cycle++) {
        assert(tl_session_restart()); timed_waits = 0;
        assert(tl_session_worker_add()); assert(pthread_create(&threads[0], NULL, sleep_wait, NULL) == 0);
        await_count(&timed_waits, 1); tl_session_request_stop();
        assert(tl_session_wait_drained(2000)); pthread_join(threads[0], NULL);
        assert(timed_waits == 1);
        assert(tl_session_restart()); timed_waits = 0; request.canceled = false;
        unsigned callbacks = request.callbacks;
        assert(tl_session_worker_add()); assert(pthread_create(&threads[0], NULL, callback_wait, NULL) == 0);
        await_count(&timed_waits, 1); tl_session_request_stop(); tl_session_request_stop();
        assert(tl_session_wait_drained(2000)); pthread_join(threads[0], NULL);
        assert(request.callbacks == callbacks + 1);
        tl_session_wait late; assert(!tl_session_wait_begin(&late, cancel_wait, NULL));
    }
    assert(tl_session_restart()); drain_waits = 0;
    assert(tl_session_worker_add()); assert(pthread_create(&threads[0], NULL, drain_wait, NULL) == 0);
    await_count(&drain_waits, 1);
    for (int i = 0; i < 1000; i++) { assert(tl_session_enter()); tl_session_leave(); }
    tl_session_worker_done(); pthread_join(threads[0], NULL);
    assert(drain_waits == 1); /* normal Java calls never wake a drain waiter */
    tl_session_set_cancellable(false); tl_session_request_stop();
    unsigned previous = poll_calls;
    assert(tl_session_poll(NULL, 0, 0) == 0 && poll_calls == previous + 1);
    tl_session_io untracked_io; assert(tl_session_io_begin(&untracked_io, -1));
    assert(!untracked_io.registered); tl_session_io_end(&untracked_io);
    tl_session_wait untracked_wait; assert(tl_session_wait_begin(&untracked_wait, cancel_wait, NULL));
    assert(!untracked_wait.registered); tl_session_wait_end(&untracked_wait);
    tl_session_set_cancellable(true); assert(tl_session_restart());
    puts("PASS: one blocking poll per wait, broadcast stop for 8 waiters, original deadlines, stop callbacks and clean restart");
}
