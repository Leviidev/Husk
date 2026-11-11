/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "husk-tl-frame-sync.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* An asynchronous command-queue driver, not immediate GL mocks: producer
 * writes and consumer reads execute only when the scheduler advances them.
 * Run producers first to expose a missing ready/done dependency. */
enum { STREAMS = 3, LIMIT = 4096 };
typedef struct { bool live, signalled; unsigned id; } fence;
typedef struct { char kind; fence *sync; int slot, value; } command;
typedef struct { command queue[LIMIT]; unsigned count, flushed, done; } stream;
static stream streams[STREAMS];
static fence fences[LIMIT];
static unsigned nfences, live_fences, dropped, flushes, finishes, waits, reads;
static int current, texture[3];
static bool fail_fence;

static void queue(char kind, fence *sync, int slot, int value)
{
    stream *s = &streams[current];
    assert(s->count < LIMIT);
    s->queue[s->count++] = (command){ kind, sync, slot, value };
}
static bool step(int which)
{
    stream *s = &streams[which];
    if (s->done == s->flushed) return false;
    command *c = &s->queue[s->done];
    if (c->kind == 'w' && !c->sync->signalled) return false;
    if (c->kind == 'f') c->sync->signalled = true;
    if (c->kind == 'W') texture[c->slot] = c->value;
    if (c->kind == 'R') { assert(texture[c->slot] == c->value); reads++; }
    s->done++;
    return true;
}
static void drain(void)
{
    while (true) {
        bool pending = false, progress = false;
        /* Both possible producer streams precede the compositor stream. */
        for (int i = 0; i < STREAMS; i++) {
            while (step(i)) progress = true;
            pending |= streams[i].done != streams[i].flushed;
        }
        if (!pending) return;
        assert(progress && "GPU waits must not form a dependency cycle");
    }
}
static void *make_fence(void)
{
    if (fail_fence) return NULL;
    assert(nfences < LIMIT);
    fence *f = &fences[nfences++];
    *f = (fence){ true, false, nfences };
    live_fences++;
    queue('f', f, 0, 0);
    return f;
}
static void wait_fence(void *sync)
{
    fence *f = sync;
    assert(f && f->live);
    /* Return immediately. The queue retains the fence after GL deletion. */
    queue('w', f, 0, 0);
    waits++;
}
static void drop_fence(void *sync)
{
    fence *f = sync;
    assert(f && f->live);
    f->live = false;
    live_fences--; dropped++;
}
static void flush(void) { streams[current].flushed = streams[current].count; flushes++; }
static void finish(void) { flush(); finishes++; drain(); }
static const tl_frame_sync_ops ops = { make_fence, wait_fence, drop_fence, flush, finish };

static void init(void)
{
    memset(streams, 0, sizeof(streams));
    memset(fences, 0, sizeof(fences));
    memset(texture, 0, sizeof(texture));
    nfences = live_fences = dropped = flushes = finishes = waits = reads = 0;
    fail_fence = false; current = 0;
}
static void write_frame(tl_frame_sync *frame, int slot, int value, bool asynchronous)
{
    tl_frame_sync_before_write(frame, &ops);
    queue('W', NULL, slot, value);
    tl_frame_sync_after_write(frame, &ops, asynchronous);
}
static void read_frame(tl_frame_sync *frame, int slot, int value)
{
    tl_frame_sync_before_read(frame, &ops);
    queue('R', NULL, slot, value);
    tl_frame_sync_after_read(frame, &ops);
}

