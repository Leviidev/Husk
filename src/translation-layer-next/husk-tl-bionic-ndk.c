/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * The NDK's own libraries: libandroid (looper, assets, windows, sensors), libmediandk,
 * and zlib. EGL and GLES are in husk-tl-egl.c.
 */
#define _DARWIN_C_SOURCE
#include "husk-tl-bionic.h"

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include <unistd.h>
#include <zlib.h>

#include "husk-tl-internal.h"
#include "husk-tl-ld.h"

/* ---------------------------------------------------------------- looper */

/*
 * Android's ALooper is a per-thread event loop that other threads wake with a write
 * to a pipe. Unity's main thread polls it between frames. This one has the wake pipe
 * and nothing else registered on it: no file descriptors, no callbacks.
 */
typedef struct looper_fd { int fd, ident, events; void *callback, *data; struct looper_fd *next; } looper_fd;
typedef struct tl_looper { int wake[2]; atomic_int refs; pthread_t thread; struct tl_looper *next; pthread_mutex_t mu; looper_fd *fds; } tl_looper;
static __thread tl_looper *t_looper;

enum { ALOOPER_POLL_WAKE = -1, ALOOPER_POLL_CALLBACK = -2, ALOOPER_POLL_TIMEOUT = -3, ALOOPER_POLL_ERROR = -4 };
enum { ALOOPER_EVENT_INPUT = 1, ALOOPER_EVENT_OUTPUT = 2, ALOOPER_EVENT_ERROR = 4, ALOOPER_EVENT_HANGUP = 8 };

static tl_looper *looper_make(void)
{
    tl_looper *l = calloc(1, sizeof(*l));
    if (pipe(l->wake) == 0) {
        fcntl(l->wake[0], F_SETFL, O_NONBLOCK);
        fcntl(l->wake[1], F_SETFL, O_NONBLOCK);
    }
    atomic_init(&l->refs, 1);
    pthread_mutex_init(&l->mu, NULL);
    l->thread = pthread_self();
    return l;
}
static void *b_ALooper_prepare(int opts) { (void)opts; if (!t_looper) t_looper = looper_make(); return t_looper; }
static void *b_ALooper_forThread(void) { return t_looper; }
static void b_ALooper_acquire(tl_looper *l) { if (l) atomic_fetch_add(&l->refs, 1); }
static void b_ALooper_release(tl_looper *l) { (void)l; }
static void b_ALooper_wake(tl_looper *l)
{
    if (!l) return;
    char c = 1;
    (void)!write(l->wake[1], &c, 1);
}

/* A file descriptor the looper watches: with a callback it is called from the polling thread; without one, its ident is returned. */
static int b_ALooper_addFd(tl_looper *l, int fd, int ident, int events, void *callback, void *data)
{
    if (!l || fd < 0) return -1;
    pthread_mutex_lock(&l->mu);
    looper_fd *f = l->fds;
    while (f && f->fd != fd) f = f->next;
    if (!f) { f = calloc(1, sizeof(*f)); f->next = l->fds; l->fds = f; }
    f->fd = fd; f->ident = ident; f->events = events; f->callback = callback; f->data = data;
    pthread_mutex_unlock(&l->mu);
    b_ALooper_wake(l);
    return 1;
}
static int b_ALooper_removeFd(tl_looper *l, int fd)
{
    if (!l) return -1;
    int found = 0;
    pthread_mutex_lock(&l->mu);
    for (looper_fd **pp = &l->fds; *pp; pp = &(*pp)->next)
        if ((*pp)->fd == fd) { looper_fd *dead = *pp; *pp = dead->next; free(dead); found = 1; break; }
    pthread_mutex_unlock(&l->mu);
    return found;
}

static int b_ALooper_pollOnce(int timeout_ms, int *out_fd, int *out_events, void **out_data)
{
    tl_looper *l = t_looper;
    if (out_fd) *out_fd = -1;
    if (out_events) *out_events = 0;
    if (out_data) *out_data = NULL;
    if (!l) return ALOOPER_POLL_ERROR;
    struct pollfd pfds[32];
    looper_fd snap[31];
    int n = 0;
    pfds[n++] = (struct pollfd){ l->wake[0], POLLIN, 0 };
    pthread_mutex_lock(&l->mu);
    for (looper_fd *f = l->fds; f && n < 32; f = f->next) {
        short ev = (short)(((f->events & ALOOPER_EVENT_INPUT) ? POLLIN : 0) | ((f->events & ALOOPER_EVENT_OUTPUT) ? POLLOUT : 0));
        snap[n - 1] = *f;
        pfds[n++] = (struct pollfd){ f->fd, ev, 0 };
    }
    pthread_mutex_unlock(&l->mu);
    { static int tr = -1; if (tr < 0) tr = getenv("TL_LOOPER_TRACE") ? 2 : 0; if (tr) { char b[200]; int k = 0; for (int i = 0; i < n && k < 180; i++) k += snprintf(b + k, sizeof(b) - (size_t)k, " %d", pfds[i].fd); tl_log_line("looper: poll(%d ms) on%s", timeout_ms, b); } }
    int r = poll(pfds, (nfds_t)n, timeout_ms);
    { static int tr = -1; if (tr < 0) tr = getenv("TL_LOOPER_TRACE") ? 2 : 0; if (tr) { char b[200]; int k = 0; for (int i = 0; i < n && k < 180; i++) if (pfds[i].revents) k += snprintf(b + k, sizeof(b) - (size_t)k, " %d:%x", pfds[i].fd, pfds[i].revents); tl_log_line("looper: poll -> %d:%s", r, b); } }
    if (r < 0) return ALOOPER_POLL_ERROR;
    if (r == 0) return ALOOPER_POLL_TIMEOUT;
    if (pfds[0].revents & POLLIN) { char buf[64]; while (read(l->wake[0], buf, sizeof(buf)) > 0) {} }
    for (int i = 1; i < n; i++) {
        if (!pfds[i].revents) continue;
        looper_fd *f = &snap[i - 1];
        int ev = ((pfds[i].revents & POLLIN) ? ALOOPER_EVENT_INPUT : 0) | ((pfds[i].revents & POLLOUT) ? ALOOPER_EVENT_OUTPUT : 0)
               | ((pfds[i].revents & POLLERR) ? ALOOPER_EVENT_ERROR : 0) | ((pfds[i].revents & POLLHUP) ? ALOOPER_EVENT_HANGUP : 0);
        if (f->callback) {
            { static int tr = -1; if (tr < 0) tr = getenv("TL_LOOPER_TRACE") ? 1 : 0; if (tr) tl_log_line("looper: callback for fd %d (%d fds watched)", f->fd, n - 1); }
            int keep = ((int (*)(int, int, void *))f->callback)(f->fd, ev, f->data);
            if (!keep) b_ALooper_removeFd(l, f->fd);
            return ALOOPER_POLL_CALLBACK;
        }
        if (out_fd) *out_fd = f->fd;
        if (out_events) *out_events = ev;
        if (out_data) *out_data = f->data;
        return f->ident;
    }
    return ALOOPER_POLL_WAKE;
}
static int b_ALooper_pollAll(int timeout_ms, int *out_fd, int *out_events, void **out_data)
{
    for (;;) {
        int r = b_ALooper_pollOnce(timeout_ms, out_fd, out_events, out_data);
        if (r != ALOOPER_POLL_CALLBACK) return r;
        timeout_ms = 0;
    }
}

/* ----------------------------------------------------------- choreographer */
/*
 * AChoreographer: frame callbacks at the display's vsync, delivered on the thread that asked, through its looper. Flutter's engine
 * times its frames with it (API 29+). One clock thread ticks at 60 Hz and pokes every choreographer that has callbacks waiting; the
 * owner thread's looper runs them with that tick's time, as Android's does.
 */
#define VSYNC_NS 16666667ll
typedef struct chcb { int kind; void *fn, *data; int64_t not_before; struct chcb *next; } chcb;    /* kind 0: frame, 1: frame64, 2: vsync */
typedef struct tl_choreo { tl_looper *looper; int pipe[2]; pthread_mutex_t mu; chcb *pending; void *rate_fn, *rate_data; struct tl_choreo *next; } tl_choreo;
static __thread tl_choreo *t_choreo;
static tl_choreo *g_choreos;
static pthread_mutex_t g_choreo_mu = PTHREAD_MUTEX_INITIALIZER;
static atomic_llong g_vsync_time;

static int64_t mono_ns(void) { struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts); return (int64_t)ts.tv_sec * 1000000000ll + ts.tv_nsec; }

