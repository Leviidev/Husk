/*
 * Husk: the audio backend.
 *
 * QEMU's audio subsystem was already compiled into this build -- the mixer, the
 * voice management and the device plumbing are all there. What was missing was
 * a host backend: the tree ships coreaudio for macOS, and its AudioUnit subtype
 * does not exist on iOS. This is the iOS half, and it deliberately does no
 * audio work of its own.
 *
 * The shape is the same as husk-display: QEMU hands over data, a ring buffer
 * holds it, and the app takes it from the other side on its own thread. Nothing
 * here talks to AVAudioSession or AudioUnit, because QEMU's audio thread must
 * not block on Core Audio and Core Audio's render thread must not block on the
 * BQL. A ring with one producer and one consumer is what keeps those two apart.
 */
#include "qemu/osdep.h"
#include "qemu/host-utils.h"
#include "qemu/module.h"
#include "audio.h"

#define AUDIO_CAP "husk"
#include "audio_int.h"

#include "husk-audio.h"

/*
 * About a third of a second at 48 kHz.
 *
 * Long enough that a scheduling hiccup on either side does not become an
 * audible gap, short enough that audio does not visibly lag the picture. Both
 * indices are free-running frame counts and only wrap at 2^32 frames, which is
 * a day of audio -- so the usual "is it full or empty" ambiguity of a ring
 * never arises and neither index needs a lock.
 */
#define HUSK_RING_FRAMES 16384
#define HUSK_RING_MASK   (HUSK_RING_FRAMES - 1)

typedef struct HuskVoiceOut {
    HWVoiceOut hw;
} HuskVoiceOut;

static struct {
    int16_t  ring[HUSK_RING_FRAMES * HUSK_AUDIO_CHANNELS];
    uint32_t write_pos;   /* frames written by QEMU, free-running */
    uint32_t read_pos;    /* frames taken by the app, free-running */
    bool     running;
    uint64_t frames_in;
    uint64_t underruns;
} husk_audio;

/* ------------------------------------------------------------------ output */

static int husk_init_out(HWVoiceOut *hw, struct audsettings *as, void *opaque)
{
    /*
     * Our format, not the guest's.
     *
     * A backend is allowed to declare what it wants and let mixeng convert;
     * every other backend in the tree does this. Fixing it here means the iOS
     * render callback never has to ask what rate or layout it is being handed,
     * and a guest that decides to play 44.1 kHz mono costs a conversion in
     * QEMU rather than a bug in the audio thread.
     */
    struct audsettings ours = {
        .freq       = HUSK_AUDIO_RATE,
        .nchannels  = HUSK_AUDIO_CHANNELS,
        .fmt        = AUDIO_FORMAT_S16,
        .endianness = 0,
    };

    audio_pcm_init_info(&hw->info, &ours);
    hw->samples = HUSK_RING_FRAMES / 4;

    qatomic_set(&husk_audio.write_pos, 0);
    qatomic_set(&husk_audio.read_pos, 0);
    husk_audio.frames_in = 0;
    husk_audio.underruns = 0;

    fprintf(stderr, "[husk-audio] output voice: guest asked for %d Hz x%d, "
                    "taking it at %d Hz x%d s16\n",
            as->freq, as->nchannels, HUSK_AUDIO_RATE, HUSK_AUDIO_CHANNELS);
    fflush(stderr);
    return 0;
}

static void husk_fini_out(HWVoiceOut *hw)
{
    qatomic_set(&husk_audio.running, false);
}

static void husk_enable_out(HWVoiceOut *hw, bool enable)
{
    qatomic_set(&husk_audio.running, enable);
    fprintf(stderr, "[husk-audio] output %s\n", enable ? "started" : "stopped");
    fflush(stderr);
}

/*
 * Take what fits and say so.
 *
 * Returning less than `len` is how a backend applies backpressure, and it is
 * the correct answer when the app has not drained the ring yet -- QEMU keeps
 * the remainder and offers it again. Writing past the reader would be the
 * alternative and it is far worse: the guest would never stall, and every
 * overrun would be heard as a tear.
 */
