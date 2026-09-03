#include "sound_system.h"
#include "audio_backend_api.h"
#include "qcommon/q_string.h"

#if defined(AUDIO_BACKEND_OPENAL)

/*
 * NOT_FROM_ORIGINAL_SOURCE: deprecated backup audio backend implemented with
 * OpenAL. AudioToolbox (macOS) or libsndfile (Linux) decodes streamed files;
 * the backend owns device output, mixing, loaded samples, raw samples, and
 * streams whenever selected through mss_3d_provider.
 */

/* NOT_FROM_ORIGINAL_SOURCE_STORAGE_FILE: all private storage in this
 * translation unit belongs to the OpenAL backend. */

#if defined(__APPLE__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#include <AudioToolbox/AudioToolbox.h>
#include <OpenAL/al.h>
#include <OpenAL/alc.h>
#include <OpenAL/MacOSX_OALExtensions.h>
#else
#include <AL/al.h>
#include <AL/alc.h>
#include <AL/efx.h>
#include <AL/efx-presets.h>
#include <sndfile.h>
#endif

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    OPENAL_PROVIDER = 1,
    OPENAL_STATUS_PLAYING = 4,
    OPENAL_WAVE_FORMAT_PCM = 1,
    OPENAL_WAVE_FORMAT_IMA_ADPCM = 17,
    OPENAL_IMA_HEADER_BYTES_PER_CHANNEL = 4,
    OPENAL_RAW_BUFFER_COUNT = 2,
    OPENAL_STREAM_BUFFER_COUNT = 3,
    OPENAL_STREAM_BUFFER_FRAMES = 4096,
    OPENAL_BUFFER_CACHE_BUCKET_COUNT = 256,
    OPENAL_FAST2D_LEFT = 0,
    OPENAL_FAST2D_RIGHT = 1,
    OPENAL_FAST2D_CHANNEL_COUNT = 2
};

typedef struct openal_driver_s {
    ALCdevice *device;
    ALCcontext *context;
    int32_t sampleRate;
#if defined(__APPLE__)
    alcASASetSourceProcPtr setSourceProperty;
    alcASASetListenerProcPtr setListenerProperty;
#else
    LPALGENEFFECTS genEffects;
    LPALDELETEEFFECTS deleteEffects;
    LPALEFFECTI effecti;
    LPALEFFECTF effectf;
    LPALEFFECTFV effectfv;
    LPALGENFILTERS genFilters;
    LPALDELETEFILTERS deleteFilters;
    LPALFILTERI filteri;
    LPALFILTERF filterf;
    LPALGENAUXILIARYEFFECTSLOTS genAuxiliaryEffectSlots;
    LPALDELETEAUXILIARYEFFECTSLOTS deleteAuxiliaryEffectSlots;
    LPALAUXILIARYEFFECTSLOTI auxiliaryEffectSloti;
    ALuint reverbEffect;
    ALuint reverbSlot;
    qboolean efxAvailable;
    qboolean efxUsesEaxReverb;
#endif
} openal_driver_t;

typedef struct openal_pcm_s {
    void *ownedBytes;
    uint32_t byteCount;
    uint32_t frameCount;
    int32_t sampleRate;
    int32_t channelCount;
    ALenum format;
} openal_pcm_t;

typedef struct openal_memory_file_s {
    const uint8_t *borrowedBytes;
    int64_t byteCount;
#if defined(__linux__)
    int64_t position;
#endif
} openal_memory_file_t;

typedef struct openal_buffer_cache_entry_s {
    const void *data;
    uint32_t dataLength;
    uint32_t blockSize;
    audio_sample_type_t sampleType;
    int32_t sampleRate;
    ALuint buffer;
    ALuint fast2dBuffers[OPENAL_FAST2D_CHANNEL_COUNT];
    uint32_t frameCount;
    struct openal_buffer_cache_entry_s *next;
} openal_buffer_cache_entry_t;

struct audio_sample_handle_s {
    ALuint source;
    ALuint buffer; /* Borrowed from openal_buffer_cache. */
    ALuint rawBuffers[OPENAL_RAW_BUFFER_COUNT];
    audio_sample_type_t sampleType;
    const void *data;
    uint32_t dataLength;
    uint32_t blockSize;
    int32_t sampleRate;
    int32_t playbackRate;
    float volume;
    float pan;
    uint32_t frameCount;
    qboolean rawMode;
#if defined(__linux__)
    ALuint reverbFilter;
#endif
    struct audio_sample_handle_s *next;
};

struct audio_3d_sample_handle_s {
    audio_provider_t provider;
    ALuint sources[OPENAL_FAST2D_CHANNEL_COUNT];
    /* Borrowed from openal_buffer_cache. */
    ALuint buffers[OPENAL_FAST2D_CHANNEL_COUNT];
    const snd_alias_sound_file_t *soundFile;
    int32_t playbackRate;
    float volume;
    float position[3];
    uint32_t frameCount;
#if defined(__linux__)
    ALuint reverbFilters[OPENAL_FAST2D_CHANNEL_COUNT];
#endif
    struct audio_3d_sample_handle_s *next;
};

struct audio_stream_handle_s {
    ALuint source;
    ALuint buffers[OPENAL_STREAM_BUFFER_COUNT];
    uint32_t bufferFrames[OPENAL_STREAM_BUFFER_COUNT];
    void *ownedFileBytes;
    size_t fileByteCount;
    openal_memory_file_t memoryFile;
#if defined(__APPLE__)
    AudioFileID audioFile;
    ExtAudioFileRef extendedFile;
    AudioStreamBasicDescription clientFormat;
#else
    SNDFILE *soundFile;
    SF_INFO soundInfo;
#endif
    void *decodePcmBuffer;
    int32_t baseRate;
    int32_t playbackRate;
    float volume;
    float pan;
    int32_t loopCount;
    uint32_t frameCount;
    uint64_t playedFrames;
    qboolean wantsPlayback;
    qboolean decoderAtEnd;
#if defined(__linux__)
    ALuint reverbFilter;
#endif
    struct audio_stream_handle_s *next;
};

static openal_driver_t openal_driver;
static struct audio_sample_handle_s *openal_samples;
static struct audio_3d_sample_handle_s *openal_3d_samples;
static struct audio_stream_handle_s *openal_streams;
static openal_buffer_cache_entry_t
    *openal_buffer_cache[
        OPENAL_BUFFER_CACHE_BUCKET_COUNT];
static audio_file_open_callback_t openal_file_open;
static audio_file_close_callback_t openal_file_close;
static audio_file_seek_callback_t openal_file_seek;
static audio_file_read_callback_t openal_file_read;
static char openal_error[256] = "no OpenAL error";

static uint16_t openal_read_u16(const uint8_t *bytes)
{
    return (uint16_t)((uint16_t)bytes[0] |
                      ((uint16_t)bytes[1] << 8));
}

static int16_t openal_read_s16(const uint8_t *bytes)
{
    return (int16_t)openal_read_u16(bytes);
}

static uint32_t openal_read_u32(const uint8_t *bytes)
{
    return (uint32_t)bytes[0] |
           ((uint32_t)bytes[1] << 8) |
           ((uint32_t)bytes[2] << 16) |
           ((uint32_t)bytes[3] << 24);
}

static float openal_clamp(float value, float minimum, float maximum)
{
    if (value < minimum)
        return minimum;
    if (value > maximum)
        return maximum;
    return value;
}

static void openal_set_error(const char *message)
{
    (void)snprintf(openal_error, sizeof(openal_error),
                   "%s", message);
}

static qboolean openal_check(const char *operation)
{
    const ALenum error = alGetError();
    if (error == AL_NO_ERROR)
        return qtrue;
    (void)snprintf(openal_error, sizeof(openal_error),
                   "%s failed with OpenAL error 0x%x",
                   operation, (unsigned int)error);
    return qfalse;
}

static void openal_clear_error(void)
{
    while (alGetError() != AL_NO_ERROR) {
    }
}

static ALenum openal_format(audio_sample_type_t sampleType)
{
    switch (sampleType) {
    case AUDIO_SAMPLE_TYPE_MONO_8:
        return AL_FORMAT_MONO8;
    case AUDIO_SAMPLE_TYPE_MONO_16:
    case AUDIO_SAMPLE_TYPE_MONO_IMA_ADPCM:
        return AL_FORMAT_MONO16;
    case AUDIO_SAMPLE_TYPE_STEREO_8:
        return AL_FORMAT_STEREO8;
    case AUDIO_SAMPLE_TYPE_STEREO_16:
    case AUDIO_SAMPLE_TYPE_STEREO_IMA_ADPCM:
        return AL_FORMAT_STEREO16;
    }
    return 0;
}

static int32_t openal_channels(audio_sample_type_t sampleType)
{
    switch (sampleType) {
    case AUDIO_SAMPLE_TYPE_MONO_8:
    case AUDIO_SAMPLE_TYPE_MONO_16:
    case AUDIO_SAMPLE_TYPE_MONO_IMA_ADPCM:
        return 1;
    case AUDIO_SAMPLE_TYPE_STEREO_8:
    case AUDIO_SAMPLE_TYPE_STEREO_16:
    case AUDIO_SAMPLE_TYPE_STEREO_IMA_ADPCM:
        return 2;
    }
    return 0;
}

static int32_t openal_bytes_per_sample(audio_sample_type_t sampleType)
{
    return sampleType == AUDIO_SAMPLE_TYPE_MONO_8 ||
                   sampleType == AUDIO_SAMPLE_TYPE_STEREO_8
               ? 1
               : 2;
}

static qboolean openal_is_adpcm(audio_sample_type_t sampleType)
{
    return sampleType == AUDIO_SAMPLE_TYPE_MONO_IMA_ADPCM ||
                   sampleType == AUDIO_SAMPLE_TYPE_STEREO_IMA_ADPCM
               ? qtrue
               : qfalse;
}

static int16_t openal_ima_nibble(
    uint8_t nibble, int32_t *predictor, int32_t *stepIndex)
{
    static const int32_t indexTable[16] = {
        -1, -1, -1, -1, 2, 4, 6, 8,
        -1, -1, -1, -1, 2, 4, 6, 8
    };
    static const int32_t stepTable[89] = {
        7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28,
        31, 34, 37, 41, 45, 50, 55, 60, 66, 73, 80, 88, 97, 107,
        118, 130, 143, 157, 173, 190, 209, 230, 253, 279, 307, 337,
        371, 408, 449, 494, 544, 598, 658, 724, 796, 876, 963,
        1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066, 2272,
        2499, 2749, 3024, 3327, 3660, 4026, 4428, 4871, 5358,
        5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635,
        13899, 15289, 16818, 18500, 20350, 22385, 24623, 27086,
        29794, 32767
    };
    const int32_t step = stepTable[*stepIndex];
    int32_t difference = step >> 3;
    if ((nibble & 1u) != 0)
        difference += step >> 2;
    if ((nibble & 2u) != 0)
        difference += step >> 1;
    if ((nibble & 4u) != 0)
        difference += step;
    if ((nibble & 8u) != 0)
        *predictor -= difference;
    else
        *predictor += difference;
    if (*predictor < -32768)
        *predictor = -32768;
    else if (*predictor > 32767)
        *predictor = 32767;
    *stepIndex += indexTable[nibble & 15u];
    if (*stepIndex < 0)
        *stepIndex = 0;
    else if (*stepIndex > 88)
        *stepIndex = 88;
    return (int16_t)*predictor;
}

static qboolean openal_decode_ima(
    const void *data, uint32_t dataLength, uint32_t blockSize,
    int32_t channelCount, openal_pcm_t *pcm)
{
    if (data == NULL || blockSize == 0 ||
        (channelCount != 1 && channelCount != 2))
        return qfalse;

    const uint32_t headerSize =
        (uint32_t)channelCount *
        OPENAL_IMA_HEADER_BYTES_PER_CHANNEL;
    if (blockSize < headerSize)
        return qfalse;
    const uint32_t framesPerBlock =
        1u + ((blockSize - headerSize) * 2u) / (uint32_t)channelCount;
    const uint32_t blockCount =
        (dataLength + blockSize - 1u) / blockSize;
    const uint64_t capacityFrames =
        (uint64_t)framesPerBlock * blockCount;
    const uint64_t capacityBytes =
        capacityFrames * (uint32_t)channelCount * sizeof(int16_t);
    if (capacityBytes == 0 || capacityBytes > UINT32_MAX)
        return qfalse;

    int16_t *const output = malloc((size_t)capacityBytes);
    if (output == NULL)
        return qfalse;

    const uint8_t *const input = data;
    uint32_t inputOffset = 0;
    uint32_t outputFrames = 0;
    while (inputOffset + headerSize <= dataLength) {
        uint32_t currentBlockSize = dataLength - inputOffset;
        if (currentBlockSize > blockSize)
            currentBlockSize = blockSize;
        const uint8_t *const block = input + inputOffset;
        int32_t predictor[2] = {0, 0};
        int32_t stepIndex[2] = {0, 0};
        for (int32_t channel = 0; channel < channelCount; ++channel) {
            const uint8_t *const header = block + channel * 4;
            predictor[channel] = openal_read_s16(header);
            stepIndex[channel] = header[2];
            if (stepIndex[channel] > 88)
                stepIndex[channel] = 88;
            output[(size_t)outputFrames * channelCount + channel] =
                (int16_t)predictor[channel];
        }
        ++outputFrames;

        uint32_t blockOffset = headerSize;
        if (channelCount == 1) {
            while (blockOffset < currentBlockSize) {
                const uint8_t packed = block[blockOffset++];
                output[outputFrames++] = openal_ima_nibble(
                    packed & 15u, &predictor[0], &stepIndex[0]);
                if (outputFrames >= capacityFrames)
                    break;
                output[outputFrames++] = openal_ima_nibble(
                    packed >> 4, &predictor[0], &stepIndex[0]);
            }
        } else {
            while (blockOffset < currentBlockSize) {
                int16_t decoded[2][8];
                uint32_t decodedCount[2] = {0, 0};
                for (int32_t channel = 0; channel < 2; ++channel) {
                    for (int32_t byteIndex = 0;
                         byteIndex < 4 && blockOffset < currentBlockSize;
                         ++byteIndex) {
                        const uint8_t packed = block[blockOffset++];
                        decoded[channel][decodedCount[channel]++] =
                            openal_ima_nibble(
                                packed & 15u, &predictor[channel],
                                &stepIndex[channel]);
                        decoded[channel][decodedCount[channel]++] =
                            openal_ima_nibble(
                                packed >> 4, &predictor[channel],
                                &stepIndex[channel]);
                    }
                }
                uint32_t groupFrames = decodedCount[0];
                if (decodedCount[1] < groupFrames)
                    groupFrames = decodedCount[1];
                for (uint32_t frame = 0; frame < groupFrames; ++frame) {
                    if (outputFrames >= capacityFrames)
                        break;
                    output[(size_t)outputFrames * 2u] = decoded[0][frame];
                    output[(size_t)outputFrames * 2u + 1u] =
                        decoded[1][frame];
                    ++outputFrames;
                }
            }
        }
        inputOffset += currentBlockSize;
    }

    pcm->ownedBytes = output;
    pcm->byteCount =
        outputFrames * (uint32_t)channelCount * sizeof(int16_t);
    pcm->frameCount = outputFrames;
    pcm->channelCount = channelCount;
    pcm->format =
        channelCount == 1 ? AL_FORMAT_MONO16 : AL_FORMAT_STEREO16;
    return qtrue;
}