int main(void)
{
    tl_frame_sync frame = { 0 };
    init();
    write_frame(&frame, 0, 7, true);
    current = 2; read_frame(&frame, 0, 7);
    /* Run the consumer before the producer gets GPU time. Its ready wait
     * must stop sampling uninitialized pixels without blocking the CPU. */
    assert(!step(2));
    drain();
    assert(reads == 1 && finishes == 0);
    tl_frame_sync_reset(&frame, &ops);
    assert(!live_fences);

    init();
    write_frame(&frame, 0, 1, true);
    assert(tl_frame_sync_pending(&frame));
    current = 2; read_frame(&frame, 0, 1);
    current = 0; write_frame(&frame, 0, 2, true);
    drain();
    assert(reads == 1 && texture[0] == 2 && finishes == 0);
    assert(waits == 3); /* ready before read, ready + sampled before rewrite */
    tl_frame_sync_reset(&frame, &ops);
    assert(!live_fences && !tl_frame_sync_pending(&frame));
    tl_frame_sync_reset(&frame, &ops); /* idempotent */

    init(); memset(&frame, 0, sizeof(frame));
    write_frame(&frame, 0, 10, true);
    current = 2; read_frame(&frame, 0, 10); read_frame(&frame, 0, 10);
    /* A second sample replaces the prior done fence, covering both reads. */
    assert(live_fences == 2);
    current = 1; write_frame(&frame, 0, 11, true);
    drain();
    assert(reads == 2 && texture[0] == 11 && finishes == 0);
    tl_frame_sync_reset(&frame, &ops);
    assert(!live_fences);

    init(); memset(&frame, 0, sizeof(frame));
    /* No consumer ever presents this frame. Switching producer streams must
     * still wait for the earlier queued copy before replacing its contents. */
    current = 1; write_frame(&frame, 0, 20, true);
    current = 0; write_frame(&frame, 0, 21, true);
    drain();
    assert(texture[0] == 21 && waits == 1 && finishes == 0);
    tl_frame_sync_reset(&frame, &ops);
    assert(!live_fences);

    init(); memset(&frame, 0, sizeof(frame));
    write_frame(&frame, 0, 30, false);
    assert(texture[0] == 30 && finishes == 1 && !tl_frame_sync_pending(&frame));
    current = 2; read_frame(&frame, 0, 30);
    assert(reads == 1 && finishes == 2 && !live_fences);

    init(); memset(&frame, 0, sizeof(frame));
    fail_fence = true;
    write_frame(&frame, 0, 40, true);
    assert(texture[0] == 40 && finishes == 1 && !frame.asynchronous);
    current = 2; read_frame(&frame, 0, 40);
    assert(reads == 1 && finishes == 2 && !live_fences);

    init(); memset(&frame, 0, sizeof(frame));
    write_frame(&frame, 0, 50, true);
    current = 2; fail_fence = true; read_frame(&frame, 0, 50);
    assert(reads == 1 && finishes == 1 && !frame.sampled);
    current = 0; fail_fence = false; write_frame(&frame, 0, 51, true);
    drain();
    assert(texture[0] == 51);
    tl_frame_sync_reset(&frame, &ops);
    assert(!live_fences);

    init();
    tl_frame_sync ring[3] = { 0 };
    for (int i = 1; i <= 192; i++) {
        int slot = i % 3;
        current = i % 2;
        write_frame(&ring[slot], slot, i, true);
        if (i % 4) { current = 2; read_frame(&ring[slot], slot, i); }
        assert(live_fences <= 6);
        if (i % 9 == 0) drain();
    }
    drain();
    for (int i = 0; i < 3; i++) tl_frame_sync_reset(&ring[i], &ops);
    assert(!live_fences && finishes == 0);

    init(); memset(&frame, 0, sizeof(frame));
    write_frame(&frame, 0, 55, true);
    current = 2; read_frame(&frame, 0, 55);
    tl_frame_sync_reset(&frame, &ops); /* commands still retain deleted syncs */
    assert(!live_fences);
    drain();
    assert(reads == 1 && finishes == 0);

    init(); memset(&frame, 0, sizeof(frame));
    write_frame(&frame, 0, 60, true);
    current = 2; read_frame(&frame, 0, 60);
    unsigned before_drop = dropped, before_wait = waits, before_finish = finishes;
    tl_frame_sync_abandon(&frame); /* invalid/lost share group: no GL calls */
    assert(!tl_frame_sync_pending(&frame) && !frame.asynchronous);
    assert(dropped == before_drop && waits == before_wait && finishes == before_finish);
    /* The driver destroys its old share group, not a new context's objects. */
    puts("PASS: asynchronous frame queues order copy/sample/reuse, producer changes, repeat/skip frames, fallback, fence failures and context loss without ES3 CPU finishes");
    return 0;
}
