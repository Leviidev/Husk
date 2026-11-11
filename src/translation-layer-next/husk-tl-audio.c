/* SPDX-License-Identifier: GPL-2.0-or-later */
#define _DARWIN_C_SOURCE
#include "husk-tl-audio.h"

#include <AudioToolbox/AudioToolbox.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "husk-tl-bionic.h"
#include "husk-tl-cocos.h"

#define NBUF 3
#define FRAMES_PER_BUF 1024
#define RING_FRAMES 8192

static struct {
    pthread_mutex_t mu;
    pthread_cond_t cv;
    AudioQueueRef q;
    AudioQueueBufferRef bufs[NBUF];
    int rate, channels;
    int16_t *ring;
    size_t head, count;           /* frames */
    bool paused, failed, closing;
} A = { .mu = PTHREAD_MUTEX_INITIALIZER, .cv = PTHREAD_COND_INITIALIZER };

/* The audio thread asks for samples; an empty ring is silence, never a wait. */
static void on_buffer(void *user, AudioQueueRef q, AudioQueueBufferRef b)
{
    (void)user;
    int16_t *out = b->mAudioData;
    size_t want = FRAMES_PER_BUF, ch = (size_t)A.channels;
    pthread_mutex_lock(&A.mu);
    size_t take = A.paused ? 0 : (A.count < want ? A.count : want);
    for (size_t i = 0; i < take; i++)
        for (size_t c = 0; c < ch; c++) out[i * ch + c] = A.ring[((A.head + i) % RING_FRAMES) * ch + c];
    A.head = (A.head + take) % RING_FRAMES;
    A.count -= take;
    pthread_cond_broadcast(&A.cv);
    pthread_mutex_unlock(&A.mu);
    if (take < want) memset(out + take * ch, 0, (want - take) * ch * sizeof(int16_t));
    b->mAudioDataByteSize = (UInt32)(want * ch * sizeof(int16_t));
    AudioQueueEnqueueBuffer(q, b, 0, NULL);
}

static bool open_queue(int rate, int channels)
{
    AudioStreamBasicDescription f = { 0 };
    f.mSampleRate = rate; f.mFormatID = kAudioFormatLinearPCM;
    f.mFormatFlags = kLinearPCMFormatFlagIsSignedInteger | kLinearPCMFormatFlagIsPacked;
    f.mBitsPerChannel = 16; f.mChannelsPerFrame = (UInt32)channels; f.mFramesPerPacket = 1;
    f.mBytesPerFrame = f.mBytesPerPacket = (UInt32)(channels * 2);
    OSStatus st = AudioQueueNewOutput(&f, on_buffer, NULL, NULL, NULL, 0, &A.q);
    if (st != noErr) { tl_log_line("audio: AudioQueueNewOutput failed (%d)", (int)st); return false; }
    A.ring = calloc((size_t)RING_FRAMES * (size_t)channels, sizeof(int16_t));
    A.rate = rate; A.channels = channels;
    for (int i = 0; i < NBUF; i++) {
        AudioQueueAllocateBuffer(A.q, FRAMES_PER_BUF * (UInt32)channels * 2, &A.bufs[i]);
        memset(A.bufs[i]->mAudioData, 0, FRAMES_PER_BUF * (size_t)channels * 2);
        A.bufs[i]->mAudioDataByteSize = FRAMES_PER_BUF * (UInt32)channels * 2;
        AudioQueueEnqueueBuffer(A.q, A.bufs[i], 0, NULL);
    }
    st = AudioQueueStart(A.q, NULL);
    if (st != noErr) { tl_log_line("audio: AudioQueueStart failed (%d)", (int)st); return false; }
    tl_log_line("audio: output running, %d Hz, %d channel(s)", rate, channels);
    return true;
}

static void sleep_for(int frames, int rate)
{
    struct timespec ts = { 0, (long)((double)frames * 1e9 / rate) };
    nanosleep(&ts, NULL);
}

static void ring_write(const int16_t *samples, int frames, int channels, int rate);

/*
 * The mixer's write. A game mixing at a low rate (Unity's FMOD runs at 24 kHz) is played at twice that: each frame, then the
 * point halfway to the next. The iPhone played Subway Surfers' 24 kHz output high-pitched and broken while the same samples
 * were right on a Mac, and every game that sounds right there runs at 44.1 or 48 kHz -- so the queue is only ever opened
 * at one of those.
 */
