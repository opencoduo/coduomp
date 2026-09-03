#include "audio_backend_api.h"

#if !defined(AUDIO_BACKEND_MILES)

#include "client/engine/platform/crt_boundary.h"

static const audio_backend_api_t *active_audio_backend;

/* NOT_FROM_ORIGINAL_SOURCE: every function in this translation unit selects
 * or dispatches a complete non-Miles audio backend. */

/* NOT_FROM_ORIGINAL_SOURCE: recognize canonical backend names and values
 * archived by earlier native-client builds. */
static qboolean audio_backend_name_matches(
    const char *candidate, const char *canonical, const char *legacy)
{
    return candidate != NULL &&
                   (coduo_crt_stricmp(candidate, canonical) == 0 ||
                    (legacy != NULL &&
                     coduo_crt_stricmp(candidate, legacy) == 0))
               ? qtrue
               : qfalse;
}

qboolean audio_select_backend(
    const char *preferredName, const char **selectedName)
{
    const audio_backend_api_t *selected = NULL;
#if defined(AUDIO_BACKEND_OPENAL)
    if (audio_backend_name_matches(
            preferredName, openal_backend.name,
            "OpenAL Fast 2D Positional Audio")) {
        selected = &openal_backend;
    }
#endif
#if defined(AUDIO_BACKEND_MINIAUDIO)
    if (selected == NULL)
        selected = &miniaudio_backend;
#endif
    if (selected == NULL)
        return qfalse;
    active_audio_backend = selected;
    if (selectedName != NULL)
        *selectedName = selected->name;
    return qtrue;
}

void audio_bind_loaded_sample(
    audio_sample_handle_t sample,
    const snd_alias_sound_file_t *soundFile)
{
    active_audio_backend->api_bind_loaded_sample(sample, soundFile);
}

void audio_forget_loaded_sound(
    const snd_alias_sound_file_t *soundFile)
{
    active_audio_backend->api_forget_loaded_sound(soundFile);
}

int32_t audio_set_preference(int32_t preference, int32_t value)
{
    return active_audio_backend->api_set_preference(preference, value);
}

void audio_set_file_callbacks(
    audio_file_open_callback_t openCallback,
    audio_file_close_callback_t closeCallback,
    audio_file_seek_callback_t seekCallback,
    audio_file_read_callback_t readCallback)
{
    active_audio_backend->api_set_file_callbacks(openCallback, closeCallback, seekCallback, readCallback);
}

void audio_set_redist_directory(const char *directory)
{
    active_audio_backend->api_set_redist_directory(directory);
}

int32_t audio_startup(void)
{
    return active_audio_backend->api_startup();
}

void audio_shutdown(void)
{
    active_audio_backend->api_shutdown();
}

void audio_close_3D_provider(audio_provider_t provider)
{
    active_audio_backend->api_close_3D_provider(provider);
}

void audio_set_DirectSound_HWND(audio_driver_t driver, audio_window_handle_t windowHandle)
{
    active_audio_backend->api_set_DirectSound_HWND(driver, windowHandle);
}

int32_t audio_WAV_info(const void *fileData, audio_sound_info_t *soundInfo)
{
    return active_audio_backend->api_WAV_info(fileData, soundInfo);
}

uint32_t audio_size_processed_digital_audio(
    uint32_t sampleRate, audio_sample_type_t sampleType,
    int32_t bufferCount, const audio_sound_info_t *sourceInfo)
{
    return active_audio_backend->api_size_processed_digital_audio(sampleRate, sampleType, bufferCount, sourceInfo);
}

int32_t audio_process_digital_audio(
    void *destination, uint32_t destinationSize, uint32_t sampleRate,
    audio_sample_type_t sampleType, int32_t bufferCount,
    audio_sound_info_t *sourceInfo)
{
    return active_audio_backend->api_process_digital_audio(destination, destinationSize, sampleRate, sampleType, bufferCount, sourceInfo);
}

int32_t audio_digital_CPU_percent(audio_driver_t driver)
{
    return active_audio_backend->api_digital_CPU_percent(driver);
}

audio_driver_t audio_open_digital_driver(int32_t sampleRate, int32_t sampleFormat, int32_t channels, int32_t flags)
{
    return active_audio_backend->api_open_digital_driver(sampleRate, sampleFormat, channels, flags);
}

const char * audio_last_error(void)
{
    return active_audio_backend->api_last_error();
}

int32_t audio_enumerate_3D_providers(audio_provider_enumerator_t *enumerator, audio_provider_t *provider, const char **providerName)
{
    return active_audio_backend->api_enumerate_3D_providers(enumerator, provider, providerName);
}

int32_t audio_open_3D_provider(audio_provider_t provider)
{
    return active_audio_backend->api_open_3D_provider(provider);
}

int32_t audio_3D_provider_attribute(audio_provider_t provider, const char *attributeName, void *value)
{
    return active_audio_backend->api_3D_provider_attribute(provider, attributeName, value);
}

int32_t audio_set_3D_provider_preference(audio_provider_t provider, const char *preferenceName, void *value)
{
    return active_audio_backend->api_set_3D_provider_preference(provider, preferenceName, value);
}

