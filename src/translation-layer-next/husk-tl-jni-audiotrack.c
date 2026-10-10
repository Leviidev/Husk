/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * android.media.AudioTrack, for engines that play through Java (GameMaker's runner): a streaming track whose write() goes to the
 * shared audio output (tl_cocos_audio_hook), which blocks until there is room -- the pacing AudioTrack.write gives on Android.
 */
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "husk-tl-bionic.h"
#include "husk-tl-jni.h"
#include "husk-tl-internal.h"

extern void (*tl_cocos_audio_hook)(const int16_t *samples, int frames, int channels, int rate);

typedef struct { int rate, channels, format; bool playing; } track;

static jvalue vi(int i) { jvalue v; v.j = 0; v.i = i; return v; }

/* AudioTrack(streamType, sampleRate, channelConfig, audioFormat, bufferSizeInBytes, mode) */
static void AT_init(tl_jcall *c)
{
    track *t = calloc(1, sizeof(*t));
    t->rate = c->args[1].i > 0 ? c->args[1].i : 44100;
    int cfg = c->args[2].i;
    t->channels = cfg == 4 || cfg == 2 ? 1 : 2;              /* CHANNEL_OUT_MONO (4) / CHANNEL_CONFIGURATION_MONO (2), else stereo */
    t->format = c->args[3].i;                                /* ENCODING_PCM_16BIT 2, PCM_8BIT 3, PCM_FLOAT 4 */
    c->self->native = t;
    tl_log_line("audiotrack: %d Hz, %d channel%s, format %d", t->rate, t->channels, t->channels == 1 ? "" : "s", t->format);
}

static void AT_minBuffer(tl_jcall *c) { c->ret = vi(8192); }
static void AT_play(tl_jcall *c) { track *t = c->self ? c->self->native : NULL; if (t) t->playing = true; }
static void AT_stop(tl_jcall *c) { track *t = c->self ? c->self->native : NULL; if (t) t->playing = false; }
static void AT_release(tl_jcall *c) { if (c->self && c->self->native) { free(c->self->native); c->self->native = NULL; } }
static void AT_noop(tl_jcall *c) { (void)c; }
static void AT_state(tl_jcall *c) { c->ret = vi(1); }                                        /* STATE_INITIALIZED */
static void AT_playState(tl_jcall *c) { track *t = c->self ? c->self->native : NULL; c->ret = vi(t && t->playing ? 3 : 1); }
static void AT_zero(tl_jcall *c) { c->ret = vi(0); }

static void push(track *t, const int16_t *s, int frames)
{
    if (tl_cocos_audio_hook && frames > 0) tl_cocos_audio_hook(s, frames, t->channels, t->rate);
}

/* write(byte[] data, int offset, int size): bytes of the track's format. */
static void AT_writeBytes(tl_jcall *c)
{
    track *t = c->self ? c->self->native : NULL;
    jobj *arr = c->args[0].l;
    int off = c->args[1].i, size = c->args[2].i;
    if (!t || !arr || off < 0 || size <= 0 || (uint32_t)(off + size) > arr->arr.len) { c->ret = vi(0); return; }
    const uint8_t *src = (const uint8_t *)arr->arr.data + off;
    if (t->format == 3) {                                     /* 8-bit unsigned: widen */
        int16_t *w = malloc((size_t)size * 2);
        for (int i = 0; i < size; i++) w[i] = (int16_t)((src[i] - 128) << 8);
        push(t, w, size / t->channels);
        free(w);
    } else {
        push(t, (const int16_t *)src, size / (2 * t->channels));
    }
    c->ret = vi(size);
}

/* write(short[] data, int offset, int size): samples. */
static void AT_writeShorts(tl_jcall *c)
{
    track *t = c->self ? c->self->native : NULL;
    jobj *arr = c->args[0].l;
    int off = c->args[1].i, size = c->args[2].i;
    if (!t || !arr || off < 0 || size <= 0 || (uint32_t)(off + size) > arr->arr.len) { c->ret = vi(0); return; }
    push(t, (const int16_t *)arr->arr.data + off, size / t->channels);
    c->ret = vi(size);
}

/* write(float[] data, int offset, int size, int mode): float samples, converted. */
static void AT_writeFloats(tl_jcall *c)
{
    track *t = c->self ? c->self->native : NULL;
    jobj *arr = c->args[0].l;
    int off = c->args[1].i, size = c->args[2].i;
    if (!t || !arr || off < 0 || size <= 0 || (uint32_t)(off + size) > arr->arr.len) { c->ret = vi(0); return; }
    const float *f = (const float *)arr->arr.data + off;
    int16_t *w = malloc((size_t)size * 2);
    for (int i = 0; i < size; i++) { float v = f[i] * 32767.f; w[i] = (int16_t)(v > 32767.f ? 32767 : v < -32768.f ? -32768 : v); }
    push(t, w, size / t->channels);
    free(w);
    c->ret = vi(size);
}

#define AT "android/media/AudioTrack"
static const tl_jhle k_audiotrack[] = {
    { AT, "<init>", "(IIIIII)V", AT_init },
    { AT, "getMinBufferSize", "(III)I", AT_minBuffer },
    { AT, "play", "()V", AT_play },
    { AT, "stop", "()V", AT_stop },
    { AT, "pause", "()V", AT_stop },
    { AT, "flush", "()V", AT_noop },
    { AT, "release", "()V", AT_release },
    { AT, "getState", "()I", AT_state },
    { AT, "getPlayState", "()I", AT_playState },
    { AT, "setStereoVolume", "(FF)I", AT_zero },
    { AT, "setVolume", "(F)I", AT_zero },
    { AT, "write", "([BII)I", AT_writeBytes },
    { AT, "write", "([SII)I", AT_writeShorts },
    { AT, "write", "([FIII)I", AT_writeFloats },
    { NULL, NULL, NULL, NULL },
};

void tl_audiotrack_hle_install(void) { tl_jni_register_hle(k_audiotrack); }
