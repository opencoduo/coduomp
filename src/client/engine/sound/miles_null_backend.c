#include "miles_boundary.h"

#if defined(CODUOMP_DISABLE_AUDIO) || (!defined(_WIN32) && !defined(__APPLE__) && !defined(__linux__))

/*
 * NOT_FROM_ORIGINAL_SOURCE: explicit no-audio implementation of the Miles
 * linkage boundary. AIL_startup reports that the compatibility boundary is
 * available so MSS_Init registers the original sound cvars; device creation
 * then fails through AIL_open_digital_driver, leaving sound disabled. The
 * remaining entries keep later shutdown, diagnostics, and defensive calls
 * safe on platforms without a native audio backend. They do not report
 * successful playback. CODUOMP_DISABLE_AUDIO selects this backend explicitly
 * for silent, build-isolated runtime validation.
 */

enum {
    MILES_NULL_STARTUP_SUCCEEDED = 1
};

int32_t MILES_CALLBACK AIL_set_preference(int32_t preference, int32_t value)
{
    (void)preference;
    (void)value;
    return 0;
}

void MILES_CALLBACK AIL_set_file_callbacks(miles_file_open_callback_t openCallback, miles_file_close_callback_t closeCallback,
                                           miles_file_seek_callback_t seekCallback, miles_file_read_callback_t readCallback)
{
    (void)openCallback;
    (void)closeCallback;
    (void)seekCallback;
    (void)readCallback;
}

void MILES_CALLBACK AIL_set_redist_directory(const char *directory)
{
    (void)directory;
}

int32_t MILES_CALLBACK AIL_startup(void)
{
    return MILES_NULL_STARTUP_SUCCEEDED;
}

void MILES_CALLBACK AIL_shutdown(void)
{
}

void MILES_CALLBACK AIL_close_3D_provider(miles_3d_provider_t provider)
{
    (void)provider;
}

void MILES_CALLBACK AIL_set_DirectSound_HWND(miles_digital_driver_t driver, miles_window_handle_t windowHandle)
{
    (void)driver;
    (void)windowHandle;
}

int32_t MILES_CALLBACK AIL_WAV_info(const void *fileData, miles_sound_info_t *soundInfo)
{
    (void)fileData;
    (void)soundInfo;
    return 0;
}

uint32_t MILES_CALLBACK AIL_size_processed_digital_audio(uint32_t sampleRate, milesSampleType_t sampleType, int32_t bufferCount,
                                                         const miles_sound_info_t *sourceInfo)
{
    (void)sampleRate;
    (void)sampleType;
    (void)bufferCount;
    (void)sourceInfo;
    return 0;
}

int32_t MILES_CALLBACK AIL_process_digital_audio(void *destination, uint32_t destinationSize, uint32_t sampleRate,
                                                 milesSampleType_t sampleType, int32_t bufferCount, miles_sound_info_t *sourceInfo)
{
    (void)destination;
    (void)destinationSize;
    (void)sampleRate;
    (void)sampleType;
    (void)bufferCount;
    (void)sourceInfo;
    return 0;
}

int32_t MILES_CALLBACK AIL_digital_CPU_percent(miles_digital_driver_t driver)
{
    (void)driver;
    return 0;
}

miles_digital_driver_t MILES_CALLBACK AIL_open_digital_driver(int32_t sampleRate, int32_t sampleFormat, int32_t channels, int32_t flags)
{
    (void)sampleRate;
    (void)sampleFormat;
    (void)channels;
    (void)flags;
    return NULL;
}

const char *MILES_CALLBACK AIL_last_error(void)
{
    return "native Miles replacement is disabled";
}

int32_t MILES_CALLBACK AIL_enumerate_3D_providers(miles_3d_provider_enumerator_t *enumerator, miles_3d_provider_t *provider,
                                                  const char **providerName)
{
    (void)enumerator;
    (void)provider;
    (void)providerName;
    return 0;
}

