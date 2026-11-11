/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef HUSK_TL_FRAME_SYNC_H
#define HUSK_TL_FRAME_SYNC_H
#include <stdbool.h>
#include <stddef.h>

/* A shared frame texture has two command streams: the guest copies into it,
 * and the compositor samples it. CPU publication and glFlush alone do not
 * order these GPU operations. The caller serializes submission/ownership with
 * its handoff mutex; all waits below are GPU server waits, not CPU waits. */
typedef struct {
    void *ready;
    void *sampled;
    bool asynchronous;
} tl_frame_sync;

typedef struct {
    void *(*fence)(void);
    void (*wait)(void *);
    void (*drop)(void *);
    void (*flush)(void);
    void (*finish)(void);
} tl_frame_sync_ops;

static inline void tl_frame_sync_before_write(tl_frame_sync *frame, const tl_frame_sync_ops *ops)
{
    /* The surface may move to another shared producer context. Its previous
     * copy can still be queued even if the UI never sampled that frame. */
    if (frame->ready) {
        ops->wait(frame->ready);
        ops->drop(frame->ready);
        frame->ready = NULL;
    }
    if (frame->sampled) {
        ops->wait(frame->sampled);
        ops->drop(frame->sampled);
        frame->sampled = NULL;
    }
}

static inline bool tl_frame_sync_pending(const tl_frame_sync *frame)
{
    return frame->ready || frame->sampled;
}

/* On context loss the old share group owns any remaining driver resources.
 * Forget CPU handles so a new group's numeric texture IDs cannot find them. */
static inline void tl_frame_sync_abandon(tl_frame_sync *frame)
{
    frame->ready = frame->sampled = NULL;
    frame->asynchronous = false;
}

static inline void tl_frame_sync_after_write(tl_frame_sync *frame, const tl_frame_sync_ops *ops, bool asynchronous)
{
    if (frame->ready) ops->drop(frame->ready);
    frame->ready = asynchronous ? ops->fence() : NULL;
    frame->asynchronous = frame->ready != NULL;
    /* A failed fence must not publish unfinished work. */
    if (frame->ready) ops->flush();
    else ops->finish();
}

static inline void tl_frame_sync_before_read(tl_frame_sync *frame, const tl_frame_sync_ops *ops)
{
    if (frame->ready) ops->wait(frame->ready);
}

static inline void tl_frame_sync_after_read(tl_frame_sync *frame, const tl_frame_sync_ops *ops)
{
    /* A newer fence on this same consumer stream also covers its earlier
     * reads, including a second presentation of the same texture. */
    if (frame->sampled) ops->drop(frame->sampled);
    frame->sampled = frame->asynchronous ? ops->fence() : NULL;
    if (frame->sampled) ops->flush();
    else ops->finish();
}

static inline void tl_frame_sync_reset(tl_frame_sync *frame, const tl_frame_sync_ops *ops)
{
    if (frame->ready) ops->drop(frame->ready);
    if (frame->sampled) ops->drop(frame->sampled);
    frame->ready = frame->sampled = NULL;
    frame->asynchronous = false;
}
#endif
