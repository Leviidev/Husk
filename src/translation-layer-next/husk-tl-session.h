/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef HUSK_TL_SESSION_H
#define HUSK_TL_SESSION_H
#include <stdbool.h>
#include <poll.h>
#include <stdint.h>

/* Cooperative Java shutdown. Never discard DEX while a call or worker still owns it. */
bool tl_session_enter(void);
void tl_session_leave(void);
bool tl_session_worker_add(void);
void tl_session_worker_done(void);
bool tl_session_stopping(void);
void tl_session_request_stop(void);
bool tl_session_drained(void);
bool tl_session_restart(void);
/* Set at engine launch: only Java sessions expose cooperative close. */
void tl_session_set_cancellable(bool enabled);
/* Wake only on stop, completion, or the caller's original deadline. A negative timeout waits indefinitely. */
bool tl_session_wait_drained(int timeout_ms);
bool tl_session_sleep(int64_t millis, int32_t nanos);
/* Registration and removal must happen outside any mutex acquired by cancel.
 * cancel runs synchronously at stop under the session lock; it must not call session APIs. */
typedef struct tl_session_wait {
    bool registered;
    void (*cancel)(void *);
    void *context;
    struct tl_session_wait *next;
} tl_session_wait;
bool tl_session_wait_begin(tl_session_wait *wait, void (*cancel)(void *), void *context);
void tl_session_wait_end(tl_session_wait *wait);
/* Only the teardown thread may run framework reset code while entry is closed. */
void tl_session_allow_cleanup(bool allow);
/* Pin only sockets currently in a blocking guest call. Stop shuts down the pin,
 * never closes an arbitrary descriptor which another thread may have reused. */
typedef struct tl_session_io { int socket; bool registered; struct tl_session_io *next; } tl_session_io;
bool tl_session_io_begin(tl_session_io *io, int fd);
void tl_session_io_end(tl_session_io *io);
int tl_session_poll(struct pollfd *fds, nfds_t count, int timeout_ms);
void tl_session_counts(unsigned *active_calls, unsigned *active_workers);
#endif
