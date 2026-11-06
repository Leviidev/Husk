/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Decoders for Java apps' android.media.MediaCodec (husk.MediaCodecNative's natives): H.264 and HEVC through VideoToolbox, AAC and
 * MP3 through AudioToolbox -- the same frameworks on iOS and macOS.
 *
 * Video: MediaCodec takes Annex-B access units (start codes, parameter sets in band or as csd-0/csd-1); VideoToolbox takes
 * length-prefixed NAL units under a format description built from the parameter sets. Each queued access unit is converted and
 * decoded synchronously; decoded frames (32BGRA pixel buffers) come out in decode order, so they wait in a small buffer sorted by
 * presentation time and leave it in display order, as MediaCodec hands them out. The Java side draws a frame into a Bitmap
 * (renderFrame) and the Bitmap into the app's Surface.
 *
 * Audio: one compressed packet per queued buffer (as MediaCodec is fed), decoded to interleaved 16-bit PCM.
 */
#define _DARWIN_C_SOURCE
#include <Accelerate/Accelerate.h>
#include <AudioToolbox/AudioToolbox.h>
#include <CoreMedia/CoreMedia.h>
#include <CoreVideo/CoreVideo.h>
#include <VideoToolbox/VideoToolbox.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "husk-tl-dvm-internal.h"

void tl_log_line(const char *fmt, ...);
typedef struct { uint32_t width, height, stride; int32_t format; uint32_t flags; } tl_abitmap_info;
int tl_AndroidBitmap_getInfo(void *env, void *bitmap, tl_abitmap_info *info);
int tl_AndroidBitmap_lockPixels(void *env, void *bitmap, void **addr);

#define NAT(fn) static bool fn(jobj *self, const jvalue *a, jvalue *ret)
static jvalue I(int32_t v) { jvalue r; r.j = (uint32_t)v; return r; }
static jvalue J(int64_t v) { jvalue r; r.j = v; return r; }
static jvalue Z(bool v) { jvalue r; r.j = v; return r; }
static jvalue L(void *p) { jvalue r; r.j = 0; r.l = p; return r; }

enum { K_AVC = 1, K_HEVC, K_AAC, K_MP3 };
enum { FLAG_EOS = 4 };
#define REORDER 4                                   /* frames held to put decode order into display order */

typedef struct frame { CVPixelBufferRef pb; int64_t pts; struct frame *next; } frame;
typedef struct pcm { uint8_t *data; int len; int64_t pts; struct pcm *next; } pcm;
typedef struct { uint8_t *d; size_t n; } blob;

typedef struct codec {
    struct codec *next;
    int kind;
    pthread_mutex_t lock;
    bool trace;
    /* video */
    blob vps, sps, pps;
    bool params_changed;
    CMVideoFormatDescriptionRef fmt;
    VTDecompressionSessionRef vt;
    frame *sorted; int nsorted;                     /* decoded, by presentation time */
    frame *ready, **ready_tail;                     /* in display order, for the Java side */
    int width, height;
    long decoded, failed;
    /* audio */
    AudioConverterRef conv;
    AudioStreamBasicDescription in, out;
    const uint8_t *packet; UInt32 packet_len; AudioStreamPacketDescription desc;
    pcm *pcms, **pcm_tail;
    int rate, channels;
} codec;
static codec *g_codecs;
static pthread_mutex_t g_codec_lock = PTHREAD_MUTEX_INITIALIZER;

static bool trace_on(void) { static int t = -1; if (t < 0) t = getenv("TL_MEDIA_TRACE") != NULL; return t; }

/* ---- video ---- */

static void blob_set(blob *b, const uint8_t *d, size_t n, bool *changed)
{
    if (b->n == n && b->d && !memcmp(b->d, d, n)) return;
    free(b->d); b->d = malloc(n); memcpy(b->d, d, n); b->n = n;
    *changed = true;
}

static void ready_push(codec *c, frame *f) { f->next = NULL; *c->ready_tail = f; c->ready_tail = &f->next; }
/* the earliest sorted frame moves to the ready queue */
static void release_sorted(codec *c, int keep)
{
    while (c->nsorted > keep) {
        frame *f = c->sorted; c->sorted = f->next; c->nsorted--;
        ready_push(c, f);
    }
}