/* What a vsync callback is given: one frame timeline, this vsync's. */
typedef struct { int64_t frame_time, deadline, expected_present; } tl_choreo_frame;
static int64_t b_AChoreographerFrameCallbackData_getFrameTimeNanos(const tl_choreo_frame *d) { return d->frame_time; }
static size_t b_AChoreographerFrameCallbackData_getFrameTimelinesLength(const tl_choreo_frame *d) { (void)d; return 1; }
static size_t b_AChoreographerFrameCallbackData_getPreferredFrameTimelineIndex(const tl_choreo_frame *d) { (void)d; return 0; }
static int64_t b_AChoreographerFrameCallbackData_getFrameTimelineVsyncId(const tl_choreo_frame *d, size_t i) { (void)i; return d->frame_time / VSYNC_NS; }
static int64_t b_AChoreographerFrameCallbackData_getFrameTimelineExpectedPresentationTimeNanos(const tl_choreo_frame *d, size_t i) { (void)i; return d->expected_present; }
static int64_t b_AChoreographerFrameCallbackData_getFrameTimelineDeadlineNanos(const tl_choreo_frame *d, size_t i) { (void)i; return d->deadline; }

static int choreo_run(int fd, int events, void *data)
{
    (void)events;
    tl_choreo *c = data;
    char buf[64];
    while (read(fd, buf, sizeof(buf)) > 0) {}
    int64_t now = atomic_load(&g_vsync_time);
    pthread_mutex_lock(&c->mu);
    chcb *due = NULL, **keep = &c->pending;
    for (chcb *cb = c->pending; cb;) {
        chcb *n = cb->next;
        if (cb->not_before <= now) { *keep = n; cb->next = due; due = cb; } else keep = &cb->next;
        cb = n;
    }
    pthread_mutex_unlock(&c->mu);
    /* in the order they were posted */
    chcb *rev = NULL;
    while (due) { chcb *n = due->next; due->next = rev; rev = due; due = n; }
    tl_choreo_frame fd_ = { now, now + VSYNC_NS - 2000000, now + 2 * VSYNC_NS };
    for (chcb *cb = rev; cb;) {
        chcb *n = cb->next;
        if (cb->kind == 0) ((void (*)(long, void *))cb->fn)((long)now, cb->data);
        else if (cb->kind == 1) ((void (*)(int64_t, void *))cb->fn)(now, cb->data);
        else ((void (*)(const tl_choreo_frame *, void *))cb->fn)(&fd_, cb->data);
        free(cb);
        cb = n;
    }
    return 1;
}

static void *choreo_clock(void *arg)
{
    (void)arg;
    pthread_setname_np("husk-vsync");
    int64_t next = mono_ns();
    for (;;) {
        next += VSYNC_NS;
        int64_t now = mono_ns();
        if (next > now) { struct timespec ts = { 0, (long)(next - now) }; nanosleep(&ts, NULL); } else next = now;
        atomic_store(&g_vsync_time, next);
        pthread_mutex_lock(&g_choreo_mu);
        for (tl_choreo *c = g_choreos; c; c = c->next) {
            pthread_mutex_lock(&c->mu);
            bool any = c->pending != NULL;
            pthread_mutex_unlock(&c->mu);
            if (any) { char b = 1; (void)!write(c->pipe[1], &b, 1); }
        }
        pthread_mutex_unlock(&g_choreo_mu);
    }
    return NULL;
}

static tl_choreo *b_AChoreographer_getInstance(void)
{
    if (t_choreo) return t_choreo;
    if (!t_looper) { tl_log_line("choreographer: asked for on a thread without a looper"); return NULL; }   /* as on Android */
    tl_choreo *c = calloc(1, sizeof(*c));
    c->looper = t_looper;
    pthread_mutex_init(&c->mu, NULL);
    if (pipe(c->pipe)) { free(c); return NULL; }
    fcntl(c->pipe[0], F_SETFL, O_NONBLOCK); fcntl(c->pipe[1], F_SETFL, O_NONBLOCK);
    b_ALooper_addFd(t_looper, c->pipe[0], ALOOPER_POLL_CALLBACK, ALOOPER_EVENT_INPUT, (void *)choreo_run, c);
    static pthread_t clock_thread;
    pthread_mutex_lock(&g_choreo_mu);
    c->next = g_choreos; g_choreos = c;
    pthread_mutex_unlock(&g_choreo_mu);
    if (!atomic_load(&g_vsync_time)) atomic_store(&g_vsync_time, mono_ns());
    static atomic_bool started;
    if (!atomic_exchange(&started, true)) pthread_create(&clock_thread, NULL, choreo_clock, NULL);
    t_choreo = c;
    return c;
}

static void choreo_post(tl_choreo *c, int kind, void *fn, void *data, int64_t delay_ms)
{
    static int tr = -1; if (tr < 0) tr = getenv("TL_CHOREO_TRACE") ? 1 : 0;
    if (tr) tl_log_line("choreographer: post kind %d on %p", kind, (void *)c);
    if (!c || !fn) return;
    chcb *cb = calloc(1, sizeof(*cb));
    cb->kind = kind; cb->fn = fn; cb->data = data;
    cb->not_before = delay_ms > 0 ? mono_ns() + delay_ms * 1000000ll : 0;
    pthread_mutex_lock(&c->mu);
    cb->next = c->pending; c->pending = cb;
    pthread_mutex_unlock(&c->mu);
}
static void b_AChoreographer_postFrameCallback(tl_choreo *c, void *fn, void *data) { choreo_post(c, 0, fn, data, 0); }
static void b_AChoreographer_postFrameCallbackDelayed(tl_choreo *c, void *fn, void *data, long ms) { choreo_post(c, 0, fn, data, ms); }
static void b_AChoreographer_postFrameCallback64(tl_choreo *c, void *fn, void *data) { choreo_post(c, 1, fn, data, 0); }
static void b_AChoreographer_postFrameCallbackDelayed64(tl_choreo *c, void *fn, void *data, uint32_t ms) { choreo_post(c, 1, fn, data, ms); }
static void b_AChoreographer_postVsyncCallback(tl_choreo *c, void *fn, void *data) { choreo_post(c, 2, fn, data, 0); }
/* The refresh rate never changes: a callback is told 60 Hz once, when it registers. */
static void b_AChoreographer_registerRefreshRateCallback(tl_choreo *c, void *fn, void *data)
{
    if (!c || !fn) return;
    c->rate_fn = fn; c->rate_data = data;
    ((void (*)(int64_t, void *))fn)(VSYNC_NS, data);
}
static void b_AChoreographer_unregisterRefreshRateCallback(tl_choreo *c, void *fn, void *data) { (void)fn; (void)data; if (c) c->rate_fn = NULL; }

/* ----------------------------------------------------------- configuration */

/* AConfiguration: an English, landscape, phone-sized device. */
typedef struct { char language[2], country[2]; int orientation, density, screen_long, screen_size, ui_mode_type, ui_mode_night; } tl_aconfig;
static tl_aconfig *b_AConfiguration_new(void)
{
    tl_aconfig *c = calloc(1, sizeof(*c));
    memcpy(c->language, "en", 2); memcpy(c->country, "US", 2);
    c->orientation = 2; c->density = 480; c->screen_long = 2; c->screen_size = 2;
    return c;
}
static void b_AConfiguration_delete(tl_aconfig *c) { free(c); }
static void b_AConfiguration_fromAssetManager(tl_aconfig *c, void *mgr) { (void)c; (void)mgr; }
static void b_AConfiguration_getLanguage(tl_aconfig *c, char *out) { out[0] = c->language[0]; out[1] = c->language[1]; }
static void b_AConfiguration_getCountry(tl_aconfig *c, char *out) { out[0] = c->country[0]; out[1] = c->country[1]; }
static int b_AConfiguration_getOrientation(tl_aconfig *c) { return c->orientation; }
static int b_AConfiguration_getDensity(tl_aconfig *c) { return c->density; }
static int b_AConfiguration_getScreenLong(tl_aconfig *c) { return c->screen_long; }
static int b_AConfiguration_getScreenSize(tl_aconfig *c) { return c->screen_size; }
static int b_AConfiguration_getUiModeType(tl_aconfig *c) { return c->ui_mode_type; }
static int b_AConfiguration_getUiModeNight(tl_aconfig *c) { return c->ui_mode_night; }
static int b_AConfiguration_getKeyboard(tl_aconfig *c) { (void)c; return 1; }
static int b_AConfiguration_getNavigation(tl_aconfig *c) { (void)c; return 1; }
static int b_AConfiguration_getTouchscreen(tl_aconfig *c) { (void)c; return 3; }
static int b_AConfiguration_getSdkVersion(tl_aconfig *c) { (void)c; return 34; }

/* ---------------------------------------------------------------- assets */

typedef struct { const uint8_t *data; size_t len; size_t pos; uint8_t *owned; } tl_asset;

static void *b_AAssetManager_fromJava(void *env, void *obj) { (void)env; (void)obj; static int mgr; return &mgr; }

