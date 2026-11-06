/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "husk-tl-session.h"
#include <pthread.h>
#include <stdatomic.h>
#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static atomic_bool stopping;
static atomic_bool cancellable = true;
static unsigned calls, workers;
static _Thread_local bool cleanup;
static tl_session_io *blocking_io;
static tl_session_wait *waiting;
static unsigned pollers, sleepers;
static unsigned drain_waiters;
static int stop_pipe[2] = { -1, -1 };
#if defined(__APPLE__)
static pthread_cond_t changed = PTHREAD_COND_INITIALIZER, stopped = PTHREAD_COND_INITIALIZER;
static void init_wait_conditions(void) { }
#else
static pthread_cond_t changed, stopped;
static pthread_once_t conditions_once = PTHREAD_ONCE_INIT;
static void make_wait_conditions(void)
{
    pthread_condattr_t attr; pthread_condattr_init(&attr);
    pthread_condattr_setclock(&attr, CLOCK_MONOTONIC);
    pthread_cond_init(&changed, &attr); pthread_cond_init(&stopped, &attr);
    pthread_condattr_destroy(&attr);
}
static void init_wait_conditions(void) { pthread_once(&conditions_once, make_wait_conditions); }
#endif

/* Keep the byte readable until the entire session drains: every poller must wake,
 * including one which starts after stop. Restart drains it only with no readers left. */
static bool make_stop_pipe(void)
{
    if (stop_pipe[0] >= 0) return true;
    int fd[2];
    if (pipe(fd) != 0) return false;
    for (int i = 0; i < 2; i++) {
        if (fcntl(fd[i], F_SETFL, O_NONBLOCK) < 0 || fcntl(fd[i], F_SETFD, FD_CLOEXEC) < 0) {
            int error = errno; close(fd[0]); close(fd[1]); errno = error; return false;
        }
    }
    stop_pipe[0] = fd[0]; stop_pipe[1] = fd[1];
    return true;
}

static bool drained(void)
{
    return calls == 0 && workers == 0 && blocking_io == NULL && waiting == NULL && pollers == 0 && sleepers == 0;
}
static void notify_drained(void)
{
    if (drain_waiters && drained()) pthread_cond_broadcast(&changed);
}

static struct timespec deadline(int64_t millis, int32_t nanos)
{
    struct timespec at; clock_gettime(CLOCK_MONOTONIC, &at);
    at.tv_sec += millis / 1000;
    at.tv_nsec += (long)(millis % 1000) * 1000000 + nanos;
    at.tv_sec += at.tv_nsec / 1000000000;
    at.tv_nsec %= 1000000000;
    return at;
}

/* Darwin's relative condition wait uses a monotonic duration. Preserve that
 * duration across spurious wakeups, rather than making sleep depend on wall time. */
static int wait_until(pthread_cond_t *cv, const struct timespec *at)
{
    struct timespec now; clock_gettime(CLOCK_MONOTONIC, &now);
    struct timespec left = { at->tv_sec - now.tv_sec, at->tv_nsec - now.tv_nsec };
    if (left.tv_nsec < 0) { left.tv_sec--; left.tv_nsec += 1000000000; }
    if (left.tv_sec < 0 || (!left.tv_sec && !left.tv_nsec)) return ETIMEDOUT;
#if defined(__APPLE__)
    return pthread_cond_timedwait_relative_np(cv, &lock, &left);
#else
    return pthread_cond_timedwait(cv, &lock, at);
#endif
}

