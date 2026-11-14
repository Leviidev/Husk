/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Test the guest AAudio API through the real host mixer. Only AudioQueue is fake. */
#include <AudioToolbox/AudioToolbox.h>
#include <math.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "husk-tl-bionic.h"
#include "husk-tl-audio.h"

void (*tl_cocos_audio_hook)(const int16_t *, int, int, int);
void tl_log_line(const char *fmt, ...) { (void)fmt; }

/* The NDK table also holds bitmap entry points. This test must never call them. */
void tl_AndroidBitmap_getDataSpace(void) { abort(); }
void tl_AndroidBitmap_getInfo(void) { abort(); }
void tl_AndroidBitmap_lockPixels(void) { abort(); }
void tl_AndroidBitmap_unlockPixels(void) { abort(); }

static int check_sleep;
static struct timespec requested_sleep;
int nanosleep(const struct timespec *request, struct timespec *remaining)
{
    if (check_sleep) { requested_sleep = *request; return 0; }
    if (remaining) memset(remaining, 0, sizeof(*remaining));
    return usleep((useconds_t)(request->tv_sec * 1000000 + request->tv_nsec / 1000));
}

static struct {
    AudioQueueOutputCallback callback;
    void *user;
    AudioQueueBufferRef buffer;
    double rate;
    int channels, started, frames, crossings, first_crossing, last_crossing;
    int16_t previous;
} sink;

OSStatus AudioQueueNewOutput(const AudioStreamBasicDescription *format,
                            AudioQueueOutputCallback callback, void *user,
                            CFRunLoopRef loop, CFStringRef mode, UInt32 flags,
                            AudioQueueRef *out)
{
    (void)loop; (void)mode; (void)flags;
    sink.callback = callback; sink.user = user;
    sink.rate = format->mSampleRate; sink.channels = (int)format->mChannelsPerFrame;
    *out = (AudioQueueRef)&sink;
    return noErr;
}
OSStatus AudioQueueAllocateBuffer(AudioQueueRef q, UInt32 bytes, AudioQueueBufferRef *out)
{
    (void)q;
    void *data = calloc(1, bytes);
    AudioQueueBuffer buffer = { .mAudioDataBytesCapacity = bytes, .mAudioData = data };
    *out = malloc(sizeof(buffer));
    if (!*out || !data) abort();
    memcpy(*out, &buffer, sizeof(buffer));
    sink.buffer = *out;
    return noErr;
}
OSStatus AudioQueueEnqueueBuffer(AudioQueueRef q, AudioQueueBufferRef buffer,
                               UInt32 count, const AudioStreamPacketDescription *packets)
{
    (void)q; (void)count; (void)packets;
    if (sink.started) {
        const int16_t *pcm = buffer->mAudioData;
        int frames = (int)buffer->mAudioDataByteSize / (sink.channels * 2);
        /* The test tone has a positive bias. An empty queue returns zero padding. */
        for (int i = 0; i < frames; i++) {
            int16_t sample = pcm[i * sink.channels];
            if (!sample) continue;
            if (sink.previous <= 16000 && sample > 16000) {
                if (!sink.crossings) sink.first_crossing = sink.frames;
                sink.last_crossing = sink.frames;
                sink.crossings++;
            }
            sink.previous = sample;
            sink.frames++;
        }
    }
    return noErr;
}
OSStatus AudioQueueStart(AudioQueueRef q, const AudioTimeStamp *time)
{
    (void)q; (void)time; sink.started = 1; return noErr;
}
OSStatus AudioQueuePause(AudioQueueRef q) { (void)q; return noErr; }

static void *symbol(const char *name)
{
    for (const tl_bionic_entry *e = tl_tab_ndk; e->name; e++)
        if (!strcmp(e->name, name)) return e->addr;
    fprintf(stderr, "missing symbol: %s\n", name); exit(1);
}
#define API(name, type) ((type)symbol(#name))

