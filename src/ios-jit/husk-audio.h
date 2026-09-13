#ifndef HUSK_AUDIO_H
#define HUSK_AUDIO_H

#include <stdbool.h>
#include <stdint.h>

#ifndef HUSK_EXPORT
#define HUSK_EXPORT __attribute__((visibility("default")))
#endif

/*
 * The one format Husk's audio path uses, end to end.
 *
 * QEMU's mixeng converts whatever the guest device asks for into the format the
 * backend declares, so declaring exactly one removes every conversion question
 * from the iOS side: the render callback can assume interleaved 16-bit stereo
 * at 48 kHz and never negotiate.
 */
#define HUSK_AUDIO_RATE     48000
#define HUSK_AUDIO_CHANNELS 2

/*
 * Take up to `frames` frames of audio the guest has produced.
 *
 * Called from the iOS audio render thread, which must never block, so this
 * takes no lock: the ring behind it has one producer (QEMU's audio thread) and
 * one consumer (this). Short reads are normal -- the guest may simply not have
 * produced that much yet -- and the remainder is filled with silence rather
 * than left undefined, because an audio callback that returns fewer frames than
 * asked for is a click.
 *
 * Returns the number of frames that were real rather than silence, which is
 * only useful for diagnostics.
 */
HUSK_EXPORT int husk_audio_pull(int16_t *dst, int frames);

/* Whether the guest currently has an output voice running. */
HUSK_EXPORT bool husk_audio_active(void);

/* Frames the guest has produced and frames that were pulled as silence,
 * for the perf line. */
HUSK_EXPORT uint64_t husk_audio_frames_in(void);
HUSK_EXPORT uint64_t husk_audio_underruns(void);

#endif