static void openal_discard_pcm(openal_pcm_t *pcm)
{
    free(pcm->ownedBytes);
    memset(pcm, 0, sizeof(*pcm));
}

static qboolean openal_prepare_pcm(
    audio_sample_type_t sampleType, const void *data, uint32_t dataLength,
    uint32_t blockSize, int32_t sampleRate, openal_pcm_t *pcm)
{
    memset(pcm, 0, sizeof(*pcm));
    pcm->sampleRate = sampleRate;
    pcm->channelCount = openal_channels(sampleType);
    pcm->format = openal_format(sampleType);
    if (data == NULL || dataLength == 0 || pcm->channelCount == 0 ||
        pcm->format == 0 || sampleRate <= 0)
        return qfalse;

    if (openal_is_adpcm(sampleType))
        return openal_decode_ima(
            data, dataLength, blockSize, pcm->channelCount, pcm);

    const int32_t frameSize =
        pcm->channelCount * openal_bytes_per_sample(sampleType);
    if (frameSize <= 0 || dataLength < (uint32_t)frameSize ||
        dataLength % (uint32_t)frameSize != 0)
        return qfalse;
    pcm->ownedBytes = malloc(dataLength);
    if (pcm->ownedBytes == NULL)
        return qfalse;
    memcpy(pcm->ownedBytes, data, dataLength);
    pcm->byteCount = dataLength;
    pcm->frameCount = dataLength / (uint32_t)frameSize;
    return qtrue;
}

/* NOT_FROM_ORIGINAL_SOURCE: hash immutable loaded-sound payloads into the
 * native OpenAL buffer cache. MSS_UnloadSoundFile evicts each entry before a
 * map hunk clear can reuse its payload address. */
static uint32_t openal_buffer_cache_bucket(
    const void *data, uint32_t dataLength, audio_sample_type_t sampleType,
    int32_t sampleRate)
{
    uintptr_t hash = (uintptr_t)data >> 4;
    hash ^= (uintptr_t)dataLength;
    hash ^= (uintptr_t)(uint32_t)sampleType << 8;
    hash ^= (uintptr_t)(uint32_t)sampleRate << 16;
    hash ^= hash >> 16;
    return (uint32_t)hash &
           (OPENAL_BUFFER_CACHE_BUCKET_COUNT - 1u);
}

/* NOT_FROM_ORIGINAL_SOURCE: create each immutable OpenAL alias buffer once.
 * This keeps allocation, ADPCM decode, and alBufferData out of repeated
 * playback starts. */
static openal_buffer_cache_entry_t *
openal_get_cached_buffer(
    audio_sample_type_t sampleType,
    const void *data, uint32_t dataLength, uint32_t blockSize,
    int32_t sampleRate)
{
    const uint32_t bucket = openal_buffer_cache_bucket(
        data, dataLength, sampleType, sampleRate);
    openal_buffer_cache_entry_t *entry =
        openal_buffer_cache[bucket];

    while (entry != NULL) {
        if (entry->data == data &&
            entry->dataLength == dataLength &&
            entry->blockSize == blockSize &&
            entry->sampleType == sampleType &&
            entry->sampleRate == sampleRate) {
            break;
        }
        entry = entry->next;
    }

    if (entry == NULL) {
        openal_pcm_t pcm;
        if (!openal_prepare_pcm(
                sampleType, data, dataLength, blockSize, sampleRate, &pcm)) {
            openal_set_error("unsupported or invalid sample data");
            return NULL;
        }

        if (pcm.byteCount > INT32_MAX) {
            openal_discard_pcm(&pcm);
            openal_set_error("sample is too large for OpenAL");
            return NULL;
        }

        ALuint cachedBuffer = 0;
        openal_clear_error();
        alGenBuffers(1, &cachedBuffer);
        if (cachedBuffer == 0 ||
            !openal_check("sample buffer allocation")) {
            openal_discard_pcm(&pcm);
            return NULL;
        }

        openal_clear_error();
        alBufferData(cachedBuffer, pcm.format, pcm.ownedBytes,
                     (ALsizei)pcm.byteCount, pcm.sampleRate);
        if (!openal_check("sample upload")) {
            alDeleteBuffers(1, &cachedBuffer);
            openal_discard_pcm(&pcm);
            return NULL;
        }

        entry = calloc(1, sizeof(*entry));
        if (entry == NULL) {
            alDeleteBuffers(1, &cachedBuffer);
            openal_discard_pcm(&pcm);
            openal_set_error("sample buffer cache allocation failed");
            return NULL;
        }

        entry->data = data;
        entry->dataLength = dataLength;
        entry->blockSize = blockSize;
        entry->sampleType = sampleType;
        entry->sampleRate = sampleRate;
        entry->buffer = cachedBuffer;
        entry->frameCount = pcm.frameCount;
        entry->next = openal_buffer_cache[bucket];
        openal_buffer_cache[bucket] = entry;
        openal_discard_pcm(&pcm);
    }

    return entry;
}

/* NOT_FROM_ORIGINAL_SOURCE: attach a shared immutable alias buffer to one
 * reusable OpenAL source. */
static qboolean openal_bind_cached_buffer(
    ALuint source, ALuint *buffer, audio_sample_type_t sampleType,
    const void *data, uint32_t dataLength, uint32_t blockSize,
    int32_t sampleRate, uint32_t *frameCount)
{
    openal_buffer_cache_entry_t *const entry =
        openal_get_cached_buffer(
            sampleType, data, dataLength, blockSize, sampleRate);
    if (entry == NULL)
        return qfalse;

    alSourceStop(source);
    alSourcei(source, AL_BUFFER, 0);
    openal_clear_error();
    alSourcei(source, AL_BUFFER, (ALint)entry->buffer);
    if (!openal_check("sample buffer attachment")) {
        *buffer = 0;
        *frameCount = 0;
        return qfalse;
    }

    *buffer = entry->buffer;
    *frameCount = entry->frameCount;
    return qtrue;
}

/* NOT_FROM_ORIGINAL_SOURCE: build the two one-sided stereo buffers needed to
 * express Miles Fast 2D's independent left/right sample-volume levels without
 * delegating the panning law to OpenAL. The original provider accepts only
 * mono sample data for 3D voices. */
static qboolean openal_create_fast2d_buffers(
    openal_buffer_cache_entry_t *entry)
{
    if (entry->fast2dBuffers[OPENAL_FAST2D_LEFT] != 0 &&
        entry->fast2dBuffers[OPENAL_FAST2D_RIGHT] != 0) {
        return qtrue;
    }

    openal_pcm_t pcm;
    if (!openal_prepare_pcm(
            entry->sampleType, entry->data, entry->dataLength,
            entry->blockSize, entry->sampleRate, &pcm)) {
        openal_set_error("unsupported or invalid 3D sample data");
        return qfalse;
    }
    if (pcm.channelCount != 1 || pcm.frameCount == 0 ||
        (pcm.format != AL_FORMAT_MONO8 &&
         pcm.format != AL_FORMAT_MONO16)) {
        openal_discard_pcm(&pcm);
        openal_set_error(
            "Miles Fast 2D requires mono sample data");
        return qfalse;
    }
    if (pcm.byteCount > (uint32_t)INT32_MAX / 2u) {
        openal_discard_pcm(&pcm);
        openal_set_error("3D sample is too large for OpenAL");
        return qfalse;
    }

    const uint32_t bytesPerSample = pcm.byteCount / pcm.frameCount;
    if ((bytesPerSample != 1u && bytesPerSample != 2u) ||
        pcm.byteCount % pcm.frameCount != 0) {
        openal_discard_pcm(&pcm);
        openal_set_error("invalid mono 3D sample layout");
        return qfalse;
    }

    const uint32_t stereoByteCount = pcm.byteCount * 2u;
    uint8_t *channelBytes[OPENAL_FAST2D_CHANNEL_COUNT] = {
        malloc(stereoByteCount), malloc(stereoByteCount)
    };
    if (channelBytes[OPENAL_FAST2D_LEFT] == NULL ||
        channelBytes[OPENAL_FAST2D_RIGHT] == NULL) {
        free(channelBytes[OPENAL_FAST2D_LEFT]);
        free(channelBytes[OPENAL_FAST2D_RIGHT]);
        openal_discard_pcm(&pcm);
        openal_set_error("3D sample channel allocation failed");
        return qfalse;
    }

    const int silence = bytesPerSample == 1u ? 128 : 0;
    memset(channelBytes[OPENAL_FAST2D_LEFT], silence,
           stereoByteCount);
    memset(channelBytes[OPENAL_FAST2D_RIGHT], silence,
           stereoByteCount);
    const uint8_t *const monoBytes = pcm.ownedBytes;
    for (uint32_t frame = 0; frame < pcm.frameCount; ++frame) {
        const uint32_t monoOffset = frame * bytesPerSample;
        const uint32_t stereoOffset = frame * bytesPerSample * 2u;
        memcpy(channelBytes[OPENAL_FAST2D_LEFT] + stereoOffset,
               monoBytes + monoOffset, bytesPerSample);
        memcpy(channelBytes[OPENAL_FAST2D_RIGHT] + stereoOffset +
                   bytesPerSample,
               monoBytes + monoOffset, bytesPerSample);
    }

    ALuint buffers[OPENAL_FAST2D_CHANNEL_COUNT] = {0, 0};
    openal_clear_error();
    alGenBuffers(OPENAL_FAST2D_CHANNEL_COUNT, buffers);
    if (!openal_check("Fast 2D buffer allocation")) {
        for (int32_t channel = 0;
             channel < OPENAL_FAST2D_CHANNEL_COUNT; ++channel) {
            if (buffers[channel] != 0)
                alDeleteBuffers(1, &buffers[channel]);
        }
        free(channelBytes[OPENAL_FAST2D_LEFT]);
        free(channelBytes[OPENAL_FAST2D_RIGHT]);
        openal_discard_pcm(&pcm);
        return qfalse;
    }

    const ALenum format = bytesPerSample == 1u
                              ? AL_FORMAT_STEREO8
                              : AL_FORMAT_STEREO16;
    for (int32_t channel = 0;
         channel < OPENAL_FAST2D_CHANNEL_COUNT; ++channel) {
        openal_clear_error();
        alBufferData(buffers[channel], format, channelBytes[channel],
                     (ALsizei)stereoByteCount, pcm.sampleRate);
        if (!openal_check("Fast 2D sample upload")) {
            alDeleteBuffers(OPENAL_FAST2D_CHANNEL_COUNT, buffers);
            free(channelBytes[OPENAL_FAST2D_LEFT]);
            free(channelBytes[OPENAL_FAST2D_RIGHT]);
            openal_discard_pcm(&pcm);
            return qfalse;
        }
    }

    memcpy(entry->fast2dBuffers, buffers, sizeof(buffers));
    free(channelBytes[OPENAL_FAST2D_LEFT]);
    free(channelBytes[OPENAL_FAST2D_RIGHT]);
    openal_discard_pcm(&pcm);
    return qtrue;
}

/* NOT_FROM_ORIGINAL_SOURCE: bind the cache-owned one-sided stereo pair used
 * by one emulated Miles Fast 2D voice. */
static qboolean openal_bind_cached_fast2d_buffers(
    ALuint sources[OPENAL_FAST2D_CHANNEL_COUNT],
    ALuint buffers[OPENAL_FAST2D_CHANNEL_COUNT],
    audio_sample_type_t sampleType, const void *data, uint32_t dataLength,
    uint32_t blockSize, int32_t sampleRate, uint32_t *frameCount)
{
    openal_buffer_cache_entry_t *const entry =
        openal_get_cached_buffer(
            sampleType, data, dataLength, blockSize, sampleRate);
    if (entry == NULL || !openal_create_fast2d_buffers(entry)) {
        memset(buffers, 0,
               sizeof(ALuint) * OPENAL_FAST2D_CHANNEL_COUNT);
        *frameCount = 0;
        return qfalse;
    }

    alSourceStopv(OPENAL_FAST2D_CHANNEL_COUNT, sources);
    for (int32_t channel = 0;
         channel < OPENAL_FAST2D_CHANNEL_COUNT; ++channel) {
        alSourcei(sources[channel], AL_BUFFER, 0);
    }
    openal_clear_error();
    for (int32_t channel = 0;
         channel < OPENAL_FAST2D_CHANNEL_COUNT; ++channel) {
        alSourcei(sources[channel], AL_BUFFER,
                  (ALint)entry->fast2dBuffers[channel]);
    }
    if (!openal_check("Fast 2D buffer attachment")) {
        for (int32_t channel = 0;
             channel < OPENAL_FAST2D_CHANNEL_COUNT; ++channel) {
            alSourcei(sources[channel], AL_BUFFER, 0);
        }
        memset(buffers, 0,
               sizeof(ALuint) * OPENAL_FAST2D_CHANNEL_COUNT);
        *frameCount = 0;
        return qfalse;
    }

    memcpy(buffers, entry->fast2dBuffers,
           sizeof(entry->fast2dBuffers));
    *frameCount = entry->frameCount;
    return qtrue;
}

/* NOT_FROM_ORIGINAL_SOURCE: detach one cache entry from the reusable Miles
 * channel sources. Com_UnloadSoundAliasSounds stops all channels before the
 * loaded-sound ownership boundary reaches this adapter. */