static int check_rate_selection(void)
{
    void *builder = NULL, *first = NULL, *next = NULL;
    if (API(AAudio_createStreamBuilder, int (*)(void **))(&builder)) return 1;
    API(AAudioStreamBuilder_setSampleRate, void (*)(void *, int))(builder, 24000);
    if (API(AAudioStreamBuilder_openStream, int (*)(void *, void **))(builder, &first)) return 1;
    API(AAudioStreamBuilder_setSampleRate, void (*)(void *, int))(builder, 48000);
    if (API(AAudioStreamBuilder_openStream, int (*)(void *, void **))(builder, &next)) return 1;
    int ok = API(AAudioStream_getSampleRate, int (*)(void *))(first) == 24000
        && API(AAudioStream_getSampleRate, int (*)(void *))(next) == 48000;
    API(AAudioStream_close, int (*)(void *))(next);
    API(AAudioStreamBuilder_setSampleRate, void (*)(void *, int))(builder, 0);
    if (API(AAudioStreamBuilder_openStream, int (*)(void *, void **))(builder, &next)) return 1;
    ok &= API(AAudioStream_getSampleRate, int (*)(void *))(next) == 48000;
    API(AAudioStream_close, int (*)(void *))(next);
    const int invalid[] = { -1, 7999, 1600001, INT32_MAX };
    for (size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); i++) {
        API(AAudioStreamBuilder_setSampleRate, void (*)(void *, int))(builder, invalid[i]);
        next = NULL;
        int result = API(AAudioStreamBuilder_openStream, int (*)(void *, void **))(builder, &next);
        printf("invalid rate %d: result=%d (expected -880)\n", invalid[i], result);
        ok &= result == -880;
        if (result == 0) API(AAudioStream_close, int (*)(void *))(next);
    }
    API(AAudioStream_close, int (*)(void *))(first);
    API(AAudioStreamBuilder_delete, int (*)(void *))(builder);
    puts(ok ? "PASS: builder reuse, unspecified reset, and invalid rates" : "FAIL: rate selection contract");
    return ok ? 0 : 1;
}

static int check_pacing(int muted)
{
    void *builder = NULL, *stream = NULL;
    if (API(AAudio_createStreamBuilder, int (*)(void **))(&builder)) return 1;
    API(AAudioStreamBuilder_setSampleRate, void (*)(void *, int))(builder, 8000);
    if (API(AAudioStreamBuilder_openStream, int (*)(void *, void **))(builder, &stream)) return 1;
    API(AAudioStreamBuilder_delete, int (*)(void *))(builder);
    if (muted) { setenv("TL_AUDIO_MUTE", "1", 1); tl_audio_install(); }
    API(AAudioStream_requestStart, int (*)(void *))(stream);
    int16_t pcm[8192 * 2] = { 0 };
    check_sleep = 1; /* Capture the OS sleep request without waiting. */
    API(AAudioStream_write, int (*)(void *, const void *, int, int64_t))(stream, pcm, 8192, 0);
    API(AAudioStream_close, int (*)(void *))(stream);
    int ok = requested_sleep.tv_sec == 1 && requested_sleep.tv_nsec == 24000000;
    printf("%s: sleep=%lld s + %ld ns (expected 1 s + 24000000 ns)\n",
           muted ? "muted host hook" : "no host hook", (long long)requested_sleep.tv_sec, requested_sleep.tv_nsec);
    puts(ok ? "PASS: pacing uses stream rate and a valid timespec" : "FAIL: stream pacing");
    return ok ? 0 : 1;
}

typedef struct tone { void *pcm; int frames, bytes; } tone;
static int fill_tone(void *stream, void *user, void *data, int frames)
{
    (void)stream;
    tone *t = user;
    if (frames != t->frames) abort();
    memcpy(data, t->pcm, (size_t)frames * t->bytes);
    return 1; /* AAUDIO_CALLBACK_RESULT_STOP */
}