static void *b_AAssetManager_open(void *mgr, const char *name, int mode)
{
    (void)mgr; (void)mode;
    char path[1024];
    snprintf(path, sizeof(path), "assets/%s", name);
    static int trace = -1;
    if (trace < 0) trace = getenv("TL_FILE_TRACE") != NULL;
    if (trace) tl_log_line("assets: open %s", name);
    /* TL_ASSET_DIR: a file of that name under this directory stands in for the one in the APK (to change a game's config while debugging) */
    if (getenv("TL_ASSET_DIR")) {
        char alt[1100];
        snprintf(alt, sizeof(alt), "%s/%s", getenv("TL_ASSET_DIR"), name);
        FILE *fp = fopen(alt, "rb");
        if (fp) {
            fseek(fp, 0, SEEK_END); long n = ftell(fp); fseek(fp, 0, SEEK_SET);
            uint8_t *buf = malloc((size_t)n + 1);
            if (buf && fread(buf, 1, (size_t)n, fp) == (size_t)n) {
                fclose(fp);
                tl_asset *a = calloc(1, sizeof(*a));
                a->data = buf; a->len = (size_t)n; a->owned = buf;
                return a;
            }
            free(buf); fclose(fp);
        }
    }
    for (int i = 0;; i++) {
        const tl_zip *z = tl_ld_apk_at(i);
        if (!z) break;
        const tl_zip_entry *e = tl_zip_find(z, path);
        if (!e) continue;
        const uint8_t *data; size_t n; bool owned; char err[160];
        if (!tl_zip_data(z, e, (size_t)2 << 30, &data, &n, &owned, err, sizeof(err))) {
            tl_log_line("assets: %s: %s", name, err);
            return NULL;
        }
        tl_asset *a = calloc(1, sizeof(*a));
        a->data = data; a->len = n;
        if (owned) a->owned = (uint8_t *)data;
        return a;
    }
    return NULL;
}
/*
 * AAudio, the way FMOD (Minecraft's audio) prefers to play: a stream whose data callback the system calls for the next burst of
 * samples. The output is always 48 kHz stereo 16-bit; a thread of ours calls the game's callback for a burst and hands the result
 * to the host audio (tl_cocos_audio_hook, which blocks until the speakers have room -- that is what paces the game's mixer).
 * Recording is not offered.
 */
extern void (*tl_cocos_audio_hook)(const int16_t *samples, int frames, int channels, int rate);

enum { AA_OK = 0, AA_UNAVAILABLE = -899, AA_TIMEOUT = -885, AA_FORMAT_I16 = 1, AA_FORMAT_FLOAT = 2, AA_STATE_OPEN = 2, AA_STATE_STARTED = 4, AA_STATE_PAUSED = 6,
       AA_STATE_FLUSHED = 8, AA_STATE_STOPPED = 10, AA_STATE_CLOSED = 12 };
#define AA_RATE 48000
#define AA_CHANNELS 2
#define AA_BURST 1024

typedef int (*aa_data_cb)(void *stream, void *user, void *data, int frames);
typedef struct aa_builder { aa_data_cb cb; void *user; int direction, per_callback, format, channels; } aa_builder;
typedef struct aa_stream {
    aa_data_cb cb; void *user;
    int burst;                                  /* frames per data callback */
    int format, channels;                       /* what the app asked for: 16-bit or float, mono or stereo */
    _Atomic int64_t frames;                     /* written to the speakers so far */
    atomic_int state, buffer_size;
    atomic_bool run;
    pthread_t thread;
    bool thread_started;
} aa_stream;

static int b_AAudio_createStreamBuilder(void **builder) { aa_builder *b = calloc(1, sizeof(*b)); *builder = b; return b ? AA_OK : AA_UNAVAILABLE; }
static int b_AAudioStreamBuilder_delete(aa_builder *b) { free(b); return AA_OK; }
static void b_AAudioStreamBuilder_setDataCallback(aa_builder *b, aa_data_cb cb, void *user) { b->cb = cb; b->user = user; }
static void b_AAudioStreamBuilder_setDirection(aa_builder *b, int d) { b->direction = d; }
static void b_AAudioStreamBuilder_ignore(aa_builder *b, int v) { (void)b; (void)v; }
static void b_AAudioStreamBuilder_setFramesPerDataCallback(aa_builder *b, int n) { b->per_callback = n; }
static void b_AAudioStreamBuilder_setFormat(aa_builder *b, int f) { b->format = f; }
static void b_AAudioStreamBuilder_setChannelCount(aa_builder *b, int n) { b->channels = n; }
static void b_AAudioStreamBuilder_setChannelMask(aa_builder *b, uint32_t m) { b->channels = __builtin_popcount(m) == 1 ? 1 : 2; }
static void b_AAudioStreamBuilder_setErrorCallback(aa_builder *b, void *cb, void *user) { (void)b; (void)cb; (void)user; }

static int b_AAudioStreamBuilder_openStream(aa_builder *b, aa_stream **out)
{
    if (b->direction != 0) return AA_UNAVAILABLE;                   /* output only */
    aa_stream *s = calloc(1, sizeof(*s));
    s->cb = b->cb; s->user = b->user;
    s->burst = b->per_callback > 0 && b->per_callback <= 8192 ? b->per_callback : AA_BURST;
    s->format = b->format == AA_FORMAT_FLOAT ? AA_FORMAT_FLOAT : AA_FORMAT_I16;
    s->channels = b->channels == 1 ? 1 : AA_CHANNELS;
    atomic_store(&s->state, AA_STATE_OPEN); atomic_store(&s->buffer_size, s->burst * 2);
    *out = s;
    tl_log_line("aaudio: stream opened (%d Hz, %d channels, %s, %s, burst %d)", AA_RATE, s->channels, s->format == AA_FORMAT_FLOAT ? "float" : "16-bit",
                s->cb ? "data callback" : "written", s->burst);
    return AA_OK;
}

/* the app's frames (its format and channel count) as 16-bit stereo for the host; out holds frames * 2 samples */
static const int16_t *aa_convert(aa_stream *s, const void *in, int frames, int16_t *out)
{
    if (s->format == AA_FORMAT_I16 && s->channels == AA_CHANNELS) return in;
    for (int i = 0; i < frames; i++)
        for (int c = 0; c < AA_CHANNELS; c++) {
            int k = i * s->channels + (s->channels == 1 ? 0 : c);
            if (s->format == AA_FORMAT_FLOAT) { float f = ((const float *)in)[k] * 32767.0f; out[i * 2 + c] = (int16_t)(f > 32767.0f ? 32767 : f < -32768.0f ? -32768 : f); }
            else out[i * 2 + c] = ((const int16_t *)in)[k];
        }
    return out;
}
static void aa_play(aa_stream *s, const void *buf, int frames)
{
    static __thread int16_t *conv; static __thread int conv_frames;
    if (frames > conv_frames) { free(conv); conv = malloc((size_t)frames * AA_CHANNELS * sizeof(int16_t)); conv_frames = conv ? frames : 0; }
    if (!conv) return;
    const int16_t *pcm = aa_convert(s, buf, frames, conv);
    if (tl_cocos_audio_hook) tl_cocos_audio_hook(pcm, frames, AA_CHANNELS, AA_RATE);
    else { struct timespec ts = { 0, (long)((double)frames * 1e9 / AA_RATE) }; nanosleep(&ts, NULL); }
}
static void *aa_pump(void *arg)
{
    aa_stream *s = arg;
    pthread_setname_np("aaudio-pump");
    int n = s->burst;
    /* sized for the app's format (Unity asks for float: a 16-bit buffer was half what it wrote) and the full buffer capacity, as Android's is */
    size_t frame_bytes = (size_t)s->channels * (s->format == AA_FORMAT_FLOAT ? 4 : 2), cap = (size_t)(n > AA_BURST * 8 ? n : AA_BURST * 8);
    uint8_t *buf = calloc(cap, frame_bytes);
    while (atomic_load(&s->run)) {
        memset(buf, 0, (size_t)n * frame_bytes);
        int r = s->cb(s, s->user, buf, n);
        if (!atomic_load(&s->run)) break;
        { static bool said; if (!said) { for (size_t i = 0; i < (size_t)n * frame_bytes; i++) if (buf[i]) { said = true; tl_log_line("aaudio: the game's first non-silent burst (byte %zu)", i); break; } } }
        aa_play(s, buf, n);
        atomic_fetch_add(&s->frames, n);
        if (r != 0) break;
    }
    free(buf);
    return NULL;
}

