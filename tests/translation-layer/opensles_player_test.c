/* SPDX-License-Identifier: GPL-2.0-or-later */
#include <assert.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#define pthread_setname_np(...) ((void)0)
void (*tl_cocos_audio_hook)(const int16_t *, int, int, int);
void tl_log_line(const char *fmt, ...) { (void)fmt; }
int tl_guest_pthread_create(pthread_t *p, const pthread_attr_t *a, void *(*f)(void *), void *v)
{ return pthread_create(p, a, f, v); }
int tl_guest_pthread_join(pthread_t p, void **v) { return pthread_join(p, v); }
#include "../../src/translation-layer-next/husk-tl-opensles.c"


#include <stdatomic.h>
#include <string.h>
#include <unistd.h>
typedef struct { int64_t position, length; int rate, channels; bool short_eof; } fake_decoder;
static atomic_int decoder_opens, decoder_closes, stream_closes, callback_count, bq_count, seek_calls;
static atomic_bool stalled;
static pthread_mutex_t fake_mu = PTHREAD_MUTEX_INITIALIZER;
static struct { bool used, playing; uint64_t frames; float left, right; } streams[32];
static int opened_fd; static int64_t opened_offset, opened_length;
static void *fake_open(int rate, int channels, int *r, int *c, int64_t *frames)
{
    fake_decoder *d = calloc(1, sizeof(*d)); assert(d);
    d->rate = *r = rate; d->channels = *c = channels; d->length = *frames = rate / 10;
    atomic_fetch_add(&decoder_opens, 1); return d;
}
void *tl_audio_decoder_open_uri(const char *uri, int *r, int *c, int64_t *f)
{
    if (!strstr(uri, "synthetic-tone.wav")) return NULL;
    fake_decoder *d = fake_open(44100, 2, r, c, f);
    d->short_eof = strstr(uri, "early-eof") != NULL;
    return d;
}
void *tl_audio_decoder_open_fd(int fd, int64_t offset, int64_t length, int *r, int *c, int64_t *f)
{ opened_fd = fd; opened_offset = offset; opened_length = length; return fake_open(48000, 1, r, c, f); }
int tl_audio_decoder_read(void *opaque, int16_t *pcm, int frames)
{
    fake_decoder *d = opaque;
    int64_t cap = d->short_eof ? d->length / 2 : d->length;
    int n = cap > d->position ? (cap - d->position < frames ? (int)(cap - d->position) : frames) : 0;
    for (int i = 0; i < n * d->channels; i++) pcm[i] = 1234;
    d->position += n; return n;
}
bool tl_audio_decoder_seek(void *opaque, int64_t frame)
{ fake_decoder *d = opaque; if (frame < 0 || frame > d->length) return false; d->position = frame; atomic_fetch_add(&seek_calls, 1); return true; }
void tl_audio_decoder_close(void *d) { free(d); atomic_fetch_add(&decoder_closes, 1); }
int tl_audio_stream_open(int rate, int channels)
{
    assert(rate > 0 && (channels == 1 || channels == 2)); pthread_mutex_lock(&fake_mu);
    for (int i = 0; i < 32; i++) if (!streams[i].used) {
        memset(&streams[i], 0, sizeof(streams[i])); streams[i].used = true; streams[i].left = streams[i].right = 1;
        pthread_mutex_unlock(&fake_mu); return i;
    }
    pthread_mutex_unlock(&fake_mu); return -1;
}
int tl_audio_stream_write(int id, const int16_t *pcm, int frames)
{
    assert(pcm && frames > 0); if (atomic_load(&stalled)) return 0;
    pthread_mutex_lock(&fake_mu); assert(streams[id].used); streams[id].frames += (uint64_t)frames; pthread_mutex_unlock(&fake_mu);
    usleep(100); return frames;
}
void tl_audio_stream_play(int id, bool playing)
{ pthread_mutex_lock(&fake_mu); streams[id].playing = playing; pthread_mutex_unlock(&fake_mu); }
void tl_audio_stream_flush(int id)
{ pthread_mutex_lock(&fake_mu); streams[id].frames = 0; pthread_mutex_unlock(&fake_mu); }
void tl_audio_stream_volume(int id, float left, float right)
{ if (id < 0) return; pthread_mutex_lock(&fake_mu); streams[id].left = left; streams[id].right = right; pthread_mutex_unlock(&fake_mu); }
uint64_t tl_audio_stream_position(int id)
{ pthread_mutex_lock(&fake_mu); uint64_t n = streams[id].frames; pthread_mutex_unlock(&fake_mu); return n; }
int tl_audio_stream_pending(int id) { (void)id; return 0; }
void tl_audio_stream_close(int id)
{ pthread_mutex_lock(&fake_mu); streams[id].used = false; pthread_mutex_unlock(&fake_mu); atomic_fetch_add(&stream_closes, 1); }
static void wait_count(atomic_int *value, int count)
{ for (int i = 0; i < 2000 && atomic_load(value) < count; i++) usleep(1000); assert(atomic_load(value) >= count); }
static void ended(const void *caller, void *context, uint32_t event)
{ (void)caller; (void)context; assert(event == 1); atomic_fetch_add(&callback_count, 1); }
static void ended_destroy(const void *caller, void *context, uint32_t event)
{ (void)caller; assert(event == 1); o_Destroy(context); atomic_fetch_add(&callback_count, 1); }
static void queued(void *caller, void *context)
{ (void)caller; (void)context; atomic_fetch_add(&bq_count, 1); }
static const void *make_url(void)
{
    sl_uri uri = { 1, "file:///synthetic-tone.wav" };
    struct { uint32_t type; const char *mime; uint32_t container; } mime = { 1, NULL, 0 };
    sl_data source = { &uri, &mime };
    const sl_iid *ids[] = { &k_iid_x_seek, &k_iid_x_prefetchstatus, &k_iid_volume };
    uint32_t required[] = { 1, 1, 1 }; const void *player = NULL;
    assert(e_CreateAudioPlayer(NULL, &player, &source, NULL, 3, ids, required) == SL_OK);
    assert(player && o_Realize(player, 0) == SL_OK); return player;
}
int main(void)
{
    const void *player = make_url(), *seek = NULL, *play = NULL, *volume = NULL, *prefetch = NULL;
    assert(o_GetInterface(player, &k_iid_x_seek, &seek) == SL_OK && seek);
    assert(o_GetInterface(player, &k_iid_play, &play) == SL_OK && play);
    assert(o_GetInterface(player, &k_iid_volume, &volume) == SL_OK && volume);
    assert(o_GetInterface(player, &k_iid_x_prefetchstatus, &prefetch) == SL_OK && prefetch);
    uint32_t duration = 0, position = 0, state = 0; int16_t fill = 0;
    assert(p_GetDuration(play, &duration) == SL_OK && duration == 100);
    assert(f_GetFill(prefetch, &fill) == SL_OK && fill == 1000);
    assert(f_GetStatus(prefetch, &state) == SL_OK && state == 2);
    assert(s_SetPosition(seek, 25, 2) == SL_OK);
    assert(p_GetPosition(play, &position) == SL_OK && position == 24); /* integer PCM frame boundary */
    assert(s_SetPosition(seek, 101, 2) == SL_PARAMETER_INVALID);
    assert(v_SetLevel(volume, -600) == SL_OK); sl_object *o = (sl_object *)player;
    assert(streams[o->stream_id].left > 0.50f && streams[o->stream_id].left < 0.502f);
    assert(v_SetMute(volume, 1) == SL_OK && streams[o->stream_id].left == 0);
    assert(v_SetMute(volume, 0) == SL_OK);
    assert(p_RegisterCallback(play, ended, NULL) == SL_OK && p_SetEvents(play, 1) == SL_OK);
    assert(p_SetPlayState(play, SL_PLAYING) == SL_OK); wait_count(&callback_count, 1);
    assert(p_GetPosition(play, &position) == SL_OK && position == 100);
    assert(p_SetPlayState(play, SL_STOPPED) == SL_OK);
    assert(p_GetPosition(play, &position) == SL_OK && position == 0);
    assert(s_SetLoop(seek, 1, 0, UINT32_MAX) == SL_OK);
    assert(s_SetLoop(seek, 1, 100, 10) == SL_PARAMETER_INVALID);
    int before = atomic_load(&seek_calls);
    assert(p_SetPlayState(play, SL_PLAYING) == SL_OK); wait_count(&seek_calls, before + 2);
    assert(p_SetPlayState(play, SL_PAUSED) == SL_OK);
    uint32_t loop, start, end; assert(s_GetLoop(seek, &loop, &start, &end) == SL_OK && loop && !start && end == UINT32_MAX);
    o_Destroy(player);

    sl_fd fd = { 0x800007BCu, 17, 123, 456 }; uint32_t mime = 1; sl_data source = { &fd, &mime };
    player = NULL; assert(e_CreateAudioPlayer(NULL, &player, &source, NULL, 0, NULL, NULL) == SL_OK);
    assert(opened_fd == 17 && opened_offset == 123 && opened_length == 456);
    o_Destroy(player);

    sl_uri eof_uri = { 1, "file:///synthetic-tone.wav?early-eof=1" };
    sl_data eof_src = { &eof_uri, NULL }; /* URI players do not require pFormat */
    player = NULL; assert(e_CreateAudioPlayer(NULL, &player, &eof_src, NULL, 0, NULL, NULL) == SL_OK);
    assert(o_Realize(player, 0) == SL_OK);
    assert(o_GetInterface(player, &k_iid_play, &play) == SL_OK);
    assert(o_GetInterface(player, &k_iid_x_seek, &seek) == SL_OK);
    assert(s_SetLoop(seek, 1, 0, UINT32_MAX) == SL_OK);
    before = atomic_load(&seek_calls);
    assert(p_SetPlayState(play, SL_PLAYING) == SL_OK);
    wait_count(&seek_calls, before + 2);
    o_Destroy(player);

    uint32_t locator = SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE;
    sl_pcm pcm = { SL_DATAFORMAT_PCM, 2, 44100000, 16, 16, 3, 2 }; source = (sl_data){ &locator, &pcm };
    assert(e_CreateAudioPlayer(NULL, &player, &source, NULL, 0, NULL, NULL) == SL_OK);
    const void *queue = NULL; assert(o_GetInterface(player, &k_iid_asbq, &queue) == SL_OK);
    assert(o_GetInterface(player, &k_iid_x_seek, &seek) == SL_FEATURE_UNSUPPORTED);
    assert(o_GetInterface(player, &k_iid_play, &play) == SL_OK);
    int16_t samples[128] = { 42 };
    assert(q_Enqueue(queue, samples, sizeof(samples)) == SL_OK);
    assert(q_RegisterCallback(queue, queued, NULL) == SL_OK);
    assert(p_SetPlayState(play, SL_PLAYING) == SL_OK); wait_count(&bq_count, 1);
    atomic_store(&stalled, true); assert(q_Enqueue(queue, samples, sizeof(samples)) == SL_OK);
    usleep(2000); o_Destroy(player); atomic_store(&stalled, false); /* shutdown cannot block on a full ring */

    pcm.bits = 32; player = (void *)1;
    assert(e_CreateAudioPlayer(NULL, &player, &source, NULL, 0, NULL, NULL) == 9 && !player);
    player = make_url(); assert(o_GetInterface(player, &k_iid_play, &play) == SL_OK);
    int closed_before = atomic_load(&stream_closes), calls_before = atomic_load(&callback_count);
    assert(p_RegisterCallback(play, ended_destroy, (void *)player) == SL_OK && p_SetEvents(play, 1) == SL_OK);
    assert(p_SetPlayState(play, SL_PLAYING) == SL_OK); pthread_t worker = ((sl_object *)player)->thread;
    wait_count(&callback_count, calls_before + 1); wait_count(&stream_closes, closed_before + 1); assert(!pthread_join(worker, NULL));
    assert(atomic_load(&decoder_opens) == atomic_load(&decoder_closes));
    puts("PASS: actual OpenSL URI/FD and PCM player creation, required SEEK/prefetch, seek/loop/duration/volume, EOF callbacks, raw queue callbacks and bounded/self-callback shutdown");
    return 0;
}