int32_t MILES_CALLBACK AIL_open_3D_provider(miles_3d_provider_t provider)
{
    (void)provider;
    return 0;
}

int32_t MILES_CALLBACK AIL_3D_provider_attribute(miles_3d_provider_t provider, const char *attributeName, void *value)
{
    (void)provider;
    (void)attributeName;
    (void)value;
    return 0;
}

int32_t MILES_CALLBACK AIL_set_3D_provider_preference(miles_3d_provider_t provider, const char *preferenceName, void *value)
{
    (void)provider;
    (void)preferenceName;
    (void)value;
    return 0;
}

void MILES_CALLBACK AIL_set_3D_distance_factor(miles_3d_provider_t provider, float distanceFactor)
{
    (void)provider;
    (void)distanceFactor;
}

miles_sample_handle_t MILES_CALLBACK AIL_allocate_sample_handle(miles_digital_driver_t driver)
{
    (void)driver;
    return NULL;
}

miles_3d_sample_handle_t MILES_CALLBACK AIL_allocate_3D_sample_handle(miles_3d_provider_t provider)
{
    (void)provider;
    return NULL;
}

void MILES_CALLBACK AIL_set_3D_position(miles_3d_sample_handle_t sample, float x, float y, float z)
{
    (void)sample;
    (void)x;
    (void)y;
    (void)z;
}

void MILES_CALLBACK AIL_end_sample(miles_sample_handle_t sample)
{
    (void)sample;
}

void MILES_CALLBACK AIL_stop_sample(miles_sample_handle_t sample)
{
    (void)sample;
}

void MILES_CALLBACK AIL_resume_sample(miles_sample_handle_t sample)
{
    (void)sample;
}

int32_t MILES_CALLBACK AIL_sample_status(miles_sample_handle_t sample)
{
    (void)sample;
    return MILES_SAMPLE_STATUS_DONE;
}

void MILES_CALLBACK AIL_end_3D_sample(miles_3d_sample_handle_t sample)
{
    (void)sample;
}

void MILES_CALLBACK AIL_stop_3D_sample(miles_3d_sample_handle_t sample)
{
    (void)sample;
}

void MILES_CALLBACK AIL_resume_3D_sample(miles_3d_sample_handle_t sample)
{
    (void)sample;
}

int32_t MILES_CALLBACK AIL_3D_sample_status(miles_3d_sample_handle_t sample)
{
    (void)sample;
    return MILES_SAMPLE_STATUS_DONE;
}

miles_stream_handle_t MILES_CALLBACK AIL_open_stream(miles_digital_driver_t driver, const char *filename, int32_t streamMemory)
{
    (void)driver;
    (void)filename;
    (void)streamMemory;
    return NULL;
}

void MILES_CALLBACK AIL_close_stream(miles_stream_handle_t stream)
{
    (void)stream;
}

void MILES_CALLBACK AIL_pause_stream(miles_stream_handle_t stream, qboolean paused)
{
    (void)stream;
    (void)paused;
}

int32_t MILES_CALLBACK AIL_stream_status(miles_stream_handle_t stream)
{
    (void)stream;
    return MILES_SAMPLE_STATUS_DONE;
}

int32_t MILES_CALLBACK AIL_stream_playback_rate(miles_stream_handle_t stream)
{
    (void)stream;
    return 0;
}

void MILES_CALLBACK AIL_set_stream_playback_rate(miles_stream_handle_t stream, int32_t playbackRate)
{
    (void)stream;
    (void)playbackRate;
}

void MILES_CALLBACK AIL_set_stream_volume_pan(miles_stream_handle_t stream, float volume, float pan)
{
    (void)stream;
    (void)volume;
    (void)pan;
}

void MILES_CALLBACK AIL_stream_volume_pan(miles_stream_handle_t stream, float *volume, float *pan)
{
    (void)stream;
    if (volume != NULL)
        *volume = 0.0f;
    if (pan != NULL)
        *pan = 0.0f;
}