bool tl_session_stopping(void) { return !cleanup && atomic_load(&stopping); }
void tl_session_set_cancellable(bool enabled) { atomic_store(&cancellable, enabled); }
void tl_session_allow_cleanup(bool allow) { cleanup = allow; }
bool tl_session_enter(void)
{
    pthread_mutex_lock(&lock);
    bool ok = cleanup || !atomic_load(&stopping);
    if (ok) calls++;
    pthread_mutex_unlock(&lock);
    return ok;
}
void tl_session_leave(void)
{
    pthread_mutex_lock(&lock); assert(calls); calls--; notify_drained(); pthread_mutex_unlock(&lock);
}
bool tl_session_worker_add(void)
{
    pthread_mutex_lock(&lock);
    bool ok = !atomic_load(&stopping);
    if (ok) workers++;
    pthread_mutex_unlock(&lock);
    return ok;
}
void tl_session_worker_done(void)
{
    pthread_mutex_lock(&lock); assert(workers); workers--; notify_drained(); pthread_mutex_unlock(&lock);
}
void tl_session_request_stop(void)
{
    pthread_mutex_lock(&lock);
    bool was_stopping = atomic_exchange(&stopping, true);
    if (!was_stopping) {
        if (stop_pipe[1] >= 0) {
            ssize_t n;
            do { n = write(stop_pipe[1], "x", 1); } while (n < 0 && errno == EINTR);
        }
        if (sleepers) pthread_cond_broadcast(&stopped);
        for (tl_session_wait *wait = waiting; wait; wait = wait->next) wait->cancel(wait->context);
    }
    for (tl_session_io *io = blocking_io; io; io = io->next)
        if (io->socket >= 0) shutdown(io->socket, SHUT_RDWR);
    pthread_mutex_unlock(&lock);
}
bool tl_session_wait_begin(tl_session_wait *wait, void (*cancel)(void *), void *context)
{
    wait->registered = false;
    if (!atomic_load(&cancellable)) return true;
    pthread_mutex_lock(&lock);
    bool ok = cleanup || !atomic_load(&stopping);
    if (ok) {
        wait->cancel = cancel; wait->context = context;
        wait->next = waiting; waiting = wait;
        wait->registered = true;
    }
    pthread_mutex_unlock(&lock);
    return ok;
}
void tl_session_wait_end(tl_session_wait *wait)
{
    if (!wait->registered) return;
    pthread_mutex_lock(&lock);
    tl_session_wait **p = &waiting;
    while (*p && *p != wait) p = &(*p)->next;
    assert(*p == wait); *p = wait->next;
    wait->registered = false;
    notify_drained();
    pthread_mutex_unlock(&lock);
}
bool tl_session_io_begin(tl_session_io *io, int fd)
{
    int saved_errno = errno;
    io->socket = -1; io->next = NULL; io->registered = false;
    if (!atomic_load(&cancellable)) return true;
    pthread_mutex_lock(&lock);
    if (!cleanup && atomic_load(&stopping)) {
        pthread_mutex_unlock(&lock); errno = ECANCELED; return false;
    }
    int type; socklen_t size = sizeof(type);
    if (getsockopt(fd, SOL_SOCKET, SO_TYPE, &type, &size) == 0) {
        io->socket = dup(fd);
        if (io->socket < 0) { pthread_mutex_unlock(&lock); return false; }
    }
    io->next = blocking_io; blocking_io = io;
    io->registered = true;
    pthread_mutex_unlock(&lock);
    errno = saved_errno;
    return true;
}
void tl_session_io_end(tl_session_io *io)
{
    if (!io->registered) return;
    int saved_errno = errno;
    pthread_mutex_lock(&lock);
    tl_session_io **p = &blocking_io;
    while (*p && *p != io) p = &(*p)->next;
    assert(*p == io); *p = io->next;
    io->registered = false;
    if (io->socket >= 0) close(io->socket);
    notify_drained();
    pthread_mutex_unlock(&lock);
    errno = saved_errno;
}
int tl_session_poll(struct pollfd *fds, nfds_t count, int timeout_ms)
{
    if (cleanup || !atomic_load(&cancellable)) return poll(fds, count, timeout_ms);
    if (timeout_ms == 0) {
        if (atomic_load(&stopping)) { errno = ECANCELED; return -1; }
        return poll(fds, count, 0);
    }
    size_t total, bytes;
    if (count == (nfds_t)-1 || __builtin_add_overflow((size_t)count, (size_t)1, &total)
        || __builtin_mul_overflow(total, sizeof(struct pollfd), &bytes)) { errno = EINVAL; return -1; }
    if (count && !fds) { errno = EFAULT; return -1; }
    struct pollfd small[17];
    struct pollfd *all = count < 17 ? small : malloc(bytes);
    if (!all) { errno = ENOMEM; return -1; }
    if (count) memcpy(all, fds, count * sizeof(*all));
    pthread_mutex_lock(&lock);
    if (atomic_load(&stopping) || stop_pipe[0] < 0) {
        int error = atomic_load(&stopping) ? ECANCELED : EINVAL;
        pthread_mutex_unlock(&lock);
        if (all != small) free(all);
        errno = error; return -1;
    }
    all[count] = (struct pollfd){ .fd = stop_pipe[0], .events = POLLIN };
    pollers++;
    pthread_mutex_unlock(&lock);
    /* One ordinary blocking poll, with exactly the guest's timeout. */
    int result = poll(all, count + 1, timeout_ms), error = errno;
    pthread_mutex_lock(&lock);
    if (atomic_load(&stopping)) { result = -1; error = ECANCELED; }
    pollers--; notify_drained();
    pthread_mutex_unlock(&lock);
    if (result >= 0) for (nfds_t i = 0; i < count; i++) fds[i].revents = all[i].revents;
    if (all != small) free(all);
    errno = error;
    return result;
}
void tl_session_counts(unsigned *active_calls, unsigned *active_workers)
{
    pthread_mutex_lock(&lock);
    *active_calls = calls; *active_workers = workers;
    pthread_mutex_unlock(&lock);
}
bool tl_session_drained(void)
{
    pthread_mutex_lock(&lock); bool ok = drained(); pthread_mutex_unlock(&lock); return ok;
}
bool tl_session_wait_drained(int timeout_ms)
{
    init_wait_conditions();
    struct timespec at = deadline(timeout_ms < 0 ? 0 : timeout_ms, 0);
    pthread_mutex_lock(&lock);
    drain_waiters++;
    while (!drained()) {
        int rc = timeout_ms < 0 ? pthread_cond_wait(&changed, &lock) : wait_until(&changed, &at);
        if (rc) break;
    }
    bool ok = drained();
    drain_waiters--;
    pthread_mutex_unlock(&lock);
    return ok;
}
bool tl_session_sleep(int64_t millis, int32_t nanos)
{
    init_wait_conditions();
    struct timespec at = deadline(millis, nanos);
    pthread_mutex_lock(&lock);
    sleepers++;
    while (cleanup || !atomic_load(&stopping)) {
        if (wait_until(&stopped, &at)) break;
    }
    bool ok = cleanup || !atomic_load(&stopping);
    sleepers--; notify_drained();
    pthread_mutex_unlock(&lock);
    return ok;
}
bool tl_session_restart(void)
{
    pthread_mutex_lock(&lock);
    bool ok = drained() && make_stop_pipe();
    if (ok) {
        if (stop_pipe[0] >= 0) {
            char bytes[64]; ssize_t n;
            do { n = read(stop_pipe[0], bytes, sizeof(bytes)); } while (n > 0 || (n < 0 && errno == EINTR));
        }
        atomic_store(&stopping, false);
    }
    pthread_mutex_unlock(&lock);
    return ok;
}
