#ifndef AUDIO_BACKEND_API_H
#define AUDIO_BACKEND_API_H

#include "sound_system.h"

#ifdef __cplusplus
extern "C" {
#endif

#if defined(AUDIO_BACKEND_MILES)

/* The legacy i686 Windows build resolves this ABI directly from mss32.dll. */
int32_t AUDIO_CALLBACK AIL_set_preference(int32_t preference, int32_t value);
void AUDIO_CALLBACK AIL_set_file_callbacks(
    audio_file_open_callback_t openCallback,
    audio_file_close_callback_t closeCallback,
    audio_file_seek_callback_t seekCallback,
    audio_file_read_callback_t readCallback);
void AUDIO_CALLBACK AIL_set_redist_directory(const char *directory);
int32_t AUDIO_CALLBACK AIL_startup(void);
void AUDIO_CALLBACK AIL_shutdown(void);
void AUDIO_CALLBACK AIL_close_3D_provider(audio_provider_t provider);
void AUDIO_CALLBACK AIL_set_DirectSound_HWND(audio_driver_t driver, audio_window_handle_t windowHandle);
int32_t AUDIO_CALLBACK AIL_WAV_info(const void *fileData, audio_sound_info_t *soundInfo);
uint32_t AUDIO_CALLBACK AIL_size_processed_digital_audio(
    uint32_t sampleRate, audio_sample_type_t sampleType,
    int32_t bufferCount, const audio_sound_info_t *sourceInfo);
int32_t AUDIO_CALLBACK AIL_process_digital_audio(
    void *destination, uint32_t destinationSize, uint32_t sampleRate,
    audio_sample_type_t sampleType, int32_t bufferCount,
    audio_sound_info_t *sourceInfo);
int32_t AUDIO_CALLBACK AIL_digital_CPU_percent(audio_driver_t driver);
audio_driver_t AUDIO_CALLBACK AIL_open_digital_driver(int32_t sampleRate, int32_t sampleFormat, int32_t channels, int32_t flags);
const char * AUDIO_CALLBACK AIL_last_error(void);
int32_t AUDIO_CALLBACK AIL_enumerate_3D_providers(
    audio_provider_enumerator_t *enumerator,
    audio_provider_t *provider, const char **providerName);
int32_t AUDIO_CALLBACK AIL_open_3D_provider(audio_provider_t provider);
int32_t AUDIO_CALLBACK AIL_3D_provider_attribute(audio_provider_t provider, const char *attributeName, void *value);
int32_t AUDIO_CALLBACK AIL_set_3D_provider_preference(audio_provider_t provider, const char *preferenceName, void *value);
void AUDIO_CALLBACK AIL_set_3D_distance_factor(audio_provider_t provider, float distanceFactor);
audio_sample_handle_t AUDIO_CALLBACK AIL_allocate_sample_handle(audio_driver_t driver);
audio_3d_sample_handle_t AUDIO_CALLBACK AIL_allocate_3D_sample_handle(audio_provider_t provider);
void AUDIO_CALLBACK AIL_set_3D_position(audio_3d_sample_handle_t sample, float x, float y, float z);
void AUDIO_CALLBACK AIL_end_sample(audio_sample_handle_t sample);
void AUDIO_CALLBACK AIL_stop_sample(audio_sample_handle_t sample);
void AUDIO_CALLBACK AIL_resume_sample(audio_sample_handle_t sample);
int32_t AUDIO_CALLBACK AIL_sample_status(audio_sample_handle_t sample);
void AUDIO_CALLBACK AIL_end_3D_sample(audio_3d_sample_handle_t sample);
void AUDIO_CALLBACK AIL_stop_3D_sample(audio_3d_sample_handle_t sample);
void AUDIO_CALLBACK AIL_resume_3D_sample(audio_3d_sample_handle_t sample);
int32_t AUDIO_CALLBACK AIL_3D_sample_status(audio_3d_sample_handle_t sample);
audio_stream_handle_t AUDIO_CALLBACK AIL_open_stream(audio_driver_t driver, const char *filename, int32_t streamMemory);
void AUDIO_CALLBACK AIL_close_stream(audio_stream_handle_t stream);
void AUDIO_CALLBACK AIL_pause_stream(audio_stream_handle_t stream, qboolean paused);
int32_t AUDIO_CALLBACK AIL_stream_status(audio_stream_handle_t stream);
int32_t AUDIO_CALLBACK AIL_stream_playback_rate(audio_stream_handle_t stream);
void AUDIO_CALLBACK AIL_set_stream_playback_rate(audio_stream_handle_t stream, int32_t playbackRate);
void AUDIO_CALLBACK AIL_set_stream_volume_pan(audio_stream_handle_t stream, float volume, float pan);
void AUDIO_CALLBACK AIL_stream_volume_pan(audio_stream_handle_t stream, float *volume, float *pan);
void AUDIO_CALLBACK AIL_set_stream_loop_count(audio_stream_handle_t stream, int32_t loopCount);
void AUDIO_CALLBACK AIL_set_stream_reverb_levels(audio_stream_handle_t stream, float dryLevel, float wetLevel);
void AUDIO_CALLBACK AIL_stream_ms_position(audio_stream_handle_t stream, int32_t *totalMsec, int32_t *currentMsec);
void AUDIO_CALLBACK AIL_set_stream_ms_position(audio_stream_handle_t stream, int32_t positionMsec);
void AUDIO_CALLBACK AIL_start_stream(audio_stream_handle_t stream);
void AUDIO_CALLBACK AIL_init_sample(audio_sample_handle_t sample);
void AUDIO_CALLBACK AIL_set_sample_type(audio_sample_handle_t sample, audio_sample_type_t sampleType, int32_t flags);
void AUDIO_CALLBACK AIL_set_sample_address(audio_sample_handle_t sample, const void *data, uint32_t dataLength);
void AUDIO_CALLBACK AIL_set_sample_adpcm_block_size(audio_sample_handle_t sample, uint32_t blockSize);
int32_t AUDIO_CALLBACK AIL_sample_playback_rate(audio_sample_handle_t sample);
void AUDIO_CALLBACK AIL_set_sample_playback_rate(audio_sample_handle_t sample, int32_t playbackRate);
void AUDIO_CALLBACK AIL_set_sample_volume_pan(audio_sample_handle_t sample, float volume, float pan);
void AUDIO_CALLBACK AIL_sample_volume_pan(audio_sample_handle_t sample, float *volume, float *pan);
void AUDIO_CALLBACK AIL_set_sample_loop_count(audio_sample_handle_t sample, int32_t loopCount);
void AUDIO_CALLBACK AIL_set_sample_reverb_levels(audio_sample_handle_t sample, float dryLevel, float wetLevel);
void AUDIO_CALLBACK AIL_sample_ms_position(audio_sample_handle_t sample, int32_t *totalMsec, int32_t *currentMsec);
void AUDIO_CALLBACK AIL_set_sample_ms_position(audio_sample_handle_t sample, int32_t positionMsec);
void AUDIO_CALLBACK AIL_start_sample(audio_sample_handle_t sample);
void AUDIO_CALLBACK AIL_release_sample_handle(audio_sample_handle_t sample);
int32_t AUDIO_CALLBACK AIL_minimum_sample_buffer_size(audio_driver_t driver, int32_t sampleRate, audio_sample_type_t sampleType);
uint32_t AUDIO_CALLBACK AIL_sample_position(audio_sample_handle_t sample);
int32_t AUDIO_CALLBACK AIL_sample_buffer_ready(audio_sample_handle_t sample);
void AUDIO_CALLBACK AIL_load_sample_buffer(audio_sample_handle_t sample, int32_t bufferIndex, const void *data, int32_t byteCount);
void AUDIO_CALLBACK AIL_set_3D_sample_info(audio_3d_sample_handle_t sample, const snd_alias_sound_file_t *soundFile);
void AUDIO_CALLBACK AIL_set_3D_sample_volume(audio_3d_sample_handle_t sample, float volume);
void AUDIO_CALLBACK AIL_set_3D_sample_distances(audio_3d_sample_handle_t sample, float maximumDistance, float minimumDistance);
int32_t AUDIO_CALLBACK AIL_3D_sample_playback_rate(audio_3d_sample_handle_t sample);
uint32_t AUDIO_CALLBACK AIL_3D_sample_offset(audio_3d_sample_handle_t sample);
uint32_t AUDIO_CALLBACK AIL_3D_sample_length(audio_3d_sample_handle_t sample);
float AUDIO_CALLBACK AIL_3D_sample_volume(audio_3d_sample_handle_t sample);
void AUDIO_CALLBACK AIL_3D_position(audio_3d_sample_handle_t sample, float *x, float *y, float *z);
void AUDIO_CALLBACK AIL_set_3D_sample_playback_rate(audio_3d_sample_handle_t sample, int32_t playbackRate);
void AUDIO_CALLBACK AIL_set_3D_sample_loop_count(audio_3d_sample_handle_t sample, int32_t loopCount);
void AUDIO_CALLBACK AIL_set_3D_sample_effects_level(audio_3d_sample_handle_t sample, float effectsLevel);
int32_t AUDIO_CALLBACK AIL_set_3D_sample_preference(audio_3d_sample_handle_t sample, const char *preferenceName, void *value);
void AUDIO_CALLBACK AIL_set_digital_master_room_type(audio_driver_t driver, int32_t roomType);
void AUDIO_CALLBACK AIL_set_3D_room_type(audio_provider_t provider, int32_t roomType);
void AUDIO_CALLBACK AIL_set_3D_sample_offset(audio_3d_sample_handle_t sample, int32_t byteOffset);
void AUDIO_CALLBACK AIL_start_3D_sample(audio_3d_sample_handle_t sample);

#define audio_set_preference AIL_set_preference
#define audio_set_file_callbacks AIL_set_file_callbacks
#define audio_set_redist_directory AIL_set_redist_directory
#define audio_startup AIL_startup
#define audio_shutdown AIL_shutdown
#define audio_close_3D_provider AIL_close_3D_provider
#define audio_set_DirectSound_HWND AIL_set_DirectSound_HWND
#define audio_WAV_info AIL_WAV_info
#define audio_size_processed_digital_audio AIL_size_processed_digital_audio
#define audio_process_digital_audio AIL_process_digital_audio
#define audio_digital_CPU_percent AIL_digital_CPU_percent
#define audio_open_digital_driver AIL_open_digital_driver
#define audio_last_error AIL_last_error
#define audio_enumerate_3D_providers AIL_enumerate_3D_providers
#define audio_open_3D_provider AIL_open_3D_provider
#define audio_3D_provider_attribute AIL_3D_provider_attribute
#define audio_set_3D_provider_preference AIL_set_3D_provider_preference
#define audio_set_3D_distance_factor AIL_set_3D_distance_factor
#define audio_allocate_sample_handle AIL_allocate_sample_handle
#define audio_allocate_3D_sample_handle AIL_allocate_3D_sample_handle
#define audio_set_3D_position AIL_set_3D_position
#define audio_end_sample AIL_end_sample
#define audio_stop_sample AIL_stop_sample
#define audio_resume_sample AIL_resume_sample
#define audio_sample_status AIL_sample_status
#define audio_end_3D_sample AIL_end_3D_sample
#define audio_stop_3D_sample AIL_stop_3D_sample
#define audio_resume_3D_sample AIL_resume_3D_sample
#define audio_3D_sample_status AIL_3D_sample_status
#define audio_open_stream AIL_open_stream
#define audio_close_stream AIL_close_stream
#define audio_pause_stream AIL_pause_stream
#define audio_stream_status AIL_stream_status
#define audio_stream_playback_rate AIL_stream_playback_rate
#define audio_set_stream_playback_rate AIL_set_stream_playback_rate
#define audio_set_stream_volume_pan AIL_set_stream_volume_pan
#define audio_stream_volume_pan AIL_stream_volume_pan
#define audio_set_stream_loop_count AIL_set_stream_loop_count
#define audio_set_stream_reverb_levels AIL_set_stream_reverb_levels
#define audio_stream_ms_position AIL_stream_ms_position
#define audio_set_stream_ms_position AIL_set_stream_ms_position
#define audio_start_stream AIL_start_stream
#define audio_init_sample AIL_init_sample
#define audio_set_sample_type AIL_set_sample_type
#define audio_set_sample_address AIL_set_sample_address
#define audio_set_sample_adpcm_block_size AIL_set_sample_adpcm_block_size
#define audio_sample_playback_rate AIL_sample_playback_rate
#define audio_set_sample_playback_rate AIL_set_sample_playback_rate
#define audio_set_sample_volume_pan AIL_set_sample_volume_pan
#define audio_sample_volume_pan AIL_sample_volume_pan
#define audio_set_sample_loop_count AIL_set_sample_loop_count
#define audio_set_sample_reverb_levels AIL_set_sample_reverb_levels
#define audio_sample_ms_position AIL_sample_ms_position
#define audio_set_sample_ms_position AIL_set_sample_ms_position
#define audio_start_sample AIL_start_sample
#define audio_release_sample_handle AIL_release_sample_handle
#define audio_minimum_sample_buffer_size AIL_minimum_sample_buffer_size
#define audio_sample_position AIL_sample_position
#define audio_sample_buffer_ready AIL_sample_buffer_ready
#define audio_load_sample_buffer AIL_load_sample_buffer
#define audio_set_3D_sample_info AIL_set_3D_sample_info
#define audio_set_3D_sample_volume AIL_set_3D_sample_volume
#define audio_set_3D_sample_distances AIL_set_3D_sample_distances
#define audio_3D_sample_playback_rate AIL_3D_sample_playback_rate
#define audio_3D_sample_offset AIL_3D_sample_offset
#define audio_3D_sample_length AIL_3D_sample_length
#define audio_3D_sample_volume AIL_3D_sample_volume
#define audio_3D_position AIL_3D_position
#define audio_set_3D_sample_playback_rate AIL_set_3D_sample_playback_rate
#define audio_set_3D_sample_loop_count AIL_set_3D_sample_loop_count
#define audio_set_3D_sample_effects_level AIL_set_3D_sample_effects_level
#define audio_set_3D_sample_preference AIL_set_3D_sample_preference
#define audio_set_digital_master_room_type AIL_set_digital_master_room_type
#define audio_set_3D_room_type AIL_set_3D_room_type
#define audio_set_3D_sample_offset AIL_set_3D_sample_offset
#define audio_start_3D_sample AIL_start_3D_sample

#else

typedef struct audio_backend_api_s {
    const char *name;
    void (*api_bind_loaded_sample)(audio_sample_handle_t sample,
                                   const snd_alias_sound_file_t *soundFile);
    void (*api_forget_loaded_sound)(
        const snd_alias_sound_file_t *soundFile);
    int32_t (*api_set_preference)(int32_t preference, int32_t value);
    void (*api_set_file_callbacks)(
        audio_file_open_callback_t openCallback,
        audio_file_close_callback_t closeCallback,
        audio_file_seek_callback_t seekCallback,
        audio_file_read_callback_t readCallback);
    void (*api_set_redist_directory)(const char *directory);
    int32_t (*api_startup)(void);
    void (*api_shutdown)(void);
    void (*api_close_3D_provider)(audio_provider_t provider);
    void (*api_set_DirectSound_HWND)(audio_driver_t driver, audio_window_handle_t windowHandle);
    int32_t (*api_WAV_info)(const void *fileData, audio_sound_info_t *soundInfo);
    uint32_t (*api_size_processed_digital_audio)(
        uint32_t sampleRate, audio_sample_type_t sampleType,
        int32_t bufferCount, const audio_sound_info_t *sourceInfo);
    int32_t (*api_process_digital_audio)(
        void *destination, uint32_t destinationSize, uint32_t sampleRate,
        audio_sample_type_t sampleType, int32_t bufferCount,
        audio_sound_info_t *sourceInfo);
    int32_t (*api_digital_CPU_percent)(audio_driver_t driver);
    audio_driver_t (*api_open_digital_driver)(int32_t sampleRate, int32_t sampleFormat, int32_t channels, int32_t flags);
    const char * (*api_last_error)(void);
    int32_t (*api_enumerate_3D_providers)(audio_provider_enumerator_t *enumerator, audio_provider_t *provider, const char **providerName);
    int32_t (*api_open_3D_provider)(audio_provider_t provider);
    int32_t (*api_3D_provider_attribute)(audio_provider_t provider, const char *attributeName, void *value);
    int32_t (*api_set_3D_provider_preference)(audio_provider_t provider, const char *preferenceName, void *value);
    void (*api_set_3D_distance_factor)(audio_provider_t provider, float distanceFactor);
    audio_sample_handle_t (*api_allocate_sample_handle)(audio_driver_t driver);
    audio_3d_sample_handle_t (*api_allocate_3D_sample_handle)(audio_provider_t provider);
    void (*api_set_3D_position)(audio_3d_sample_handle_t sample, float x, float y, float z);
    void (*api_end_sample)(audio_sample_handle_t sample);
    void (*api_stop_sample)(audio_sample_handle_t sample);
    void (*api_resume_sample)(audio_sample_handle_t sample);
    int32_t (*api_sample_status)(audio_sample_handle_t sample);
    void (*api_end_3D_sample)(audio_3d_sample_handle_t sample);
    void (*api_stop_3D_sample)(audio_3d_sample_handle_t sample);
    void (*api_resume_3D_sample)(audio_3d_sample_handle_t sample);
    int32_t (*api_3D_sample_status)(audio_3d_sample_handle_t sample);
    audio_stream_handle_t (*api_open_stream)(audio_driver_t driver, const char *filename, int32_t streamMemory);
    void (*api_close_stream)(audio_stream_handle_t stream);
    void (*api_pause_stream)(audio_stream_handle_t stream, qboolean paused);
    int32_t (*api_stream_status)(audio_stream_handle_t stream);
    int32_t (*api_stream_playback_rate)(audio_stream_handle_t stream);
    void (*api_set_stream_playback_rate)(audio_stream_handle_t stream, int32_t playbackRate);
    void (*api_set_stream_volume_pan)(audio_stream_handle_t stream, float volume, float pan);
    void (*api_stream_volume_pan)(audio_stream_handle_t stream, float *volume, float *pan);
    void (*api_set_stream_loop_count)(audio_stream_handle_t stream, int32_t loopCount);
    void (*api_set_stream_reverb_levels)(audio_stream_handle_t stream, float dryLevel, float wetLevel);
    void (*api_stream_ms_position)(audio_stream_handle_t stream, int32_t *totalMsec, int32_t *currentMsec);
    void (*api_set_stream_ms_position)(audio_stream_handle_t stream, int32_t positionMsec);
    void (*api_start_stream)(audio_stream_handle_t stream);
    void (*api_init_sample)(audio_sample_handle_t sample);
    void (*api_set_sample_type)(audio_sample_handle_t sample, audio_sample_type_t sampleType, int32_t flags);
    void (*api_set_sample_address)(audio_sample_handle_t sample, const void *data, uint32_t dataLength);
    void (*api_set_sample_adpcm_block_size)(audio_sample_handle_t sample, uint32_t blockSize);
    int32_t (*api_sample_playback_rate)(audio_sample_handle_t sample);
    void (*api_set_sample_playback_rate)(audio_sample_handle_t sample, int32_t playbackRate);
    void (*api_set_sample_volume_pan)(audio_sample_handle_t sample, float volume, float pan);
    void (*api_sample_volume_pan)(audio_sample_handle_t sample, float *volume, float *pan);
    void (*api_set_sample_loop_count)(audio_sample_handle_t sample, int32_t loopCount);
    void (*api_set_sample_reverb_levels)(audio_sample_handle_t sample, float dryLevel, float wetLevel);
    void (*api_sample_ms_position)(audio_sample_handle_t sample, int32_t *totalMsec, int32_t *currentMsec);
    void (*api_set_sample_ms_position)(audio_sample_handle_t sample, int32_t positionMsec);
    void (*api_start_sample)(audio_sample_handle_t sample);
    void (*api_release_sample_handle)(audio_sample_handle_t sample);
    int32_t (*api_minimum_sample_buffer_size)(audio_driver_t driver, int32_t sampleRate, audio_sample_type_t sampleType);
    uint32_t (*api_sample_position)(audio_sample_handle_t sample);
    int32_t (*api_sample_buffer_ready)(audio_sample_handle_t sample);
    void (*api_load_sample_buffer)(audio_sample_handle_t sample, int32_t bufferIndex, const void *data, int32_t byteCount);
    void (*api_set_3D_sample_info)(audio_3d_sample_handle_t sample, const snd_alias_sound_file_t *soundFile);
    void (*api_set_3D_sample_volume)(audio_3d_sample_handle_t sample, float volume);
    void (*api_set_3D_sample_distances)(audio_3d_sample_handle_t sample, float maximumDistance, float minimumDistance);
    int32_t (*api_3D_sample_playback_rate)(audio_3d_sample_handle_t sample);
    uint32_t (*api_3D_sample_offset)(audio_3d_sample_handle_t sample);
    uint32_t (*api_3D_sample_length)(audio_3d_sample_handle_t sample);
    float (*api_3D_sample_volume)(audio_3d_sample_handle_t sample);
    void (*api_3D_position)(audio_3d_sample_handle_t sample, float *x, float *y, float *z);
    void (*api_set_3D_sample_playback_rate)(audio_3d_sample_handle_t sample, int32_t playbackRate);
    void (*api_set_3D_sample_loop_count)(audio_3d_sample_handle_t sample, int32_t loopCount);
    void (*api_set_3D_sample_effects_level)(audio_3d_sample_handle_t sample, float effectsLevel);
    int32_t (*api_set_3D_sample_preference)(audio_3d_sample_handle_t sample, const char *preferenceName, void *value);
    void (*api_set_digital_master_room_type)(audio_driver_t driver, int32_t roomType);
    void (*api_set_3D_room_type)(audio_provider_t provider, int32_t roomType);
    void (*api_set_3D_sample_offset)(audio_3d_sample_handle_t sample, int32_t byteOffset);
    void (*api_start_3D_sample)(audio_3d_sample_handle_t sample);
} audio_backend_api_t;

/* NOT_FROM_ORIGINAL_SOURCE: select the complete native audio backend named by
 * the stock mss_3d_provider cvar, falling back to Miniaudio. */
qboolean audio_select_backend(const char *preferredName,
                                      const char **selectedName);
void audio_bind_loaded_sample(
    audio_sample_handle_t sample,
    const snd_alias_sound_file_t *soundFile);
void audio_forget_loaded_sound(
    const snd_alias_sound_file_t *soundFile);

int32_t audio_set_preference(int32_t preference, int32_t value);
void audio_set_file_callbacks(
    audio_file_open_callback_t openCallback,
    audio_file_close_callback_t closeCallback,
    audio_file_seek_callback_t seekCallback,
    audio_file_read_callback_t readCallback);
void audio_set_redist_directory(const char *directory);
int32_t audio_startup(void);
void audio_shutdown(void);
void audio_close_3D_provider(audio_provider_t provider);
void audio_set_DirectSound_HWND(audio_driver_t driver, audio_window_handle_t windowHandle);
int32_t audio_WAV_info(const void *fileData, audio_sound_info_t *soundInfo);
uint32_t audio_size_processed_digital_audio(
    uint32_t sampleRate, audio_sample_type_t sampleType,
    int32_t bufferCount, const audio_sound_info_t *sourceInfo);
int32_t audio_process_digital_audio(
    void *destination, uint32_t destinationSize, uint32_t sampleRate,
    audio_sample_type_t sampleType, int32_t bufferCount,
    audio_sound_info_t *sourceInfo);
int32_t audio_digital_CPU_percent(audio_driver_t driver);
audio_driver_t audio_open_digital_driver(int32_t sampleRate, int32_t sampleFormat, int32_t channels, int32_t flags);
const char * audio_last_error(void);
int32_t audio_enumerate_3D_providers(audio_provider_enumerator_t *enumerator, audio_provider_t *provider, const char **providerName);
int32_t audio_open_3D_provider(audio_provider_t provider);
int32_t audio_3D_provider_attribute(audio_provider_t provider, const char *attributeName, void *value);
int32_t audio_set_3D_provider_preference(audio_provider_t provider, const char *preferenceName, void *value);
void audio_set_3D_distance_factor(audio_provider_t provider, float distanceFactor);
audio_sample_handle_t audio_allocate_sample_handle(audio_driver_t driver);
audio_3d_sample_handle_t audio_allocate_3D_sample_handle(audio_provider_t provider);
void audio_set_3D_position(audio_3d_sample_handle_t sample, float x, float y, float z);
void audio_end_sample(audio_sample_handle_t sample);
void audio_stop_sample(audio_sample_handle_t sample);
void audio_resume_sample(audio_sample_handle_t sample);
int32_t audio_sample_status(audio_sample_handle_t sample);
void audio_end_3D_sample(audio_3d_sample_handle_t sample);
void audio_stop_3D_sample(audio_3d_sample_handle_t sample);
void audio_resume_3D_sample(audio_3d_sample_handle_t sample);
int32_t audio_3D_sample_status(audio_3d_sample_handle_t sample);
audio_stream_handle_t audio_open_stream(audio_driver_t driver, const char *filename, int32_t streamMemory);
void audio_close_stream(audio_stream_handle_t stream);
void audio_pause_stream(audio_stream_handle_t stream, qboolean paused);
int32_t audio_stream_status(audio_stream_handle_t stream);
int32_t audio_stream_playback_rate(audio_stream_handle_t stream);
void audio_set_stream_playback_rate(audio_stream_handle_t stream, int32_t playbackRate);
void audio_set_stream_volume_pan(audio_stream_handle_t stream, float volume, float pan);
void audio_stream_volume_pan(audio_stream_handle_t stream, float *volume, float *pan);
void audio_set_stream_loop_count(audio_stream_handle_t stream, int32_t loopCount);
void audio_set_stream_reverb_levels(audio_stream_handle_t stream, float dryLevel, float wetLevel);
void audio_stream_ms_position(audio_stream_handle_t stream, int32_t *totalMsec, int32_t *currentMsec);
void audio_set_stream_ms_position(audio_stream_handle_t stream, int32_t positionMsec);
void audio_start_stream(audio_stream_handle_t stream);
void audio_init_sample(audio_sample_handle_t sample);
void audio_set_sample_type(audio_sample_handle_t sample, audio_sample_type_t sampleType, int32_t flags);
void audio_set_sample_address(audio_sample_handle_t sample, const void *data, uint32_t dataLength);
void audio_set_sample_adpcm_block_size(audio_sample_handle_t sample, uint32_t blockSize);
int32_t audio_sample_playback_rate(audio_sample_handle_t sample);
void audio_set_sample_playback_rate(audio_sample_handle_t sample, int32_t playbackRate);
void audio_set_sample_volume_pan(audio_sample_handle_t sample, float volume, float pan);
void audio_sample_volume_pan(audio_sample_handle_t sample, float *volume, float *pan);
void audio_set_sample_loop_count(audio_sample_handle_t sample, int32_t loopCount);
void audio_set_sample_reverb_levels(audio_sample_handle_t sample, float dryLevel, float wetLevel);
void audio_sample_ms_position(audio_sample_handle_t sample, int32_t *totalMsec, int32_t *currentMsec);
void audio_set_sample_ms_position(audio_sample_handle_t sample, int32_t positionMsec);
void audio_start_sample(audio_sample_handle_t sample);
void audio_release_sample_handle(audio_sample_handle_t sample);
int32_t audio_minimum_sample_buffer_size(audio_driver_t driver, int32_t sampleRate, audio_sample_type_t sampleType);
uint32_t audio_sample_position(audio_sample_handle_t sample);
int32_t audio_sample_buffer_ready(audio_sample_handle_t sample);
void audio_load_sample_buffer(audio_sample_handle_t sample, int32_t bufferIndex, const void *data, int32_t byteCount);
void audio_set_3D_sample_info(audio_3d_sample_handle_t sample, const snd_alias_sound_file_t *soundFile);
void audio_set_3D_sample_volume(audio_3d_sample_handle_t sample, float volume);
void audio_set_3D_sample_distances(audio_3d_sample_handle_t sample, float maximumDistance, float minimumDistance);
int32_t audio_3D_sample_playback_rate(audio_3d_sample_handle_t sample);
uint32_t audio_3D_sample_offset(audio_3d_sample_handle_t sample);
uint32_t audio_3D_sample_length(audio_3d_sample_handle_t sample);
float audio_3D_sample_volume(audio_3d_sample_handle_t sample);
void audio_3D_position(audio_3d_sample_handle_t sample, float *x, float *y, float *z);
void audio_set_3D_sample_playback_rate(audio_3d_sample_handle_t sample, int32_t playbackRate);
void audio_set_3D_sample_loop_count(audio_3d_sample_handle_t sample, int32_t loopCount);
void audio_set_3D_sample_effects_level(audio_3d_sample_handle_t sample, float effectsLevel);
int32_t audio_set_3D_sample_preference(audio_3d_sample_handle_t sample, const char *preferenceName, void *value);
void audio_set_digital_master_room_type(audio_driver_t driver, int32_t roomType);
void audio_set_3D_room_type(audio_provider_t provider, int32_t roomType);
void audio_set_3D_sample_offset(audio_3d_sample_handle_t sample, int32_t byteOffset);
void audio_start_3D_sample(audio_3d_sample_handle_t sample);

#if defined(AUDIO_BACKEND_MINIAUDIO)
extern const audio_backend_api_t miniaudio_backend;
#endif
#if defined(AUDIO_BACKEND_OPENAL)
extern const audio_backend_api_t openal_backend;
#endif

#endif

#ifdef __cplusplus
}
#endif

#endif