static size_t husk_write(HWVoiceOut *hw, void *buf, size_t len)
{
    const size_t frame_bytes = sizeof(int16_t) * HUSK_AUDIO_CHANNELS;
    uint32_t w = qatomic_read(&husk_audio.write_pos);
    uint32_t r = qatomic_read(&husk_audio.read_pos);
    size_t free_frames = HUSK_RING_FRAMES - (size_t)(w - r);
    size_t want = len / frame_bytes;
    size_t n = want < free_frames ? want : free_frames;
    const int16_t *src = buf;

    for (size_t i = 0; i < n; i++) {
        size_t slot = ((w + i) & HUSK_RING_MASK) * HUSK_AUDIO_CHANNELS;
        for (int c = 0; c < HUSK_AUDIO_CHANNELS; c++) {
            husk_audio.ring[slot + c] = src[i * HUSK_AUDIO_CHANNELS + c];
        }
    }

    /* Published only after the samples are in place, so a reader that sees the
     * new index cannot read a frame that has not been written. */
    smp_wmb();
    qatomic_set(&husk_audio.write_pos, w + (uint32_t)n);
    husk_audio.frames_in += n;

    return n * frame_bytes;
}

static size_t husk_buffer_get_free(HWVoiceOut *hw)
{
    uint32_t w = qatomic_read(&husk_audio.write_pos);
    uint32_t r = qatomic_read(&husk_audio.read_pos);
    return (HUSK_RING_FRAMES - (size_t)(w - r))
         * sizeof(int16_t) * HUSK_AUDIO_CHANNELS;
}

/* ------------------------------------------------------------- app-facing */

int husk_audio_pull(int16_t *dst, int frames)
{
    uint32_t r = qatomic_read(&husk_audio.read_pos);
    uint32_t w = qatomic_read(&husk_audio.write_pos);
    size_t have = (size_t)(w - r);
    size_t n = (size_t)frames < have ? (size_t)frames : have;

    smp_rmb();
    for (size_t i = 0; i < n; i++) {
        size_t slot = ((r + i) & HUSK_RING_MASK) * HUSK_AUDIO_CHANNELS;
        for (int c = 0; c < HUSK_AUDIO_CHANNELS; c++) {
            dst[i * HUSK_AUDIO_CHANNELS + c] = husk_audio.ring[slot + c];
        }
    }
    qatomic_set(&husk_audio.read_pos, r + (uint32_t)n);

    /* Silence rather than stale data for the shortfall: a render callback that
     * leaves its buffer untouched replays whatever was there last, which is a
     * far more noticeable artefact than a gap. */
    if ((size_t)frames > n) {
        memset(dst + n * HUSK_AUDIO_CHANNELS, 0,
               ((size_t)frames - n) * HUSK_AUDIO_CHANNELS * sizeof(int16_t));
        husk_audio.underruns += (uint64_t)frames - n;
    }
    return (int)n;
}

bool husk_audio_active(void)
{
    return qatomic_read(&husk_audio.running);
}

uint64_t husk_audio_frames_in(void) { return husk_audio.frames_in; }
uint64_t husk_audio_underruns(void) { return husk_audio.underruns; }

/* ----------------------------------------------------------- registration */

static void *husk_audio_init(Audiodev *dev, Error **errp)
{
    fprintf(stderr, "[husk-audio] backend ready (%d Hz, %d channels, "
                    "%d frame ring)\n",
            HUSK_AUDIO_RATE, HUSK_AUDIO_CHANNELS, HUSK_RING_FRAMES);
    fflush(stderr);
    return &husk_audio;
}

static void husk_audio_fini(void *opaque)
{
    qatomic_set(&husk_audio.running, false);
}

static struct audio_pcm_ops husk_pcm_ops = {
    .init_out         = husk_init_out,
    .fini_out         = husk_fini_out,
    .write            = husk_write,
    .buffer_get_free  = husk_buffer_get_free,
    .enable_out       = husk_enable_out,
};

static struct audio_driver husk_audio_driver = {
    .name           = "husk",
    .descr          = "Husk iOS audio",
    .init           = husk_audio_init,
    .fini           = husk_audio_fini,
    .pcm_ops        = &husk_pcm_ops,
    .max_voices_out = 1,
    .max_voices_in  = 0,
    .voice_size_out = sizeof(HuskVoiceOut),
    .voice_size_in  = 0,
};

static void register_audio_husk(void)
{
    audio_driver_register(&husk_audio_driver);
}
type_init(register_audio_husk);
