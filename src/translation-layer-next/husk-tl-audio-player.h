/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef HUSK_TL_AUDIO_PLAYER_H
#define HUSK_TL_AUDIO_PLAYER_H
#include <stdbool.h>
#include <stdint.h>

/* Shared native/Java mixer. Streams keep their own source format; only the
 * mixer writes the host sink, at one stable rate and channel layout. */
int tl_audio_stream_open(int rate, int channels);
int tl_audio_stream_write(int id, const int16_t *pcm, int frames);
void tl_audio_stream_play(int id, bool playing);
void tl_audio_stream_flush(int id);
void tl_audio_stream_volume(int id, float left, float right);
uint64_t tl_audio_stream_position(int id);
int tl_audio_stream_pending(int id);
void tl_audio_stream_close(int id);

/* Streaming decoder: the compressed source is bounded; PCM is read in blocks.
 * FD offsets/lengths are bytes, positions/counts are source PCM frames. */
void *tl_audio_decoder_open_fd(int fd, int64_t offset, int64_t length,
                               int *rate, int *channels, int64_t *frames);
void *tl_audio_decoder_open_uri(const char *uri, int *rate, int *channels, int64_t *frames);
int tl_audio_decoder_read(void *decoder, int16_t *pcm, int frames);
bool tl_audio_decoder_seek(void *decoder, int64_t frame);
void tl_audio_decoder_close(void *decoder);
#endif