static void openal_detach_cache_entry(
    const openal_buffer_cache_entry_t *entry,
    const snd_alias_sound_file_t *soundFile)
{
    for (struct audio_sample_handle_s *sample = openal_samples;
         sample != NULL; sample = sample->next) {
        const qboolean ownsPayload = sample->data == soundFile->data;
        const qboolean hasBuffer =
            entry->buffer != 0 && sample->buffer == entry->buffer;
        if (!ownsPayload && !hasBuffer)
            continue;
        if (hasBuffer) {
            alSourceStop(sample->source);
            alSourcei(sample->source, AL_BUFFER, 0);
            sample->buffer = 0;
        }
        sample->frameCount = 0;
        if (ownsPayload) {
            sample->data = NULL;
            sample->dataLength = 0;
        }
    }

    for (struct audio_3d_sample_handle_s *sample =
             openal_3d_samples;
         sample != NULL; sample = sample->next) {
        qboolean attached = sample->soundFile == soundFile;
        for (int32_t channel = 0;
             channel < OPENAL_FAST2D_CHANNEL_COUNT; ++channel) {
            if (sample->buffers[channel] != 0 &&
                sample->buffers[channel] ==
                    entry->fast2dBuffers[channel]) {
                attached = qtrue;
            }
        }
        if (!attached)
            continue;

        alSourceStopv(OPENAL_FAST2D_CHANNEL_COUNT,
                      sample->sources);
        for (int32_t channel = 0;
             channel < OPENAL_FAST2D_CHANNEL_COUNT; ++channel) {
            alSourcei(sample->sources[channel], AL_BUFFER, 0);
        }
        memset(sample->buffers, 0, sizeof(sample->buffers));
        sample->soundFile = NULL;
        sample->frameCount = 0;
    }
}

/* NOT_FROM_ORIGINAL_SOURCE: mirror the engine's loaded-sound ownership in
 * the native OpenAL cache. Without this eviction, every map retains its
 * buffers and a reused hunk address can resolve to stale audio. */
void openal_forget_loaded_sound(
    const snd_alias_sound_file_t *soundFile)
{
    if (soundFile == NULL || soundFile->data == NULL)
        return;

    openal_clear_error();
    for (uint32_t bucket = 0;
         bucket < OPENAL_BUFFER_CACHE_BUCKET_COUNT; ++bucket) {
        openal_buffer_cache_entry_t **link =
            &openal_buffer_cache[bucket];
        while (*link != NULL) {
            openal_buffer_cache_entry_t *const entry = *link;
            if (entry->data != soundFile->data) {
                link = &entry->next;
                continue;
            }

            openal_detach_cache_entry(entry, soundFile);
            if (entry->buffer != 0)
                alDeleteBuffers(1, &entry->buffer);
            if (entry->fast2dBuffers[OPENAL_FAST2D_LEFT] != 0 &&
                entry->fast2dBuffers[OPENAL_FAST2D_RIGHT] != 0) {
                alDeleteBuffers(OPENAL_FAST2D_CHANNEL_COUNT,
                                entry->fast2dBuffers);
            }
            *link = entry->next;
            free(entry);
        }
    }
    openal_clear_error();
}

/* NOT_FROM_ORIGINAL_SOURCE: release cache-owned alias buffers after every
 * borrowing source has been deleted and while the OpenAL context is current. */
static void openal_clear_buffer_cache(void)
{
    for (uint32_t bucket = 0;
         bucket < OPENAL_BUFFER_CACHE_BUCKET_COUNT; ++bucket) {
        openal_buffer_cache_entry_t *entry =
            openal_buffer_cache[bucket];
        while (entry != NULL) {
            openal_buffer_cache_entry_t *const next = entry->next;
            if (entry->buffer != 0)
                alDeleteBuffers(1, &entry->buffer);
            if (entry->fast2dBuffers[OPENAL_FAST2D_LEFT] != 0 &&
                entry->fast2dBuffers[OPENAL_FAST2D_RIGHT] != 0) {
                alDeleteBuffers(OPENAL_FAST2D_CHANNEL_COUNT,
                                entry->fast2dBuffers);
            }
            free(entry);
            entry = next;
        }
        openal_buffer_cache[bucket] = NULL;
    }
}

static void openal_apply_rate(
    ALuint source, int32_t baseRate, int32_t playbackRate)
{
    if (baseRate <= 0 || playbackRate <= 0)
        return;
    alSourcef(source, AL_PITCH, openal_clamp(
        (float)playbackRate / (float)baseRate, 0.01f, 4.0f));
}

static void openal_apply_pan(
    ALuint source, float pan)
{
    const float clampedPan = openal_clamp(pan, 0.0f, 1.0f);
    const float x = clampedPan * 2.0f - 1.0f;
    const float z = -sqrtf(fmaxf(0.0f, 1.0f - x * x));
    alSource3f(source, AL_POSITION, x, 0.0f, z);
}

/* NOT_FROM_ORIGINAL_SOURCE: reproduce the selected original provider's
 * channel-level panning in the OpenAL compatibility adapter. In
 * msssoft.m3d RVA 0x1094..0x11bd, volume is raised to 5/3, a source no
 * farther than 0.0001 from the listener is centered, and otherwise its left
 * fraction is acos(normalizedRight) / pi. The right fraction is the
 * complement, and a negative normalized-forward component reduces both
 * levels to 75 percent. Provider activation at RVA 0x1f4f..0x1f84 sets its
 * listener right/forward axes to +X/+Z. */
static void openal_apply_fast2d_gains(
    struct audio_3d_sample_handle_s *sample)
{
    static const float nearListenerDistance = 0.0001f;
    static const float inversePi = 0.3183098733425140380859375f;
    static const float centeredFraction = 0.5f;
    static const float rearVolumeScale = 0.75f;
    static const double volumeExponent = 1.6666666666666667;

    const float right = sample->position[0];
    const float up = sample->position[1];
    const float forward = sample->position[2];
    const float distance = (float)sqrt(
        ((double)forward * forward + (double)up * up) +
        (double)right * right);
    float leftFraction = centeredFraction;
    const float clampedVolume =
        openal_clamp(sample->volume, 0.0f, 1.0f);
    float levelVolume =
        (float)pow((double)clampedVolume, volumeExponent);

    if (distance > nearListenerDistance) {
        const double normalizationDistance = sqrt(
            ((double)right * right + (double)up * up) +
            (double)forward * forward);
        const double inverseNormalizationDistance =
            1.0 / normalizationDistance;
        const float normalizedRight =
            (float)((double)right * inverseNormalizationDistance);
        const float normalizedForward =
            (float)((double)forward * inverseNormalizationDistance);
        leftFraction = (float)(acos((double)normalizedRight) *
                               (double)inversePi);
        if (normalizedForward < 0.0f)
            levelVolume *= rearVolumeScale;
    }

    const float leftLevel = levelVolume * leftFraction;
    const float rightLevel = levelVolume * (1.0f - leftFraction);
    alSourcef(sample->sources[OPENAL_FAST2D_LEFT], AL_GAIN,
              leftLevel);
    alSourcef(sample->sources[OPENAL_FAST2D_RIGHT], AL_GAIN,
              rightLevel);
}

#if defined(__linux__)
/* NOT_FROM_ORIGINAL_SOURCE: configure an OpenAL EFX reverb preset. */
static qboolean openal_apply_efx_preset(int32_t roomType)
{
    static const EFXEAXREVERBPROPERTIES presets[] = {
        EFX_REVERB_PRESET_GENERIC,
        EFX_REVERB_PRESET_PADDEDCELL,
        EFX_REVERB_PRESET_ROOM,
        EFX_REVERB_PRESET_BATHROOM,
        EFX_REVERB_PRESET_LIVINGROOM,
        EFX_REVERB_PRESET_STONEROOM,
        EFX_REVERB_PRESET_AUDITORIUM,
        EFX_REVERB_PRESET_CONCERTHALL,
        EFX_REVERB_PRESET_CAVE,
        EFX_REVERB_PRESET_ARENA,
        EFX_REVERB_PRESET_HANGAR,
        EFX_REVERB_PRESET_CARPETEDHALLWAY,
        EFX_REVERB_PRESET_HALLWAY,
        EFX_REVERB_PRESET_STONECORRIDOR,
        EFX_REVERB_PRESET_ALLEY,
        EFX_REVERB_PRESET_FOREST,
        EFX_REVERB_PRESET_CITY,
        EFX_REVERB_PRESET_MOUNTAINS,
        EFX_REVERB_PRESET_QUARRY,
        EFX_REVERB_PRESET_PLAIN,
        EFX_REVERB_PRESET_PARKINGLOT,
        EFX_REVERB_PRESET_SEWERPIPE,
        EFX_REVERB_PRESET_UNDERWATER,
        EFX_REVERB_PRESET_DRUGGED,
        EFX_REVERB_PRESET_DIZZY,
        EFX_REVERB_PRESET_PSYCHOTIC
    };
    if (!openal_driver.efxAvailable)
        return qfalse;
    if (roomType < 0 || roomType >= (int32_t)(
            sizeof(presets) / sizeof(presets[0]))) {
        roomType = 0;
    }
    const EFXEAXREVERBPROPERTIES *const preset = &presets[roomType];
    const ALuint effect = openal_driver.reverbEffect;
    openal_clear_error();
    if (openal_driver.efxUsesEaxReverb) {
        openal_driver.effectf(
            effect, AL_EAXREVERB_DENSITY, preset->flDensity);
        openal_driver.effectf(
            effect, AL_EAXREVERB_DIFFUSION, preset->flDiffusion);
        openal_driver.effectf(
            effect, AL_EAXREVERB_GAIN, preset->flGain);
        openal_driver.effectf(
            effect, AL_EAXREVERB_GAINHF, preset->flGainHF);
        openal_driver.effectf(
            effect, AL_EAXREVERB_GAINLF, preset->flGainLF);
        openal_driver.effectf(
            effect, AL_EAXREVERB_DECAY_TIME, preset->flDecayTime);
        openal_driver.effectf(
            effect, AL_EAXREVERB_DECAY_HFRATIO, preset->flDecayHFRatio);
        openal_driver.effectf(
            effect, AL_EAXREVERB_DECAY_LFRATIO, preset->flDecayLFRatio);
        openal_driver.effectf(
            effect, AL_EAXREVERB_REFLECTIONS_GAIN,
            preset->flReflectionsGain);
        openal_driver.effectf(
            effect, AL_EAXREVERB_REFLECTIONS_DELAY,
            preset->flReflectionsDelay);
        openal_driver.effectfv(
            effect, AL_EAXREVERB_REFLECTIONS_PAN,
            preset->flReflectionsPan);
        openal_driver.effectf(
            effect, AL_EAXREVERB_LATE_REVERB_GAIN,
            preset->flLateReverbGain);
        openal_driver.effectf(
            effect, AL_EAXREVERB_LATE_REVERB_DELAY,
            preset->flLateReverbDelay);
        openal_driver.effectfv(
            effect, AL_EAXREVERB_LATE_REVERB_PAN,
            preset->flLateReverbPan);
        openal_driver.effectf(
            effect, AL_EAXREVERB_ECHO_TIME, preset->flEchoTime);
        openal_driver.effectf(
            effect, AL_EAXREVERB_ECHO_DEPTH, preset->flEchoDepth);
        openal_driver.effectf(
            effect, AL_EAXREVERB_MODULATION_TIME,
            preset->flModulationTime);
        openal_driver.effectf(
            effect, AL_EAXREVERB_MODULATION_DEPTH,
            preset->flModulationDepth);
        openal_driver.effectf(
            effect, AL_EAXREVERB_AIR_ABSORPTION_GAINHF,
            preset->flAirAbsorptionGainHF);
        openal_driver.effectf(
            effect, AL_EAXREVERB_HFREFERENCE, preset->flHFReference);
        openal_driver.effectf(
            effect, AL_EAXREVERB_LFREFERENCE, preset->flLFReference);
        openal_driver.effectf(
            effect, AL_EAXREVERB_ROOM_ROLLOFF_FACTOR,
            preset->flRoomRolloffFactor);
        openal_driver.effecti(
            effect, AL_EAXREVERB_DECAY_HFLIMIT,
            preset->iDecayHFLimit);
    } else {
        openal_driver.effectf(
            effect, AL_REVERB_DENSITY, preset->flDensity);
        openal_driver.effectf(
            effect, AL_REVERB_DIFFUSION, preset->flDiffusion);
        openal_driver.effectf(
            effect, AL_REVERB_GAIN, preset->flGain);
        openal_driver.effectf(
            effect, AL_REVERB_GAINHF, preset->flGainHF);
        openal_driver.effectf(
            effect, AL_REVERB_DECAY_TIME, preset->flDecayTime);
        openal_driver.effectf(
            effect, AL_REVERB_DECAY_HFRATIO, preset->flDecayHFRatio);
        openal_driver.effectf(
            effect, AL_REVERB_REFLECTIONS_GAIN,
            preset->flReflectionsGain);
        openal_driver.effectf(
            effect, AL_REVERB_REFLECTIONS_DELAY,
            preset->flReflectionsDelay);
        openal_driver.effectf(
            effect, AL_REVERB_LATE_REVERB_GAIN,
            preset->flLateReverbGain);
        openal_driver.effectf(
            effect, AL_REVERB_LATE_REVERB_DELAY,
            preset->flLateReverbDelay);
        openal_driver.effectf(
            effect, AL_REVERB_AIR_ABSORPTION_GAINHF,
            preset->flAirAbsorptionGainHF);
        openal_driver.effectf(
            effect, AL_REVERB_ROOM_ROLLOFF_FACTOR,
            preset->flRoomRolloffFactor);
        openal_driver.effecti(
            effect, AL_REVERB_DECAY_HFLIMIT,
            preset->iDecayHFLimit);
    }
    openal_driver.auxiliaryEffectSloti(
        openal_driver.reverbSlot, AL_EFFECTSLOT_EFFECT,
        (ALint)effect);
    return alGetError() == AL_NO_ERROR ? qtrue : qfalse;
}

/* NOT_FROM_ORIGINAL_SOURCE: release optional Linux EFX objects and procs. */
static void openal_shutdown_efx(void)
{
    openal_clear_error();
    if (openal_driver.reverbSlot != 0 &&
        openal_driver.deleteAuxiliaryEffectSlots != NULL) {
        openal_driver.deleteAuxiliaryEffectSlots(
            1, &openal_driver.reverbSlot);
    }
    if (openal_driver.reverbEffect != 0 &&
        openal_driver.deleteEffects != NULL) {
        openal_driver.deleteEffects(
            1, &openal_driver.reverbEffect);
    }
    openal_clear_error();
    openal_driver.genEffects = NULL;
    openal_driver.deleteEffects = NULL;
    openal_driver.effecti = NULL;
    openal_driver.effectf = NULL;
    openal_driver.effectfv = NULL;
    openal_driver.genFilters = NULL;
    openal_driver.deleteFilters = NULL;
    openal_driver.filteri = NULL;
    openal_driver.filterf = NULL;
    openal_driver.genAuxiliaryEffectSlots = NULL;
    openal_driver.deleteAuxiliaryEffectSlots = NULL;
    openal_driver.auxiliaryEffectSloti = NULL;
    openal_driver.reverbEffect = 0;
    openal_driver.reverbSlot = 0;
    openal_driver.efxAvailable = qfalse;
    openal_driver.efxUsesEaxReverb = qfalse;
}