static void vt_out(void *ref, void *src, OSStatus st, VTDecodeInfoFlags fl, CVImageBufferRef img, CMTime pts, CMTime dur)
{
    (void)src; (void)fl; (void)dur;
    codec *c = ref;
    if (st != noErr || !img) { c->failed++; if (c->trace || c->failed < 4) tl_log_line("media: frame not decoded (%d)", (int)st); return; }
    frame *f = calloc(1, sizeof(*f));
    f->pb = CVPixelBufferRetain(img);
    f->pts = CMTIME_IS_VALID(pts) ? CMTimeConvertScale(pts, 1000000, kCMTimeRoundingMethod_Default).value : 0;
    pthread_mutex_lock(&c->lock);
    frame **p = &c->sorted;
    while (*p && (*p)->pts <= f->pts) p = &(*p)->next;
    f->next = *p; *p = f; c->nsorted++;
    release_sorted(c, REORDER);
    c->decoded++;
    pthread_mutex_unlock(&c->lock);
}

static void session_drop(codec *c)
{
    if (c->vt) { VTDecompressionSessionWaitForAsynchronousFrames(c->vt); VTDecompressionSessionInvalidate(c->vt); CFRelease(c->vt); c->vt = NULL; }
}

static bool session_make(codec *c)
{
    if (c->fmt) { CFRelease(c->fmt); c->fmt = NULL; }
    OSStatus st;
    if (c->kind == K_AVC) {
        if (!c->sps.n || !c->pps.n) return false;
        const uint8_t *ps[2] = { c->sps.d, c->pps.d }; size_t sz[2] = { c->sps.n, c->pps.n };
        st = CMVideoFormatDescriptionCreateFromH264ParameterSets(NULL, 2, ps, sz, 4, &c->fmt);
    } else {
        if (!c->vps.n || !c->sps.n || !c->pps.n) return false;
        const uint8_t *ps[3] = { c->vps.d, c->sps.d, c->pps.d }; size_t sz[3] = { c->vps.n, c->sps.n, c->pps.n };
        st = CMVideoFormatDescriptionCreateFromHEVCParameterSets(NULL, 3, ps, sz, 4, NULL, &c->fmt);
    }
    if (st != noErr) { tl_log_line("media: the stream's parameter sets were refused (%d)", (int)st); c->fmt = NULL; return false; }
    CGSize dims = CMVideoFormatDescriptionGetPresentationDimensions(c->fmt, true, false);
    c->width = (int)dims.width; c->height = (int)dims.height;
    if (c->vt && VTDecompressionSessionCanAcceptFormatDescription(c->vt, c->fmt)) return true;
    session_drop(c);
    int fmt = kCVPixelFormatType_32BGRA;
    CFNumberRef n = CFNumberCreate(NULL, kCFNumberIntType, &fmt);
    const void *k[] = { kCVPixelBufferPixelFormatTypeKey }, *v[] = { n };
    CFDictionaryRef dest = CFDictionaryCreate(NULL, k, v, 1, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    VTDecompressionOutputCallbackRecord cb = { vt_out, c };
    st = VTDecompressionSessionCreate(NULL, c->fmt, NULL, dest, &cb, &c->vt);
    CFRelease(dest); CFRelease(n);
    if (st != noErr) { tl_log_line("media: no decoder session (%d)", (int)st); c->vt = NULL; return false; }
    tl_log_line("media: %s decoder for %dx%d", c->kind == K_AVC ? "H.264" : "HEVC", c->width, c->height);
    return true;
}

static int nal_type(const codec *c, const uint8_t *n) { return c->kind == K_AVC ? (n[0] & 0x1f) : ((n[0] >> 1) & 0x3f); }

/* Annex-B (start codes) or already length-prefixed units: each NAL unit to fn */
static void each_nal(const uint8_t *d, size_t n, void (*fn)(void *, const uint8_t *, size_t), void *arg)
{
    bool annexb = n >= 3 && d[0] == 0 && d[1] == 0 && (d[2] == 1 || (n >= 4 && d[2] == 0 && d[3] == 1));
    if (!annexb) {
        size_t i = 0;
        while (i + 4 <= n) {
            uint32_t len = (uint32_t)d[i] << 24 | (uint32_t)d[i + 1] << 16 | (uint32_t)d[i + 2] << 8 | d[i + 3];
            i += 4;
            if (len == 0 || len > n - i) break;
            fn(arg, d + i, len); i += len;
        }
        return;
    }
    size_t i = 0, start = (size_t)-1;
    while (i + 3 <= n) {
        if (d[i] == 0 && d[i + 1] == 0 && d[i + 2] == 1) {
            if (start != (size_t)-1) {
                size_t end = i; while (end > start && d[end - 1] == 0) end--;     /* the zero before a 4-byte start code */
                if (end > start) fn(arg, d + start, end - start);
            }
            i += 3; start = i;
        } else i++;
    }
    if (start != (size_t)-1 && start < n) fn(arg, d + start, n - start);
}

typedef struct { codec *c; uint8_t *out; size_t len, cap; } nal_walk;
static void take_nal(void *arg, const uint8_t *nal, size_t n)
{
    nal_walk *w = arg; codec *c = w->c;
    if (n == 0) return;
    int t = nal_type(c, nal);
    if (c->kind == K_AVC) {
        if (t == 7) { blob_set(&c->sps, nal, n, &c->params_changed); return; }
        if (t == 8) { blob_set(&c->pps, nal, n, &c->params_changed); return; }
        if (t == 9) return;                                         /* access unit delimiter */
    } else {
        if (t == 32) { blob_set(&c->vps, nal, n, &c->params_changed); return; }
        if (t == 33) { blob_set(&c->sps, nal, n, &c->params_changed); return; }
        if (t == 34) { blob_set(&c->pps, nal, n, &c->params_changed); return; }
        if (t == 35) return;
    }
    if (w->len + n + 4 > w->cap) { w->cap = (w->len + n + 4) * 2; w->out = realloc(w->out, w->cap); }
    w->out[w->len++] = (uint8_t)(n >> 24); w->out[w->len++] = (uint8_t)(n >> 16); w->out[w->len++] = (uint8_t)(n >> 8); w->out[w->len++] = (uint8_t)n;
    memcpy(w->out + w->len, nal, n); w->len += n;
}

static void video_drain(codec *c)
{
    if (c->vt) { VTDecompressionSessionFinishDelayedFrames(c->vt); VTDecompressionSessionWaitForAsynchronousFrames(c->vt); }
    pthread_mutex_lock(&c->lock); release_sorted(c, 0); pthread_mutex_unlock(&c->lock);
}

static void video_decode(codec *c, const uint8_t *d, size_t n, int64_t pts, int flags)
{
    nal_walk w = { c, NULL, 0, 0 };
    each_nal(d, n, take_nal, &w);
    if (c->params_changed) {
        c->params_changed = false;
        if ((c->vt || c->fmt) && c->vt) video_drain(c);
        session_make(c);
    }
    if (w.len && c->vt) {
        CMBlockBufferRef bb = NULL; CMSampleBufferRef sb = NULL;
        OSStatus st = CMBlockBufferCreateWithMemoryBlock(NULL, NULL, w.len, NULL, NULL, 0, w.len, kCMBlockBufferAssureMemoryNowFlag, &bb);
        if (st == noErr) st = CMBlockBufferReplaceDataBytes(w.out, bb, 0, w.len);
        CMSampleTimingInfo tm = { kCMTimeInvalid, CMTimeMake(pts, 1000000), kCMTimeInvalid };
        size_t ss = w.len;
        if (st == noErr) st = CMSampleBufferCreateReady(NULL, bb, c->fmt, 1, 1, &tm, 1, &ss, &sb);
        if (st == noErr) {
            VTDecodeInfoFlags info = 0;
            st = VTDecompressionSessionDecodeFrame(c->vt, sb, 0, NULL, &info);
            if (st != noErr) { c->failed++; if (c->failed < 4 || c->trace) tl_log_line("media: decode refused (%d)", (int)st); }
            if (st == kVTInvalidSessionErr) { session_drop(c); session_make(c); }
        }
        if (sb) CFRelease(sb);
        if (bb) CFRelease(bb);
    }
    free(w.out);
    if (flags & FLAG_EOS) video_drain(c);
}

/* ---- audio ---- */

/* AudioToolbox wants AAC's AudioSpecificConfig inside an ES descriptor (the 'esds' magic cookie) */
static size_t esds_from_asc(const uint8_t *asc, size_t n, uint8_t *out)
{
    size_t i = 0;
    out[i++] = 0x03; out[i++] = (uint8_t)(23 + n); out[i++] = 0; out[i++] = 0; out[i++] = 0;     /* ES_Descriptor, ES_ID, flags */
    out[i++] = 0x04; out[i++] = (uint8_t)(15 + n); out[i++] = 0x40; out[i++] = 0x15;             /* DecoderConfigDescriptor: AAC, audio stream */
    out[i++] = 0; out[i++] = 0; out[i++] = 0;                                                     /* buffer size */
    for (int k = 0; k < 8; k++) out[i++] = 0;                                                     /* max and average bitrate */
    out[i++] = 0x05; out[i++] = (uint8_t)n; memcpy(out + i, asc, n); i += n;                      /* DecoderSpecificInfo */
    out[i++] = 0x06; out[i++] = 1; out[i++] = 2;                                                  /* SLConfigDescriptor */
    return i;
}

static bool audio_open(codec *c, const uint8_t *asc, size_t asc_n, int rate, int channels)
{
    memset(&c->in, 0, sizeof(c->in));
    uint8_t esds[128]; size_t esds_n = 0;
    if (c->kind == K_AAC) {
        if (asc && asc_n && asc_n < 64) {
            esds_n = esds_from_asc(asc, asc_n, esds);
            UInt32 sz = sizeof(c->in);
            if (AudioFormatGetProperty(kAudioFormatProperty_ASBDFromESDS, (UInt32)esds_n, esds, &sz, &c->in) != noErr) memset(&c->in, 0, sizeof(c->in));
        }
        if (!c->in.mFormatID) {
            c->in.mFormatID = kAudioFormatMPEG4AAC; c->in.mSampleRate = rate > 0 ? rate : 44100;
            c->in.mChannelsPerFrame = channels > 0 ? (UInt32)channels : 2; c->in.mFramesPerPacket = 1024;
        }
    } else {
        c->in.mFormatID = kAudioFormatMPEGLayer3; c->in.mSampleRate = rate > 0 ? rate : 44100;
        c->in.mChannelsPerFrame = channels > 0 ? (UInt32)channels : 2; c->in.mFramesPerPacket = 1152;
    }
    c->rate = (int)c->in.mSampleRate; c->channels = (int)c->in.mChannelsPerFrame;
    c->out = (AudioStreamBasicDescription){ .mSampleRate = c->in.mSampleRate, .mFormatID = kAudioFormatLinearPCM,
        .mFormatFlags = kLinearPCMFormatFlagIsSignedInteger | kLinearPCMFormatFlagIsPacked, .mBytesPerPacket = 2 * c->in.mChannelsPerFrame,
        .mFramesPerPacket = 1, .mBytesPerFrame = 2 * c->in.mChannelsPerFrame, .mChannelsPerFrame = c->in.mChannelsPerFrame, .mBitsPerChannel = 16 };
    if (c->conv) { AudioConverterDispose(c->conv); c->conv = NULL; }
    OSStatus st = AudioConverterNew(&c->in, &c->out, &c->conv);
    if (st != noErr) { tl_log_line("media: no %s decoder (%d)", c->kind == K_AAC ? "AAC" : "MP3", (int)st); c->conv = NULL; return false; }
    if (esds_n) AudioConverterSetProperty(c->conv, kAudioConverterDecompressionMagicCookie, (UInt32)esds_n, esds);
    tl_log_line("media: %s decoder, %d Hz, %d channels", c->kind == K_AAC ? "AAC" : "MP3", c->rate, c->channels);
    return true;
}

static OSStatus audio_feed(AudioConverterRef conv, UInt32 *npk, AudioBufferList *bl, AudioStreamPacketDescription **desc, void *arg)
{
    (void)conv;
    codec *c = arg;
    if (!c->packet) { *npk = 0; return 1; }                         /* no more input for this call: AudioConverter returns what it has */
    bl->mNumberBuffers = 1;
    bl->mBuffers[0].mData = (void *)c->packet; bl->mBuffers[0].mDataByteSize = c->packet_len; bl->mBuffers[0].mNumberChannels = c->in.mChannelsPerFrame;
    c->desc = (AudioStreamPacketDescription){ 0, 0, c->packet_len };
    if (desc) *desc = &c->desc;
    *npk = 1; c->packet = NULL;
    return noErr;
}

static void audio_decode(codec *c, const uint8_t *d, size_t n, int64_t pts)
{
    if (!c->conv && !audio_open(c, NULL, 0, 0, 0)) return;
    if (!n) return;
    c->packet = d; c->packet_len = (UInt32)n;
    UInt32 frames = 4096, bytes = frames * c->out.mBytesPerFrame;
    uint8_t *buf = malloc(bytes);
    AudioBufferList bl = { 1, { { c->out.mChannelsPerFrame, bytes, buf } } };
    OSStatus st = AudioConverterFillComplexBuffer(c->conv, audio_feed, c, &frames, &bl, NULL);
    c->packet = NULL;
    if ((st != noErr && st != 1) || frames == 0) { free(buf); if (st != noErr && st != 1) { c->failed++; if (c->failed < 4) tl_log_line("media: audio packet not decoded (%d)", (int)st); } return; }
    pcm *p = calloc(1, sizeof(*p));
    p->data = buf; p->len = (int)(frames * c->out.mBytesPerFrame); p->pts = pts;
    pthread_mutex_lock(&c->lock); *c->pcm_tail = p; c->pcm_tail = &p->next; c->decoded++; pthread_mutex_unlock(&c->lock);
}

/* ---- natives ---- */

static int kind_of(const char *mime)
{
    if (!mime) return 0;
    if (!strcasecmp(mime, "video/avc")) return K_AVC;
    if (!strcasecmp(mime, "video/hevc")) return K_HEVC;
    if (!strcasecmp(mime, "audio/mp4a-latm")) return K_AAC;
    if (!strcasecmp(mime, "audio/mpeg")) return K_MP3;
    return 0;
}
#define C ((codec *)(uintptr_t)a[0].j)

NAT(M_supported)
{
    (void)self;
    int k = kind_of(tl_jni_string(a[0].l));
    bool ok = k == K_AVC || k == K_AAC || k == K_MP3;
    if (k == K_HEVC) {
        if (__builtin_available(macOS 10.13, iOS 11.0, *)) ok = VTIsHardwareDecodeSupported(kCMVideoCodecType_HEVC);
    }
    *ret = Z(ok);
    return true;
}
NAT(M_create)
{
    (void)self;
    int k = kind_of(tl_jni_string(a[0].l));
    if (!k) { *ret = J(0); return true; }
    codec *c = calloc(1, sizeof(*c));
    if (!c) { *ret = J(0); return true; }
    c->kind = k; c->trace = trace_on();
    pthread_mutex_init(&c->lock, NULL);
    c->ready_tail = &c->ready; c->pcm_tail = &c->pcms;
    pthread_mutex_lock(&g_codec_lock); c->next = g_codecs; g_codecs = c; pthread_mutex_unlock(&g_codec_lock);
    *ret = J((int64_t)(uintptr_t)c);
    return true;
}
/* configure(h, width, height, rate, channels, csd0, csd1, csd2): codec-specific data as the format carries it */
NAT(M_configure)
{
    (void)self;
    codec *c = C;
    jobj *csd[3] = { a[5].l, a[6].l, a[7].l };
    if (c->kind == K_AVC || c->kind == K_HEVC) {
        c->width = a[1].i; c->height = a[2].i;
        for (int i = 0; i < 3; i++) if (csd[i]) { nal_walk w = { c, NULL, 0, 0 }; each_nal(csd[i]->arr.data, csd[i]->arr.len, take_nal, &w); free(w.out); }
        if (c->params_changed) { c->params_changed = false; session_make(c); }
        *ret = Z(true);
    } else {
        *ret = Z(audio_open(c, csd[0] ? csd[0]->arr.data : NULL, csd[0] ? csd[0]->arr.len : 0, a[3].i, a[4].i));
    }
    return true;
}
/* decode(h, data, len, ptsUs, flags): one access unit or packet; csd (BUFFER_FLAG_CODEC_CONFIG) goes through here too */
NAT(M_decode)
{
    (void)self;
    codec *c = C;
    jobj *b = a[1].l;
    size_t n = b ? (size_t)a[2].i : 0;
    if (b && n > b->arr.len) n = b->arr.len;
    const uint8_t *d = b ? b->arr.data : NULL;
    if (c->kind == K_AVC || c->kind == K_HEVC) video_decode(c, d, n, a[3].j, a[4].i);
    else if (a[4].i & 2) audio_open(c, d, n, c->rate, c->channels);           /* codec config: an AudioSpecificConfig */
    else audio_decode(c, d, n, a[3].j);
    *ret = I(0);
    return true;
}
/* nextFrame(h, info) -> a frame handle (0: none yet); info = { ptsUs, width, height } */
NAT(M_nextFrame)
{
    (void)self;
    codec *c = C;
    pthread_mutex_lock(&c->lock);
    frame *f = c->ready;
    if (f) { c->ready = f->next; if (!c->ready) c->ready_tail = &c->ready; }
    pthread_mutex_unlock(&c->lock);
    if (!f) { *ret = J(0); return true; }
    int64_t *info = a[1].l ? (int64_t *)((jobj *)a[1].l)->arr.data : NULL;
    if (info) {
        CGRect r = CVImageBufferGetCleanRect(f->pb);
        int w = (int)r.size.width, h = (int)r.size.height;
        if (w <= 0 || h <= 0) { w = (int)CVPixelBufferGetWidth(f->pb); h = (int)CVPixelBufferGetHeight(f->pb); }
        info[0] = f->pts; info[1] = w; info[2] = h;
    }
    *ret = J((int64_t)(uintptr_t)f);
    return true;
}
/* nextPcm(h, info) -> 16-bit PCM bytes (null: none yet); info = { ptsUs, rate, channels } */
NAT(M_nextPcm)
{
    (void)self;
    codec *c = C;
    pthread_mutex_lock(&c->lock);
    pcm *p = c->pcms;
    if (p) { c->pcms = p->next; if (!c->pcms) c->pcm_tail = &c->pcms; }
    pthread_mutex_unlock(&c->lock);
    if (!p) { *ret = L(NULL); return true; }
    int64_t *info = a[1].l ? (int64_t *)((jobj *)a[1].l)->arr.data : NULL;
    if (info) { info[0] = p->pts; info[1] = c->rate; info[2] = c->channels; }
    jobj *arr = tl_jni_new_prim_array('B', (uint32_t)p->len);
    arr->refs = 1u << 30;
    memcpy(arr->arr.data, p->data, (size_t)p->len);
    free(p->data); free(p);
    *ret = L(arr);
    return true;
}
/* renderFrame(frame, bitmap): the frame's picture into a Bitmap of its size (RGBA) */
NAT(M_renderFrame)
{
    (void)self;
    frame *f = (frame *)(uintptr_t)a[0].j;
    tl_abitmap_info bi; void *px = NULL;
    if (!f || !a[1].l || tl_AndroidBitmap_getInfo(NULL, a[1].l, &bi) || tl_AndroidBitmap_lockPixels(NULL, a[1].l, &px) || !px) { *ret = Z(false); return true; }
    CVPixelBufferLockBaseAddress(f->pb, kCVPixelBufferLock_ReadOnly);
    uint8_t *base = CVPixelBufferGetBaseAddress(f->pb);
    size_t stride = CVPixelBufferGetBytesPerRow(f->pb);
    CGRect r = CVImageBufferGetCleanRect(f->pb);
    size_t x0 = (size_t)r.origin.x, y0 = (size_t)r.origin.y;
    size_t w = bi.width, h = bi.height;
    if (w > CVPixelBufferGetWidth(f->pb) - x0) w = CVPixelBufferGetWidth(f->pb) - x0;
    if (h > CVPixelBufferGetHeight(f->pb) - y0) h = CVPixelBufferGetHeight(f->pb) - y0;
    bool ok = false;
    if (base) {
        vImage_Buffer s = { base + y0 * stride + x0 * 4, h, w, stride }, d = { px, h, w, bi.stride };
        const uint8_t map[4] = { 2, 1, 0, 3 };                     /* BGRA -> RGBA */
        ok = vImagePermuteChannels_ARGB8888(&s, &d, map, kvImageNoFlags) == kvImageNoError;
    }
    CVPixelBufferUnlockBaseAddress(f->pb, kCVPixelBufferLock_ReadOnly);
    *ret = Z(ok);
    return true;
}
NAT(M_freeFrame) { (void)self; (void)ret; frame *f = (frame *)(uintptr_t)a[0].j; if (f) { CVPixelBufferRelease(f->pb); free(f); } return true; }
static void drop_outputs(codec *c)
{
    pthread_mutex_lock(&c->lock);
    for (frame *f = c->sorted, *n; f; f = n) { n = f->next; CVPixelBufferRelease(f->pb); free(f); }
    for (frame *f = c->ready, *n; f; f = n) { n = f->next; CVPixelBufferRelease(f->pb); free(f); }
    c->sorted = NULL; c->nsorted = 0; c->ready = NULL; c->ready_tail = &c->ready;
    for (pcm *p = c->pcms, *n; p; p = n) { n = p->next; free(p->data); free(p); }
    c->pcms = NULL; c->pcm_tail = &c->pcms;
    pthread_mutex_unlock(&c->lock);
}
NAT(M_flush)
{
    (void)self; (void)ret;
    codec *c = C;
    if (c->vt) { VTDecompressionSessionFinishDelayedFrames(c->vt); VTDecompressionSessionWaitForAsynchronousFrames(c->vt); }
    if (c->conv) AudioConverterReset(c->conv);
    drop_outputs(c);
    return true;
}
static void codec_destroy(codec *c)
{
    if (!c) return;
    pthread_mutex_lock(&g_codec_lock);
    codec **p = &g_codecs; while (*p && *p != c) p = &(*p)->next;
    if (!*p) { pthread_mutex_unlock(&g_codec_lock); return; }
    *p = c->next;
    pthread_mutex_unlock(&g_codec_lock);
    session_drop(c);
    drop_outputs(c);
    if (c->fmt) CFRelease(c->fmt);
    if (c->conv) AudioConverterDispose(c->conv);
    free(c->vps.d); free(c->sps.d); free(c->pps.d);
    if (c->trace) tl_log_line("media: codec closed after %ld outputs, %ld failures", c->decoded, c->failed);
    pthread_mutex_destroy(&c->lock);
    free(c);
}
void tl_media_reset_session(void) { while (g_codecs) codec_destroy(g_codecs); }
NAT(M_destroy)
{
    (void)self; (void)ret;
    codec_destroy(C);
    return true;
}
NAT(M_size) { (void)self; codec *c = C; *ret = I(c->width << 16 | (c->height & 0xffff)); return true; }

static const struct { const char *name, *sig; dvm_native_fn fn; } k_media[] = {
    { "supported", "(Ljava/lang/String;)Z", M_supported },
    { "create", "(Ljava/lang/String;)J", M_create },
    { "configure", "(JIIII[B[B[B)Z", M_configure },
    { "decode", "(J[BIJI)I", M_decode },
    { "nextFrame", "(J[J)J", M_nextFrame },
    { "nextPcm", "(J[J)[B", M_nextPcm },
    { "renderFrame", "(JLandroid/graphics/Bitmap;)Z", M_renderFrame },
    { "freeFrame", "(J)V", M_freeFrame },
    { "flush", "(J)V", M_flush },
    { "destroy", "(J)V", M_destroy },
    { "size", "(J)I", M_size },
    { NULL, NULL, NULL },
};
dvm_native_fn tl_media_native(const char *name, const char *sig);
dvm_native_fn tl_media_native(const char *name, const char *sig)
{
    for (int i = 0; k_media[i].name; i++) if (!strcmp(k_media[i].name, name) && !strcmp(k_media[i].sig, sig)) return k_media[i].fn;
    return NULL;
}