void audio_set_3D_distance_factor(audio_provider_t provider, float distanceFactor)
{
    active_audio_backend->api_set_3D_distance_factor(provider, distanceFactor);
}

audio_sample_handle_t audio_allocate_sample_handle(audio_driver_t driver)
{
    return active_audio_backend->api_allocate_sample_handle(driver);
}

audio_3d_sample_handle_t audio_allocate_3D_sample_handle(audio_provider_t provider)
{
    return active_audio_backend->api_allocate_3D_sample_handle(provider);
}

void audio_set_3D_position(audio_3d_sample_handle_t sample, float x, float y, float z)
{
    active_audio_backend->api_set_3D_position(sample, x, y, z);
}

void audio_end_sample(audio_sample_handle_t sample)
{
    active_audio_backend->api_end_sample(sample);
}

void audio_stop_sample(audio_sample_handle_t sample)
{
    active_audio_backend->api_stop_sample(sample);
}

void audio_resume_sample(audio_sample_handle_t sample)
{
    active_audio_backend->api_resume_sample(sample);
}

int32_t audio_sample_status(audio_sample_handle_t sample)
{
    return active_audio_backend->api_sample_status(sample);
}

void audio_end_3D_sample(audio_3d_sample_handle_t sample)
{
    active_audio_backend->api_end_3D_sample(sample);
}

void audio_stop_3D_sample(audio_3d_sample_handle_t sample)
{
    active_audio_backend->api_stop_3D_sample(sample);
}

void audio_resume_3D_sample(audio_3d_sample_handle_t sample)
{
    active_audio_backend->api_resume_3D_sample(sample);
}

int32_t audio_3D_sample_status(audio_3d_sample_handle_t sample)
{
    return active_audio_backend->api_3D_sample_status(sample);
}

audio_stream_handle_t audio_open_stream(audio_driver_t driver, const char *filename, int32_t streamMemory)
{
    return active_audio_backend->api_open_stream(driver, filename, streamMemory);
}

void audio_close_stream(audio_stream_handle_t stream)
{
    active_audio_backend->api_close_stream(stream);
}

void audio_pause_stream(audio_stream_handle_t stream, qboolean paused)
{
    active_audio_backend->api_pause_stream(stream, paused);
}

int32_t audio_stream_status(audio_stream_handle_t stream)
{
    return active_audio_backend->api_stream_status(stream);
}

int32_t audio_stream_playback_rate(audio_stream_handle_t stream)
{
    return active_audio_backend->api_stream_playback_rate(stream);
}

void audio_set_stream_playback_rate(audio_stream_handle_t stream, int32_t playbackRate)
{
    active_audio_backend->api_set_stream_playback_rate(stream, playbackRate);
}

void audio_set_stream_volume_pan(audio_stream_handle_t stream, float volume, float pan)
{
    active_audio_backend->api_set_stream_volume_pan(stream, volume, pan);
}

void audio_stream_volume_pan(audio_stream_handle_t stream, float *volume, float *pan)
{
    active_audio_backend->api_stream_volume_pan(stream, volume, pan);
}

void audio_set_stream_loop_count(audio_stream_handle_t stream, int32_t loopCount)
{
    active_audio_backend->api_set_stream_loop_count(stream, loopCount);
}

void audio_set_stream_reverb_levels(audio_stream_handle_t stream, float dryLevel, float wetLevel)
{
    active_audio_backend->api_set_stream_reverb_levels(stream, dryLevel, wetLevel);
}

void audio_stream_ms_position(audio_stream_handle_t stream, int32_t *totalMsec, int32_t *currentMsec)
{
    active_audio_backend->api_stream_ms_position(stream, totalMsec, currentMsec);
}

void audio_set_stream_ms_position(audio_stream_handle_t stream, int32_t positionMsec)
{
    active_audio_backend->api_set_stream_ms_position(stream, positionMsec);
}

void audio_start_stream(audio_stream_handle_t stream)
{
    active_audio_backend->api_start_stream(stream);
}

void audio_init_sample(audio_sample_handle_t sample)
{
    active_audio_backend->api_init_sample(sample);
}

void audio_set_sample_type(audio_sample_handle_t sample, audio_sample_type_t sampleType, int32_t flags)
{
    active_audio_backend->api_set_sample_type(sample, sampleType, flags);
}

void audio_set_sample_address(audio_sample_handle_t sample, const void *data, uint32_t dataLength)
{
    active_audio_backend->api_set_sample_address(sample, data, dataLength);
}

void audio_set_sample_adpcm_block_size(audio_sample_handle_t sample, uint32_t blockSize)
{
    active_audio_backend->api_set_sample_adpcm_block_size(sample, blockSize);
}

int32_t audio_sample_playback_rate(audio_sample_handle_t sample)
{
    return active_audio_backend->api_sample_playback_rate(sample);
}

void audio_set_sample_playback_rate(audio_sample_handle_t sample, int32_t playbackRate)
{
    active_audio_backend->api_set_sample_playback_rate(sample, playbackRate);
}