void MILES_CALLBACK AIL_set_stream_loop_count(miles_stream_handle_t stream, int32_t loopCount)
{
    (void)stream;
    (void)loopCount;
}

void MILES_CALLBACK AIL_set_stream_reverb_levels(miles_stream_handle_t stream, float dryLevel, float wetLevel)
{
    (void)stream;
    (void)dryLevel;
    (void)wetLevel;
}

void MILES_CALLBACK AIL_stream_ms_position(miles_stream_handle_t stream, int32_t *totalMsec, int32_t *currentMsec)
{
    (void)stream;
    if (totalMsec != NULL)
        *totalMsec = 0;
    if (currentMsec != NULL)
        *currentMsec = 0;
}

void MILES_CALLBACK AIL_set_stream_ms_position(miles_stream_handle_t stream, int32_t positionMsec)
{
    (void)stream;
    (void)positionMsec;
}

void MILES_CALLBACK AIL_start_stream(miles_stream_handle_t stream)
{
    (void)stream;
}

void MILES_CALLBACK AIL_init_sample(miles_sample_handle_t sample)
{
    (void)sample;
}

void MILES_CALLBACK AIL_set_sample_type(miles_sample_handle_t sample, milesSampleType_t sampleType, int32_t flags)
{
    (void)sample;
    (void)sampleType;
    (void)flags;
}

void MILES_CALLBACK AIL_set_sample_address(miles_sample_handle_t sample, const void *data, uint32_t dataLength)
{
    (void)sample;
    (void)data;
    (void)dataLength;
}

void MILES_CALLBACK AIL_set_sample_adpcm_block_size(miles_sample_handle_t sample, uint32_t blockSize)
{
    (void)sample;
    (void)blockSize;
}

int32_t MILES_CALLBACK AIL_sample_playback_rate(miles_sample_handle_t sample)
{
    (void)sample;
    return 0;
}

void MILES_CALLBACK AIL_set_sample_playback_rate(miles_sample_handle_t sample, int32_t playbackRate)
{
    (void)sample;
    (void)playbackRate;
}

/* NOT_FROM_ORIGINAL_SOURCE: the explicit no-audio backend has no sample
 * buffer to bind, but retains the native compatibility boundary. */
void coduomp_openal_bind_loaded_sample(miles_sample_handle_t sample, const snd_alias_sound_file_t *soundFile)
{
    (void)sample;
    (void)soundFile;
}

void MILES_CALLBACK AIL_set_sample_volume_pan(miles_sample_handle_t sample, float volume, float pan)
{
    (void)sample;
    (void)volume;
    (void)pan;
}

void MILES_CALLBACK AIL_sample_volume_pan(miles_sample_handle_t sample, float *volume, float *pan)
{
    (void)sample;
    if (volume != NULL)
        *volume = 0.0f;
    if (pan != NULL)
        *pan = 0.0f;
}

void MILES_CALLBACK AIL_set_sample_loop_count(miles_sample_handle_t sample, int32_t loopCount)
{
    (void)sample;
    (void)loopCount;
}

void MILES_CALLBACK AIL_set_sample_reverb_levels(miles_sample_handle_t sample, float dryLevel, float wetLevel)
{
    (void)sample;
    (void)dryLevel;
    (void)wetLevel;
}

void MILES_CALLBACK AIL_sample_ms_position(miles_sample_handle_t sample, int32_t *totalMsec, int32_t *currentMsec)
{
    (void)sample;
    if (totalMsec != NULL)
        *totalMsec = 0;
    if (currentMsec != NULL)
        *currentMsec = 0;
}

void MILES_CALLBACK AIL_set_sample_ms_position(miles_sample_handle_t sample, int32_t positionMsec)
{
    (void)sample;
    (void)positionMsec;
}

void MILES_CALLBACK AIL_start_sample(miles_sample_handle_t sample)
{
    (void)sample;
}

void MILES_CALLBACK AIL_release_sample_handle(miles_sample_handle_t sample)
{
    (void)sample;
}