/* NOT_FROM_ORIGINAL_SOURCE: dynamically enable EFX without requiring it. */
static void openal_initialize_efx(void)
{
    if (!alcIsExtensionPresent(
            openal_driver.device, "ALC_EXT_EFX")) {
        return;
    }
    openal_driver.genEffects =
        (LPALGENEFFECTS)alGetProcAddress("alGenEffects");
    openal_driver.deleteEffects =
        (LPALDELETEEFFECTS)alGetProcAddress("alDeleteEffects");
    openal_driver.effecti =
        (LPALEFFECTI)alGetProcAddress("alEffecti");
    openal_driver.effectf =
        (LPALEFFECTF)alGetProcAddress("alEffectf");
    openal_driver.effectfv =
        (LPALEFFECTFV)alGetProcAddress("alEffectfv");
    openal_driver.genFilters =
        (LPALGENFILTERS)alGetProcAddress("alGenFilters");
    openal_driver.deleteFilters =
        (LPALDELETEFILTERS)alGetProcAddress("alDeleteFilters");
    openal_driver.filteri =
        (LPALFILTERI)alGetProcAddress("alFilteri");
    openal_driver.filterf =
        (LPALFILTERF)alGetProcAddress("alFilterf");
    openal_driver.genAuxiliaryEffectSlots =
        (LPALGENAUXILIARYEFFECTSLOTS)alGetProcAddress(
            "alGenAuxiliaryEffectSlots");
    openal_driver.deleteAuxiliaryEffectSlots =
        (LPALDELETEAUXILIARYEFFECTSLOTS)alGetProcAddress(
            "alDeleteAuxiliaryEffectSlots");
    openal_driver.auxiliaryEffectSloti =
        (LPALAUXILIARYEFFECTSLOTI)alGetProcAddress(
            "alAuxiliaryEffectSloti");
    if (openal_driver.genEffects == NULL ||
        openal_driver.deleteEffects == NULL ||
        openal_driver.effecti == NULL ||
        openal_driver.effectf == NULL ||
        openal_driver.effectfv == NULL ||
        openal_driver.genFilters == NULL ||
        openal_driver.deleteFilters == NULL ||
        openal_driver.filteri == NULL ||
        openal_driver.filterf == NULL ||
        openal_driver.genAuxiliaryEffectSlots == NULL ||
        openal_driver.deleteAuxiliaryEffectSlots == NULL ||
        openal_driver.auxiliaryEffectSloti == NULL) {
        openal_shutdown_efx();
        return;
    }

    openal_clear_error();
    openal_driver.genEffects(
        1, &openal_driver.reverbEffect);
    openal_driver.effecti(
        openal_driver.reverbEffect,
        AL_EFFECT_TYPE, AL_EFFECT_EAXREVERB);
    if (alGetError() == AL_NO_ERROR) {
        openal_driver.efxUsesEaxReverb = qtrue;
    } else {
        openal_clear_error();
        openal_driver.effecti(
            openal_driver.reverbEffect,
            AL_EFFECT_TYPE, AL_EFFECT_REVERB);
        if (alGetError() != AL_NO_ERROR) {
            openal_shutdown_efx();
            return;
        }
    }
    openal_driver.genAuxiliaryEffectSlots(
        1, &openal_driver.reverbSlot);
    if (openal_driver.reverbSlot == 0 ||
        alGetError() != AL_NO_ERROR) {
        openal_shutdown_efx();
        return;
    }
    openal_driver.efxAvailable = qtrue;
    if (!openal_apply_efx_preset(0))
        openal_shutdown_efx();
}

/* NOT_FROM_ORIGINAL_SOURCE: release a per-source optional EFX send filter. */
static void openal_delete_reverb_filter(ALuint *filter)
{
    if (filter == NULL || *filter == 0 ||
        openal_driver.deleteFilters == NULL) {
        return;
    }
    openal_driver.deleteFilters(1, filter);
    *filter = 0;
    openal_clear_error();
}
#endif

static void openal_apply_reverb(
    ALuint source, ALuint *filter, float wetLevel)
{
#if defined(__APPLE__)
    (void)filter;
    if (openal_driver.setSourceProperty == NULL)
        return;
    ALfloat level = openal_clamp(wetLevel, 0.0f, 1.0f);
    (void)openal_driver.setSourceProperty(
        ALC_ASA_REVERB_SEND_LEVEL, source, &level, sizeof(level));
#else
    if (!openal_driver.efxAvailable || filter == NULL)
        return;
    const ALfloat level = openal_clamp(wetLevel, 0.0f, 1.0f);
    openal_clear_error();
    if (level <= 0.0f) {
        alSource3i(source, AL_AUXILIARY_SEND_FILTER,
                   AL_EFFECTSLOT_NULL, 0, AL_FILTER_NULL);
        openal_clear_error();
        return;
    }
    if (*filter == 0) {
        openal_driver.genFilters(1, filter);
        openal_driver.filteri(
            *filter, AL_FILTER_TYPE, AL_FILTER_LOWPASS);
        openal_driver.filterf(
            *filter, AL_LOWPASS_GAINHF, 1.0f);
    }
    openal_driver.filterf(
        *filter, AL_LOWPASS_GAIN, level);
    alSource3i(source, AL_AUXILIARY_SEND_FILTER,
               (ALint)openal_driver.reverbSlot, 0,
               (ALint)*filter);
    if (alGetError() != AL_NO_ERROR) {
        alSource3i(source, AL_AUXILIARY_SEND_FILTER,
                   AL_EFFECTSLOT_NULL, 0, AL_FILTER_NULL);
        openal_delete_reverb_filter(filter);
    }
#endif
}

static void openal_apply_room_type(int32_t roomType)
{
#if defined(__APPLE__)
    static const ALint roomTypes[26] = {
        ALC_ASA_REVERB_ROOM_TYPE_MediumRoom,
        ALC_ASA_REVERB_ROOM_TYPE_SmallRoom,
        ALC_ASA_REVERB_ROOM_TYPE_MediumRoom,
        ALC_ASA_REVERB_ROOM_TYPE_MediumChamber,
        ALC_ASA_REVERB_ROOM_TYPE_MediumRoom,
        ALC_ASA_REVERB_ROOM_TYPE_LargeRoom,
        ALC_ASA_REVERB_ROOM_TYPE_MediumHall,
        ALC_ASA_REVERB_ROOM_TYPE_LargeHall,
        ALC_ASA_REVERB_ROOM_TYPE_Cathedral,
        ALC_ASA_REVERB_ROOM_TYPE_LargeHall2,
        ALC_ASA_REVERB_ROOM_TYPE_LargeRoom2,
        ALC_ASA_REVERB_ROOM_TYPE_MediumChamber,
        ALC_ASA_REVERB_ROOM_TYPE_MediumChamber,
        ALC_ASA_REVERB_ROOM_TYPE_LargeChamber,
        ALC_ASA_REVERB_ROOM_TYPE_MediumHall,
        ALC_ASA_REVERB_ROOM_TYPE_LargeRoom,
        ALC_ASA_REVERB_ROOM_TYPE_MediumHall,
        ALC_ASA_REVERB_ROOM_TYPE_LargeHall,
        ALC_ASA_REVERB_ROOM_TYPE_LargeRoom,
        ALC_ASA_REVERB_ROOM_TYPE_LargeRoom,
        ALC_ASA_REVERB_ROOM_TYPE_LargeRoom2,
        ALC_ASA_REVERB_ROOM_TYPE_LargeChamber,
        ALC_ASA_REVERB_ROOM_TYPE_LargeChamber,
        ALC_ASA_REVERB_ROOM_TYPE_Plate,
        ALC_ASA_REVERB_ROOM_TYPE_Plate,
        ALC_ASA_REVERB_ROOM_TYPE_Plate
    };
    if (openal_driver.setListenerProperty == NULL)
        return;
    if (roomType < 0 || roomType >= (int32_t)(
            sizeof(roomTypes) / sizeof(roomTypes[0]))) {
        roomType = 0;
    }
    ALint selectedRoom = roomTypes[roomType];
    (void)openal_driver.setListenerProperty(
        ALC_ASA_REVERB_ROOM_TYPE, &selectedRoom, sizeof(selectedRoom));
#else
    (void)openal_apply_efx_preset(roomType);
#endif
}

#if defined(__APPLE__)
static OSStatus openal_audio_read(
    void *clientData, SInt64 position, UInt32 requestCount,
    void *buffer, UInt32 *actualCount)
{
    const openal_memory_file_t *const file = clientData;
    if (position < 0 || position >= file->byteCount) {
        *actualCount = 0;
        return noErr;
    }
    int64_t available = file->byteCount - position;
    if (available > requestCount)
        available = requestCount;
    memcpy(buffer, file->borrowedBytes + position, (size_t)available);
    *actualCount = (UInt32)available;
    return noErr;
}

static SInt64 openal_audio_size(void *clientData)
{
    const openal_memory_file_t *const file = clientData;
    return file->byteCount;
}
#else
/* NOT_FROM_ORIGINAL_SOURCE: libsndfile virtual-file length callback. */
static sf_count_t openal_sndfile_length(void *clientData)
{
    const openal_memory_file_t *const file = clientData;
    return (sf_count_t)file->byteCount;
}

/* NOT_FROM_ORIGINAL_SOURCE: libsndfile virtual-file seek callback. */
static sf_count_t openal_sndfile_seek(
    sf_count_t offset, int whence, void *clientData)
{
    openal_memory_file_t *const file = clientData;
    int64_t base;
    if (whence == SEEK_SET)
        base = 0;
    else if (whence == SEEK_CUR)
        base = file->position;
    else if (whence == SEEK_END)
        base = file->byteCount;
    else
        return -1;
    if (offset < -base || offset > file->byteCount - base)
        return -1;
    file->position = base + offset;
    return (sf_count_t)file->position;
}

/* NOT_FROM_ORIGINAL_SOURCE: libsndfile virtual-file read callback. */
static sf_count_t openal_sndfile_read(
    void *buffer, sf_count_t requestCount, void *clientData)
{
    openal_memory_file_t *const file = clientData;
    if (requestCount <= 0 || file->position >= file->byteCount)
        return 0;
    int64_t available = file->byteCount - file->position;
    if (available > requestCount)
        available = requestCount;
    memcpy(buffer, file->borrowedBytes + file->position,
           (size_t)available);
    file->position += available;
    return (sf_count_t)available;
}

/* NOT_FROM_ORIGINAL_SOURCE: reject writes to read-only stream storage. */
static sf_count_t openal_sndfile_write(
    const void *buffer, sf_count_t requestCount, void *clientData)
{
    (void)buffer;
    (void)requestCount;
    (void)clientData;
    return 0;
}

/* NOT_FROM_ORIGINAL_SOURCE: libsndfile virtual-file position callback. */
static sf_count_t openal_sndfile_tell(void *clientData)
{
    const openal_memory_file_t *const file = clientData;
    return (sf_count_t)file->position;
}
#endif

static int32_t openal_stream_buffer_index(
    const struct audio_stream_handle_s *stream, ALuint buffer)
{
    for (int32_t index = 0;
         index < OPENAL_STREAM_BUFFER_COUNT; ++index) {
        if (stream->buffers[index] == buffer)
            return index;
    }
    return -1;
}

static qboolean openal_stream_seek_decoder(
    struct audio_stream_handle_s *stream, uint32_t frame)
{
#if defined(__APPLE__)
    if (ExtAudioFileSeek(stream->extendedFile, frame) != noErr) {
        openal_set_error("AudioToolbox stream seek failed");
        return qfalse;
    }
#else
    if (sf_seek(stream->soundFile, (sf_count_t)frame, SEEK_SET) !=
        (sf_count_t)frame) {
        openal_set_error("libsndfile stream seek failed");
        return qfalse;
    }
#endif
    stream->decoderAtEnd = qfalse;
    return qtrue;
}

static qboolean openal_stream_fill_buffer(
    struct audio_stream_handle_s *stream, int32_t bufferIndex)
{
#if defined(__APPLE__)
    UInt32 decodedFrames = OPENAL_STREAM_BUFFER_FRAMES;
    AudioBufferList buffers;
    buffers.mNumberBuffers = 1;
    buffers.mBuffers[0].mNumberChannels =
        stream->clientFormat.mChannelsPerFrame;
    buffers.mBuffers[0].mDataByteSize =
        decodedFrames * stream->clientFormat.mBytesPerFrame;
    buffers.mBuffers[0].mData = stream->decodePcmBuffer;
    const OSStatus result = ExtAudioFileRead(
        stream->extendedFile, &decodedFrames, &buffers);
    if (result != noErr) {
        openal_set_error("AudioToolbox stream decode failed");
        stream->decoderAtEnd = qtrue;
        return qfalse;
    }
    if (decodedFrames == 0) {
        stream->decoderAtEnd = qtrue;
        return qfalse;
    }
    if (decodedFrames < OPENAL_STREAM_BUFFER_FRAMES)
        stream->decoderAtEnd = qtrue;

    const ALenum format =
        stream->clientFormat.mChannelsPerFrame == 1
            ? AL_FORMAT_MONO16
            : AL_FORMAT_STEREO16;
    const uint32_t byteCount =
        decodedFrames * stream->clientFormat.mBytesPerFrame;
#else
    const sf_count_t decodedFrames = sf_readf_short(
        stream->soundFile, stream->decodePcmBuffer,
        OPENAL_STREAM_BUFFER_FRAMES);
    if (decodedFrames < 0) {
        openal_set_error("libsndfile stream decode failed");
        stream->decoderAtEnd = qtrue;
        return qfalse;
    }
    if (decodedFrames == 0) {
        stream->decoderAtEnd = qtrue;
        return qfalse;
    }
    if (decodedFrames < OPENAL_STREAM_BUFFER_FRAMES)
        stream->decoderAtEnd = qtrue;