static int b_AAudioStream_requestStart(aa_stream *s)
{
    atomic_store(&s->state, AA_STATE_STARTED);
    if (!s->cb) return AA_OK;                                       /* written streams play as write() is called */
    if (atomic_exchange(&s->run, true)) return AA_OK;
    pthread_attr_t a; pthread_attr_init(&a); pthread_attr_setstacksize(&a, 1u << 20);
    s->thread_started = pthread_create(&s->thread, &a, aa_pump, s) == 0;
    pthread_attr_destroy(&a);
    return AA_OK;
}
static int aa_stop(aa_stream *s, int state)
{
    if (atomic_exchange(&s->run, false) && s->thread_started) { pthread_join(s->thread, NULL); s->thread_started = false; }
    atomic_store(&s->state, state);
    return AA_OK;
}
static int b_AAudioStream_requestStop(aa_stream *s) { return aa_stop(s, AA_STATE_STOPPED); }
static int b_AAudioStream_close(aa_stream *s) { aa_stop(s, AA_STATE_CLOSED); free(s); return AA_OK; }
static int b_AAudioStream_getSampleRate(aa_stream *s) { (void)s; return AA_RATE; }
static int b_AAudioStream_getChannelCount(aa_stream *s) { return s->channels; }
static int b_AAudioStream_getFormat(aa_stream *s) { return s->format; }
static int b_AAudioStream_getDeviceId(aa_stream *s) { (void)s; return 0; }
static int b_AAudioStream_getFramesPerBurst(aa_stream *s) { return s->burst; }
static int b_AAudioStream_getBufferCapacityInFrames(aa_stream *s) { (void)s; return AA_BURST * 8; }
static int b_AAudioStream_getBufferSizeInFrames(aa_stream *s) { return atomic_load(&s->buffer_size); }
static int b_AAudioStream_setBufferSizeInFrames(aa_stream *s, int n) { atomic_store(&s->buffer_size, n); return n; }
static int b_AAudioStream_getXRunCount(aa_stream *s) { (void)s; return 0; }
static int b_AAudioStream_getState(aa_stream *s) { return atomic_load(&s->state); }
static int b_AAudioStream_read(aa_stream *s, void *buf, int frames, long timeout) { (void)s; (void)buf; (void)frames; (void)timeout; return AA_UNAVAILABLE; }
static int b_AAudioStream_requestPause(aa_stream *s) { return aa_stop(s, AA_STATE_PAUSED); }
static int b_AAudioStream_requestFlush(aa_stream *s) { atomic_store(&s->state, AA_STATE_FLUSHED); return AA_OK; }
static int b_AAudioStream_release(aa_stream *s) { (void)s; return AA_OK; }
/* a stream without a data callback: the app pushes 16-bit stereo frames, and the host audio's blocking paces it */
static int b_AAudioStream_write(aa_stream *s, const void *buf, int frames, int64_t timeout)
{
    (void)timeout;
    if (atomic_load(&s->state) != AA_STATE_STARTED) return 0;
    aa_play(s, buf, frames);
    atomic_fetch_add(&s->frames, frames);
    return frames;
}
static int b_AAudioStream_waitForStateChange(aa_stream *s, int input, int *next, int64_t timeout_ns)
{
    for (int64_t waited = 0;; waited += 5000000) {
        int st = atomic_load(&s->state);
        if (st != input || waited >= timeout_ns) { if (next) *next = st; return st != input ? AA_OK : AA_TIMEOUT; }
        struct timespec ts = { 0, 5000000 }; nanosleep(&ts, NULL);
    }
}
static int b_AAudioStream_getTimestamp(aa_stream *s, int clockid, int64_t *pos, int64_t *ns)
{
    (void)clockid;
    struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t);
    if (pos) *pos = atomic_load(&s->frames);
    if (ns) *ns = (int64_t)t.tv_sec * 1000000000ll + t.tv_nsec;
    return AA_OK;
}
static int64_t b_AAudioStream_getFramesWritten(aa_stream *s) { return atomic_load(&s->frames); }
static int64_t b_AAudioStream_getFramesRead(aa_stream *s) { return atomic_load(&s->frames); }
static int b_AAudioStream_getPerformanceMode(aa_stream *s) { (void)s; return 10; }            /* NONE */
static int b_AAudioStream_getSharingMode(aa_stream *s) { (void)s; return 1; }                 /* SHARED */
static int b_AAudioStream_getUsage(aa_stream *s) { (void)s; return 14; }                      /* GAME */
static int b_AAudioStream_getContentType(aa_stream *s) { (void)s; return 2; }                 /* MUSIC */
static int b_AAudioStream_getInputPreset(aa_stream *s) { (void)s; return 6; }                 /* VOICE_RECOGNITION */
static int b_AAudioStream_getSessionId(aa_stream *s) { (void)s; return -1; }                  /* NONE */
static int b_AAudioStream_getAllowedCapturePolicy(aa_stream *s) { (void)s; return 1; }        /* ALL */
static bool b_AAudioStream_false(aa_stream *s) { (void)s; return false; }
static uint32_t b_AAudioStream_getChannelMask(aa_stream *s) { return s->channels == 1 ? 1 : 3; }   /* MONO / STEREO */
static int b_AAudioStream_getSpatializationBehavior(aa_stream *s) { (void)s; return 1; }      /* AUTO */
static int b_AAudioStream_getHardwareChannelCount(aa_stream *s) { (void)s; return AA_CHANNELS; }
static int b_AAudioStream_getHardwareSampleRate(aa_stream *s) { (void)s; return AA_RATE; }
static int b_AAudioStream_getHardwareFormat(aa_stream *s) { (void)s; return AA_FORMAT_I16; }
static const char *b_AAudio_convertResultToText(int r) { (void)r; return "AAudio is not available"; }

/* AAssetDir: the files directly under an asset directory, across the APKs */
typedef struct { char **names; size_t n, pos; } tl_assetdir;
static void *b_AAssetManager_openDir(void *mgr, const char *dir)
{
    (void)mgr;
    if (getenv("TL_FILE_TRACE")) tl_log_line("assets: openDir %s", dir);
    char prefix[1024];
    size_t pl = (size_t)snprintf(prefix, sizeof(prefix), "assets/%s%s", dir, dir[0] && dir[strlen(dir) - 1] != '/' ? "/" : "");
    tl_assetdir *d = calloc(1, sizeof(*d));
    size_t cap = 0;
    for (int i = 0;; i++) {
        const tl_zip *z = tl_ld_apk_at(i);
        if (!z) break;
        for (size_t k = 0; k < z->count; k++) {
            const char *name = z->entries[k].name;
            if (strncmp(name, prefix, pl) != 0 || !name[pl] || strchr(name + pl, '/')) continue;
            if (d->n == cap) { cap = cap ? cap * 2 : 64; d->names = realloc(d->names, cap * sizeof(char *)); }
            d->names[d->n++] = strdup(name + pl);
        }
    }
    return d;
}
static const char *b_AAssetDir_getNextFileName(tl_assetdir *d) { return d && d->pos < d->n ? d->names[d->pos++] : NULL; }
static void b_AAssetDir_close(tl_assetdir *d) { if (!d) return; for (size_t i = 0; i < d->n; i++) free(d->names[i]); free(d->names); free(d); }
static unsigned long b_deflateBound(void *strm, unsigned long n) { (void)strm; return n + (n >> 12) + (n >> 14) + (n >> 25) + 13; }

static void b_AAsset_close(tl_asset *a) { if (a) { free(a->owned); free(a); } }
static int b_AAsset_read(tl_asset *a, void *buf, size_t n)
{
    size_t left = a->len - a->pos;
    if (n > left) n = left;
    memcpy(buf, a->data + a->pos, n);
    a->pos += n;
    return (int)n;
}
static long b_AAsset_getLength(tl_asset *a) { return (long)a->len; }
static long b_AAsset_getRemainingLength(tl_asset *a) { return (long)(a->len - a->pos); }
static const void *b_AAsset_getBuffer(tl_asset *a) { return a->data; }
static long b_AAsset_seek(tl_asset *a, long off, int whence)
{
    long base = whence == 0 ? 0 : whence == 1 ? (long)a->pos : (long)a->len;
    long np = base + off;
    if (np < 0 || (size_t)np > a->len) return -1;
    a->pos = (size_t)np;
    return np;
}
static int b_AAsset_isAllocated(tl_asset *a) { return a->owned != NULL; }

/* ---------------------------------------------------------------- windows */

/*
 * The input queue a NativeActivity's glue attaches to its looper (Unreal Engine's does). Events are the NDK's opaque AInputEvent: here a small record the accessors
 * below read. A touch from the host is queued and a byte written to a pipe the looper watches, which is how the real queue wakes the thread that owns it.
 */
typedef struct tl_ievent {
    int type;                                   /* AINPUT_EVENT_TYPE_KEY 1, _MOTION 2 */
    int source, device, action, meta, flags, keycode, button_state, nptr;
    int pid[10]; float px[10], py[10];
    float axis[48]; bool has_axes;                /* a joystick event: every axis by Android's axis number */
    int64_t time;
    struct tl_ievent *next;
} tl_ievent;
static struct {
    int rd, wr, ident;
    bool ready, attached;
    pthread_mutex_t mu;
    tl_ievent *head, *tail;
    int nptr, pid[10]; float px[10], py[10];    /* the fingers down now, to say who else is touching when one changes */
} IQ = { .rd = -1, .wr = -1, .mu = PTHREAD_MUTEX_INITIALIZER };