int32_t MILES_CALLBACK AIL_minimum_sample_buffer_size(miles_digital_driver_t driver, int32_t sampleRate, milesSampleType_t sampleType)
{
    (void)driver;
    (void)sampleRate;
    (void)sampleType;
    return 0;
}

uint32_t MILES_CALLBACK AIL_sample_position(miles_sample_handle_t sample)
{
    (void)sample;
    return 0;
}

int32_t MILES_CALLBACK AIL_sample_buffer_ready(miles_sample_handle_t sample)
{
    (void)sample;
    return -1;
}

void MILES_CALLBACK AIL_load_sample_buffer(miles_sample_handle_t sample, int32_t bufferIndex, const void *data, int32_t byteCount)
{
    (void)sample;
    (void)bufferIndex;
    (void)data;
    (void)byteCount;
}

void MILES_CALLBACK AIL_set_3D_sample_info(miles_3d_sample_handle_t sample, const snd_alias_sound_file_t *soundFile)
{
    (void)sample;
    (void)soundFile;
}

void MILES_CALLBACK AIL_set_3D_sample_volume(miles_3d_sample_handle_t sample, float volume)
{
    (void)sample;
    (void)volume;
}

void MILES_CALLBACK AIL_set_3D_sample_distances(miles_3d_sample_handle_t sample, float maximumDistance, float minimumDistance)
{
    (void)sample;
    (void)maximumDistance;
    (void)minimumDistance;
}

int32_t MILES_CALLBACK AIL_3D_sample_playback_rate(miles_3d_sample_handle_t sample)
{
    (void)sample;
    return 0;
}

uint32_t MILES_CALLBACK AIL_3D_sample_offset(miles_3d_sample_handle_t sample)
{
    (void)sample;
    return 0;
}

uint32_t MILES_CALLBACK AIL_3D_sample_length(miles_3d_sample_handle_t sample)
{
    (void)sample;
    return 0;
}

float MILES_CALLBACK AIL_3D_sample_volume(miles_3d_sample_handle_t sample)
{
    (void)sample;
    return 0.0f;
}

void MILES_CALLBACK AIL_3D_position(miles_3d_sample_handle_t sample, float *x, float *y, float *z)
{
    (void)sample;
    if (x != NULL)
        *x = 0.0f;
    if (y != NULL)
        *y = 0.0f;
    if (z != NULL)
        *z = 0.0f;
}

void MILES_CALLBACK AIL_set_3D_sample_playback_rate(miles_3d_sample_handle_t sample, int32_t playbackRate)
{
    (void)sample;
    (void)playbackRate;
}

void MILES_CALLBACK AIL_set_3D_sample_loop_count(miles_3d_sample_handle_t sample, int32_t loopCount)
{
    (void)sample;
    (void)loopCount;
}

void MILES_CALLBACK AIL_set_3D_sample_effects_level(miles_3d_sample_handle_t sample, float effectsLevel)
{
    (void)sample;
    (void)effectsLevel;
}

int32_t MILES_CALLBACK AIL_set_3D_sample_preference(miles_3d_sample_handle_t sample, const char *preferenceName, void *value)
{
    (void)sample;
    (void)preferenceName;
    (void)value;
    return 0;
}

void MILES_CALLBACK AIL_set_digital_master_room_type(miles_digital_driver_t driver, int32_t roomType)
{
    (void)driver;
    (void)roomType;
}

void MILES_CALLBACK AIL_set_3D_room_type(miles_3d_provider_t provider, int32_t roomType)
{
    (void)provider;
    (void)roomType;
}

void MILES_CALLBACK AIL_set_3D_sample_offset(miles_3d_sample_handle_t sample, int32_t byteOffset)
{
    (void)sample;
    (void)byteOffset;
}

void MILES_CALLBACK AIL_start_3D_sample(miles_3d_sample_handle_t sample)
{
    (void)sample;
}

#endif