    const ALenum format =
        stream->soundInfo.channels == 1
            ? AL_FORMAT_MONO16
            : AL_FORMAT_STEREO16;
    const uint32_t byteCount =
        (uint32_t)decodedFrames *
        (uint32_t)stream->soundInfo.channels * 2u;
#endif
    openal_clear_error();
    alBufferData(stream->buffers[bufferIndex], format,
                 stream->decodePcmBuffer, (ALsizei)byteCount,
                 stream->baseRate);
    if (!openal_check("stream buffer upload"))
        return qfalse;
    stream->bufferFrames[bufferIndex] = decodedFrames;
    return qtrue;
}

static void openal_stream_unqueue_all(
    struct audio_stream_handle_s *stream)
{
    alSourceStop(stream->source);
    ALint queued = 0;
    alGetSourcei(stream->source, AL_BUFFERS_QUEUED, &queued);
    while (queued-- > 0) {
        ALuint buffer = 0;
        alSourceUnqueueBuffers(stream->source, 1, &buffer);
        const int32_t index =
            openal_stream_buffer_index(stream, buffer);
        if (index >= 0)
            stream->bufferFrames[index] = 0;
    }
}

static qboolean openal_stream_queue_initial(
    struct audio_stream_handle_s *stream)
{
    qboolean queuedAny = qfalse;
    for (int32_t index = 0;
         index < OPENAL_STREAM_BUFFER_COUNT; ++index) {
        if (!openal_stream_fill_buffer(stream, index))
            break;
        alSourceQueueBuffers(stream->source, 1, &stream->buffers[index]);
        queuedAny = qtrue;
    }
    return queuedAny;
}

static qboolean openal_stream_rewind_if_empty(
    struct audio_stream_handle_s *stream)
{
    ALint queued = 0;
    alGetSourcei(stream->source, AL_BUFFERS_QUEUED, &queued);
    if (queued > 0)
        return qtrue;
    if (!openal_stream_seek_decoder(stream, 0))
        return qfalse;
    stream->playedFrames = 0;
    return openal_stream_queue_initial(stream);
}

static void openal_service_stream(
    struct audio_stream_handle_s *stream)
{
    ALint processed = 0;
    alGetSourcei(stream->source, AL_BUFFERS_PROCESSED, &processed);
    while (processed-- > 0) {
        ALuint buffer = 0;
        alSourceUnqueueBuffers(stream->source, 1, &buffer);
        const int32_t index =
            openal_stream_buffer_index(stream, buffer);
        if (index < 0)
            continue;
        stream->playedFrames += stream->bufferFrames[index];
        stream->bufferFrames[index] = 0;

        if (stream->decoderAtEnd && stream->loopCount == 0 &&
            !openal_stream_seek_decoder(stream, 0)) {
            continue;
        }
        if (stream->decoderAtEnd)
            continue;
        if (openal_stream_fill_buffer(stream, index))
            alSourceQueueBuffers(stream->source, 1, &buffer);
    }

    ALint queued = 0;
    ALint state = AL_STOPPED;
    alGetSourcei(stream->source, AL_BUFFERS_QUEUED, &queued);
    alGetSourcei(stream->source, AL_SOURCE_STATE, &state);
    if (stream->wantsPlayback && queued > 0 &&
        state != AL_PLAYING && state != AL_PAUSED) {
        alSourcePlay(stream->source);
    }
}

static qboolean openal_open_stream_decoder(
    struct audio_stream_handle_s *stream)
{
    stream->memoryFile.borrowedBytes = stream->ownedFileBytes;
#if defined(__APPLE__)
    stream->memoryFile.byteCount = (int64_t)stream->fileByteCount;
    OSStatus result = AudioFileOpenWithCallbacks(
        &stream->memoryFile, openal_audio_read, NULL,
        openal_audio_size, NULL, 0, &stream->audioFile);
    if (result != noErr)
        return qfalse;

    AudioStreamBasicDescription sourceFormat;
    UInt32 propertySize = sizeof(sourceFormat);
    result = AudioFileGetProperty(
        stream->audioFile, kAudioFilePropertyDataFormat,
        &propertySize, &sourceFormat);
    if (result != noErr || sourceFormat.mChannelsPerFrame == 0 ||
        !isfinite(sourceFormat.mSampleRate) ||
        sourceFormat.mSampleRate < 1.0 ||
        sourceFormat.mSampleRate > (double)INT32_MAX) {
        return qfalse;
    }

    result = ExtAudioFileWrapAudioFileID(
        stream->audioFile, false, &stream->extendedFile);
    if (result != noErr)
        return qfalse;

    const UInt32 channelCount =
        sourceFormat.mChannelsPerFrame > 1 ? 2u : 1u;
    memset(&stream->clientFormat, 0, sizeof(stream->clientFormat));
    stream->clientFormat.mSampleRate = sourceFormat.mSampleRate;
    stream->clientFormat.mFormatID = kAudioFormatLinearPCM;
    stream->clientFormat.mFormatFlags =
        kLinearPCMFormatFlagIsSignedInteger |
        kAudioFormatFlagsNativeEndian |
        kAudioFormatFlagIsPacked;
    stream->clientFormat.mBytesPerPacket = channelCount * 2u;
    stream->clientFormat.mFramesPerPacket = 1;
    stream->clientFormat.mBytesPerFrame = channelCount * 2u;
    stream->clientFormat.mChannelsPerFrame = channelCount;
    stream->clientFormat.mBitsPerChannel = 16;
    result = ExtAudioFileSetProperty(
        stream->extendedFile, kExtAudioFileProperty_ClientDataFormat,
        sizeof(stream->clientFormat), &stream->clientFormat);
    if (result != noErr)
        return qfalse;

    SInt64 fileFrames = 0;
    propertySize = sizeof(fileFrames);
    result = ExtAudioFileGetProperty(
        stream->extendedFile, kExtAudioFileProperty_FileLengthFrames,
        &propertySize, &fileFrames);
    if (result != noErr || fileFrames <= 0 ||
        (uint64_t)fileFrames > UINT32_MAX)
        return qfalse;

    stream->frameCount = (uint32_t)fileFrames;
    stream->baseRate = (int32_t)stream->clientFormat.mSampleRate;
    stream->playbackRate = stream->baseRate;
    stream->decodePcmBuffer = malloc(
        OPENAL_STREAM_BUFFER_FRAMES *
        stream->clientFormat.mBytesPerFrame);
    if (stream->decodePcmBuffer == NULL)
        return qfalse;
    return qtrue;
#else
    static SF_VIRTUAL_IO virtualIo = {
        openal_sndfile_length,
        openal_sndfile_seek,
        openal_sndfile_read,
        openal_sndfile_write,
        openal_sndfile_tell
    };
#if SIZE_MAX > INT64_MAX
    if (stream->fileByteCount > INT64_MAX)
        return qfalse;
#endif
    stream->memoryFile.byteCount = (int64_t)stream->fileByteCount;
    stream->memoryFile.position = 0;
    memset(&stream->soundInfo, 0, sizeof(stream->soundInfo));
    stream->soundFile = sf_open_virtual(
        &virtualIo, SFM_READ, &stream->soundInfo,
        &stream->memoryFile);
    if (stream->soundFile == NULL ||
        (stream->soundInfo.channels != 1 &&
         stream->soundInfo.channels != 2) ||
        stream->soundInfo.samplerate <= 0 ||
        stream->soundInfo.frames <= 0 ||
        (uint64_t)stream->soundInfo.frames > UINT32_MAX) {
        return qfalse;
    }
    stream->frameCount = (uint32_t)stream->soundInfo.frames;
    stream->baseRate = stream->soundInfo.samplerate;
    stream->playbackRate = stream->baseRate;
    stream->decodePcmBuffer = malloc(
        OPENAL_STREAM_BUFFER_FRAMES *
        (size_t)stream->soundInfo.channels * sizeof(int16_t));
    if (stream->decodePcmBuffer == NULL)
        return qfalse;
    return qtrue;
#endif
}

static qboolean openal_read_file(
    const char *filename, void **outBytes, size_t *outSize)
{
    *outBytes = NULL;
    *outSize = 0;
    if (openal_file_open != NULL &&
        openal_file_close != NULL &&
        openal_file_read != NULL) {
        int32_t handle;
        const int32_t fileSize =
            openal_file_open(filename, &handle);
        if (fileSize <= 0)
            return qfalse;
        void *const bytes = malloc((size_t)fileSize);
        if (bytes == NULL) {
            openal_file_close(handle);
            return qfalse;
        }
        const int32_t bytesRead =
            openal_file_read(handle, bytes, fileSize);
        openal_file_close(handle);
        if (bytesRead != fileSize) {
            free(bytes);
            return qfalse;
        }
        *outBytes = bytes;
        *outSize = (size_t)fileSize;
        return qtrue;
    }

    FILE *const file = fopen(filename, "rb");
    if (file == NULL)
        return qfalse;
    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return qfalse;
    }
    const long fileSize = ftell(file);
    if (fileSize <= 0 || fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return qfalse;
    }
    void *const bytes = malloc((size_t)fileSize);
    if (bytes == NULL) {
        fclose(file);
        return qfalse;
    }
    const size_t bytesRead = fread(bytes, 1, (size_t)fileSize, file);
    fclose(file);
    if (bytesRead != (size_t)fileSize) {
        free(bytes);
        return qfalse;
    }
    *outBytes = bytes;
    *outSize = (size_t)fileSize;
    return qtrue;
}

static void openal_delete_sample(
    struct audio_sample_handle_s *sample)
{
    if (sample->source != 0) {
        alSourceStop(sample->source);
        alDeleteSources(1, &sample->source);
    }
    for (int32_t index = 0;
         index < OPENAL_RAW_BUFFER_COUNT; ++index) {
        if (sample->rawBuffers[index] != 0)
            alDeleteBuffers(1, &sample->rawBuffers[index]);
    }
#if defined(__linux__)
    openal_delete_reverb_filter(&sample->reverbFilter);
#endif
    free(sample);
}

static void openal_delete_3d_sample(
    struct audio_3d_sample_handle_s *sample)
{
    if (sample->sources[OPENAL_FAST2D_LEFT] != 0 &&
        sample->sources[OPENAL_FAST2D_RIGHT] != 0) {
        alSourceStopv(OPENAL_FAST2D_CHANNEL_COUNT,
                      sample->sources);
        alDeleteSources(OPENAL_FAST2D_CHANNEL_COUNT,
                        sample->sources);
    }
#if defined(__linux__)
    for (int32_t channel = 0;
         channel < OPENAL_FAST2D_CHANNEL_COUNT; ++channel) {
        openal_delete_reverb_filter(
            &sample->reverbFilters[channel]);
    }
#endif
    free(sample);
}

static void openal_delete_stream(
    struct audio_stream_handle_s *stream)
{
    if (stream->source != 0) {
        alSourceStop(stream->source);
        alDeleteSources(1, &stream->source);
    }
    for (int32_t index = 0;
         index < OPENAL_STREAM_BUFFER_COUNT; ++index) {
        if (stream->buffers[index] != 0)
            alDeleteBuffers(1, &stream->buffers[index]);
    }
#if defined(__APPLE__)
    if (stream->extendedFile != NULL)
        ExtAudioFileDispose(stream->extendedFile);
    if (stream->audioFile != NULL)
        AudioFileClose(stream->audioFile);
#else
    if (stream->soundFile != NULL)
        (void)sf_close(stream->soundFile);
    openal_delete_reverb_filter(&stream->reverbFilter);
#endif
    free(stream->decodePcmBuffer);
    free(stream->ownedFileBytes);
    free(stream);
}

int32_t openal_set_preference(int32_t preference, int32_t value)
{
    (void)preference;
    (void)value;
    return 0;
}

void openal_set_file_callbacks(
    audio_file_open_callback_t openCallback,
    audio_file_close_callback_t closeCallback,
    audio_file_seek_callback_t seekCallback,
    audio_file_read_callback_t readCallback)
{
    openal_file_open = openCallback;
    openal_file_close = closeCallback;
    openal_file_seek = seekCallback;
    openal_file_read = readCallback;
}

void openal_set_redist_directory(const char *directory)
{
    (void)directory;
}

int32_t openal_startup(void)
{
    openal_set_error("no OpenAL error");
    return 1;
}

void openal_shutdown(void)
{
    while (openal_streams != NULL) {
        struct audio_stream_handle_s *const next =
            openal_streams->next;
        openal_delete_stream(openal_streams);
        openal_streams = next;
    }
    while (openal_3d_samples != NULL) {
        struct audio_3d_sample_handle_s *const next =
            openal_3d_samples->next;
        openal_delete_3d_sample(openal_3d_samples);
        openal_3d_samples = next;
    }
    while (openal_samples != NULL) {
        struct audio_sample_handle_s *const next =
            openal_samples->next;
        openal_delete_sample(openal_samples);
        openal_samples = next;
    }
    openal_clear_buffer_cache();
    if (openal_driver.context != NULL) {
#if defined(__linux__)
        openal_shutdown_efx();
#endif
        alcMakeContextCurrent(NULL);
        alcDestroyContext(openal_driver.context);
        openal_driver.context = NULL;
    }
    if (openal_driver.device != NULL) {
        alcCloseDevice(openal_driver.device);
        openal_driver.device = NULL;
    }
#if defined(__APPLE__)
    openal_driver.setSourceProperty = NULL;
    openal_driver.setListenerProperty = NULL;
#endif
}

void openal_close_3D_provider(
    audio_provider_t provider)
{
    (void)provider;
    /* The complete OpenAL backend remains alive until openal_shutdown. */
}

void openal_set_DirectSound_HWND(
    audio_driver_t driver, audio_window_handle_t windowHandle)
{
    (void)driver;
    (void)windowHandle;
}

/* NOT_FROM_ORIGINAL_SOURCE: native mirror of the bundled Miles first-match,
 * case-insensitive top-level chunk search. MSS_LoadSoundFile has already
 * bounded and, where needed, repaired the scan extent. */
static qboolean openal_find_wav_chunk(
    const uint8_t *bytes, uint32_t scanEnd, const char *chunkId,
    const uint8_t **outData, uint32_t *outSize)
{
    uint32_t offset = 12;
    for (;;) {
        const uint8_t *const chunk = bytes + offset;
        const uint32_t chunkSize = openal_read_u32(chunk + 4);
        if (Q_stricmpn((const char *)chunk, chunkId, 4) == 0) {
            *outData = chunk + 8;
            if (outSize != NULL)
                *outSize = chunkSize;
            return qtrue;
        }

        const uint64_t nextOffset =
            (uint64_t)offset + 8u + chunkSize + (chunkSize & 1u);
        if (nextOffset >= scanEnd || nextOffset > UINT32_MAX)
            return qfalse;
        offset = (uint32_t)nextOffset;
    }
}