static void iq_init(void)
{
    if (IQ.ready) return;
    int fds[2];
    if (pipe(fds) != 0) return;
    fcntl(fds[0], F_SETFL, O_NONBLOCK); fcntl(fds[1], F_SETFL, O_NONBLOCK);
    IQ.rd = fds[0]; IQ.wr = fds[1]; IQ.ready = true;
}
static void iq_push(tl_ievent *e)
{
    pthread_mutex_lock(&IQ.mu);
    if (IQ.tail) IQ.tail->next = e; else IQ.head = e;
    IQ.tail = e;
    pthread_mutex_unlock(&IQ.mu);
    char c = 1; (void)!write(IQ.wr, &c, 1);
}
/* A touch in surface pixels: phase 0 down, 1 move, 2 up, 3 cancel -- the same words the other engines are given. Safe from any thread. */
void tl_inq_touch(int phase, int id, float x, float y)
{
    iq_init();
    if (!IQ.ready || !IQ.attached) return;
    pthread_mutex_lock(&IQ.mu);
    int idx = -1;
    for (int i = 0; i < IQ.nptr; i++) if (IQ.pid[i] == id) idx = i;
    int action;
    if (phase == 0) {
        if (idx < 0 && IQ.nptr < 10) { idx = IQ.nptr++; IQ.pid[idx] = id; }
        if (idx < 0) { pthread_mutex_unlock(&IQ.mu); return; }
        IQ.px[idx] = x; IQ.py[idx] = y;
        action = IQ.nptr == 1 ? 0 /* DOWN */ : (5 /* POINTER_DOWN */ | (idx << 8));
    } else if (idx < 0) { pthread_mutex_unlock(&IQ.mu); return; }
    else if (phase == 1) { IQ.px[idx] = x; IQ.py[idx] = y; action = 2; }
    else if (phase == 2) { IQ.px[idx] = x; IQ.py[idx] = y; action = IQ.nptr == 1 ? 1 /* UP */ : (6 /* POINTER_UP */ | (idx << 8)); }
    else action = 3;
    tl_ievent *e = calloc(1, sizeof(*e));
    e->type = 2; e->source = 0x1002 /* touchscreen */; e->device = 1; e->action = action; e->nptr = IQ.nptr;
    for (int i = 0; i < IQ.nptr; i++) { e->pid[i] = IQ.pid[i]; e->px[i] = IQ.px[i]; e->py[i] = IQ.py[i]; }
    struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts); e->time = (int64_t)ts.tv_sec * 1000000000ll + ts.tv_nsec;
    if (phase == 2 || phase == 3) {                       /* the finger leaves the list after the event that says so */
        if (phase == 3) IQ.nptr = 0;
        else { for (int i = idx; i + 1 < IQ.nptr; i++) { IQ.pid[i] = IQ.pid[i + 1]; IQ.px[i] = IQ.px[i + 1]; IQ.py[i] = IQ.py[i + 1]; } IQ.nptr--; }
    }
    pthread_mutex_unlock(&IQ.mu);
    iq_push(e);
}

/* A controller button (action 0 down, 1 up, an Android key code) and the state of its sticks, triggers and hat (axes by Android axis number), from any thread. */
void tl_inq_key(int device, int source, int action, int keycode)
{
    iq_init();
    if (!IQ.ready || !IQ.attached) return;
    tl_ievent *e = calloc(1, sizeof(*e));
    e->type = 1; e->source = source; e->device = device; e->action = action; e->keycode = keycode;
    struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts); e->time = (int64_t)ts.tv_sec * 1000000000ll + ts.tv_nsec;
    iq_push(e);
}
void tl_inq_axes(int device, const float *axes48)
{
    iq_init();
    if (!IQ.ready || !IQ.attached) return;
    tl_ievent *e = calloc(1, sizeof(*e));
    e->type = 2; e->source = 0x01000010 /* joystick */; e->device = device; e->action = 2 /* MOVE */; e->nptr = 1; e->has_axes = true;
    memcpy(e->axis, axes48, sizeof(e->axis));
    e->px[0] = e->axis[0]; e->py[0] = e->axis[1];
    struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts); e->time = (int64_t)ts.tv_sec * 1000000000ll + ts.tv_nsec;
    iq_push(e);
}

static void b_AInputQueue_attachLooper(void *q, void *looper, int ident, void *cb, void *data)
{
    (void)q;
    iq_init();
    if (!IQ.ready || !looper) return;
    IQ.ident = ident; IQ.attached = true;
    b_ALooper_addFd(looper, IQ.rd, ident, ALOOPER_EVENT_INPUT, cb, data);
}
static void b_AInputQueue_detachLooper(void *q) { (void)q; IQ.attached = false; if (t_looper && IQ.ready) b_ALooper_removeFd(t_looper, IQ.rd); }
static int32_t b_AInputQueue_hasEvents(void *q) { (void)q; return IQ.head != NULL; }
static int32_t b_AInputQueue_getEvent(void *q, void **ev)
{
    (void)q;
    pthread_mutex_lock(&IQ.mu);
    tl_ievent *e = IQ.head;
    if (e) { IQ.head = e->next; if (!IQ.head) IQ.tail = NULL; }
    pthread_mutex_unlock(&IQ.mu);
    if (ev) *ev = e;
    if (e && IQ.ready) { char c; (void)!read(IQ.rd, &c, 1); }
    return e ? 0 : -1;
}
static int32_t b_AInputQueue_preDispatchEvent(void *q, void *ev) { (void)q; (void)ev; return 0; }
static void b_AInputQueue_finishEvent(void *q, void *ev, int handled) { (void)q; (void)handled; free(ev); }

static int32_t b_AInputEvent_getType(const tl_ievent *e) { return e->type; }
static int32_t b_AInputEvent_getSource(const tl_ievent *e) { return e->source; }
static int32_t b_AInputEvent_getDeviceId(const tl_ievent *e) { return e->device; }
static int32_t b_AMotionEvent_getAction(const tl_ievent *e) { return e->action; }
static int32_t b_AMotionEvent_getFlags(const tl_ievent *e) { return e->flags; }
static int32_t b_AMotionEvent_getMetaState(const tl_ievent *e) { return e->meta; }
static int32_t b_AMotionEvent_getButtonState(const tl_ievent *e) { return e->button_state; }
static int32_t b_AMotionEvent_getEdgeFlags(const tl_ievent *e) { (void)e; return 0; }
static int64_t b_AMotionEvent_getEventTime(const tl_ievent *e) { return e->time; }
static int64_t b_AMotionEvent_getDownTime(const tl_ievent *e) { return e->time; }
static size_t b_AMotionEvent_getPointerCount(const tl_ievent *e) { return (size_t)e->nptr; }
static int32_t b_AMotionEvent_getPointerId(const tl_ievent *e, size_t i) { return i < (size_t)e->nptr ? e->pid[i] : 0; }
static float b_AMotionEvent_getX(const tl_ievent *e, size_t i) { return i < (size_t)e->nptr ? e->px[i] : 0.0f; }
static float b_AMotionEvent_getY(const tl_ievent *e, size_t i) { return i < (size_t)e->nptr ? e->py[i] : 0.0f; }
static float b_AMotionEvent_getPressure(const tl_ievent *e, size_t i) { (void)e; (void)i; return 1.0f; }
static float b_AMotionEvent_getSize(const tl_ievent *e, size_t i) { (void)e; (void)i; return 0.1f; }
static size_t b_AMotionEvent_getHistorySize(const tl_ievent *e) { (void)e; return 0; }
static float b_AMotionEvent_getAxisValue(const tl_ievent *e, int32_t axis, size_t i)
{
    if (e->has_axes && axis >= 0 && axis < 48) return e->axis[axis];
    if (axis == 0) return b_AMotionEvent_getX(e, i);
    if (axis == 1) return b_AMotionEvent_getY(e, i);
    return 0.0f;
}
static int32_t b_AKeyEvent_getAction(const tl_ievent *e) { return e->action; }
static int32_t b_AKeyEvent_getKeyCode(const tl_ievent *e) { return e->keycode; }
static int32_t b_AKeyEvent_getFlags(const tl_ievent *e) { return e->flags; }
static int32_t b_AKeyEvent_getMetaState(const tl_ievent *e) { return e->meta; }
static int32_t b_AKeyEvent_getRepeatCount(const tl_ievent *e) { (void)e; return 0; }
static int32_t b_AKeyEvent_getScanCode(const tl_ievent *e) { (void)e; return 0; }
static void b_ANativeActivity_noop(void *a) { (void)a; }
static void b_ANativeActivity_flags(void *a, uint32_t add, uint32_t remove) { (void)a; (void)add; (void)remove; }
static void b_ANativeActivity_input(void *a, uint32_t flags) { (void)a; (void)flags; }


typedef struct tl_nwindow { atomic_int refs; int width, height, format; void *layer; } tl_nwindow;
static tl_nwindow g_window = { 1, 1080, 2400, 1, NULL };

void tl_nwindow_configure(int w, int h, void *layer) { g_window.width = w; g_window.height = h; g_window.layer = layer; }
void tl_nwindow_resize(int w, int h) { g_window.width = w; g_window.height = h; }
void *tl_nwindow_get(void) { atomic_fetch_add(&g_window.refs, 1); return &g_window; }
void *tl_nwindow_native(void *window) { return window ? ((tl_nwindow *)window)->layer : NULL; }
int tl_nwindow_width(void *window) { return window ? ((tl_nwindow *)window)->width : 0; }
int tl_nwindow_height(void *window) { return window ? ((tl_nwindow *)window)->height : 0; }