static void host_write(const int16_t *samples, int frames, int channels, int rate)
{
    if (rate > 0 && rate * 2 <= 48000 && channels > 0 && channels <= 2 && frames > 0) {
        static int16_t prev[2];
        static bool have_prev;
        int16_t *up = malloc((size_t)frames * 2 * (size_t)channels * sizeof(int16_t));
        if (up) {
            for (int f = 0; f < frames; f++)
                for (int c = 0; c < channels; c++) {
                    int16_t cur = samples[(size_t)f * channels + c];
                    int16_t before = f > 0 ? samples[(size_t)(f - 1) * channels + c] : (have_prev ? prev[c] : cur);
                    up[(size_t)(2 * f) * channels + c] = (int16_t)(((int)before + cur) / 2);
                    up[(size_t)(2 * f + 1) * channels + c] = cur;
                }
            for (int c = 0; c < channels; c++) prev[c] = samples[(size_t)(frames - 1) * channels + c];
            have_prev = true;
            ring_write(up, frames * 2, channels, rate * 2);
            free(up);
            return;
        }
    }
    ring_write(samples, frames, channels, rate);
}

/* Copy into the ring, blocking while it is full (that is the pacing). */
static void ring_write(const int16_t *samples, int frames, int channels, int rate)
{
    pthread_mutex_lock(&A.mu);
    if (A.closing) { pthread_mutex_unlock(&A.mu); sleep_for(frames, rate); return; }
    if (!A.q && !A.failed) { if (!open_queue(rate, channels)) A.failed = true; }
    if (A.failed || channels != A.channels) { pthread_mutex_unlock(&A.mu); sleep_for(frames, rate); return; }
    int done = 0;
    while (done < frames) {
        while (!A.closing && (A.count >= RING_FRAMES || A.paused)) pthread_cond_wait(&A.cv, &A.mu);
        if (A.closing) { pthread_mutex_unlock(&A.mu); sleep_for(frames - done, rate); return; }
        size_t room = RING_FRAMES - A.count, n = (size_t)(frames - done) < room ? (size_t)(frames - done) : room;
        size_t tail = (A.head + A.count) % RING_FRAMES;
        for (size_t i = 0; i < n; i++)
            memcpy(&A.ring[((tail + i) % RING_FRAMES) * (size_t)channels], &samples[(size_t)(done + (int)i) * (size_t)channels], (size_t)channels * sizeof(int16_t));
        A.count += n;
        done += (int)n;
    }
    pthread_mutex_unlock(&A.mu);
}

/* TL_AUDIO_MUTE: pace like a device but make no sound (for test runs on a Mac). TL_AUDIO_STATS=1 also logs, about once a second,
 * how loud the game's output was, so a silent run still shows whether the game is making sound. */
static void muted_write(const int16_t *samples, int frames, int channels, int rate)
{
    static int stats = -1, seen, peak;
    static long loud, total;
    if (stats < 0) stats = getenv("TL_AUDIO_STATS") != NULL;
    if (stats) {
        for (int i = 0; i < frames * channels; i++) {
            int v = samples[i] < 0 ? -samples[i] : samples[i];
            if (v > peak) peak = v;
            if (v > 64) loud++;
        }
        total += frames * channels;
        if ((seen += frames) >= rate) {
            tl_log_line("audio: %d Hz x %d, peak %d, %.0f%% of samples audible", rate, channels, peak, 100.0 * loud / (total ? total : 1));
            seen = 0; peak = 0; loud = 0; total = 0;
        }
    }
    sleep_for(frames, rate);
}

void tl_audio_install(void) { tl_cocos_audio_hook = getenv("TL_AUDIO_MUTE") ? muted_write : host_write; }

void tl_audio_set_paused(bool paused)
{
    pthread_mutex_lock(&A.mu);
    A.paused = paused;
    if (A.q) { if (paused) AudioQueuePause(A.q); else AudioQueueStart(A.q, NULL); }
    pthread_cond_broadcast(&A.cv);
    pthread_mutex_unlock(&A.mu);
}

void tl_audio_reset(void)
{
    /* Dispose waits for AudioQueue's own callbacks. Never hold A.mu while
       waiting: on_buffer takes it before returning. Guest writers are joined. */
    if (A.q) AudioQueueDispose(A.q, true);
    pthread_mutex_lock(&A.mu);
    A.q = NULL;
    free(A.ring); A.ring = NULL;
    memset(A.bufs, 0, sizeof(A.bufs));
    A.head = 0; A.count = 0; A.rate = 0; A.channels = 0;
    A.paused = false; A.failed = false; A.closing = false;
    pthread_cond_broadcast(&A.cv);
    pthread_mutex_unlock(&A.mu);
}

void tl_audio_begin_close(void)
{
    pthread_mutex_lock(&A.mu);
    A.closing = true;
    pthread_cond_broadcast(&A.cv);
    pthread_mutex_unlock(&A.mu);
}