int32_t openal_WAV_info(
    const void *fileData, audio_sound_info_t *soundInfo)
{
    if (fileData == NULL || soundInfo == NULL)
        return 0;
    snd_alias_sound_file_t *const publicInfo = &soundInfo->publicInfo;
    const uint8_t *const bytes = fileData;
    if (Q_stricmpn((const char *)bytes + 8, "WAVE", 4) != 0)
        return 0;

    const uint32_t scanEnd = openal_read_u32(bytes + 4);
    const uint8_t *formatChunk = NULL;
    const uint8_t *sampleData = NULL;
    uint32_t sampleDataSize = 0;
    if (openal_find_wav_chunk(
            bytes, scanEnd, "fmt ", &formatChunk, NULL) == qfalse ||
        openal_find_wav_chunk(
            bytes, scanEnd, "data", &sampleData,
            &sampleDataSize) == qfalse) {
        return 0;
    }

    const uint16_t formatTag = openal_read_u16(formatChunk);
    const uint16_t channelCount =
        openal_read_u16(formatChunk + 2);
    const uint32_t sampleRate =
        openal_read_u32(formatChunk + 4);
    const uint16_t blockSize =
        openal_read_u16(formatChunk + 12);
    const uint16_t bitsPerSample =
        openal_read_u16(formatChunk + 14);

    uint32_t sampleCount;
    if (formatTag == OPENAL_WAVE_FORMAT_IMA_ADPCM &&
        bitsPerSample == 4) {
        const uint8_t *factData = NULL;
        if (openal_find_wav_chunk(
                bytes, scanEnd, "fact", &factData, NULL) == qtrue) {
            sampleCount = openal_read_u32(factData);
        } else {
            const uint32_t headerSize =
                UINT32_C(4) << ((channelCount / 2u) & 31u);
            if (headerSize == 0 || blockSize == 0)
                return 0;
            const uint32_t samplesPerBlock =
                1u + (((uint32_t)blockSize - headerSize) * 8u) /
                         headerSize;
            const uint32_t blockCount =
                (sampleDataSize + blockSize - 1u) / blockSize;
            sampleCount = samplesPerBlock * blockCount;
        }
    } else if (bitsPerSample != 0) {
        sampleCount = sampleDataSize * 8u / bitsPerSample;
    } else {
        sampleCount = 0;
    }

    memset(soundInfo, 0, sizeof(*soundInfo));
    publicInfo->formatTag = formatTag;
    publicInfo->data = (void *)sampleData;
    publicInfo->dataLength = sampleDataSize;
    publicInfo->sampleRate = sampleRate;
    publicInfo->bitsPerSample = bitsPerSample;
    publicInfo->channelCount = channelCount;
    publicInfo->sampleCount = sampleCount;
    publicInfo->blockSize = blockSize;
    publicInfo->initialData = (void *)sampleData;
    return 1;
}

uint32_t openal_size_processed_digital_audio(
    uint32_t sampleRate, audio_sample_type_t sampleType,
    int32_t bufferCount, const audio_sound_info_t *sourceInfo)
{
    (void)bufferCount;
    if (sourceInfo == NULL || sourceInfo->publicInfo.sampleRate == 0)
        return 0;
    const snd_alias_sound_file_t *const publicInfo =
        &sourceInfo->publicInfo;
    if (sampleRate == 0 || publicInfo->sampleCount == 0 ||
        openal_channels(sampleType) == 0)
        return 0;
    if (openal_is_adpcm(sampleType))
        return publicInfo->dataLength;
    const uint64_t sourceFrames =
        publicInfo->sampleCount / (uint32_t)publicInfo->channelCount;
    const uint64_t outputFrames =
        sourceFrames * sampleRate /
        publicInfo->sampleRate;
    const uint64_t byteCount =
        outputFrames * (uint32_t)openal_channels(sampleType) *
        (uint32_t)openal_bytes_per_sample(sampleType);
    return byteCount <= UINT32_MAX ? (uint32_t)byteCount : 0;
}

int32_t openal_process_digital_audio(
    void *destination, uint32_t destinationSize, uint32_t sampleRate,
    audio_sample_type_t sampleType, int32_t bufferCount,
    audio_sound_info_t *sourceInfo)
{
    (void)bufferCount;
    if (destination == NULL || sourceInfo == NULL)
        return 0;
    const snd_alias_sound_file_t *const publicInfo =
        &sourceInfo->publicInfo;
    if (publicInfo->data == NULL || sampleRate == 0 ||
        publicInfo->sampleCount == 0 ||
        (publicInfo->channelCount != 1 &&
         publicInfo->channelCount != 2))
        return 0;
    if (openal_is_adpcm(sampleType)) {
        if (destinationSize < publicInfo->dataLength)
            return 0;
        memcpy(destination, publicInfo->data, publicInfo->dataLength);
        return 1;
    }
    if (publicInfo->formatTag != OPENAL_WAVE_FORMAT_PCM ||
        (publicInfo->bitsPerSample != 8 &&
         publicInfo->bitsPerSample != 16) ||
        publicInfo->sampleRate == 0)
        return 0;
    const int32_t outputChannels = openal_channels(sampleType);
    if (outputChannels == 0)
        return 0;
    const uint32_t sourceBytesPerSample =
        publicInfo->bitsPerSample / 8u;
    const uint64_t sourceByteCount =
        (uint64_t)publicInfo->sampleCount * sourceBytesPerSample;
    if (sourceByteCount > publicInfo->dataLength)
        return 0;

    const uint32_t sourceFrames =
        publicInfo->sampleCount / (uint32_t)publicInfo->channelCount;
    const uint32_t outputFrames =
        (uint32_t)((uint64_t)sourceFrames * sampleRate /
                   publicInfo->sampleRate);
    const int32_t outputBytes =
        openal_bytes_per_sample(sampleType);
    const uint64_t required =
        (uint64_t)outputFrames * outputChannels * outputBytes;
    if (required > destinationSize)
        return 0;

    const uint8_t *const input = publicInfo->data;
    uint8_t *const output = destination;
    for (uint32_t frame = 0; frame < outputFrames; ++frame) {
        uint32_t sourceFrame =
            (uint32_t)((uint64_t)frame * publicInfo->sampleRate /
                       sampleRate);
        if (sourceFrame >= publicInfo->sampleCount)
            sourceFrame = publicInfo->sampleCount - 1u;
        int32_t left;
        int32_t right;
        if (publicInfo->bitsPerSample == 16) {
            const size_t sourceIndex =
                (size_t)sourceFrame * publicInfo->channelCount;
            int16_t leftSample;
            memcpy(&leftSample, input + sourceIndex * sizeof(leftSample),
                   sizeof(leftSample));
            left = leftSample;
            if (publicInfo->channelCount == 2) {
                int16_t rightSample;
                memcpy(&rightSample,
                       input + (sourceIndex + 1u) * sizeof(rightSample),
                       sizeof(rightSample));
                right = rightSample;
            } else {
                right = left;
            }
        } else {
            left = ((int32_t)input[(size_t)sourceFrame *
                                   publicInfo->channelCount] -
                    128) << 8;
            right = publicInfo->channelCount == 2
                        ? ((int32_t)input[(size_t)sourceFrame * 2u + 1u] -
                           128) << 8
                        : left;
        }
        if (outputChannels == 1)
            left = (left + right) / 2;
        for (int32_t channel = 0; channel < outputChannels; ++channel) {
            const int32_t value = channel == 0 ? left : right;
            const size_t outputIndex =
                (size_t)frame * outputChannels + channel;
            if (outputBytes == 2) {
                const int16_t outputSample = (int16_t)value;
                memcpy(output + outputIndex * sizeof(outputSample),
                       &outputSample, sizeof(outputSample));
            } else {
                output[outputIndex] = (uint8_t)((value >> 8) + 128);
            }
        }
    }
    return 1;
}

int32_t openal_digital_CPU_percent(
    audio_driver_t driver)
{
    (void)driver;
    return 0;
}

audio_driver_t openal_open_digital_driver(
    int32_t sampleRate, int32_t sampleFormat, int32_t channels,
    int32_t flags)
{
    (void)sampleFormat;
    (void)channels;
    (void)flags;
    if (openal_driver.context != NULL)
        return &openal_driver;
    openal_driver.device = alcOpenDevice(NULL);
    if (openal_driver.device == NULL) {
        openal_set_error("could not open the default OpenAL device");
        return NULL;
    }
#if defined(__linux__)
    if (alcIsExtensionPresent(
            openal_driver.device, "ALC_EXT_EFX")) {
        const ALCint attributes[] = {
            ALC_MAX_AUXILIARY_SENDS, 1,
            0
        };
        openal_driver.context = alcCreateContext(
            openal_driver.device, attributes);
    }
    if (openal_driver.context == NULL) {
        openal_driver.context = alcCreateContext(
            openal_driver.device, NULL);
    }
#else
    openal_driver.context = alcCreateContext(
        openal_driver.device, NULL);
#endif
    if (openal_driver.context == NULL ||
        !alcMakeContextCurrent(openal_driver.context)) {
        openal_set_error("could not create the OpenAL context");
        openal_shutdown();
        return NULL;
    }
    alDistanceModel(AL_NONE);
    alListener3f(AL_POSITION, 0.0f, 0.0f, 0.0f);
    openal_driver.sampleRate = sampleRate;
#if defined(__APPLE__)
    if (alcIsExtensionPresent(
            openal_driver.device, "ALC_EXT_ASA")) {
        openal_driver.setSourceProperty =
            (alcASASetSourceProcPtr)alcGetProcAddress(
                openal_driver.device, "alcASASetSource");
        openal_driver.setListenerProperty =
            (alcASASetListenerProcPtr)alcGetProcAddress(
                openal_driver.device, "alcASASetListener");
        if (openal_driver.setListenerProperty != NULL) {
            ALuint enabled = AL_TRUE;
            (void)openal_driver.setListenerProperty(
                ALC_ASA_REVERB_ON, &enabled, sizeof(enabled));
        }
    }
#else
    openal_initialize_efx();
#endif
    return &openal_driver;
}

const char *openal_last_error(void)
{
    return openal_error;
}

int32_t openal_enumerate_3D_providers(
    audio_provider_enumerator_t *enumerator,
    audio_provider_t *provider, const char **providerName)
{
    static const char openalName[] = AUDIO_BACKEND_OPENAL_NAME;
    if (enumerator == NULL || provider == NULL || providerName == NULL ||
        *enumerator != 0)
        return 0;
    *enumerator = 1;
    *provider = OPENAL_PROVIDER;
    *providerName = openalName;
    return 1;
}

int32_t openal_open_3D_provider(audio_provider_t provider)
{
    return provider == OPENAL_PROVIDER ? 0 : 1;
}

int32_t openal_3D_provider_attribute(
    audio_provider_t provider, const char *attributeName, void *value)
{
    if (provider != OPENAL_PROVIDER)
        return 0;
    if (attributeName == NULL || value == NULL)
        return 0;
    if (strcmp(attributeName, "Maximum supported samples") == 0)
        *(int32_t *)value = MSS_3D_CHANNEL_CAPACITY;
    else if (strcmp(attributeName, "EAX3 room LF") == 0)
        *(int32_t *)value = -1;
    return 1;
}

int32_t openal_set_3D_provider_preference(
    audio_provider_t provider, const char *preferenceName, void *value)
{
    (void)provider;
    (void)preferenceName;
    (void)value;
    return 0;
}

void openal_set_3D_distance_factor(
    audio_provider_t provider, float distanceFactor)
{
    (void)provider;
    (void)distanceFactor;
}

audio_sample_handle_t openal_allocate_sample_handle(
    audio_driver_t driver)
{
    if (driver == NULL)
        return NULL;
    struct audio_sample_handle_s *const sample =
        calloc(1, sizeof(*sample));
    if (sample == NULL)
        return NULL;
    openal_clear_error();
    alGenSources(1, &sample->source);
    if (!openal_check("2D source allocation")) {
        if (sample->source != 0)
            alDeleteSources(1, &sample->source);
        free(sample);
        return NULL;
    }
    alSourcei(sample->source, AL_SOURCE_RELATIVE, AL_TRUE);
    alSourcef(sample->source, AL_ROLLOFF_FACTOR, 0.0f);
    sample->volume = 1.0f;
    sample->pan = 0.5f;
    sample->next = openal_samples;
    openal_samples = sample;
    return sample;
}

audio_3d_sample_handle_t openal_allocate_3D_sample_handle(
    audio_provider_t provider)
{
    if (provider != OPENAL_PROVIDER)
        return NULL;
    struct audio_3d_sample_handle_s *const sample =
        calloc(1, sizeof(*sample));
    if (sample == NULL)
        return NULL;
    sample->provider = provider;
    openal_clear_error();
    alGenSources(OPENAL_FAST2D_CHANNEL_COUNT,
                 sample->sources);
    if (!openal_check("3D source allocation")) {
        for (int32_t channel = 0;
             channel < OPENAL_FAST2D_CHANNEL_COUNT; ++channel) {
            if (sample->sources[channel] != 0)
                alDeleteSources(1, &sample->sources[channel]);
        }
        free(sample);
        return NULL;
    }
    for (int32_t channel = 0;
         channel < OPENAL_FAST2D_CHANNEL_COUNT; ++channel) {
        alSourcei(sample->sources[channel], AL_SOURCE_RELATIVE, AL_TRUE);
        alSourcef(sample->sources[channel], AL_ROLLOFF_FACTOR, 0.0f);
    }
    sample->volume = 1.0f;
    openal_apply_fast2d_gains(sample);
    sample->next = openal_3d_samples;
    openal_3d_samples = sample;
    return sample;
}

void openal_set_3D_position(
    audio_3d_sample_handle_t sample, float x, float y, float z)
{
    if (sample == NULL)
        return;
    sample->position[0] = x;
    sample->position[1] = y;
    sample->position[2] = z;
    openal_apply_fast2d_gains(sample);
}

void openal_end_sample(audio_sample_handle_t sample)
{
    if (sample != NULL) {
        alSourceStop(sample->source);
        alSourceRewind(sample->source);
    }
}

void openal_stop_sample(audio_sample_handle_t sample)
{
    if (sample != NULL)
        alSourcePause(sample->source);
}

void openal_resume_sample(audio_sample_handle_t sample)
{
    if (sample != NULL)
        alSourcePlay(sample->source);
}

