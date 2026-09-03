#ifndef AUDIO_BACKEND_MINIAUDIO_H
#define AUDIO_BACKEND_MINIAUDIO_H

#include "sound_system.h"

#if defined(AUDIO_BACKEND_MINIAUDIO)

/* NOT_FROM_ORIGINAL_SOURCE: attach loaded alias metadata to a reusable
 * Miniaudio voice before its per-play parameters are applied. */
void miniaudio_bind_loaded_sample(
    audio_sample_handle_t sample,
    const snd_alias_sound_file_t *soundFile);

/* NOT_FROM_ORIGINAL_SOURCE: release Miniaudio voice state before the owning
 * loaded-alias hunk payload becomes reusable. */
void miniaudio_forget_loaded_sound(
    const snd_alias_sound_file_t *soundFile);

#endif

#endif