static int play_tone(int request, int mode, int format, int channels)
{
    sink.frames = sink.crossings = sink.first_crossing = sink.last_crossing = 0;
    sink.previous = 0;
    int rate = request ? request : 48000, frames = rate / 10;
    tone t = { .frames = frames, .bytes = channels * (format == 2 ? 4 : 2) };
    t.pcm = malloc((size_t)frames * t.bytes);
    if (!t.pcm) abort();
    for (int i = 0; i < frames; i++) {
        double sample = 16000 + 8000 * sin(2 * M_PI * 1000 * i / rate);
        for (int c = 0; c < channels; c++) {
            if (format == 2) ((float *)t.pcm)[i * channels + c] = (float)(sample / 32767);
            else ((int16_t *)t.pcm)[i * channels + c] = (int16_t)sample;
        }
    }
    void *builder = NULL, *stream = NULL;
    if (API(AAudio_createStreamBuilder, int (*)(void **))(&builder)) return 1;
    if (request) API(AAudioStreamBuilder_setSampleRate, void (*)(void *, int))(builder, request);
    API(AAudioStreamBuilder_setChannelCount, void (*)(void *, int))(builder, channels);
    API(AAudioStreamBuilder_setFormat, void (*)(void *, int))(builder, format);
    if (mode == 1) {
        API(AAudioStreamBuilder_setFramesPerDataCallback, void (*)(void *, int))(builder, frames);
        API(AAudioStreamBuilder_setDataCallback, void (*)(void *, int (*)(void *, void *, void *, int), void *))(builder, fill_tone, &t);
    }
    if (API(AAudioStreamBuilder_openStream, int (*)(void *, void **))(builder, &stream)) return 1;
    API(AAudioStreamBuilder_delete, int (*)(void *))(builder);
    int reported = API(AAudioStream_getSampleRate, int (*)(void *))(stream);
    int hardware = API(AAudioStream_getHardwareSampleRate, int (*)(void *))(stream);
    tl_audio_install();
    API(AAudioStream_requestStart, int (*)(void *))(stream);
    int written = frames;
    if (mode == 1) {
        for (int i = 0; i < 2000; i++) {
            if (API(AAudioStream_getFramesWritten, int64_t (*)(void *))(stream) == frames) break;
            struct timespec ts = { 0, 1000000 }; nanosleep(&ts, NULL);
        }
    } else {
        written = 0;
        for (int offset = 0; offset < frames;) {
            int count = mode == 2 && frames - offset > 257 ? 257 : frames - offset;
            written += API(AAudioStream_write, int (*)(void *, const void *, int, int64_t))(stream,
                (uint8_t *)t.pcm + (size_t)offset * t.bytes, count, 0);
            offset += count;
        }
    }
    int64_t counted = API(AAudioStream_getFramesWritten, int64_t (*)(void *))(stream);
    API(AAudioStream_requestStop, int (*)(void *))(stream);
    API(AAudioStream_close, int (*)(void *))(stream);
    free(t.pcm);
    if (!sink.callback) { fputs("FAIL: no output queue\n", stderr); return 1; }
    for (int i = 0; i < 8; i++) sink.callback(sink.user, (AudioQueueRef)&sink, sink.buffer);
    double seconds = sink.frames / sink.rate;
    double frequency = sink.last_crossing > sink.first_crossing
        ? (sink.crossings - 1) * sink.rate / (sink.last_crossing - sink.first_crossing) : 0;
    printf("request=%d, %s, %s/%d: reported=%d, queue=%.0f Hz, duration=%.3f s, tone=%.1f Hz\n",
           request, mode == 1 ? "callback" : mode == 2 ? "split" : "write", format == 2 ? "float" : "i16", channels,
           reported, sink.rate, seconds, frequency);
    if (reported != rate || hardware != 48000 || written != frames || counted != frames
        || sink.channels != 2 || fabs(seconds - 0.1) > 0.00001 || fabs(frequency - 1000) > 5) {
        fputs("FAIL: expected requested rate, 0.100 s duration, and 1000 Hz tone\n", stderr);
        return 1;
    }
    puts("PASS: stream rate, duration, pitch, channel conversion, and frame count");
    return 0;
}

int main(int argc, char **argv)
{
    if (argc == 2 && !strcmp(argv[1], "rate-selection")) return check_rate_selection();
    if (argc == 2 && !strcmp(argv[1], "pacing")) return check_pacing(0);
    if (argc == 2 && !strcmp(argv[1], "pacing-muted")) return check_pacing(1);
    if (argc == 2 && !strcmp(argv[1], "mixed"))
        return play_tone(44100, 0, 1, 2) || play_tone(48000, 1, 2, 2) || play_tone(24000, 1, 2, 2);
    int request = 24000, mode = 0, format = 1, channels = 2;
    if (argc > 5) goto usage;
    if (argc > 1) {
        if (!strcmp(argv[1], "0")) request = 0;
        else if (!strcmp(argv[1], "44100")) request = 44100;
        else if (!strcmp(argv[1], "48000")) request = 48000;
        else if (strcmp(argv[1], "24000")) goto usage;
    }
    if (argc > 2) {
        if (!strcmp(argv[2], "callback")) mode = 1;
        else if (!strcmp(argv[2], "split")) mode = 2;
        else if (strcmp(argv[2], "write")) goto usage;
    }
    if (argc > 3) {
        if (!strcmp(argv[3], "float")) format = 2;
        else if (strcmp(argv[3], "i16")) goto usage;
    }
    if (argc > 4) {
        if (!strcmp(argv[4], "1")) channels = 1;
        else if (strcmp(argv[4], "2")) goto usage;
    }
    return play_tone(request, mode, format, channels);
usage:
    fputs("usage: aaudio_rate [0|24000|44100|48000 [write|callback|split [i16|float [1|2]]]]\n", stderr);
    return 2;
}