int32_t openal_sample_status(audio_sample_handle_t sample)
{
    if (sample == NULL)
        return AUDIO_SAMPLE_STATUS_DONE;
    ALint state = AL_STOPPED;
    alGetSourcei(sample->source, AL_SOURCE_STATE, &state);
    return state == AL_PLAYING || state == AL_PAUSED
               ? OPENAL_STATUS_PLAYING
               : AUDIO_SAMPLE_STATUS_DONE;
}

void openal_end_3D_sample(audio_3d_sample_handle_t sample)
{
    if (sample == NULL)
        return;
    alSourceStopv(OPENAL_FAST2D_CHANNEL_COUNT,
                  sample->sources);
    alSourceRewindv(OPENAL_FAST2D_CHANNEL_COUNT,
                    sample->sources);
}

void openal_stop_3D_sample(audio_3d_sample_handle_t sample)
{
    if (sample == NULL)
        return;
    alSourcePausev(OPENAL_FAST2D_CHANNEL_COUNT,
                   sample->sources);
}

void openal_resume_3D_sample(audio_3d_sample_handle_t sample)
{
    if (sample == NULL)
        return;
    alSourcePlayv(OPENAL_FAST2D_CHANNEL_COUNT,
                  sample->sources);
}

int32_t openal_3D_sample_status(
    audio_3d_sample_handle_t sample)
{
    if (sample == NULL)
        return AUDIO_SAMPLE_STATUS_DONE;
    ALint state = AL_STOPPED;
    alGetSourcei(sample->sources[OPENAL_FAST2D_LEFT],
                 AL_SOURCE_STATE, &state);
    return state == AL_PLAYING || state == AL_PAUSED
               ? OPENAL_STATUS_PLAYING
               : AUDIO_SAMPLE_STATUS_DONE;
}

audio_stream_handle_t openal_open_stream(
    audio_driver_t driver, const char *filename,
    int32_t streamMemory)
{
    (void)streamMemory;
    if (driver == NULL || filename == NULL)
        return NULL;
    struct audio_stream_handle_s *const stream =
        calloc(1, sizeof(*stream));
    if (stream == NULL)
        return NULL;
    if (!openal_read_file(
            filename, &stream->ownedFileBytes,
            &stream->fileByteCount)) {
        openal_set_error("could not read streamed sound");
        openal_delete_stream(stream);
        return NULL;
    }
    if (!openal_open_stream_decoder(stream)) {
        openal_set_error("could not decode streamed sound");
        openal_delete_stream(stream);
        return NULL;
    }

    openal_clear_error();
    alGenSources(1, &stream->source);
    if (!openal_check("stream source allocation")) {
        openal_delete_stream(stream);
        return NULL;
    }

    openal_clear_error();
    alGenBuffers(OPENAL_STREAM_BUFFER_COUNT, stream->buffers);
    if (!openal_check("stream buffer allocation")) {
        openal_delete_stream(stream);
        return NULL;
    }
    alSourcei(stream->source, AL_SOURCE_RELATIVE, AL_TRUE);
    alSourcef(stream->source, AL_ROLLOFF_FACTOR, 0.0f);
    stream->volume = 1.0f;
    stream->pan = 0.5f;
    stream->loopCount = 1;
    if (!openal_stream_queue_initial(stream)) {
        openal_set_error("streamed sound contained no audio");
        openal_delete_stream(stream);
        return NULL;
    }
    stream->next = openal_streams;
    openal_streams = stream;
    return stream;
}

void openal_close_stream(audio_stream_handle_t stream)
{
    if (stream == NULL)
        return;
    struct audio_stream_handle_s **link = &openal_streams;
    while (*link != NULL && *link != stream)
        link = &(*link)->next;
    if (*link == stream)
        *link = stream->next;
    openal_delete_stream(stream);
}

void openal_pause_stream(
    audio_stream_handle_t stream, qboolean paused)
{
    if (stream == NULL)
        return;
    openal_service_stream(stream);
    stream->wantsPlayback = paused ? qfalse : qtrue;
    if (paused)
        alSourcePause(stream->source);
    else if (openal_stream_rewind_if_empty(stream))
        alSourcePlay(stream->source);
}

int32_t openal_stream_status(audio_stream_handle_t stream)
{
    if (stream == NULL)
        return AUDIO_SAMPLE_STATUS_DONE;
    openal_service_stream(stream);
    ALint state = AL_STOPPED;
    alGetSourcei(stream->source, AL_SOURCE_STATE, &state);
    return state == AL_PLAYING || state == AL_PAUSED
               ? OPENAL_STATUS_PLAYING
               : AUDIO_SAMPLE_STATUS_DONE;
}

int32_t openal_stream_playback_rate(
    audio_stream_handle_t stream)
{
    return stream != NULL ? stream->playbackRate : 0;
}

void openal_set_stream_playback_rate(
    audio_stream_handle_t stream, int32_t playbackRate)
{
    if (stream == NULL)
        return;
    openal_service_stream(stream);
    stream->playbackRate = playbackRate;
    openal_apply_rate(
        stream->source, stream->baseRate, playbackRate);
}

void openal_set_stream_volume_pan(
    audio_stream_handle_t stream, float volume, float pan)
{
    if (stream == NULL)
        return;
    openal_service_stream(stream);
    stream->volume = volume;
    stream->pan = pan;
    alSourcef(stream->source, AL_GAIN,
              openal_clamp(volume, 0.0f, 1.0f));
    openal_apply_pan(stream->source, pan);
}

void openal_stream_volume_pan(
    audio_stream_handle_t stream, float *volume, float *pan)
{
    if (stream == NULL)
        return;
    if (volume != NULL)
        *volume = stream->volume;
    if (pan != NULL)
        *pan = stream->pan;
}

void openal_set_stream_loop_count(
    audio_stream_handle_t stream, int32_t loopCount)
{
    if (stream == NULL)
        return;
    openal_service_stream(stream);
    stream->loopCount = loopCount;
}

void openal_set_stream_reverb_levels(
    audio_stream_handle_t stream, float dryLevel, float wetLevel)
{
    (void)dryLevel;
    if (stream != NULL) {
        ALuint *filter = NULL;
#if defined(__linux__)
        filter = &stream->reverbFilter;
#endif
        openal_apply_reverb(
            stream->source, filter, wetLevel);
    }
}

void openal_stream_ms_position(
    audio_stream_handle_t stream, int32_t *totalMsec,
    int32_t *currentMsec)
{
    if (stream == NULL)
        return;
    openal_service_stream(stream);
    if (totalMsec != NULL)
        *totalMsec = stream->baseRate > 0
                         ? (int32_t)((uint64_t)stream->frameCount * 1000u /
                                     (uint32_t)stream->baseRate)
                         : 0;
    if (currentMsec != NULL) {
        ALint sampleOffset = 0;
        alGetSourcei(stream->source, AL_SAMPLE_OFFSET, &sampleOffset);
        uint64_t currentFrame = stream->playedFrames;
        if (sampleOffset > 0)
            currentFrame += (uint32_t)sampleOffset;
        if (stream->loopCount == 0 && stream->frameCount > 0)
            currentFrame %= stream->frameCount;
        else if (currentFrame > stream->frameCount)
            currentFrame = stream->frameCount;
        *currentMsec =
            stream->baseRate > 0
                ? (int32_t)(currentFrame * 1000u /
                            (uint32_t)stream->baseRate)
                : 0;
    }
}

void openal_set_stream_ms_position(
    audio_stream_handle_t stream, int32_t positionMsec)
{
    if (stream == NULL)
        return;
    openal_service_stream(stream);
    uint64_t frame = positionMsec > 0 && stream->baseRate > 0
                         ? (uint64_t)(uint32_t)positionMsec *
                               (uint32_t)stream->baseRate / 1000u
                         : 0;
    if (frame > stream->frameCount)
        frame = stream->frameCount;
    const qboolean resume = stream->wantsPlayback;
    openal_stream_unqueue_all(stream);
    if (!openal_stream_seek_decoder(stream, (uint32_t)frame))
        return;
    stream->playedFrames = frame;
    if (!openal_stream_queue_initial(stream))
        return;
    if (resume)
        alSourcePlay(stream->source);
}

void openal_start_stream(audio_stream_handle_t stream)
{
    if (stream == NULL)
        return;
    stream->wantsPlayback = qtrue;
    openal_service_stream(stream);
    if (openal_stream_rewind_if_empty(stream))
        alSourcePlay(stream->source);
}

void openal_init_sample(audio_sample_handle_t sample)
{
    if (sample == NULL)
        return;
    alSourceStop(sample->source);
    if (sample->rawMode) {
        ALint queued = 0;
        alGetSourcei(sample->source, AL_BUFFERS_QUEUED, &queued);
        while (queued-- > 0) {
            ALuint queuedBuffer = 0;
            alSourceUnqueueBuffers(sample->source, 1, &queuedBuffer);
        }
    } else {
        alSourcei(sample->source, AL_BUFFER, 0);
    }
    sample->buffer = 0;
    for (int32_t index = 0;
         index < OPENAL_RAW_BUFFER_COUNT; ++index) {
        if (sample->rawBuffers[index] != 0) {
            alDeleteBuffers(1, &sample->rawBuffers[index]);
            sample->rawBuffers[index] = 0;
        }
    }
    sample->data = NULL;
    sample->dataLength = 0;
    sample->blockSize = 0;
    sample->frameCount = 0;
    sample->sampleRate = 0;
    sample->playbackRate = 0;
    sample->rawMode = qfalse;
    alSourcei(sample->source, AL_LOOPING, AL_FALSE);
    alSourcef(sample->source, AL_SEC_OFFSET, 0.0f);
}

void openal_set_sample_type(
    audio_sample_handle_t sample, audio_sample_type_t sampleType,
    int32_t flags)
{
    (void)flags;
    if (sample != NULL)
        sample->sampleType = sampleType;
}

void openal_set_sample_address(
    audio_sample_handle_t sample, const void *data, uint32_t dataLength)
{
    if (sample == NULL)
        return;
    sample->data = data;
    sample->dataLength = dataLength;
}

void openal_set_sample_adpcm_block_size(
    audio_sample_handle_t sample, uint32_t blockSize)
{
    if (sample != NULL)
        sample->blockSize = blockSize;
}

int32_t openal_sample_playback_rate(
    audio_sample_handle_t sample)
{
    return sample != NULL ? sample->playbackRate : 0;
}

void openal_set_sample_playback_rate(
    audio_sample_handle_t sample, int32_t playbackRate)
{
    if (sample == NULL)
        return;
    sample->playbackRate = playbackRate;
    if (sample->sampleRate == 0)
        sample->sampleRate = playbackRate;
    if (!sample->rawMode && sample->buffer == 0 &&
        sample->data != NULL) {
        (void)openal_bind_cached_buffer(
            sample->source, &sample->buffer, sample->sampleType,
            sample->data, sample->dataLength, sample->blockSize,
            sample->sampleRate, &sample->frameCount);
    }
    openal_apply_rate(
        sample->source, sample->sampleRate, playbackRate);
}

/* NOT_FROM_ORIGINAL_SOURCE: give the native backend the loaded sound's base
 * sample rate before pitch is applied, and eagerly bind its shared cache entry
 * so differently pitched starts reuse one OpenAL buffer. */
void openal_bind_loaded_sample(
    audio_sample_handle_t sample,
    const snd_alias_sound_file_t *soundFile)
{
    if (sample == NULL || soundFile == NULL)
        return;

    sample->sampleRate = (int32_t)soundFile->sampleRate;
    (void)openal_bind_cached_buffer(
        sample->source, &sample->buffer, sample->sampleType,
        soundFile->data, soundFile->dataLength, soundFile->blockSize,
        sample->sampleRate, &sample->frameCount);
}

void openal_set_sample_volume_pan(
    audio_sample_handle_t sample, float volume, float pan)
{
    if (sample == NULL)
        return;
    sample->volume = volume;
    sample->pan = pan;
    alSourcef(sample->source, AL_GAIN,
              openal_clamp(volume, 0.0f, 1.0f));
    openal_apply_pan(sample->source, pan);
}

void openal_sample_volume_pan(
    audio_sample_handle_t sample, float *volume, float *pan)
{
    if (sample == NULL)
        return;
    if (volume != NULL)
        *volume = sample->volume;
    if (pan != NULL)
        *pan = sample->pan;
}

void openal_set_sample_loop_count(
    audio_sample_handle_t sample, int32_t loopCount)
{
    if (sample == NULL)
        return;
    alSourcei(sample->source, AL_LOOPING,
              loopCount == 0 ? AL_TRUE : AL_FALSE);
}

void openal_set_sample_reverb_levels(
    audio_sample_handle_t sample, float dryLevel, float wetLevel)
{
    (void)dryLevel;
    if (sample != NULL) {
        ALuint *filter = NULL;
#if defined(__linux__)
        filter = &sample->reverbFilter;
#endif
        openal_apply_reverb(
            sample->source, filter, wetLevel);
    }
}

void openal_sample_ms_position(
    audio_sample_handle_t sample, int32_t *totalMsec,
    int32_t *currentMsec)
{
    if (sample == NULL)
        return;
    if (totalMsec != NULL)
        *totalMsec = sample->playbackRate > 0
                         ? (int32_t)((uint64_t)sample->frameCount * 1000u /
                                     (uint32_t)sample->playbackRate)
                         : 0;
    if (currentMsec != NULL) {
        ALfloat seconds = 0.0f;
        alGetSourcef(sample->source, AL_SEC_OFFSET, &seconds);
        *currentMsec =
            sample->sampleRate > 0 && sample->playbackRate > 0
                ? (int32_t)(seconds * 1000.0f *
                            (float)sample->sampleRate /
                            (float)sample->playbackRate)
                : 0;
    }
}

void openal_set_sample_ms_position(
    audio_sample_handle_t sample, int32_t positionMsec)
{
    if (sample != NULL && sample->sampleRate > 0 &&
        sample->playbackRate > 0) {
        alSourcef(sample->source, AL_SEC_OFFSET,
                  (float)positionMsec / 1000.0f *
                      (float)sample->playbackRate /
                      (float)sample->sampleRate);
    }
}

void openal_start_sample(audio_sample_handle_t sample)
{
    if (sample != NULL && sample->buffer != 0)
        alSourcePlay(sample->source);
}

void openal_release_sample_handle(
    audio_sample_handle_t sample)
{
    if (sample == NULL)
        return;
    struct audio_sample_handle_s **link = &openal_samples;
    while (*link != NULL && *link != sample)
        link = &(*link)->next;
    if (*link == sample)
        *link = sample->next;
    openal_delete_sample(sample);
}

