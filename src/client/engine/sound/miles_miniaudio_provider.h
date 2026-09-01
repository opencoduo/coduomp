#ifndef CODUOMP_MILES_MINIAUDIO_PROVIDER_H
#define CODUOMP_MILES_MINIAUDIO_PROVIDER_H

#include "miles_boundary.h"

#if !defined(CODUOMP_DISABLE_AUDIO) && \
    (defined(__APPLE__) || defined(__linux__))

typedef struct coduomp_miniaudio_3d_sample_s
    coduomp_miniaudio_3d_sample_t;

/* NOT_FROM_ORIGINAL_SOURCE: statically linked Miniaudio implementation of
 * the original Miles Fast 2D provider boundary. */
qboolean coduomp_miniaudio_provider_init(int32_t sampleRate);
void coduomp_miniaudio_provider_shutdown(void);
const char *coduomp_miniaudio_provider_last_error(void);

coduomp_miniaudio_3d_sample_t *coduomp_miniaudio_3d_sample_create(void);
void coduomp_miniaudio_3d_sample_destroy(
    coduomp_miniaudio_3d_sample_t *sample);
qboolean coduomp_miniaudio_3d_sample_set_info(
    coduomp_miniaudio_3d_sample_t *sample,
    const snd_alias_sound_file_t *soundFile);
void coduomp_miniaudio_3d_sample_forget_sound(
    coduomp_miniaudio_3d_sample_t *sample,
    const snd_alias_sound_file_t *soundFile);
void coduomp_miniaudio_3d_sample_set_channel_gains(
    coduomp_miniaudio_3d_sample_t *sample, float left, float right);
void coduomp_miniaudio_3d_sample_set_playback_rate(
    coduomp_miniaudio_3d_sample_t *sample, int32_t playbackRate,
    int32_t baseRate);
void coduomp_miniaudio_3d_sample_set_loop_count(
    coduomp_miniaudio_3d_sample_t *sample, int32_t loopCount);
void coduomp_miniaudio_3d_sample_end(
    coduomp_miniaudio_3d_sample_t *sample);
void coduomp_miniaudio_3d_sample_stop(
    coduomp_miniaudio_3d_sample_t *sample);
void coduomp_miniaudio_3d_sample_resume(
    coduomp_miniaudio_3d_sample_t *sample);
void coduomp_miniaudio_3d_sample_start(
    coduomp_miniaudio_3d_sample_t *sample);
qboolean coduomp_miniaudio_3d_sample_is_active(
    coduomp_miniaudio_3d_sample_t *sample);
uint32_t coduomp_miniaudio_3d_sample_frame_count(
    const coduomp_miniaudio_3d_sample_t *sample);
uint32_t coduomp_miniaudio_3d_sample_cursor_frame(
    const coduomp_miniaudio_3d_sample_t *sample);
void coduomp_miniaudio_3d_sample_seek_frame(
    coduomp_miniaudio_3d_sample_t *sample, uint32_t frame);

#endif

#endif