static void b_atrace_noop(void) {}
static bool b_atrace_false(void) { return false; }
int tl_AndroidBitmap_getInfo(void *env, void *bitmap, void *info);
int tl_AndroidBitmap_lockPixels(void *env, void *bitmap, void **addr);
int tl_AndroidBitmap_unlockPixels(void *env, void *bitmap);
int32_t tl_AndroidBitmap_getDataSpace(void *env, void *bitmap);
static void *b_ANativeWindow_fromSurface(void *env, void *surface) { (void)env; (void)surface; atomic_fetch_add(&g_window.refs, 1); return &g_window; }
static void b_ANativeWindow_acquire(tl_nwindow *w) { if (w) atomic_fetch_add(&w->refs, 1); }
static void b_ANativeWindow_release(tl_nwindow *w) { if (w) atomic_fetch_sub(&w->refs, 1); }
static int b_ANativeWindow_getWidth(tl_nwindow *w) { return w ? w->width : 0; }
static int b_ANativeWindow_getHeight(tl_nwindow *w) { return w ? w->height : 0; }
static int b_ANativeWindow_getFormat(tl_nwindow *w) { return w ? w->format : 0; }
static int b_ANativeWindow_setBuffersGeometry(tl_nwindow *w, int width, int height, int format)
{
    (void)width; (void)height; (void)format; (void)w;
    return 0;
}
static void *b_ANativeWindow_toSurface(void *env, void *w) { (void)env; (void)w; return NULL; }

/* ---------------------------------------------------------------- sensors */

static void *b_ASensorManager_getInstance(void) { static int m; return &m; }
static void *b_ASensorManager_getDefaultSensor(void *m, int type) { (void)m; (void)type; return NULL; }
static int b_ASensorManager_getSensorList(void *m, const void ***list) { (void)m; if (list) *list = NULL; return 0; }
static void *b_ASensorManager_createEventQueue(void *m, void *looper, int ident, void *cb, void *data)
{
    (void)m; (void)looper; (void)ident; (void)cb; (void)data;
    static int q; return &q;
}
static int b_ASensorManager_destroyEventQueue(void *m, void *q) { (void)m; (void)q; return 0; }
static int b_ASensorEventQueue_enableSensor(void *q, void *s) { (void)q; (void)s; return -1; }
static int b_ASensorEventQueue_disableSensor(void *q, void *s) { (void)q; (void)s; return -1; }
static int b_ASensorEventQueue_setEventRate(void *q, void *s, int us) { (void)q; (void)s; (void)us; return -1; }
static int b_ASensorEventQueue_hasEvents(void *q) { (void)q; return 0; }
static long b_ASensorEventQueue_getEvents(void *q, void *ev, size_t n) { (void)q; (void)ev; (void)n; return 0; }
static const char *b_ASensor_getName(void *s) { (void)s; return ""; }
static const char *b_ASensor_getVendor(void *s) { (void)s; return ""; }
static int b_ASensor_getType(void *s) { (void)s; return 0; }
static float b_ASensor_getResolution(void *s) { (void)s; return 0.f; }
static int b_ASensor_getMinDelay(void *s) { (void)s; return 0; }

/* ------------------------------------------------------------ media (none) */

#define MEDIA_ERR (-10000)
static void *b_media_null(void) { tl_note_once("libmediandk: media decoding is not provided (returning NULL)"); return NULL; }
static int b_media_err(void) { return MEDIA_ERR; }
static int b_media_zero(void) { return 0; }

#define KEY(sym, text) static const char *g_##sym = text;
KEY(AMEDIAFORMAT_KEY_CHANNEL_COUNT, "channel-count") KEY(AMEDIAFORMAT_KEY_COLOR_FORMAT, "color-format")
KEY(AMEDIAFORMAT_KEY_COLOR_RANGE, "color-range") KEY(AMEDIAFORMAT_KEY_COLOR_STANDARD, "color-standard")
KEY(AMEDIAFORMAT_KEY_DURATION, "durationUs") KEY(AMEDIAFORMAT_KEY_ENCODER_DELAY, "encoder-delay")
KEY(AMEDIAFORMAT_KEY_FRAME_RATE, "frame-rate") KEY(AMEDIAFORMAT_KEY_HEIGHT, "height")
KEY(AMEDIAFORMAT_KEY_LANGUAGE, "language") KEY(AMEDIAFORMAT_KEY_MIME, "mime")
KEY(AMEDIAFORMAT_KEY_ROTATION, "rotation-degrees") KEY(AMEDIAFORMAT_KEY_SAMPLE_RATE, "sample-rate")
KEY(AMEDIAFORMAT_KEY_SLICE_HEIGHT, "slice-height") KEY(AMEDIAFORMAT_KEY_STRIDE, "stride")
KEY(AMEDIAFORMAT_KEY_WIDTH, "width")

#define MEDIA_NULL(n)  TL_WRAP(n, b_media_null)
#define MEDIA_ERRF(n)  TL_WRAP(n, b_media_err)
#define MEDIA_DATA(n)  TL_DATA(#n, &g_##n)