int32_t openal_minimum_sample_buffer_size(
    audio_driver_t driver, int32_t sampleRate,
    audio_sample_type_t sampleType)
{
    (void)driver;
    return sampleRate * openal_channels(sampleType) *
           openal_bytes_per_sample(sampleType) / 10;
}

uint32_t openal_sample_position(audio_sample_handle_t sample)
{
    if (sample == NULL)
        return 0;
    ALint byteOffset = 0;
    alGetSourcei(sample->source, AL_BYTE_OFFSET, &byteOffset);
    return byteOffset > 0 ? (uint32_t)byteOffset : 0;
}

int32_t openal_sample_buffer_ready(
    audio_sample_handle_t sample)
{
    if (sample == NULL)
        return -1;
    if (!sample->rawMode) {
        alSourceStop(sample->source);
        alSourcei(sample->source, AL_BUFFER, 0);
        if (sample->buffer != 0) {
            alDeleteBuffers(1, &sample->buffer);
            sample->buffer = 0;
        }
        sample->rawMode = qtrue;
        openal_clear_error();
        alGenBuffers(OPENAL_RAW_BUFFER_COUNT, sample->rawBuffers);
        if (!openal_check("raw sample buffer allocation"))
            return -1;
        return 0;
    }
    ALint queued = 0;
    ALint processed = 0;
    alGetSourcei(sample->source, AL_BUFFERS_QUEUED, &queued);
    alGetSourcei(sample->source, AL_BUFFERS_PROCESSED, &processed);
    if (queued < OPENAL_RAW_BUFFER_COUNT)
        return queued;
    if (processed > 0) {
        ALuint buffer;
        alSourceUnqueueBuffers(sample->source, 1, &buffer);
        for (int32_t index = 0;
             index < OPENAL_RAW_BUFFER_COUNT; ++index) {
            if (sample->rawBuffers[index] == buffer)
                return index;
        }
    }
    return -1;
}

void openal_load_sample_buffer(
    audio_sample_handle_t sample, int32_t bufferIndex,
    const void *data, int32_t byteCount)
{
    if (sample == NULL || bufferIndex < 0 ||
        bufferIndex >= OPENAL_RAW_BUFFER_COUNT ||
        sample->rawBuffers[bufferIndex] == 0 ||
        sample->sampleRate <= 0 || data == NULL || byteCount <= 0)
        return;
    const ALenum format = openal_format(sample->sampleType);
    if (format == 0)
        return;
    alBufferData(sample->rawBuffers[bufferIndex], format, data,
                 byteCount, sample->sampleRate);
    alSourceQueueBuffers(
        sample->source, 1, &sample->rawBuffers[bufferIndex]);
    ALint state = AL_STOPPED;
    alGetSourcei(sample->source, AL_SOURCE_STATE, &state);
    if (state != AL_PLAYING)
        alSourcePlay(sample->source);
}

void openal_set_3D_sample_info(
    audio_3d_sample_handle_t sample,
    const snd_alias_sound_file_t *soundFile)
{
    if (sample == NULL || soundFile == NULL)
        return;
    sample->soundFile = soundFile;
    sample->playbackRate = (int32_t)soundFile->sampleRate;
    const audio_sample_type_t sampleType =
        soundFile->channelCount == 1
            ? (soundFile->formatTag ==
                       OPENAL_WAVE_FORMAT_IMA_ADPCM
                   ? AUDIO_SAMPLE_TYPE_MONO_IMA_ADPCM
                   : (soundFile->bitsPerSample == 8
                          ? AUDIO_SAMPLE_TYPE_MONO_8
                          : AUDIO_SAMPLE_TYPE_MONO_16))
            : (soundFile->formatTag ==
                       OPENAL_WAVE_FORMAT_IMA_ADPCM
                   ? AUDIO_SAMPLE_TYPE_STEREO_IMA_ADPCM
                   : (soundFile->bitsPerSample == 8
                          ? AUDIO_SAMPLE_TYPE_STEREO_8
                          : AUDIO_SAMPLE_TYPE_STEREO_16));
    (void)openal_bind_cached_fast2d_buffers(
        sample->sources, sample->buffers, sampleType,
        soundFile->data, soundFile->dataLength, soundFile->blockSize,
        (int32_t)soundFile->sampleRate, &sample->frameCount);
}

void openal_set_3D_sample_volume(
    audio_3d_sample_handle_t sample, float volume)
{
    if (sample == NULL)
        return;
    sample->volume = volume;
    openal_apply_fast2d_gains(sample);
}

void openal_set_3D_sample_distances(
    audio_3d_sample_handle_t sample, float maximumDistance,
    float minimumDistance)
{
    (void)maximumDistance;
    (void)minimumDistance;
    if (sample != NULL &&
        sample->provider == OPENAL_PROVIDER) {
        for (int32_t channel = 0;
             channel < OPENAL_FAST2D_CHANNEL_COUNT; ++channel) {
            alSourcef(sample->sources[channel], AL_ROLLOFF_FACTOR, 0.0f);
        }
    }
}

int32_t openal_3D_sample_playback_rate(
    audio_3d_sample_handle_t sample)
{
    return sample != NULL ? sample->playbackRate : 0;
}

uint32_t openal_3D_sample_offset(
    audio_3d_sample_handle_t sample)
{
    if (sample == NULL || sample->soundFile == NULL ||
        sample->frameCount == 0)
        return 0;
    ALfloat seconds = 0.0f;
    alGetSourcef(sample->sources[OPENAL_FAST2D_LEFT],
                 AL_SEC_OFFSET, &seconds);
    const float duration =
        (float)sample->frameCount / sample->soundFile->sampleRate;
    if (duration <= 0.0f)
        return 0;
    return (uint32_t)(openal_clamp(
        seconds / duration, 0.0f, 1.0f) *
        sample->soundFile->dataLength);
}

uint32_t openal_3D_sample_length(
    audio_3d_sample_handle_t sample)
{
    return sample != NULL && sample->soundFile != NULL
               ? sample->soundFile->dataLength
               : 0;
}

float openal_3D_sample_volume(
    audio_3d_sample_handle_t sample)
{
    return sample != NULL ? sample->volume : 0.0f;
}

void openal_3D_position(
    audio_3d_sample_handle_t sample, float *x, float *y, float *z)
{
    if (sample == NULL)
        return;
    if (x != NULL)
        *x = sample->position[0];
    if (y != NULL)
        *y = sample->position[1];
    if (z != NULL)
        *z = sample->position[2];
}

void openal_set_3D_sample_playback_rate(
    audio_3d_sample_handle_t sample, int32_t playbackRate)
{
    if (sample == NULL)
        return;
    sample->playbackRate = playbackRate;
    if (sample->soundFile != NULL) {
        for (int32_t channel = 0;
             channel < OPENAL_FAST2D_CHANNEL_COUNT; ++channel) {
            openal_apply_rate(
                sample->sources[channel],
                (int32_t)sample->soundFile->sampleRate,
                playbackRate);
        }
    }
}

void openal_set_3D_sample_loop_count(
    audio_3d_sample_handle_t sample, int32_t loopCount)
{
    if (sample == NULL)
        return;
    for (int32_t channel = 0;
         channel < OPENAL_FAST2D_CHANNEL_COUNT; ++channel) {
        alSourcei(sample->sources[channel], AL_LOOPING,
                  loopCount == 0 ? AL_TRUE : AL_FALSE);
    }
}

void openal_set_3D_sample_effects_level(
    audio_3d_sample_handle_t sample, float effectsLevel)
{
    if (sample != NULL &&
        sample->provider == OPENAL_PROVIDER) {
        for (int32_t channel = 0;
             channel < OPENAL_FAST2D_CHANNEL_COUNT; ++channel) {
            ALuint *filter = NULL;
#if defined(__linux__)
            filter = &sample->reverbFilters[channel];
#endif
            openal_apply_reverb(
                sample->sources[channel], filter, effectsLevel);
        }
    }
}

int32_t openal_set_3D_sample_preference(
    audio_3d_sample_handle_t sample, const char *preferenceName,
    void *value)
{
    (void)sample;
    (void)preferenceName;
    (void)value;
    return 0;
}

void openal_set_digital_master_room_type(
    audio_driver_t driver, int32_t roomType)
{
    (void)driver;
    openal_apply_room_type(roomType);
}

void openal_set_3D_room_type(
    audio_provider_t provider, int32_t roomType)
{
    if (provider == OPENAL_PROVIDER)
        openal_apply_room_type(roomType);
}

void openal_set_3D_sample_offset(
    audio_3d_sample_handle_t sample, int32_t byteOffset)
{
    if (sample == NULL || sample->soundFile == NULL ||
        sample->soundFile->dataLength == 0)
        return;
    const float fraction = openal_clamp(
        (float)byteOffset / (float)sample->soundFile->dataLength,
        0.0f, 1.0f);
    const float seconds =
        fraction * (float)sample->frameCount /
        sample->soundFile->sampleRate;
    for (int32_t channel = 0;
         channel < OPENAL_FAST2D_CHANNEL_COUNT; ++channel) {
        alSourcef(sample->sources[channel], AL_SEC_OFFSET, seconds);
    }
}

void openal_start_3D_sample(
    audio_3d_sample_handle_t sample)
{
    if (sample != NULL &&
        sample->buffers[OPENAL_FAST2D_LEFT] != 0 &&
        sample->buffers[OPENAL_FAST2D_RIGHT] != 0) {
        alSourcePlayv(OPENAL_FAST2D_CHANNEL_COUNT,
                      sample->sources);
    }
}

#if defined(__APPLE__)
#pragma clang diagnostic pop
#endif

const audio_backend_api_t openal_backend = {
    .name = AUDIO_BACKEND_OPENAL_NAME,
    .api_bind_loaded_sample = openal_bind_loaded_sample,
    .api_forget_loaded_sound = openal_forget_loaded_sound,
    .api_set_preference = openal_set_preference,
    .api_set_file_callbacks = openal_set_file_callbacks,
    .api_set_redist_directory = openal_set_redist_directory,
    .api_startup = openal_startup,
    .api_shutdown = openal_shutdown,
    .api_close_3D_provider = openal_close_3D_provider,
    .api_set_DirectSound_HWND = openal_set_DirectSound_HWND,
    .api_WAV_info = openal_WAV_info,
    .api_size_processed_digital_audio = openal_size_processed_digital_audio,
    .api_process_digital_audio = openal_process_digital_audio,
    .api_digital_CPU_percent = openal_digital_CPU_percent,
    .api_open_digital_driver = openal_open_digital_driver,
    .api_last_error = openal_last_error,
    .api_enumerate_3D_providers = openal_enumerate_3D_providers,
    .api_open_3D_provider = openal_open_3D_provider,
    .api_3D_provider_attribute = openal_3D_provider_attribute,
    .api_set_3D_provider_preference = openal_set_3D_provider_preference,
    .api_set_3D_distance_factor = openal_set_3D_distance_factor,
    .api_allocate_sample_handle = openal_allocate_sample_handle,
    .api_allocate_3D_sample_handle = openal_allocate_3D_sample_handle,
    .api_set_3D_position = openal_set_3D_position,
    .api_end_sample = openal_end_sample,
    .api_stop_sample = openal_stop_sample,
    .api_resume_sample = openal_resume_sample,
    .api_sample_status = openal_sample_status,
    .api_end_3D_sample = openal_end_3D_sample,
    .api_stop_3D_sample = openal_stop_3D_sample,
    .api_resume_3D_sample = openal_resume_3D_sample,
    .api_3D_sample_status = openal_3D_sample_status,
    .api_open_stream = openal_open_stream,
    .api_close_stream = openal_close_stream,
    .api_pause_stream = openal_pause_stream,
    .api_stream_status = openal_stream_status,
    .api_stream_playback_rate = openal_stream_playback_rate,
    .api_set_stream_playback_rate = openal_set_stream_playback_rate,
    .api_set_stream_volume_pan = openal_set_stream_volume_pan,
    .api_stream_volume_pan = openal_stream_volume_pan,
    .api_set_stream_loop_count = openal_set_stream_loop_count,
    .api_set_stream_reverb_levels = openal_set_stream_reverb_levels,
    .api_stream_ms_position = openal_stream_ms_position,
    .api_set_stream_ms_position = openal_set_stream_ms_position,
    .api_start_stream = openal_start_stream,
    .api_init_sample = openal_init_sample,
    .api_set_sample_type = openal_set_sample_type,
    .api_set_sample_address = openal_set_sample_address,
    .api_set_sample_adpcm_block_size = openal_set_sample_adpcm_block_size,
    .api_sample_playback_rate = openal_sample_playback_rate,
    .api_set_sample_playback_rate = openal_set_sample_playback_rate,
    .api_set_sample_volume_pan = openal_set_sample_volume_pan,
    .api_sample_volume_pan = openal_sample_volume_pan,
    .api_set_sample_loop_count = openal_set_sample_loop_count,
    .api_set_sample_reverb_levels = openal_set_sample_reverb_levels,
    .api_sample_ms_position = openal_sample_ms_position,
    .api_set_sample_ms_position = openal_set_sample_ms_position,
    .api_start_sample = openal_start_sample,
    .api_release_sample_handle = openal_release_sample_handle,
    .api_minimum_sample_buffer_size = openal_minimum_sample_buffer_size,
    .api_sample_position = openal_sample_position,
    .api_sample_buffer_ready = openal_sample_buffer_ready,
    .api_load_sample_buffer = openal_load_sample_buffer,
    .api_set_3D_sample_info = openal_set_3D_sample_info,
    .api_set_3D_sample_volume = openal_set_3D_sample_volume,
    .api_set_3D_sample_distances = openal_set_3D_sample_distances,
    .api_3D_sample_playback_rate = openal_3D_sample_playback_rate,
    .api_3D_sample_offset = openal_3D_sample_offset,
    .api_3D_sample_length = openal_3D_sample_length,
    .api_3D_sample_volume = openal_3D_sample_volume,
    .api_3D_position = openal_3D_position,
    .api_set_3D_sample_playback_rate = openal_set_3D_sample_playback_rate,
    .api_set_3D_sample_loop_count = openal_set_3D_sample_loop_count,
    .api_set_3D_sample_effects_level = openal_set_3D_sample_effects_level,
    .api_set_3D_sample_preference = openal_set_3D_sample_preference,
    .api_set_digital_master_room_type = openal_set_digital_master_room_type,
    .api_set_3D_room_type = openal_set_3D_room_type,
    .api_set_3D_sample_offset = openal_set_3D_sample_offset,
    .api_start_3D_sample = openal_start_3D_sample,
};

#endif
