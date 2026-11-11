/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "husk-tl-session.h"
#include <assert.h>
#include <pthread.h>
#include <stdatomic.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/socket.h>

static atomic_bool entered, release_worker;
static void *blocked_read(void *arg)
{
    int fd = *(int *)arg;
    assert(tl_session_enter());
    tl_session_io io; assert(tl_session_io_begin(&io, fd));
    atomic_store(&entered, true);
    char byte;
    assert(read(fd, &byte, 1) <= 0);
    tl_session_io_end(&io);
    tl_session_leave(); tl_session_worker_done();
    return NULL;
}
static void *blocked_poll(void *unused)
{
    (void)unused;
    assert(tl_session_enter());
    atomic_store(&entered, true);
    assert(tl_session_poll(NULL, 0, -1) == -1 && errno == ECANCELED);
    tl_session_leave(); tl_session_worker_done();
    return NULL;
}
static void *worker(void *unused)
{
    (void)unused;
    assert(tl_session_enter());
    atomic_store(&entered, true);
    while (!tl_session_stopping()) usleep(1000);
    tl_session_leave();
    /* A thread's cleanup still references app objects after its Java call returns. */
    while (!atomic_load(&release_worker)) usleep(1000);
    tl_session_worker_done();
    return NULL;
}
int main(void)
{
    for (int cycle = 0; cycle < 20; cycle++) {
        assert(tl_session_restart());
        atomic_store(&entered, false); atomic_store(&release_worker, false);
        assert(tl_session_worker_add());
        pthread_t thread; assert(pthread_create(&thread, NULL, worker, NULL) == 0);
        while (!atomic_load(&entered)) usleep(1000);
        tl_session_request_stop();
        assert(!tl_session_enter());
        assert(!tl_session_worker_add());
        assert(!tl_session_drained());
        assert(!tl_session_restart());
        atomic_store(&release_worker, true); pthread_join(thread, NULL);
        assert(tl_session_drained());
        tl_session_allow_cleanup(true);
        assert(!tl_session_stopping()); assert(tl_session_enter());
        assert(!tl_session_drained()); tl_session_leave();
        tl_session_allow_cleanup(false);
        assert(tl_session_stopping());
    }
    for (int cycle = 0; cycle < 20; cycle++) {
        assert(tl_session_restart());
        int sv[2]; assert(socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == 0);
        atomic_store(&entered, false); assert(tl_session_worker_add());
        pthread_t thread; assert(pthread_create(&thread, NULL, blocked_read, &sv[0]) == 0);
        while (!atomic_load(&entered)) usleep(1000);
        tl_session_request_stop(); pthread_join(thread, NULL);
        assert(tl_session_drained()); close(sv[0]); close(sv[1]);

        assert(tl_session_restart());
        atomic_store(&entered, false); assert(tl_session_worker_add());
        assert(pthread_create(&thread, NULL, blocked_poll, NULL) == 0);
        while (!atomic_load(&entered)) usleep(1000);
        tl_session_request_stop(); pthread_join(thread, NULL);
        assert(tl_session_drained());
    }
    assert(tl_session_restart());
    int sv[2]; assert(socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == 0);
    tl_session_io io; assert(tl_session_io_begin(&io, sv[0]));
    int reused = sv[0]; close(sv[0]);
    int other[2]; assert(socketpair(AF_UNIX, SOCK_STREAM, 0, other) == 0);
    assert(other[0] == reused);
    tl_session_request_stop();
    // The old socket's pin is stopped; an unrelated socket at its old fd is untouched.
    assert(write(other[0], "x", 1) == 1);
    char byte; assert(read(other[1], &byte, 1) == 1 && byte == 'x');
    assert(!tl_session_drained()); tl_session_io_end(&io);
    close(sv[1]); close(other[0]); close(other[1]);
    assert(tl_session_drained()); assert(tl_session_restart());
    assert(tl_session_poll(NULL, 0, 0) == 0);
    assert(tl_session_poll(NULL, 0, 25) == 0);
    puts("PASS: repeated close/reopen, blocked socket reads and infinite polls; descriptor reuse stays safe");
}