void audio_set_sample_volume_pan(audio_sample_handle_t sample, float volume, float pan)
{
    active_audio_backend->api_set_sample_volume_pan(sample, volume, pan);
}

void audio_sample_volume_pan(audio_sample_handle_t sample, float *volume, float *pan)
{
    active_audio_backend->api_sample_volume_pan(sample, volume, pan);
}

void audio_set_sample_loop_count(audio_sample_handle_t sample, int32_t loopCount)
{
    active_audio_backend->api_set_sample_loop_count(sample, loopCount);
}

void audio_set_sample_reverb_levels(audio_sample_handle_t sample, float dryLevel, float wetLevel)
{
    active_audio_backend->api_set_sample_reverb_levels(sample, dryLevel, wetLevel);
}

void audio_sample_ms_position(audio_sample_handle_t sample, int32_t *totalMsec, int32_t *currentMsec)
{
    active_audio_backend->api_sample_ms_position(sample, totalMsec, currentMsec);
}

void audio_set_sample_ms_position(audio_sample_handle_t sample, int32_t positionMsec)
{
    active_audio_backend->api_set_sample_ms_position(sample, positionMsec);
}

void audio_start_sample(audio_sample_handle_t sample)
{
    active_audio_backend->api_start_sample(sample);
}

void audio_release_sample_handle(audio_sample_handle_t sample)
{
    active_audio_backend->api_release_sample_handle(sample);
}

int32_t audio_minimum_sample_buffer_size(audio_driver_t driver, int32_t sampleRate, audio_sample_type_t sampleType)
{
    return active_audio_backend->api_minimum_sample_buffer_size(driver, sampleRate, sampleType);
}

uint32_t audio_sample_position(audio_sample_handle_t sample)
{
    return active_audio_backend->api_sample_position(sample);
}

int32_t audio_sample_buffer_ready(audio_sample_handle_t sample)
{
    return active_audio_backend->api_sample_buffer_ready(sample);
}

void audio_load_sample_buffer(audio_sample_handle_t sample, int32_t bufferIndex, const void *data, int32_t byteCount)
{
    active_audio_backend->api_load_sample_buffer(sample, bufferIndex, data, byteCount);
}

void audio_set_3D_sample_info(audio_3d_sample_handle_t sample, const snd_alias_sound_file_t *soundFile)
{
    active_audio_backend->api_set_3D_sample_info(sample, soundFile);
}

void audio_set_3D_sample_volume(audio_3d_sample_handle_t sample, float volume)
{
    active_audio_backend->api_set_3D_sample_volume(sample, volume);
}

void audio_set_3D_sample_distances(audio_3d_sample_handle_t sample, float maximumDistance, float minimumDistance)
{
    active_audio_backend->api_set_3D_sample_distances(sample, maximumDistance, minimumDistance);
}

int32_t audio_3D_sample_playback_rate(audio_3d_sample_handle_t sample)
{
    return active_audio_backend->api_3D_sample_playback_rate(sample);
}

uint32_t audio_3D_sample_offset(audio_3d_sample_handle_t sample)
{
    return active_audio_backend->api_3D_sample_offset(sample);
}

uint32_t audio_3D_sample_length(audio_3d_sample_handle_t sample)
{
    return active_audio_backend->api_3D_sample_length(sample);
}

float audio_3D_sample_volume(audio_3d_sample_handle_t sample)
{
    return active_audio_backend->api_3D_sample_volume(sample);
}

void audio_3D_position(audio_3d_sample_handle_t sample, float *x, float *y, float *z)
{
    active_audio_backend->api_3D_position(sample, x, y, z);
}

void audio_set_3D_sample_playback_rate(audio_3d_sample_handle_t sample, int32_t playbackRate)
{
    active_audio_backend->api_set_3D_sample_playback_rate(sample, playbackRate);
}

void audio_set_3D_sample_loop_count(audio_3d_sample_handle_t sample, int32_t loopCount)
{
    active_audio_backend->api_set_3D_sample_loop_count(sample, loopCount);
}

void audio_set_3D_sample_effects_level(audio_3d_sample_handle_t sample, float effectsLevel)
{
    active_audio_backend->api_set_3D_sample_effects_level(sample, effectsLevel);
}

int32_t audio_set_3D_sample_preference(audio_3d_sample_handle_t sample, const char *preferenceName, void *value)
{
    return active_audio_backend->api_set_3D_sample_preference(sample, preferenceName, value);
}

void audio_set_digital_master_room_type(audio_driver_t driver, int32_t roomType)
{
    active_audio_backend->api_set_digital_master_room_type(driver, roomType);
}

void audio_set_3D_room_type(audio_provider_t provider, int32_t roomType)
{
    active_audio_backend->api_set_3D_room_type(provider, roomType);
}

void audio_set_3D_sample_offset(audio_3d_sample_handle_t sample, int32_t byteOffset)
{
    active_audio_backend->api_set_3D_sample_offset(sample, byteOffset);
}

void audio_start_3D_sample(audio_3d_sample_handle_t sample)
{
    active_audio_backend->api_start_3D_sample(sample);
}

#endif
