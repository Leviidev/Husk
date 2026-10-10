/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Sound for Java apps on Husk's Dalvik runtime (husk.Audio's natives): AudioTrack, SoundPool and MediaPlayer all play here.
 *
 * One mixer thread adds everything that plays -- streams (an AudioTrack, a MediaPlayer's decoded music: PCM the app writes, kept in
 * a ring at 44.1 kHz stereo) and voices (SoundPool's decoded samples, played by id, looped, pitched) -- a block at a time, and hands
 * the block to the speaker output the native runtime's games use (tl_cocos_audio_hook), which blocks while the speakers are full:
 * that paces the mixer, and the mixer's rings pace the app's writes, as AudioTrack.write blocks on Android.
 *
 * Decoding: Ogg Vorbis through stb_vorbis (iOS has no Vorbis decoder), everything else (MP3, AAC/M4A, WAV, CAF, FLAC) through
 * AudioToolbox's ExtAudioFile, from memory. Music is decoded as it plays; SoundPool samples once, when loaded.
 */
#define _DARWIN_C_SOURCE
#include <AudioToolbox/AudioToolbox.h>
#include <math.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

#include <sys/stat.h>
#include <time.h>

#include "husk-tl-dvm-internal.h"
#include "husk-tl-internal.h"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Weverything"
#define STB_VORBIS_HEADER_ONLY
#define STB_VORBIS_NO_STDIO
#define STB_VORBIS_NO_PUSHDATA_API
#include "../translation-layer/stb_vorbis.inc"
#pragma clang diagnostic pop

void tl_log_line(const char *fmt, ...);
extern void (*tl_cocos_audio_hook)(const int16_t *samples, int frames, int channels, int rate);
void tl_audio_install(void);

#define OUT_RATE 44100
#define BLOCK 512
#define MAX_STREAMS 32
#define MAX_SAMPLES 1024
#define MAX_VOICES 64
#define RING (OUT_RATE / 4)                         /* a quarter second per stream */

typedef struct {
    bool used, playing;
    int rate, channels;
    float l, r;
    int16_t *ring; size_t head, count;              /* stereo frames at OUT_RATE */
    double pos; int16_t prev[2];                    /* resampling: where between the last input frame and the next */
    uint64_t consumed;                              /* output frames mixed */
    uint64_t written_in;                            /* input frames written */
} stream;

typedef struct { int16_t *pcm; int frames, channels, rate; bool used; } sample;
typedef struct { bool used, paused; int sample, id, loops; double pos, step; float l, r; } voice;

static struct {
    pthread_mutex_t mu; pthread_cond_t room, work;
    pthread_t thread; bool started;
    stream s[MAX_STREAMS];
    sample smp[MAX_SAMPLES];
    voice v[MAX_VOICES];
    int next_voice;
} M = { .mu = PTHREAD_MUTEX_INITIALIZER, .room = PTHREAD_COND_INITIALIZER, .work = PTHREAD_COND_INITIALIZER, .next_voice = 1 };

static inline int16_t clamp16(int v) { return (int16_t)(v > 32767 ? 32767 : v < -32768 ? -32768 : v); }

static bool active_locked(void)
{
    for (int i = 0; i < MAX_STREAMS; i++) if (M.s[i].used && M.s[i].playing && M.s[i].count) return true;
    for (int i = 0; i < MAX_VOICES; i++) if (M.v[i].used && !M.v[i].paused) return true;
    return false;
}

static void *mixer_main(void *arg)
{
    (void)arg;
    pthread_setname_np("husk-mixer");
    static int32_t acc[BLOCK * 2];
    static int16_t out[BLOCK * 2];
    for (;;) {
        pthread_mutex_lock(&M.mu);
        while (!active_locked()) pthread_cond_wait(&M.work, &M.mu);
        memset(acc, 0, sizeof(acc));
        for (int i = 0; i < MAX_STREAMS; i++) {
            stream *s = &M.s[i];
            if (!s->used || !s->playing) continue;
            int n = s->count < BLOCK ? (int)s->count : BLOCK;
            for (int f = 0; f < n; f++) {
                const int16_t *p = &s->ring[((s->head + (size_t)f) % RING) * 2];
                acc[2 * f] += (int32_t)(p[0] * s->l); acc[2 * f + 1] += (int32_t)(p[1] * s->r);
            }
            s->head = (s->head + (size_t)n) % RING; s->count -= (size_t)n; s->consumed += (uint64_t)n;
        }
        for (int i = 0; i < MAX_VOICES; i++) {
            voice *v = &M.v[i];
            if (!v->used || v->paused) continue;
            sample *sm = &M.smp[v->sample];
            if (!sm->used || !sm->frames) { v->used = false; continue; }
            for (int f = 0; f < BLOCK; f++) {
                int k = (int)v->pos;
                if (k >= sm->frames) {
                    if (v->loops != 0) { if (v->loops > 0) v->loops--; v->pos -= sm->frames; k = (int)v->pos; if (k >= sm->frames) k = 0; }
                    else { v->used = false; break; }
                }
                int16_t a, b;
                if (sm->channels == 2) { a = sm->pcm[2 * k]; b = sm->pcm[2 * k + 1]; } else a = b = sm->pcm[k];
                acc[2 * f] += (int32_t)(a * v->l); acc[2 * f + 1] += (int32_t)(b * v->r);
                v->pos += v->step;
            }
        }
        pthread_cond_broadcast(&M.room);
        pthread_mutex_unlock(&M.mu);
        for (int i = 0; i < BLOCK * 2; i++) out[i] = clamp16(acc[i]);
        { static FILE *dump; static int tried; if (!tried) { tried = 1; const char *p = getenv("TL_AUDIO_DUMP"); if (p) dump = fopen(p, "wb"); } if (dump) fwrite(out, 2, BLOCK * 2, dump); }
        if (tl_cocos_audio_hook) tl_cocos_audio_hook(out, BLOCK, 2, OUT_RATE);
        else usleep(BLOCK * 1000000 / OUT_RATE);
    }
    return NULL;
}

static void ensure_started_locked(void)
{
    if (M.started) return;
    M.started = true;
    if (!tl_cocos_audio_hook) tl_audio_install();
    pthread_attr_t at; pthread_attr_init(&at);
    pthread_create(&M.thread, &at, mixer_main, NULL);
}

/* ------------------------------------------------------------------ streams */

static int stream_open(int rate, int channels)
{
    pthread_mutex_lock(&M.mu);
    ensure_started_locked();
    int id = -1;
    for (int i = 0; i < MAX_STREAMS && id < 0; i++) if (!M.s[i].used) id = i;
    if (id >= 0) {
        stream *s = &M.s[id];
        int16_t *ring = s->ring ? s->ring : malloc(RING * 2 * sizeof(int16_t));
        memset(s, 0, sizeof(*s));
        s->ring = ring; s->used = true; s->rate = rate > 0 ? rate : OUT_RATE; s->channels = channels == 1 ? 1 : 2; s->l = s->r = 1.0f;
    }
    pthread_mutex_unlock(&M.mu);
    return id;
}

/* Write interleaved frames (16-bit), converting to the mixer's rate and stereo. Blocks while the ring is full when asked. */
static int stream_write(int id, const int16_t *in, int frames, bool blocking)
{
    if (id < 0 || id >= MAX_STREAMS || frames <= 0) return 0;
    pthread_mutex_lock(&M.mu);
    stream *s = &M.s[id];
    if (!s->used) { pthread_mutex_unlock(&M.mu); return 0; }
    double step = (double)s->rate / OUT_RATE;
    int done = 0;
    while (done < frames) {
        while (s->used && s->count >= RING - 1) {
            if (!blocking) goto out;
            if (!s->playing) {
                /* a paused track that is full: Android's write blocks; wait (the app plays or flushes it from another thread) */
                struct timespec ts; clock_gettime(CLOCK_REALTIME, &ts); ts.tv_nsec += 50 * 1000000; if (ts.tv_nsec >= 1000000000) { ts.tv_sec++; ts.tv_nsec -= 1000000000; }
                pthread_cond_timedwait(&M.room, &M.mu, &ts);
            } else pthread_cond_wait(&M.room, &M.mu);
        }
        if (!s->used) break;
        /* produce output frames while there is input: linear interpolation between prev and the current input frame */
        while (done < frames && s->count < RING - 1) {
            const int16_t *cur = &in[(size_t)done * (size_t)s->channels];
            int16_t c0 = cur[0], c1 = s->channels == 2 ? cur[1] : cur[0];
            while (s->pos < 1.0 && s->count < RING - 1) {
                double t = s->pos;
                int16_t *o = &s->ring[((s->head + s->count) % RING) * 2];
                o[0] = (int16_t)(s->prev[0] + (c0 - s->prev[0]) * t);
                o[1] = (int16_t)(s->prev[1] + (c1 - s->prev[1]) * t);
                s->count++;
                s->pos += step;
            }
            if (s->pos >= 1.0) { s->pos -= 1.0; s->prev[0] = c0; s->prev[1] = c1; done++; s->written_in++; }
        }
        if (s->playing) pthread_cond_signal(&M.work);
    }
out:
    if (s->playing && s->count) pthread_cond_signal(&M.work);
    pthread_mutex_unlock(&M.mu);
    return done;
}

/* ------------------------------------------------------------------ decoding */

typedef struct {
    stb_vorbis *ogg;
    ExtAudioFileRef ext; AudioFileID af;
    uint8_t *data; size_t len;
    int rate, channels; int64_t frames;
} decoder;

static OSStatus mem_read(void *u, SInt64 pos, UInt32 n, void *buf, UInt32 *got)
{
    decoder *d = u;
    if (pos >= (SInt64)d->len) { *got = 0; return noErr; }
    size_t k = (size_t)pos + n > d->len ? d->len - (size_t)pos : n;
    memcpy(buf, d->data + pos, k); *got = (UInt32)k;
    return noErr;
}
static SInt64 mem_size(void *u) { return (SInt64)((decoder *)u)->len; }

static decoder *dec_open(uint8_t *data, size_t len)
{
    decoder *d = calloc(1, sizeof(*d));
    d->data = data; d->len = len;
    if (len > 4 && !memcmp(data, "OggS", 4)) {
        int err = 0;
        d->ogg = stb_vorbis_open_memory(data, (int)len, &err, NULL);
        if (d->ogg) {
            stb_vorbis_info i = stb_vorbis_get_info(d->ogg);
            d->rate = (int)i.sample_rate; d->channels = i.channels > 2 ? 2 : i.channels;
            d->frames = stb_vorbis_stream_length_in_samples(d->ogg);
            if (i.channels > 2) d->channels = 2;
            return d;
        }
        tl_log_line("audio: an Ogg file stb_vorbis cannot read (error %d)", err);
        free(d); return NULL;
    }
    OSStatus st = AudioFileOpenWithCallbacks(d, mem_read, NULL, mem_size, NULL, 0, &d->af);
    if (st == noErr) st = ExtAudioFileWrapAudioFileID(d->af, false, &d->ext);
    if (st != noErr) { if (d->af) AudioFileClose(d->af); tl_log_line("audio: AudioToolbox cannot read this file (%d)", (int)st); free(d); return NULL; }
    AudioStreamBasicDescription in; UInt32 sz = sizeof(in);
    ExtAudioFileGetProperty(d->ext, kExtAudioFileProperty_FileDataFormat, &sz, &in);
    d->rate = (int)in.mSampleRate; d->channels = in.mChannelsPerFrame >= 2 ? 2 : 1;
    AudioStreamBasicDescription out = { .mSampleRate = in.mSampleRate, .mFormatID = kAudioFormatLinearPCM,
        .mFormatFlags = kLinearPCMFormatFlagIsSignedInteger | kLinearPCMFormatFlagIsPacked, .mBytesPerPacket = (UInt32)(2 * d->channels),
        .mFramesPerPacket = 1, .mBytesPerFrame = (UInt32)(2 * d->channels), .mChannelsPerFrame = (UInt32)d->channels, .mBitsPerChannel = 16 };
    ExtAudioFileSetProperty(d->ext, kExtAudioFileProperty_ClientDataFormat, sizeof(out), &out);
    SInt64 fr = 0; sz = sizeof(fr);
    ExtAudioFileGetProperty(d->ext, kExtAudioFileProperty_FileLengthFrames, &sz, &fr);
    d->frames = fr;
    return d;
}
static int dec_read(decoder *d, int16_t *buf, int frames)
{
    if (d->ogg) return stb_vorbis_get_samples_short_interleaved(d->ogg, d->channels, buf, frames * d->channels);
    AudioBufferList bl = { .mNumberBuffers = 1, .mBuffers = { { .mNumberChannels = (UInt32)d->channels, .mDataByteSize = (UInt32)(frames * 2 * d->channels), .mData = buf } } };
    UInt32 n = (UInt32)frames;
    if (ExtAudioFileRead(d->ext, &n, &bl) != noErr) return 0;
    return (int)n;
}
static void dec_seek(decoder *d, int64_t frame)
{
    if (d->ogg) stb_vorbis_seek(d->ogg, (unsigned)frame);
    else ExtAudioFileSeek(d->ext, frame);
}
static void dec_close(decoder *d)
{
    if (!d) return;
    if (d->ogg) stb_vorbis_close(d->ogg);
    if (d->ext) ExtAudioFileDispose(d->ext);
    if (d->af) AudioFileClose(d->af);
    free(d->data);
    free(d);
}

/* ------------------------------------------------------------------ natives (husk.Audio) */

#define NAT(fn) static bool fn(jobj *self, const jvalue *a, jvalue *ret)
static jvalue I(int32_t i) { jvalue v; v.j = (uint32_t)i; return v; }
static jvalue J(int64_t j) { jvalue v; v.j = j; return v; }
static stream *S(int id) { return id >= 0 && id < MAX_STREAMS && M.s[id].used ? &M.s[id] : NULL; }

NAT(A_trackOpen) { (void)self; *ret = I(stream_open(a[0].i, a[1].i)); return true; }
NAT(A_trackWrite)
{
    (void)self;
    jobj *arr = a[1].l; int off = a[2].i, n = a[3].i;
    stream *s = S(a[0].i);
    if (!s || !arr || off < 0 || n <= 0 || (uint32_t)(off + n) > arr->arr.len) { *ret = I(0); return true; }
    int ch = s->channels;
    int w = stream_write(a[0].i, (const int16_t *)arr->arr.data + off, n / ch, a[4].z);
    *ret = I(w * ch);
    return true;
}
NAT(A_trackWriteBytes)
{
    (void)self;
    jobj *arr = a[1].l; int off = a[2].i, n = a[3].i, fmt = a[4].i;
    stream *s = S(a[0].i);
    if (!s || !arr || off < 0 || n <= 0 || (uint32_t)(off + n) > arr->arr.len) { *ret = I(0); return true; }
    const uint8_t *src = (const uint8_t *)arr->arr.data + off;
    int ch = s->channels, w;
    if (fmt == 3) {                                                /* 8-bit unsigned */
        int16_t *tmp = malloc((size_t)n * 2);
        for (int i = 0; i < n; i++) tmp[i] = (int16_t)((src[i] - 128) << 8);
        w = stream_write(a[0].i, tmp, n / ch, a[5].z) * ch;
        free(tmp);
    } else if (fmt == 4) {                                         /* float */
        int k = n / 4; int16_t *tmp = malloc((size_t)k * 2); const float *f = (const float *)src;
        for (int i = 0; i < k; i++) tmp[i] = clamp16((int)(f[i] * 32767.0f));
        w = stream_write(a[0].i, tmp, k / ch, a[5].z) * ch * 4;
        free(tmp);
    } else w = stream_write(a[0].i, (const int16_t *)src, n / 2 / ch, a[5].z) * ch * 2;
    *ret = I(w);
    return true;
}
NAT(A_trackWriteFloat)
{
    (void)self;
    jobj *arr = a[1].l; int off = a[2].i, n = a[3].i;
    stream *s = S(a[0].i);
    if (!s || !arr || off < 0 || n <= 0 || (uint32_t)(off + n) > arr->arr.len) { *ret = I(0); return true; }
    const float *f = (const float *)arr->arr.data + off;
    int16_t *tmp = malloc((size_t)n * 2);
    for (int i = 0; i < n; i++) tmp[i] = clamp16((int)(f[i] * 32767.0f));
    int ch = s->channels;
    *ret = I(stream_write(a[0].i, tmp, n / ch, a[4].z) * ch);
    free(tmp);
    return true;
}
NAT(A_trackPlay) { (void)self; (void)ret; pthread_mutex_lock(&M.mu); stream *s = S(a[0].i); if (s) { s->playing = a[1].z; pthread_cond_broadcast(&M.room); if (s->playing) pthread_cond_signal(&M.work); } pthread_mutex_unlock(&M.mu); return true; }
NAT(A_trackFlush) { (void)self; (void)ret; pthread_mutex_lock(&M.mu); stream *s = S(a[0].i); if (s) { s->count = 0; pthread_cond_broadcast(&M.room); } pthread_mutex_unlock(&M.mu); return true; }
NAT(A_trackVolume) { (void)self; (void)ret; pthread_mutex_lock(&M.mu); stream *s = S(a[0].i); if (s) { s->l = a[1].f; s->r = a[2].f; } pthread_mutex_unlock(&M.mu); return true; }
NAT(A_trackPosition)
{
    (void)self;
    pthread_mutex_lock(&M.mu);
    stream *s = S(a[0].i);
    int64_t p = s ? (int64_t)((double)s->consumed * s->rate / OUT_RATE) : 0;
    pthread_mutex_unlock(&M.mu);
    *ret = J(p);
    return true;
}
NAT(A_trackPending) { (void)self; pthread_mutex_lock(&M.mu); stream *s = S(a[0].i); int c = s ? (int)s->count : 0; pthread_mutex_unlock(&M.mu); *ret = I(c); return true; }
NAT(A_trackClose) { (void)self; (void)ret; pthread_mutex_lock(&M.mu); stream *s = S(a[0].i); if (s) { s->used = false; s->playing = false; s->count = 0; pthread_cond_broadcast(&M.room); } pthread_mutex_unlock(&M.mu); return true; }

static uint8_t *copy_bytes(jobj *arr, size_t *len)
{
    if (!arr || !arr->arr.len) return NULL;
    uint8_t *d = malloc(arr->arr.len);
    memcpy(d, arr->arr.data, arr->arr.len);
    *len = arr->arr.len;
    return d;
}
static uint8_t *read_fd(int fd, int64_t off, int64_t len, size_t *out)
{
    if (len <= 0) { struct stat st; if (fstat(fd, &st) != 0) return NULL; len = st.st_size - off; }
    if (len <= 0 || len > (256 << 20)) return NULL;
    uint8_t *d = malloc((size_t)len);
    size_t got = 0;
    while (got < (size_t)len) { ssize_t r = pread(fd, d + got, (size_t)len - got, off + (off_t)got); if (r <= 0) break; got += (size_t)r; }
    *out = got;
    return d;
}

/* SoundPool: decode once, play by id */
static int sample_load(uint8_t *data, size_t len)
{
    decoder *d = dec_open(data, len);
    if (!d) return -1;
    size_t cap = d->frames > 0 ? (size_t)d->frames : 44100, n = 0;
    int16_t *pcm = malloc(cap * (size_t)d->channels * 2);
    for (;;) {
        if (n + 4096 > cap) { cap = cap * 2 + 4096; pcm = realloc(pcm, cap * (size_t)d->channels * 2); }
        int r = dec_read(d, pcm + n * (size_t)d->channels, 4096);
        if (r <= 0) break;
        n += (size_t)r;
    }
    int rate = d->rate, ch = d->channels;
    dec_close(d);
    pthread_mutex_lock(&M.mu);
    int id = -1;
    for (int i = 1; i < MAX_SAMPLES && id < 0; i++) if (!M.smp[i].used) id = i;
    if (id > 0) M.smp[id] = (sample){ .pcm = pcm, .frames = (int)n, .channels = ch, .rate = rate, .used = true };
    else free(pcm);
    pthread_mutex_unlock(&M.mu);
    return id;
}
NAT(A_soundLoad) { (void)self; size_t len = 0; uint8_t *d = copy_bytes(a[0].l, &len); *ret = I(d ? sample_load(d, len) : -1); return true; }
NAT(A_soundLoadFd) { (void)self; size_t len = 0; uint8_t *d = read_fd(a[0].i, a[1].j, a[2].j, &len); *ret = I(d ? sample_load(d, len) : -1); return true; }
NAT(A_soundUnload)
{
    (void)self; (void)ret;
    pthread_mutex_lock(&M.mu);
    int id = a[0].i;
    if (id > 0 && id < MAX_SAMPLES && M.smp[id].used) {
        for (int i = 0; i < MAX_VOICES; i++) if (M.v[i].used && M.v[i].sample == id) M.v[i].used = false;
        free(M.smp[id].pcm); M.smp[id] = (sample){ 0 };
    }
    pthread_mutex_unlock(&M.mu);
    return true;
}
NAT(A_soundDuration) { (void)self; int id = a[0].i; *ret = I(id > 0 && id < MAX_SAMPLES && M.smp[id].used && M.smp[id].rate ? (int)((int64_t)M.smp[id].frames * 1000 / M.smp[id].rate) : 0); return true; }
NAT(A_soundPlay)
{
    (void)self;
    int id = a[0].i;
    pthread_mutex_lock(&M.mu);
    ensure_started_locked();
    int vid = 0;
    if (id > 0 && id < MAX_SAMPLES && M.smp[id].used) {
        int slot = -1;
        for (int i = 0; i < MAX_VOICES && slot < 0; i++) if (!M.v[i].used) slot = i;
        if (slot < 0) slot = 0;                                   /* all busy: the oldest slot goes */
        float rate = a[4].f > 0 ? a[4].f : 1.0f;
        vid = M.next_voice++;
        M.v[slot] = (voice){ .used = true, .sample = id, .id = vid, .loops = a[3].i, .pos = 0, .step = (double)M.smp[id].rate * rate / OUT_RATE, .l = a[1].f, .r = a[2].f };
        pthread_cond_signal(&M.work);
    }
    pthread_mutex_unlock(&M.mu);
    *ret = I(vid);
    return true;
}
static voice *V(int id) { for (int i = 0; i < MAX_VOICES; i++) if (M.v[i].used && M.v[i].id == id) return &M.v[i]; return NULL; }
NAT(A_voiceStop) { (void)self; (void)ret; pthread_mutex_lock(&M.mu); voice *v = V(a[0].i); if (v) v->used = false; pthread_mutex_unlock(&M.mu); return true; }
NAT(A_voicePause) { (void)self; (void)ret; pthread_mutex_lock(&M.mu); voice *v = V(a[0].i); if (v) { v->paused = a[1].z; if (!v->paused) pthread_cond_signal(&M.work); } pthread_mutex_unlock(&M.mu); return true; }
NAT(A_voiceVolume) { (void)self; (void)ret; pthread_mutex_lock(&M.mu); voice *v = V(a[0].i); if (v) { v->l = a[1].f; v->r = a[2].f; } pthread_mutex_unlock(&M.mu); return true; }
NAT(A_voiceRate) { (void)self; (void)ret; pthread_mutex_lock(&M.mu); voice *v = V(a[0].i); if (v) v->step = (double)M.smp[v->sample].rate * (a[1].f > 0 ? a[1].f : 1) / OUT_RATE; pthread_mutex_unlock(&M.mu); return true; }
NAT(A_voiceLoop) { (void)self; (void)ret; pthread_mutex_lock(&M.mu); voice *v = V(a[0].i); if (v) v->loops = a[1].i; pthread_mutex_unlock(&M.mu); return true; }
NAT(A_pauseAll) { (void)self; (void)ret; pthread_mutex_lock(&M.mu); for (int i = 0; i < MAX_VOICES; i++) if (M.v[i].used) M.v[i].paused = a[0].z; if (!a[0].z) pthread_cond_signal(&M.work); pthread_mutex_unlock(&M.mu); return true; }

/* MediaPlayer: a decoder read as it plays */
NAT(A_decOpen) { (void)self; size_t len = 0; uint8_t *d = copy_bytes(a[0].l, &len); decoder *dc = d ? dec_open(d, len) : NULL; if (d && !dc) free(d); *ret = J((int64_t)(uintptr_t)dc); return true; }
NAT(A_decOpenFd) { (void)self; size_t len = 0; uint8_t *d = read_fd(a[0].i, a[1].j, a[2].j, &len); decoder *dc = d ? dec_open(d, len) : NULL; if (d && !dc) free(d); *ret = J((int64_t)(uintptr_t)dc); return true; }
NAT(A_decInfo)
{
    (void)self; (void)ret;
    decoder *d = (decoder *)(uintptr_t)a[0].j; jobj *o = a[1].l;
    if (d && o && o->arr.len >= 3) { int32_t *v = o->arr.data; v[0] = d->rate; v[1] = d->channels; v[2] = d->rate ? (int32_t)(d->frames * 1000 / d->rate) : 0; }
    return true;
}
NAT(A_decRead)
{
    (void)self;
    decoder *d = (decoder *)(uintptr_t)a[0].j; jobj *o = a[1].l;
    if (!d || !o) { *ret = I(0); return true; }
    int frames = (int)(o->arr.len / (uint32_t)d->channels);
    *ret = I(dec_read(d, o->arr.data, frames) * d->channels);
    return true;
}
NAT(A_decSeek) { (void)self; (void)ret; decoder *d = (decoder *)(uintptr_t)a[0].j; if (d) dec_seek(d, (int64_t)a[1].i * d->rate / 1000); return true; }
NAT(A_decClose) { (void)self; (void)ret; dec_close((decoder *)(uintptr_t)a[0].j); return true; }

static const struct { const char *name, *sig; dvm_native_fn fn; } k_audio[] = {
    { "trackOpen", "(II)I", A_trackOpen },
    { "trackWrite", "(I[SIIZ)I", A_trackWrite },
    { "trackWriteBytes", "(I[BIIIZ)I", A_trackWriteBytes },
    { "trackWriteFloat", "(I[FIIZ)I", A_trackWriteFloat },
    { "trackPlay", "(IZ)V", A_trackPlay },
    { "trackFlush", "(I)V", A_trackFlush },
    { "trackVolume", "(IFF)V", A_trackVolume },
    { "trackPosition", "(I)J", A_trackPosition },
    { "trackPending", "(I)I", A_trackPending },
    { "trackClose", "(I)V", A_trackClose },
    { "soundLoad", "([B)I", A_soundLoad },
    { "soundLoadFd", "(IJJ)I", A_soundLoadFd },
    { "soundUnload", "(I)V", A_soundUnload },
    { "soundDuration", "(I)I", A_soundDuration },
    { "soundPlay", "(IFFIF)I", A_soundPlay },
    { "voiceStop", "(I)V", A_voiceStop },
    { "voicePause", "(IZ)V", A_voicePause },
    { "voiceVolume", "(IFF)V", A_voiceVolume },
    { "voiceRate", "(IF)V", A_voiceRate },
    { "voiceLoop", "(II)V", A_voiceLoop },
    { "pauseAll", "(Z)V", A_pauseAll },
    { "decOpen", "([B)J", A_decOpen },
    { "decOpenFd", "(IJJ)J", A_decOpenFd },
    { "decInfo", "(J[I)V", A_decInfo },
    { "decRead", "(J[S)I", A_decRead },
    { "decSeek", "(JI)V", A_decSeek },
    { "decClose", "(J)V", A_decClose },
    { NULL, NULL, NULL },
};
dvm_native_fn tl_audio_native(const char *name, const char *sig);
dvm_native_fn tl_audio_native(const char *name, const char *sig)
{
    for (int i = 0; k_audio[i].name; i++) if (!strcmp(k_audio[i].name, name) && !strcmp(k_audio[i].sig, sig)) return k_audio[i].fn;
    return NULL;
}