const tl_bionic_entry tl_tab_ndk[] = {
    /* zlib */
    TL_DIRECT(deflate), TL_DIRECT(deflateEnd), TL_DIRECT(deflateInit2_), TL_DIRECT(deflateInit_),
    TL_DIRECT(inflate), TL_DIRECT(inflateEnd), TL_DIRECT(inflateInit2_), TL_DIRECT(inflateInit_), TL_DIRECT(zError),
    TL_DIRECT(compressBound), TL_DIRECT(zlibVersion), TL_DIRECT(compress), TL_DIRECT(compress2), TL_DIRECT(uncompress), TL_DIRECT(crc32), TL_DIRECT(adler32),
    TL_DIRECT(inflateReset), TL_DIRECT(inflateReset2), TL_DIRECT(deflateReset), TL_DIRECT(inflateSync), TL_DIRECT(deflateParams), TL_DIRECT(deflateSetDictionary), TL_DIRECT(inflateSetDictionary),
    /* looper */
    TL_WRAP("ALooper_prepare", b_ALooper_prepare), TL_WRAP("ALooper_forThread", b_ALooper_forThread),
    TL_WRAP("ALooper_acquire", b_ALooper_acquire), TL_WRAP("ALooper_release", b_ALooper_release),
    TL_WRAP("ALooper_wake", b_ALooper_wake), TL_WRAP("ALooper_pollOnce", b_ALooper_pollOnce), TL_WRAP("ALooper_pollAll", b_ALooper_pollAll),
    TL_WRAP("ALooper_addFd", b_ALooper_addFd), TL_WRAP("ALooper_removeFd", b_ALooper_removeFd),
    TL_WRAP("AChoreographer_getInstance", b_AChoreographer_getInstance),
    TL_WRAP("AChoreographer_postFrameCallback", b_AChoreographer_postFrameCallback),
    TL_WRAP("AChoreographer_postFrameCallbackDelayed", b_AChoreographer_postFrameCallbackDelayed),
    TL_WRAP("AChoreographer_postFrameCallback64", b_AChoreographer_postFrameCallback64),
    TL_WRAP("AChoreographer_postFrameCallbackDelayed64", b_AChoreographer_postFrameCallbackDelayed64),
    TL_WRAP("AChoreographer_postVsyncCallback", b_AChoreographer_postVsyncCallback),
    TL_WRAP("AChoreographer_registerRefreshRateCallback", b_AChoreographer_registerRefreshRateCallback),
    TL_WRAP("AChoreographer_unregisterRefreshRateCallback", b_AChoreographer_unregisterRefreshRateCallback),
    TL_WRAP("AChoreographerFrameCallbackData_getFrameTimeNanos", b_AChoreographerFrameCallbackData_getFrameTimeNanos),
    TL_WRAP("AChoreographerFrameCallbackData_getFrameTimelinesLength", b_AChoreographerFrameCallbackData_getFrameTimelinesLength),
    TL_WRAP("AChoreographerFrameCallbackData_getPreferredFrameTimelineIndex", b_AChoreographerFrameCallbackData_getPreferredFrameTimelineIndex),
    TL_WRAP("AChoreographerFrameCallbackData_getFrameTimelineVsyncId", b_AChoreographerFrameCallbackData_getFrameTimelineVsyncId),
    TL_WRAP("AChoreographerFrameCallbackData_getFrameTimelineExpectedPresentationTimeNanos", b_AChoreographerFrameCallbackData_getFrameTimelineExpectedPresentationTimeNanos),
    TL_WRAP("AChoreographerFrameCallbackData_getFrameTimelineDeadlineNanos", b_AChoreographerFrameCallbackData_getFrameTimelineDeadlineNanos),
    /* configuration */
    TL_WRAP("AConfiguration_new", b_AConfiguration_new), TL_WRAP("AConfiguration_delete", b_AConfiguration_delete),
    TL_WRAP("AConfiguration_fromAssetManager", b_AConfiguration_fromAssetManager), TL_WRAP("AConfiguration_getLanguage", b_AConfiguration_getLanguage),
    TL_WRAP("AConfiguration_getCountry", b_AConfiguration_getCountry), TL_WRAP("AConfiguration_getOrientation", b_AConfiguration_getOrientation),
    TL_WRAP("AConfiguration_getDensity", b_AConfiguration_getDensity), TL_WRAP("AConfiguration_getScreenLong", b_AConfiguration_getScreenLong),
    TL_WRAP("AConfiguration_getScreenSize", b_AConfiguration_getScreenSize), TL_WRAP("AConfiguration_getUiModeType", b_AConfiguration_getUiModeType),
    TL_WRAP("AConfiguration_getUiModeNight", b_AConfiguration_getUiModeNight), TL_WRAP("AConfiguration_getKeyboard", b_AConfiguration_getKeyboard),
    TL_WRAP("AConfiguration_getNavigation", b_AConfiguration_getNavigation), TL_WRAP("AConfiguration_getTouchscreen", b_AConfiguration_getTouchscreen),
    TL_WRAP("AConfiguration_getSdkVersion", b_AConfiguration_getSdkVersion),
    /* assets */
    TL_WRAP("AAudio_createStreamBuilder", b_AAudio_createStreamBuilder), TL_WRAP("AAudio_convertResultToText", b_AAudio_convertResultToText),
    TL_WRAP("AAudioStreamBuilder_delete", b_AAudioStreamBuilder_delete), TL_WRAP("AAudioStreamBuilder_setDataCallback", b_AAudioStreamBuilder_setDataCallback),
    TL_WRAP("AAudioStreamBuilder_setDirection", b_AAudioStreamBuilder_setDirection), TL_WRAP("AAudioStreamBuilder_setErrorCallback", b_AAudioStreamBuilder_setErrorCallback),
    TL_WRAP("AAudioStreamBuilder_openStream", b_AAudioStreamBuilder_openStream), TL_WRAP("AAudioStreamBuilder_setDeviceId", b_AAudioStreamBuilder_ignore),
    TL_WRAP("AAudioStreamBuilder_setBufferCapacityInFrames", b_AAudioStreamBuilder_ignore), TL_WRAP("AAudioStreamBuilder_setInputPreset", b_AAudioStreamBuilder_ignore),
    TL_WRAP("AAudioStreamBuilder_setPerformanceMode", b_AAudioStreamBuilder_ignore), TL_WRAP("AAudioStreamBuilder_setUsage", b_AAudioStreamBuilder_ignore),
    TL_WRAP("AAudioStreamBuilder_setSampleRate", b_AAudioStreamBuilder_ignore), TL_WRAP("AAudioStreamBuilder_setChannelCount", b_AAudioStreamBuilder_setChannelCount),
    TL_WRAP("AAudioStreamBuilder_setFormat", b_AAudioStreamBuilder_setFormat), TL_WRAP("AAudioStreamBuilder_setSharingMode", b_AAudioStreamBuilder_ignore),
    TL_WRAP("AAudioStream_requestStart", b_AAudioStream_requestStart), TL_WRAP("AAudioStream_requestStop", b_AAudioStream_requestStop),
    TL_WRAP("AAudioStream_close", b_AAudioStream_close), TL_WRAP("AAudioStream_getSampleRate", b_AAudioStream_getSampleRate),
    TL_WRAP("AAudioStream_getChannelCount", b_AAudioStream_getChannelCount), TL_WRAP("AAudioStream_getFormat", b_AAudioStream_getFormat),
    TL_WRAP("AAudioStream_getDeviceId", b_AAudioStream_getDeviceId), TL_WRAP("AAudioStream_getFramesPerBurst", b_AAudioStream_getFramesPerBurst),
    TL_WRAP("AAudioStream_getBufferCapacityInFrames", b_AAudioStream_getBufferCapacityInFrames), TL_WRAP("AAudioStream_getBufferSizeInFrames", b_AAudioStream_getBufferSizeInFrames),
    TL_WRAP("AAudioStream_setBufferSizeInFrames", b_AAudioStream_setBufferSizeInFrames), TL_WRAP("AAudioStream_getXRunCount", b_AAudioStream_getXRunCount),
    TL_WRAP("AAudioStream_getState", b_AAudioStream_getState), TL_WRAP("AAudioStream_read", b_AAudioStream_read),
    TL_WRAP("AAudioStreamBuilder_setFramesPerDataCallback", b_AAudioStreamBuilder_setFramesPerDataCallback),
    TL_WRAP("AAudioStreamBuilder_setContentType", b_AAudioStreamBuilder_ignore), TL_WRAP("AAudioStreamBuilder_setSessionId", b_AAudioStreamBuilder_ignore),
    TL_WRAP("AAudioStreamBuilder_setAllowedCapturePolicy", b_AAudioStreamBuilder_ignore), TL_WRAP("AAudioStreamBuilder_setPrivacySensitive", b_AAudioStreamBuilder_ignore),
    TL_WRAP("AAudioStreamBuilder_setPackageName", b_AAudioStreamBuilder_ignore), TL_WRAP("AAudioStreamBuilder_setAttributionTag", b_AAudioStreamBuilder_ignore),
    TL_WRAP("AAudioStreamBuilder_setChannelMask", b_AAudioStreamBuilder_setChannelMask), TL_WRAP("AAudioStreamBuilder_setIsContentSpatialized", b_AAudioStreamBuilder_ignore),
    TL_WRAP("AAudioStreamBuilder_setSpatializationBehavior", b_AAudioStreamBuilder_ignore),
    TL_WRAP("AAudioStream_write", b_AAudioStream_write), TL_WRAP("AAudioStream_waitForStateChange", b_AAudioStream_waitForStateChange),
    TL_WRAP("AAudioStream_getTimestamp", b_AAudioStream_getTimestamp), TL_WRAP("AAudioStream_release", b_AAudioStream_release),
    TL_WRAP("AAudioStream_getFramesRead", b_AAudioStream_getFramesRead), TL_WRAP("AAudioStream_getFramesWritten", b_AAudioStream_getFramesWritten),
    TL_WRAP("AAudioStream_getPerformanceMode", b_AAudioStream_getPerformanceMode), TL_WRAP("AAudioStream_getSharingMode", b_AAudioStream_getSharingMode),
    TL_WRAP("AAudioStream_requestPause", b_AAudioStream_requestPause), TL_WRAP("AAudioStream_requestFlush", b_AAudioStream_requestFlush),
    TL_WRAP("AAudioStream_getUsage", b_AAudioStream_getUsage), TL_WRAP("AAudioStream_getContentType", b_AAudioStream_getContentType),
    TL_WRAP("AAudioStream_getInputPreset", b_AAudioStream_getInputPreset), TL_WRAP("AAudioStream_getSessionId", b_AAudioStream_getSessionId),
    TL_WRAP("AAudioStream_getAllowedCapturePolicy", b_AAudioStream_getAllowedCapturePolicy), TL_WRAP("AAudioStream_isPrivacySensitive", b_AAudioStream_false),
    TL_WRAP("AAudioStream_getChannelMask", b_AAudioStream_getChannelMask), TL_WRAP("AAudioStream_isContentSpatialized", b_AAudioStream_false),
    TL_WRAP("AAudioStream_getSpatializationBehavior", b_AAudioStream_getSpatializationBehavior),
    TL_WRAP("AAudioStream_getHardwareChannelCount", b_AAudioStream_getHardwareChannelCount), TL_WRAP("AAudioStream_getHardwareSampleRate", b_AAudioStream_getHardwareSampleRate),
    TL_WRAP("AAudioStream_getHardwareFormat", b_AAudioStream_getHardwareFormat),
    TL_WRAP("AAssetManager_openDir", b_AAssetManager_openDir), TL_WRAP("AAssetDir_getNextFileName", b_AAssetDir_getNextFileName),
    TL_WRAP("AAssetDir_close", b_AAssetDir_close), TL_WRAP("deflateBound", b_deflateBound),
    TL_WRAP("AAssetManager_fromJava", b_AAssetManager_fromJava), TL_WRAP("AAssetManager_open", b_AAssetManager_open),
    TL_WRAP("AAsset_close", b_AAsset_close), TL_WRAP("AAsset_read", b_AAsset_read), TL_WRAP("AAsset_getLength", b_AAsset_getLength),
    TL_WRAP("AAsset_getLength64", b_AAsset_getLength), TL_WRAP("AAsset_getRemainingLength", b_AAsset_getRemainingLength),
    TL_WRAP("AAsset_getRemainingLength64", b_AAsset_getRemainingLength), TL_WRAP("AAsset_getBuffer", b_AAsset_getBuffer),
    TL_WRAP("AAsset_seek", b_AAsset_seek), TL_WRAP("AAsset_seek64", b_AAsset_seek), TL_WRAP("AAsset_isAllocated", b_AAsset_isAllocated),
    /* systrace (libandroid): nothing records traces here */
    TL_WRAP("ATrace_beginSection", b_atrace_noop), TL_WRAP("ATrace_endSection", b_atrace_noop), TL_WRAP("ATrace_isEnabled", b_atrace_false),
    TL_WRAP("ATrace_beginAsyncSection", b_atrace_noop), TL_WRAP("ATrace_endAsyncSection", b_atrace_noop), TL_WRAP("ATrace_setCounter", b_atrace_noop),
    /* bitmaps (libjnigraphics) */
    TL_WRAP("AndroidBitmap_getInfo", tl_AndroidBitmap_getInfo), TL_WRAP("AndroidBitmap_lockPixels", tl_AndroidBitmap_lockPixels),
    TL_WRAP("AndroidBitmap_unlockPixels", tl_AndroidBitmap_unlockPixels), TL_WRAP("AndroidBitmap_getDataSpace", tl_AndroidBitmap_getDataSpace),
    /* windows */
    TL_WRAP("ANativeWindow_fromSurface", b_ANativeWindow_fromSurface), TL_WRAP("ANativeWindow_acquire", b_ANativeWindow_acquire),
    TL_WRAP("ANativeWindow_release", b_ANativeWindow_release), TL_WRAP("ANativeWindow_getWidth", b_ANativeWindow_getWidth),
    TL_WRAP("ANativeWindow_getHeight", b_ANativeWindow_getHeight), TL_WRAP("ANativeWindow_getFormat", b_ANativeWindow_getFormat),
    TL_WRAP("ANativeWindow_setBuffersGeometry", b_ANativeWindow_setBuffersGeometry),
    TL_WRAP("ANativeWindow_toSurface", b_ANativeWindow_toSurface),
    TL_WRAP("AInputQueue_attachLooper", b_AInputQueue_attachLooper), TL_WRAP("AInputQueue_detachLooper", b_AInputQueue_detachLooper),
    TL_WRAP("AInputQueue_hasEvents", b_AInputQueue_hasEvents), TL_WRAP("AInputQueue_getEvent", b_AInputQueue_getEvent),
    TL_WRAP("AInputQueue_preDispatchEvent", b_AInputQueue_preDispatchEvent), TL_WRAP("AInputQueue_finishEvent", b_AInputQueue_finishEvent),
    TL_WRAP("AInputEvent_getType", b_AInputEvent_getType), TL_WRAP("AInputEvent_getSource", b_AInputEvent_getSource), TL_WRAP("AInputEvent_getDeviceId", b_AInputEvent_getDeviceId),
    TL_WRAP("AMotionEvent_getAction", b_AMotionEvent_getAction), TL_WRAP("AMotionEvent_getFlags", b_AMotionEvent_getFlags), TL_WRAP("AMotionEvent_getMetaState", b_AMotionEvent_getMetaState),
    TL_WRAP("AMotionEvent_getButtonState", b_AMotionEvent_getButtonState), TL_WRAP("AMotionEvent_getEdgeFlags", b_AMotionEvent_getEdgeFlags),
    TL_WRAP("AMotionEvent_getEventTime", b_AMotionEvent_getEventTime), TL_WRAP("AMotionEvent_getDownTime", b_AMotionEvent_getDownTime),
    TL_WRAP("AMotionEvent_getPointerCount", b_AMotionEvent_getPointerCount), TL_WRAP("AMotionEvent_getPointerId", b_AMotionEvent_getPointerId),
    TL_WRAP("AMotionEvent_getX", b_AMotionEvent_getX), TL_WRAP("AMotionEvent_getY", b_AMotionEvent_getY), TL_WRAP("AMotionEvent_getRawX", b_AMotionEvent_getX), TL_WRAP("AMotionEvent_getRawY", b_AMotionEvent_getY), TL_WRAP("AMotionEvent_getPressure", b_AMotionEvent_getPressure),
    TL_WRAP("AMotionEvent_getSize", b_AMotionEvent_getSize), TL_WRAP("AMotionEvent_getHistorySize", b_AMotionEvent_getHistorySize), TL_WRAP("AMotionEvent_getAxisValue", b_AMotionEvent_getAxisValue),
    TL_WRAP("AKeyEvent_getAction", b_AKeyEvent_getAction), TL_WRAP("AKeyEvent_getKeyCode", b_AKeyEvent_getKeyCode), TL_WRAP("AKeyEvent_getFlags", b_AKeyEvent_getFlags),
    TL_WRAP("AKeyEvent_getMetaState", b_AKeyEvent_getMetaState), TL_WRAP("AKeyEvent_getRepeatCount", b_AKeyEvent_getRepeatCount), TL_WRAP("AKeyEvent_getScanCode", b_AKeyEvent_getScanCode),
    TL_WRAP("ANativeActivity_finish", b_ANativeActivity_noop), TL_WRAP("ANativeActivity_setWindowFormat", b_ANativeActivity_input), TL_WRAP("ANativeActivity_setWindowFlags", b_ANativeActivity_flags),
    TL_WRAP("ANativeActivity_showSoftInput", b_ANativeActivity_input), TL_WRAP("ANativeActivity_hideSoftInput", b_ANativeActivity_input),
    /* sensors */
    TL_WRAP("ASensorManager_getInstance", b_ASensorManager_getInstance), TL_WRAP("ASensorManager_getDefaultSensor", b_ASensorManager_getDefaultSensor),
    TL_WRAP("ASensorManager_getSensorList", b_ASensorManager_getSensorList), TL_WRAP("ASensorManager_createEventQueue", b_ASensorManager_createEventQueue),
    TL_WRAP("ASensorManager_destroyEventQueue", b_ASensorManager_destroyEventQueue), TL_WRAP("ASensorEventQueue_enableSensor", b_ASensorEventQueue_enableSensor),
    TL_WRAP("ASensorEventQueue_disableSensor", b_ASensorEventQueue_disableSensor), TL_WRAP("ASensorEventQueue_setEventRate", b_ASensorEventQueue_setEventRate),
    TL_WRAP("ASensorEventQueue_hasEvents", b_ASensorEventQueue_hasEvents), TL_WRAP("ASensorEventQueue_getEvents", b_ASensorEventQueue_getEvents),
    TL_WRAP("ASensor_getName", b_ASensor_getName), TL_WRAP("ASensor_getVendor", b_ASensor_getVendor), TL_WRAP("ASensor_getType", b_ASensor_getType),
    TL_WRAP("ASensor_getResolution", b_ASensor_getResolution), TL_WRAP("ASensor_getMinDelay", b_ASensor_getMinDelay),
    /* media */
    MEDIA_NULL("AMediaCodec_createDecoderByType"), MEDIA_NULL("AMediaExtractor_new"), MEDIA_NULL("AMediaCodec_getOutputFormat"),
    MEDIA_NULL("AMediaExtractor_getTrackFormat"), MEDIA_NULL("AMediaCodec_getInputBuffer"), MEDIA_NULL("AMediaCodec_getOutputBuffer"),
    MEDIA_ERRF("AMediaCodec_configure"), MEDIA_ERRF("AMediaCodec_start"), MEDIA_ERRF("AMediaCodec_stop"), MEDIA_ERRF("AMediaCodec_flush"),
    MEDIA_ERRF("AMediaCodec_delete"), MEDIA_ERRF("AMediaCodec_dequeueInputBuffer"), MEDIA_ERRF("AMediaCodec_dequeueOutputBuffer"),
    MEDIA_ERRF("AMediaCodec_queueInputBuffer"), MEDIA_ERRF("AMediaCodec_releaseOutputBuffer"),
    MEDIA_ERRF("AMediaExtractor_setDataSource"), MEDIA_ERRF("AMediaExtractor_setDataSourceFd"), MEDIA_ERRF("AMediaExtractor_selectTrack"),
    MEDIA_ERRF("AMediaExtractor_seekTo"), MEDIA_ERRF("AMediaExtractor_delete"), MEDIA_ERRF("AMediaExtractor_advance"),
    MEDIA_ERRF("AMediaExtractor_readSampleData"), MEDIA_ERRF("AMediaExtractor_getSampleTime"), MEDIA_ERRF("AMediaExtractor_getSampleTrackIndex"),
    TL_WRAP("AMediaExtractor_getTrackCount", b_media_zero), MEDIA_ERRF("AMediaFormat_delete"), MEDIA_ERRF("AMediaFormat_getFloat"),
    MEDIA_ERRF("AMediaFormat_getInt32"), MEDIA_ERRF("AMediaFormat_getInt64"), MEDIA_ERRF("AMediaFormat_getString"),
    MEDIA_ERRF("AMediaFormat_setInt32"),
    MEDIA_DATA(AMEDIAFORMAT_KEY_CHANNEL_COUNT), MEDIA_DATA(AMEDIAFORMAT_KEY_COLOR_FORMAT), MEDIA_DATA(AMEDIAFORMAT_KEY_COLOR_RANGE),
    MEDIA_DATA(AMEDIAFORMAT_KEY_COLOR_STANDARD), MEDIA_DATA(AMEDIAFORMAT_KEY_DURATION), MEDIA_DATA(AMEDIAFORMAT_KEY_ENCODER_DELAY),
    MEDIA_DATA(AMEDIAFORMAT_KEY_FRAME_RATE), MEDIA_DATA(AMEDIAFORMAT_KEY_HEIGHT), MEDIA_DATA(AMEDIAFORMAT_KEY_LANGUAGE),
    MEDIA_DATA(AMEDIAFORMAT_KEY_MIME), MEDIA_DATA(AMEDIAFORMAT_KEY_ROTATION), MEDIA_DATA(AMEDIAFORMAT_KEY_SAMPLE_RATE),
    MEDIA_DATA(AMEDIAFORMAT_KEY_SLICE_HEIGHT), MEDIA_DATA(AMEDIAFORMAT_KEY_STRIDE), MEDIA_DATA(AMEDIAFORMAT_KEY_WIDTH),
    TL_END
};
