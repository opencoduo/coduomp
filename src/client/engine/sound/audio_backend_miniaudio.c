#include "audio_backend_miniaudio.h"
#include "audio_backend_api.h"

#if defined(AUDIO_BACKEND_MINIAUDIO)

/* NOT_FROM_ORIGINAL_SOURCE: Miniaudio is compiled into the client and owns
 * device output, mixing, loaded samples, raw samples, and streams. */
#define MA_NO_FLAC
#define MA_NO_VORBIS
#define MINIAUDIO_IMPLEMENTATION
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wshorten-64-to-32"
#endif
#include "../../../../vendor/miniaudio/miniaudio.h"
#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "qcommon/q_string.h"

#include <math.h>

enum {
    MINIAUDIO_WAVE_FORMAT_PCM = 1,
    MINIAUDIO_WAVE_FORMAT_IMA_ADPCM = 17,
    MINIAUDIO_IMA_HEADER_BYTES = 4
};

typedef struct miniaudio_pan_node_s {
    ma_node_base base;
    ma_atomic_float leftGain;
    ma_atomic_float rightGain;
    ma_uint32 inputChannels;
} miniaudio_pan_node_t;

typedef struct miniaudio_3d_sample_s
    miniaudio_3d_sample_t;

struct miniaudio_3d_sample_s {
    ma_audio_buffer buffer;
    miniaudio_pan_node_t panNode;
    ma_sound sound;
    void *ownedPcm;
    const snd_alias_sound_file_t *soundFile;
    uint32_t frameCount;
    float leftGain;
    float rightGain;
    qboolean bufferInitialized;
    qboolean nodeInitialized;
    qboolean soundInitialized;
    qboolean active;
};

static ma_engine miniaudio_engine;
static qboolean miniaudio_engine_initialized;
static char miniaudio_error[256] = "no Miniaudio error";

/* NOT_FROM_ORIGINAL_SOURCE: publish a stable backend error string. */
static void miniaudio_set_error(const char *operation,
                                        ma_result result)
{
    (void)snprintf(miniaudio_error,
                   sizeof(miniaudio_error), "%s: %s",
                   operation, ma_result_description(result));
}

static uint16_t miniaudio_read_u16(const uint8_t *bytes)
{
    return (uint16_t)((uint16_t)bytes[0] |
                      ((uint16_t)bytes[1] << 8));
}

/* NOT_FROM_ORIGINAL_SOURCE: decode the mono or stereo Microsoft IMA blocks
 * already accepted by the recovered Miles load path. */
static int16_t miniaudio_decode_ima_nibble(
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

static qboolean miniaudio_decode_ima(
    const snd_alias_sound_file_t *soundFile, void **outPcm,
    uint32_t *outFrameCount)
{
    const uint32_t blockSize = soundFile->blockSize;
    const uint32_t channelCount = soundFile->channelCount;
    if (soundFile->data == NULL || soundFile->dataLength == 0 ||
        (channelCount != 1 && channelCount != 2))
        return qfalse;

    const uint32_t headerSize =
        channelCount * MINIAUDIO_IMA_HEADER_BYTES;
    if (blockSize < headerSize)
        return qfalse;
    const uint32_t framesPerBlock =
        1u + ((blockSize - headerSize) * 2u) / channelCount;
    const uint32_t blockCount =
        (soundFile->dataLength + blockSize - 1u) / blockSize;
    const uint64_t capacityFrames =
        (uint64_t)framesPerBlock * blockCount;
    const uint64_t capacityBytes =
        capacityFrames * channelCount * sizeof(int16_t);
    if (capacityFrames == 0 || capacityFrames > UINT32_MAX ||
        capacityBytes > SIZE_MAX)
        return qfalse;

    int16_t *const output =
        malloc((size_t)capacityBytes);
    if (output == NULL)
        return qfalse;

    const uint8_t *const input = soundFile->data;
    uint32_t inputOffset = 0;
    uint32_t outputFrames = 0;
    while (inputOffset + headerSize <= soundFile->dataLength) {
        uint32_t currentBlockSize = soundFile->dataLength - inputOffset;
        if (currentBlockSize > blockSize)
            currentBlockSize = blockSize;
        const uint8_t *const block = input + inputOffset;
        int32_t predictor[2] = {0, 0};
        int32_t stepIndex[2] = {0, 0};
        for (uint32_t channel = 0; channel < channelCount; ++channel) {
            const uint8_t *const header = block + channel * 4u;
            predictor[channel] =
                (int16_t)miniaudio_read_u16(header);
            stepIndex[channel] = header[2];
            if (stepIndex[channel] > 88)
                stepIndex[channel] = 88;
            output[(size_t)outputFrames * channelCount + channel] =
                (int16_t)predictor[channel];
        }
        ++outputFrames;

        uint32_t blockOffset = headerSize;
        if (channelCount == 1) {
            while (blockOffset < currentBlockSize &&
                   outputFrames < capacityFrames) {
                const uint8_t packed = block[blockOffset++];
                output[outputFrames++] =
                    miniaudio_decode_ima_nibble(
                        packed & 15u, &predictor[0], &stepIndex[0]);
                if (outputFrames < capacityFrames) {
                    output[outputFrames++] =
                        miniaudio_decode_ima_nibble(
                            packed >> 4, &predictor[0], &stepIndex[0]);
                }
            }
        } else {
            while (blockOffset < currentBlockSize) {
                int16_t decoded[2][8];
                uint32_t decodedCount[2] = {0, 0};
                for (uint32_t channel = 0; channel < 2; ++channel) {
                    for (uint32_t byteIndex = 0;
                         byteIndex < 4 &&
                         blockOffset < currentBlockSize;
                         ++byteIndex) {
                        const uint8_t packed = block[blockOffset++];
                        decoded[channel][decodedCount[channel]++] =
                            miniaudio_decode_ima_nibble(
                                packed & 15u, &predictor[channel],
                                &stepIndex[channel]);
                        decoded[channel][decodedCount[channel]++] =
                            miniaudio_decode_ima_nibble(
                                packed >> 4, &predictor[channel],
                                &stepIndex[channel]);
                    }
                }
                uint32_t groupFrames = decodedCount[0];
                if (decodedCount[1] < groupFrames)
                    groupFrames = decodedCount[1];
                for (uint32_t frame = 0;
                     frame < groupFrames &&
                     outputFrames < capacityFrames;
                     ++frame) {
                    output[(size_t)outputFrames * 2u] =
                        decoded[0][frame];
                    output[(size_t)outputFrames * 2u + 1u] =
                        decoded[1][frame];
                    ++outputFrames;
                }
            }
        }
        inputOffset += currentBlockSize;
    }

    if (outputFrames == 0) {
        free(output);
        return qfalse;
    }
    *outPcm = output;
    *outFrameCount = outputFrames;
    return qtrue;
}

/* NOT_FROM_ORIGINAL_SOURCE: exact two-channel terminal node for the original
 * Fast 2D provider's independently computed sample-volume levels. One input
 * frame and one playback cursor feed both output channels. */
static void miniaudio_pan_process(
    ma_node *node, const float **inputFrames, ma_uint32 *inputFrameCount,
    float **outputFrames, ma_uint32 *outputFrameCount)
{
    miniaudio_pan_node_t *const pan =
        (miniaudio_pan_node_t *)node;
    ma_uint32 frameCount = *inputFrameCount;
    if (frameCount > *outputFrameCount)
        frameCount = *outputFrameCount;
    const float left = ma_atomic_float_get(&pan->leftGain);
    const float right = ma_atomic_float_get(&pan->rightGain);
    const float *const input = inputFrames[0];
    float *const output = outputFrames[0];
    for (ma_uint32 frame = 0; frame < frameCount; ++frame) {
        const float inputLeft = input[frame * pan->inputChannels];
        const float inputRight = pan->inputChannels == 2
                                     ? input[frame * 2u + 1u]
                                     : inputLeft;
        output[frame * 2u] = inputLeft * left;
        output[frame * 2u + 1u] = inputRight * right;
    }
    *inputFrameCount = frameCount;
    *outputFrameCount = frameCount;
}

static ma_node_vtable miniaudio_pan_vtable = {
    miniaudio_pan_process,
    NULL,
    1,
    1,
    0
};

/* NOT_FROM_ORIGINAL_SOURCE: tear down one reusable provider voice without
 * releasing the small outer handle allocated for the engine channel. */
static void miniaudio_3d_sample_reset(
    miniaudio_3d_sample_t *sample)
{
    if (sample->soundInitialized) {
        (void)ma_sound_stop(&sample->sound);
        ma_sound_uninit(&sample->sound);
        sample->soundInitialized = qfalse;
    }
    if (sample->nodeInitialized) {
        ma_node_uninit((ma_node *)&sample->panNode, NULL);
        sample->nodeInitialized = qfalse;
    }
    if (sample->bufferInitialized) {
        ma_audio_buffer_uninit(&sample->buffer);
        sample->bufferInitialized = qfalse;
    }
    free(sample->ownedPcm);
    sample->ownedPcm = NULL;
    sample->soundFile = NULL;
    sample->frameCount = 0;
    sample->active = qfalse;
}

qboolean miniaudio_engine_init(int32_t sampleRate)
{
    if (miniaudio_engine_initialized)
        return qtrue;
    ma_engine_config config = ma_engine_config_init();
    config.channels = 2;
    if (sampleRate > 0)
        config.sampleRate = (ma_uint32)sampleRate;
    config.defaultVolumeSmoothTimeInPCMFrames = 0;
    const ma_result result =
        ma_engine_init(&config, &miniaudio_engine);
    if (result != MA_SUCCESS) {
        miniaudio_set_error("could not open Miniaudio device",
                                    result);
        return qfalse;
    }
    miniaudio_engine_initialized = qtrue;
    (void)snprintf(miniaudio_error,
                   sizeof(miniaudio_error),
                   "%s", "no Miniaudio error");
    return qtrue;
}

void miniaudio_engine_shutdown(void)
{
    if (!miniaudio_engine_initialized)
        return;
    ma_engine_uninit(&miniaudio_engine);
    memset(&miniaudio_engine, 0,
           sizeof(miniaudio_engine));
    miniaudio_engine_initialized = qfalse;
}

const char *miniaudio_engine_last_error(void)
{
    return miniaudio_error;
}

miniaudio_3d_sample_t *miniaudio_3d_sample_create(void)
{
    if (!miniaudio_engine_initialized)
        return NULL;
    miniaudio_3d_sample_t *const sample =
        calloc(1, sizeof(*sample));
    if (sample != NULL) {
        sample->leftGain = 1.0f;
        sample->rightGain = 1.0f;
    }
    return sample;
}

void miniaudio_3d_sample_destroy(
    miniaudio_3d_sample_t *sample)
{
    if (sample == NULL)
        return;
    miniaudio_3d_sample_reset(sample);
    free(sample);
}

qboolean miniaudio_3d_sample_set_info(
    miniaudio_3d_sample_t *sample,
    const snd_alias_sound_file_t *soundFile)
{
    if (sample == NULL || soundFile == NULL ||
        !miniaudio_engine_initialized ||
        (soundFile->channelCount != 1 && soundFile->channelCount != 2) ||
        soundFile->sampleRate == 0 ||
        soundFile->data == NULL || soundFile->dataLength == 0)
        return qfalse;

    miniaudio_3d_sample_reset(sample);
    ma_format format;
    const void *pcmData = soundFile->data;
    if (soundFile->formatTag == MINIAUDIO_WAVE_FORMAT_PCM &&
        soundFile->bitsPerSample == 8) {
        format = ma_format_u8;
        sample->frameCount = soundFile->dataLength /
                             soundFile->channelCount;
    } else if (
        soundFile->formatTag == MINIAUDIO_WAVE_FORMAT_PCM &&
        soundFile->bitsPerSample == 16 &&
        soundFile->dataLength % sizeof(int16_t) == 0) {
        format = ma_format_s16;
        sample->frameCount =
            soundFile->dataLength /
            ((uint32_t)sizeof(int16_t) * soundFile->channelCount);
    } else if (
        soundFile->formatTag ==
            MINIAUDIO_WAVE_FORMAT_IMA_ADPCM) {
        format = ma_format_s16;
        if (!miniaudio_decode_ima(
                soundFile, &sample->ownedPcm, &sample->frameCount)) {
            (void)snprintf(miniaudio_error,
                           sizeof(miniaudio_error), "%s",
                           "could not decode Miniaudio 3D sample");
            return qfalse;
        }
        pcmData = sample->ownedPcm;
    } else {
        (void)snprintf(miniaudio_error,
                       sizeof(miniaudio_error), "%s",
                       "unsupported Miniaudio 3D sample format");
        return qfalse;
    }

    ma_audio_buffer_config bufferConfig = ma_audio_buffer_config_init(
        format, soundFile->channelCount, sample->frameCount, pcmData, NULL);
    bufferConfig.sampleRate = soundFile->sampleRate;
    ma_result result = ma_audio_buffer_init(
        &bufferConfig, &sample->buffer);
    if (result != MA_SUCCESS) {
        miniaudio_set_error(
            "could not initialize Miniaudio sample buffer", result);
        miniaudio_3d_sample_reset(sample);
        return qfalse;
    }
    sample->bufferInitialized = qtrue;

    const ma_uint32 inputChannels[1] = {soundFile->channelCount};
    const ma_uint32 outputChannels[1] = {2};
    ma_node_config nodeConfig = ma_node_config_init();
    nodeConfig.vtable = &miniaudio_pan_vtable;
    nodeConfig.pInputChannels = inputChannels;
    nodeConfig.pOutputChannels = outputChannels;
    result = ma_node_init(
        ma_engine_get_node_graph(&miniaudio_engine),
        &nodeConfig, NULL, (ma_node *)&sample->panNode);
    if (result != MA_SUCCESS) {
        miniaudio_set_error(
            "could not initialize Miniaudio Fast 2D node", result);
        miniaudio_3d_sample_reset(sample);
        return qfalse;
    }
    sample->nodeInitialized = qtrue;
    sample->panNode.inputChannels = soundFile->channelCount;
    ma_atomic_float_set(&sample->panNode.leftGain, sample->leftGain);
    ma_atomic_float_set(&sample->panNode.rightGain, sample->rightGain);
    result = ma_node_attach_output_bus(
        (ma_node *)&sample->panNode, 0,
        ma_engine_get_endpoint(&miniaudio_engine), 0);
    if (result != MA_SUCCESS) {
        miniaudio_set_error(
            "could not attach Miniaudio Fast 2D node", result);
        miniaudio_3d_sample_reset(sample);
        return qfalse;
    }

    ma_sound_config soundConfig =
        ma_sound_config_init_2(&miniaudio_engine);
    soundConfig.pDataSource = (ma_data_source *)&sample->buffer;
    soundConfig.pInitialAttachment = (ma_node *)&sample->panNode;
    soundConfig.channelsOut = MA_SOUND_SOURCE_CHANNEL_COUNT;
    soundConfig.flags = MA_SOUND_FLAG_NO_SPATIALIZATION;
    result = ma_sound_init_ex(
        &miniaudio_engine, &soundConfig, &sample->sound);
    if (result != MA_SUCCESS) {
        miniaudio_set_error(
            "could not initialize Miniaudio 3D voice", result);
        miniaudio_3d_sample_reset(sample);
        return qfalse;
    }
    sample->soundInitialized = qtrue;
    sample->soundFile = soundFile;
    return qtrue;
}

void miniaudio_3d_sample_forget_sound(
    miniaudio_3d_sample_t *sample,
    const snd_alias_sound_file_t *soundFile)
{
    if (sample != NULL && sample->soundFile == soundFile)
        miniaudio_3d_sample_reset(sample);
}

void miniaudio_3d_sample_set_channel_gains(
    miniaudio_3d_sample_t *sample, float left, float right)
{
    if (sample == NULL)
        return;
    sample->leftGain = left;
    sample->rightGain = right;
    if (sample->nodeInitialized) {
        ma_atomic_float_set(&sample->panNode.leftGain, left);
        ma_atomic_float_set(&sample->panNode.rightGain, right);
    }
}

void miniaudio_3d_sample_set_playback_rate(
    miniaudio_3d_sample_t *sample, int32_t playbackRate,
    int32_t baseRate)
{
    if (sample == NULL || !sample->soundInitialized ||
        playbackRate <= 0 || baseRate <= 0)
        return;
    float pitch = (float)playbackRate / (float)baseRate;
    if (pitch < 0.01f)
        pitch = 0.01f;
    else if (pitch > 4.0f)
        pitch = 4.0f;
    ma_sound_set_pitch(&sample->sound, pitch);
}

void miniaudio_3d_sample_set_loop_count(
    miniaudio_3d_sample_t *sample, int32_t loopCount)
{
    if (sample != NULL && sample->soundInitialized)
        ma_sound_set_looping(&sample->sound,
                             loopCount == 0 ? MA_TRUE : MA_FALSE);
}

void miniaudio_3d_sample_end(
    miniaudio_3d_sample_t *sample)
{
    if (sample == NULL || !sample->soundInitialized)
        return;
    (void)ma_sound_stop(&sample->sound);
    (void)ma_sound_seek_to_pcm_frame(&sample->sound, 0);
    sample->active = qfalse;
}

void miniaudio_3d_sample_stop(
    miniaudio_3d_sample_t *sample)
{
    if (sample != NULL && sample->soundInitialized)
        (void)ma_sound_stop(&sample->sound);
}

void miniaudio_3d_sample_resume(
    miniaudio_3d_sample_t *sample)
{
    if (sample != NULL && sample->soundInitialized &&
        ma_sound_start(&sample->sound) == MA_SUCCESS) {
        sample->active = qtrue;
    }
}

void miniaudio_3d_sample_start(
    miniaudio_3d_sample_t *sample)
{
    if (sample == NULL || !sample->soundInitialized)
        return;
    if (ma_sound_at_end(&sample->sound))
        (void)ma_sound_seek_to_pcm_frame(&sample->sound, 0);
    if (ma_sound_start(&sample->sound) == MA_SUCCESS)
        sample->active = qtrue;
}

qboolean miniaudio_3d_sample_is_active(
    miniaudio_3d_sample_t *sample)
{
    return sample != NULL && sample->soundInitialized && sample->active &&
                   !ma_sound_at_end(&sample->sound)
               ? qtrue
               : qfalse;
}

uint32_t miniaudio_3d_sample_frame_count(
    const miniaudio_3d_sample_t *sample)
{
    return sample != NULL ? sample->frameCount : 0;
}

uint32_t miniaudio_3d_sample_cursor_frame(
    const miniaudio_3d_sample_t *sample)
{
    if (sample == NULL || !sample->soundInitialized)
        return 0;
    ma_uint64 cursor = 0;
    if (ma_sound_get_cursor_in_pcm_frames(&sample->sound, &cursor) !=
        MA_SUCCESS)
        return 0;
    return cursor <= UINT32_MAX ? (uint32_t)cursor : UINT32_MAX;
}

void miniaudio_3d_sample_seek_frame(
    miniaudio_3d_sample_t *sample, uint32_t frame)
{
    if (sample == NULL || !sample->soundInitialized)
        return;
    if (frame > sample->frameCount)
        frame = sample->frameCount;
    (void)ma_sound_seek_to_pcm_frame(&sample->sound, frame);
}

/*
 * NOT_FROM_ORIGINAL_SOURCE: complete engine audio backend implemented with
 * the same statically linked Miniaudio engine on every supported native
 * target.
 */

enum {
    MINIAUDIO_BACKEND_PROVIDER = 1,
    MINIAUDIO_BACKEND_STATUS_PLAYING = 4
};

struct audio_sample_handle_s {
    miniaudio_3d_sample_t *voice;
    miniaudio_3d_sample_t *rawVoices[MSS_RAW_BUFFER_COUNT];
    const snd_alias_sound_file_t *soundFile;
    const void *data;
    uint32_t dataLength;
    uint32_t blockSize;
    uint32_t frameCount;
    int32_t sampleRate;
    int32_t playbackRate;
    audio_sample_type_t sampleType;
    float volume;
    float pan;
    uint64_t rawNextStartFrame;
    qboolean rawMode;
    struct audio_sample_handle_s *next;
};

struct audio_3d_sample_handle_s {
    miniaudio_3d_sample_t *voice;
    const snd_alias_sound_file_t *soundFile;
    uint32_t frameCount;
    int32_t playbackRate;
    float volume;
    vec3_t position;
    struct audio_3d_sample_handle_s *next;
};

struct audio_stream_handle_s {
    ma_decoder decoder;
    ma_sound sound;
    void *ownedFileBytes;
    size_t fileByteCount;
    uint64_t frameCount;
    int32_t baseRate;
    int32_t playbackRate;
    float volume;
    float pan;
    qboolean decoderInitialized;
    qboolean soundInitialized;
    qboolean started;
    struct audio_stream_handle_s *next;
};

typedef struct miniaudio_backend_driver_s {
    int32_t sampleRate;
} miniaudio_backend_driver_t;

static miniaudio_backend_driver_t miniaudio_backend_driver;
static struct audio_sample_handle_s *miniaudio_backend_samples;
static struct audio_3d_sample_handle_s *miniaudio_backend_3d_samples;
static struct audio_stream_handle_s *miniaudio_backend_streams;
static audio_file_open_callback_t miniaudio_backend_file_open;
static audio_file_close_callback_t miniaudio_backend_file_close;
static audio_file_read_callback_t miniaudio_backend_file_read;

/* NOT_FROM_ORIGINAL_SOURCE: clamp one backend scalar. */
static float miniaudio_backend_clamp(float value, float minimum,
                                             float maximum)
{
    if (value < minimum)
        return minimum;
    if (value > maximum)
        return maximum;
    return value;
}

/* NOT_FROM_ORIGINAL_SOURCE: map an engine sample format to channel count. */
static int32_t miniaudio_backend_channels(
    audio_sample_type_t sampleType)
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

/* NOT_FROM_ORIGINAL_SOURCE: map an engine sample format to PCM byte width. */
static int32_t miniaudio_backend_bytes_per_sample(
    audio_sample_type_t sampleType)
{
    return sampleType == AUDIO_SAMPLE_TYPE_MONO_8 ||
                   sampleType == AUDIO_SAMPLE_TYPE_STEREO_8
               ? 1
               : 2;
}

/* NOT_FROM_ORIGINAL_SOURCE: identify the two engine IMA sample types. */
static qboolean miniaudio_backend_is_adpcm(
    audio_sample_type_t sampleType)
{
    return sampleType == AUDIO_SAMPLE_TYPE_MONO_IMA_ADPCM ||
                   sampleType == AUDIO_SAMPLE_TYPE_STEREO_IMA_ADPCM
               ? qtrue
               : qfalse;
}

/* NOT_FROM_ORIGINAL_SOURCE: apply Miles-style 2D volume and pan gains. */
static void miniaudio_backend_apply_2d_gains(
    struct audio_sample_handle_s *sample)
{
    if (sample == NULL)
        return;
    const float volume = miniaudio_backend_clamp(
        sample->volume, 0.0f, 1.0f);
    const float pan = miniaudio_backend_clamp(
        sample->pan, 0.0f, 1.0f);
    const float left = volume * (pan <= 0.5f ? 1.0f : 2.0f - pan * 2.0f);
    const float right = volume * (pan >= 0.5f ? 1.0f : pan * 2.0f);
    miniaudio_3d_sample_set_channel_gains(
        sample->voice, left, right);
    for (int32_t index = 0; index < MSS_RAW_BUFFER_COUNT; ++index) {
        miniaudio_3d_sample_set_channel_gains(
            sample->rawVoices[index], left, right);
    }
}

/* NOT_FROM_ORIGINAL_SOURCE: reproduce the original Fast 2D provider's
 * channel-level panning with one Miniaudio transport. */
static void miniaudio_backend_apply_3d_gains(
    struct audio_3d_sample_handle_s *sample)
{
    static const float nearListenerDistance = 0.0001f;
    static const float inversePi = 0.3183098733425140380859375f;
    static const float centeredFraction = 0.5f;
    static const float rearVolumeScale = 0.75f;
    static const double volumeExponent = 1.6666666666666667;

    if (sample == NULL)
        return;
    const float right = sample->position[0];
    const float up = sample->position[1];
    const float forward = sample->position[2];
    const float distance = (float)sqrt(
        ((double)forward * forward + (double)up * up) +
        (double)right * right);
    float leftFraction = centeredFraction;
    float levelVolume = (float)pow(
        (double)miniaudio_backend_clamp(
            sample->volume, 0.0f, 1.0f),
        volumeExponent);
    if (distance > nearListenerDistance) {
        const double inverseDistance = 1.0 / (double)distance;
        const float normalizedRight =
            (float)((double)right * inverseDistance);
        const float normalizedForward =
            (float)((double)forward * inverseDistance);
        leftFraction = (float)(acos((double)normalizedRight) *
                                   (double)inversePi);
        if (normalizedForward < 0.0f)
            levelVolume *= rearVolumeScale;
    }
    miniaudio_3d_sample_set_channel_gains(
        sample->voice, levelVolume * leftFraction,
        levelVolume * (1.0f - leftFraction));
}

/* NOT_FROM_ORIGINAL_SOURCE: release all backend state owned by one 2D
 * channel while retaining the reusable outer handle. */
static void miniaudio_backend_reset_sample(
    struct audio_sample_handle_s *sample)
{
    if (sample == NULL)
        return;
    if (sample->voice != NULL)
        miniaudio_3d_sample_reset(sample->voice);
    for (int32_t index = 0; index < MSS_RAW_BUFFER_COUNT; ++index) {
        miniaudio_3d_sample_destroy(sample->rawVoices[index]);
        sample->rawVoices[index] = NULL;
    }
    sample->soundFile = NULL;
    sample->data = NULL;
    sample->dataLength = 0;
    sample->blockSize = 0;
    sample->frameCount = 0;
    sample->sampleRate = 0;
    sample->playbackRate = 0;
    sample->rawNextStartFrame = 0;
    sample->rawMode = qfalse;
}

/* NOT_FROM_ORIGINAL_SOURCE: destroy one Miniaudio stream. */
static void miniaudio_backend_delete_stream(
    struct audio_stream_handle_s *stream)
{
    if (stream == NULL)
        return;
    if (stream->soundInitialized) {
        (void)ma_sound_stop(&stream->sound);
        ma_sound_uninit(&stream->sound);
    }
    if (stream->decoderInitialized)
        ma_decoder_uninit(&stream->decoder);
    free(stream->ownedFileBytes);
    free(stream);
}

/* NOT_FROM_ORIGINAL_SOURCE: read a stream through the engine's registered
 * callbacks so archive-contained music remains supported. */
static qboolean miniaudio_backend_read_file(
    const char *filename, void **outBytes, size_t *outSize)
{
    *outBytes = NULL;
    *outSize = 0;
    if (miniaudio_backend_file_open != NULL &&
        miniaudio_backend_file_close != NULL &&
        miniaudio_backend_file_read != NULL) {
        int32_t handle;
        const int32_t fileSize =
            miniaudio_backend_file_open(filename, &handle);
        if (fileSize <= 0)
            return qfalse;
        void *const bytes = malloc((size_t)fileSize);
        if (bytes == NULL) {
            miniaudio_backend_file_close(handle);
            return qfalse;
        }
        const int32_t bytesRead = miniaudio_backend_file_read(
            handle, bytes, fileSize);
        miniaudio_backend_file_close(handle);
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

/* NOT_FROM_ORIGINAL_SOURCE: read one little-endian RIFF dword. */
static uint32_t miniaudio_backend_read_u32(const uint8_t *bytes)
{
    return (uint32_t)bytes[0] |
           ((uint32_t)bytes[1] << 8) |
           ((uint32_t)bytes[2] << 16) |
           ((uint32_t)bytes[3] << 24);
}

/* NOT_FROM_ORIGINAL_SOURCE: mirror Miles' first-match top-level RIFF chunk
 * lookup after the recovered loader has bounded the scan extent. */
static qboolean miniaudio_backend_find_wav_chunk(
    const uint8_t *bytes, uint32_t scanEnd, const char *chunkId,
    const uint8_t **outData, uint32_t *outSize)
{
    uint32_t offset = 12;
    for (;;) {
        const uint8_t *const chunk = bytes + offset;
        const uint32_t chunkSize =
            miniaudio_backend_read_u32(chunk + 4);
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

int32_t miniaudio_set_preference(int32_t preference, int32_t value)
{
    (void)preference;
    (void)value;
    return 0;
}

void miniaudio_set_file_callbacks(
    audio_file_open_callback_t openCallback,
    audio_file_close_callback_t closeCallback,
    audio_file_seek_callback_t seekCallback,
    audio_file_read_callback_t readCallback)
{
    (void)seekCallback;
    miniaudio_backend_file_open = openCallback;
    miniaudio_backend_file_close = closeCallback;
    miniaudio_backend_file_read = readCallback;
}

void miniaudio_set_redist_directory(const char *directory)
{
    (void)directory;
}

int32_t miniaudio_startup(void)
{
    (void)snprintf(miniaudio_error,
                   sizeof(miniaudio_error), "%s",
                   "no Miniaudio error");
    return 1;
}

void miniaudio_shutdown(void)
{
    while (miniaudio_backend_streams != NULL) {
        struct audio_stream_handle_s *const next =
            miniaudio_backend_streams->next;
        miniaudio_backend_delete_stream(
            miniaudio_backend_streams);
        miniaudio_backend_streams = next;
    }
    while (miniaudio_backend_3d_samples != NULL) {
        struct audio_3d_sample_handle_s *const next =
            miniaudio_backend_3d_samples->next;
        miniaudio_3d_sample_destroy(
            miniaudio_backend_3d_samples->voice);
        free(miniaudio_backend_3d_samples);
        miniaudio_backend_3d_samples = next;
    }
    while (miniaudio_backend_samples != NULL) {
        struct audio_sample_handle_s *const next =
            miniaudio_backend_samples->next;
        miniaudio_backend_reset_sample(
            miniaudio_backend_samples);
        miniaudio_3d_sample_destroy(
            miniaudio_backend_samples->voice);
        free(miniaudio_backend_samples);
        miniaudio_backend_samples = next;
    }
    miniaudio_engine_shutdown();
    memset(&miniaudio_backend_driver, 0,
           sizeof(miniaudio_backend_driver));
}

void miniaudio_close_3D_provider(audio_provider_t provider)
{
    (void)provider;
}

void miniaudio_set_DirectSound_HWND(
    audio_driver_t driver, audio_window_handle_t windowHandle)
{
    (void)driver;
    (void)windowHandle;
}

int32_t miniaudio_WAV_info(
    const void *fileData, audio_sound_info_t *soundInfo)
{
    if (fileData == NULL || soundInfo == NULL)
        return 0;
    const uint8_t *const bytes = fileData;
    if (Q_stricmpn((const char *)bytes + 8, "WAVE", 4) != 0)
        return 0;
    const uint32_t scanEnd =
        miniaudio_backend_read_u32(bytes + 4);
    const uint8_t *formatChunk = NULL;
    const uint8_t *sampleData = NULL;
    uint32_t sampleDataSize = 0;
    if (!miniaudio_backend_find_wav_chunk(
            bytes, scanEnd, "fmt ", &formatChunk, NULL) ||
        !miniaudio_backend_find_wav_chunk(
            bytes, scanEnd, "data", &sampleData, &sampleDataSize)) {
        return 0;
    }

    const uint16_t formatTag = miniaudio_read_u16(formatChunk);
    const uint16_t channelCount =
        miniaudio_read_u16(formatChunk + 2);
    const uint32_t sampleRate =
        miniaudio_backend_read_u32(formatChunk + 4);
    const uint16_t blockSize =
        miniaudio_read_u16(formatChunk + 12);
    const uint16_t bitsPerSample =
        miniaudio_read_u16(formatChunk + 14);
    uint32_t sampleCount;
    if (formatTag == MINIAUDIO_WAVE_FORMAT_IMA_ADPCM &&
        bitsPerSample == 4) {
        const uint8_t *factData = NULL;
        if (miniaudio_backend_find_wav_chunk(
                bytes, scanEnd, "fact", &factData, NULL)) {
            sampleCount =
                miniaudio_backend_read_u32(factData);
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
    snd_alias_sound_file_t *const publicInfo = &soundInfo->publicInfo;
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

uint32_t miniaudio_size_processed_digital_audio(
    uint32_t sampleRate, audio_sample_type_t sampleType,
    int32_t bufferCount, const audio_sound_info_t *sourceInfo)
{
    (void)bufferCount;
    if (sourceInfo == NULL || sourceInfo->publicInfo.sampleRate == 0)
        return 0;
    const snd_alias_sound_file_t *const publicInfo =
        &sourceInfo->publicInfo;
    const int32_t channels =
        miniaudio_backend_channels(sampleType);
    if (sampleRate == 0 || publicInfo->sampleCount == 0 || channels == 0)
        return 0;
    if (miniaudio_backend_is_adpcm(sampleType))
        return publicInfo->dataLength;
    const uint64_t sourceFrames =
        publicInfo->sampleCount / (uint32_t)publicInfo->channelCount;
    const uint64_t outputFrames =
        sourceFrames * sampleRate / publicInfo->sampleRate;
    const uint64_t byteCount =
        outputFrames * (uint32_t)channels *
        (uint32_t)miniaudio_backend_bytes_per_sample(sampleType);
    return byteCount <= UINT32_MAX ? (uint32_t)byteCount : 0;
}

int32_t miniaudio_process_digital_audio(
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
        (publicInfo->channelCount != 1 && publicInfo->channelCount != 2)) {
        return 0;
    }
    if (miniaudio_backend_is_adpcm(sampleType)) {
        if (destinationSize < publicInfo->dataLength)
            return 0;
        memcpy(destination, publicInfo->data, publicInfo->dataLength);
        return 1;
    }
    if (publicInfo->formatTag != MINIAUDIO_WAVE_FORMAT_PCM ||
        (publicInfo->bitsPerSample != 8 &&
         publicInfo->bitsPerSample != 16) ||
        publicInfo->sampleRate == 0) {
        return 0;
    }
    const int32_t outputChannels =
        miniaudio_backend_channels(sampleType);
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
    const uint32_t outputFrames = (uint32_t)(
        (uint64_t)sourceFrames * sampleRate / publicInfo->sampleRate);
    const int32_t outputBytes =
        miniaudio_backend_bytes_per_sample(sampleType);
    const uint64_t required =
        (uint64_t)outputFrames * outputChannels * outputBytes;
    if (required > destinationSize)
        return 0;

    const uint8_t *const input = publicInfo->data;
    uint8_t *const output = destination;
    for (uint32_t frame = 0; frame < outputFrames; ++frame) {
        uint32_t sourceFrame = (uint32_t)(
            (uint64_t)frame * publicInfo->sampleRate / sampleRate);
        if (sourceFrame >= sourceFrames)
            sourceFrame = sourceFrames - 1u;
        int32_t left;
        int32_t right;
        if (publicInfo->bitsPerSample == 16) {
            const size_t sourceIndex =
                (size_t)sourceFrame * publicInfo->channelCount;
            int16_t leftSample;
            memcpy(&leftSample,
                   input + sourceIndex * sizeof(leftSample),
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

int32_t miniaudio_digital_CPU_percent(
    audio_driver_t driver)
{
    (void)driver;
    return 0;
}

audio_driver_t miniaudio_open_digital_driver(
    int32_t sampleRate, int32_t sampleFormat, int32_t channels,
    int32_t flags)
{
    (void)sampleFormat;
    (void)channels;
    (void)flags;
    if (!miniaudio_engine_init(sampleRate))
        return NULL;
    miniaudio_backend_driver.sampleRate = sampleRate;
    return &miniaudio_backend_driver;
}

const char *miniaudio_last_error(void)
{
    return miniaudio_engine_last_error();
}

int32_t miniaudio_enumerate_3D_providers(
    audio_provider_enumerator_t *enumerator,
    audio_provider_t *provider, const char **providerName)
{
    static const char miniaudioName[] =
        AUDIO_BACKEND_MINIAUDIO_NAME;
    if (enumerator == NULL || provider == NULL || providerName == NULL ||
        *enumerator != 0) {
        return 0;
    }
    *enumerator = 1;
    *provider = MINIAUDIO_BACKEND_PROVIDER;
    *providerName = miniaudioName;
    return 1;
}

int32_t miniaudio_open_3D_provider(audio_provider_t provider)
{
    return provider == MINIAUDIO_BACKEND_PROVIDER &&
                   miniaudio_engine_initialized
               ? 0
               : 1;
}

int32_t miniaudio_3D_provider_attribute(
    audio_provider_t provider, const char *attributeName, void *value)
{
    if (provider != MINIAUDIO_BACKEND_PROVIDER ||
        attributeName == NULL || value == NULL) {
        return 0;
    }
    if (strcmp(attributeName, "Maximum supported samples") == 0)
        *(int32_t *)value = MSS_3D_CHANNEL_CAPACITY;
    else if (strcmp(attributeName, "EAX3 room LF") == 0)
        *(int32_t *)value = -1;
    return 1;
}

int32_t miniaudio_set_3D_provider_preference(
    audio_provider_t provider, const char *preferenceName, void *value)
{
    (void)provider;
    (void)preferenceName;
    (void)value;
    return 0;
}

void miniaudio_set_3D_distance_factor(
    audio_provider_t provider, float distanceFactor)
{
    (void)provider;
    (void)distanceFactor;
}

audio_sample_handle_t miniaudio_allocate_sample_handle(
    audio_driver_t driver)
{
    if (driver == NULL)
        return NULL;
    struct audio_sample_handle_s *const sample =
        calloc(1, sizeof(*sample));
    if (sample == NULL)
        return NULL;
    sample->voice = miniaudio_3d_sample_create();
    if (sample->voice == NULL) {
        free(sample);
        return NULL;
    }
    sample->volume = 1.0f;
    sample->pan = 0.5f;
    sample->next = miniaudio_backend_samples;
    miniaudio_backend_samples = sample;
    miniaudio_backend_apply_2d_gains(sample);
    return sample;
}

audio_3d_sample_handle_t miniaudio_allocate_3D_sample_handle(
    audio_provider_t provider)
{
    if (provider != MINIAUDIO_BACKEND_PROVIDER)
        return NULL;
    struct audio_3d_sample_handle_s *const sample =
        calloc(1, sizeof(*sample));
    if (sample == NULL)
        return NULL;
    sample->voice = miniaudio_3d_sample_create();
    if (sample->voice == NULL) {
        free(sample);
        return NULL;
    }
    sample->volume = 1.0f;
    sample->next = miniaudio_backend_3d_samples;
    miniaudio_backend_3d_samples = sample;
    miniaudio_backend_apply_3d_gains(sample);
    return sample;
}

void miniaudio_set_3D_position(
    audio_3d_sample_handle_t sample, float x, float y, float z)
{
    if (sample == NULL)
        return;
    sample->position[0] = x;
    sample->position[1] = y;
    sample->position[2] = z;
    miniaudio_backend_apply_3d_gains(sample);
}

void miniaudio_end_sample(audio_sample_handle_t sample)
{
    if (sample == NULL)
        return;
    miniaudio_3d_sample_end(sample->voice);
    for (int32_t index = 0; index < MSS_RAW_BUFFER_COUNT; ++index)
        miniaudio_3d_sample_end(sample->rawVoices[index]);
}

void miniaudio_stop_sample(audio_sample_handle_t sample)
{
    if (sample == NULL)
        return;
    miniaudio_3d_sample_stop(sample->voice);
    for (int32_t index = 0; index < MSS_RAW_BUFFER_COUNT; ++index)
        miniaudio_3d_sample_stop(sample->rawVoices[index]);
}

void miniaudio_resume_sample(audio_sample_handle_t sample)
{
    if (sample == NULL)
        return;
    miniaudio_3d_sample_resume(sample->voice);
    for (int32_t index = 0; index < MSS_RAW_BUFFER_COUNT; ++index)
        miniaudio_3d_sample_resume(sample->rawVoices[index]);
}

int32_t miniaudio_sample_status(audio_sample_handle_t sample)
{
    if (sample == NULL)
        return AUDIO_SAMPLE_STATUS_DONE;
    if (miniaudio_3d_sample_is_active(sample->voice))
        return MINIAUDIO_BACKEND_STATUS_PLAYING;
    for (int32_t index = 0; index < MSS_RAW_BUFFER_COUNT; ++index) {
        if (miniaudio_3d_sample_is_active(
                sample->rawVoices[index])) {
            return MINIAUDIO_BACKEND_STATUS_PLAYING;
        }
    }
    return AUDIO_SAMPLE_STATUS_DONE;
}

void miniaudio_end_3D_sample(audio_3d_sample_handle_t sample)
{
    if (sample != NULL)
        miniaudio_3d_sample_end(sample->voice);
}

void miniaudio_stop_3D_sample(audio_3d_sample_handle_t sample)
{
    if (sample != NULL)
        miniaudio_3d_sample_stop(sample->voice);
}

void miniaudio_resume_3D_sample(audio_3d_sample_handle_t sample)
{
    if (sample != NULL)
        miniaudio_3d_sample_resume(sample->voice);
}

int32_t miniaudio_3D_sample_status(
    audio_3d_sample_handle_t sample)
{
    return sample != NULL &&
                   miniaudio_3d_sample_is_active(sample->voice)
               ? MINIAUDIO_BACKEND_STATUS_PLAYING
               : AUDIO_SAMPLE_STATUS_DONE;
}

audio_stream_handle_t miniaudio_open_stream(
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
    if (!miniaudio_backend_read_file(
            filename, &stream->ownedFileBytes,
            &stream->fileByteCount)) {
        (void)snprintf(miniaudio_error,
                       sizeof(miniaudio_error), "%s",
                       "could not read Miniaudio stream");
        miniaudio_backend_delete_stream(stream);
        return NULL;
    }
    const ma_decoder_config decoderConfig =
        ma_decoder_config_init(ma_format_f32, 2, 0);
    ma_result result = ma_decoder_init_memory(
        stream->ownedFileBytes, stream->fileByteCount,
        &decoderConfig, &stream->decoder);
    if (result != MA_SUCCESS) {
        miniaudio_set_error(
            "could not decode Miniaudio stream", result);
        miniaudio_backend_delete_stream(stream);
        return NULL;
    }
    stream->decoderInitialized = qtrue;
    ma_uint32 sampleRate = 0;
    if (ma_data_source_get_data_format(
            (ma_data_source *)&stream->decoder, NULL, NULL,
            &sampleRate, NULL, 0) != MA_SUCCESS ||
        sampleRate == 0 ||
        ma_decoder_get_length_in_pcm_frames(
            &stream->decoder, &stream->frameCount) != MA_SUCCESS ||
        stream->frameCount == 0 || sampleRate > INT32_MAX) {
        (void)snprintf(miniaudio_error,
                       sizeof(miniaudio_error), "%s",
                       "invalid Miniaudio stream format");
        miniaudio_backend_delete_stream(stream);
        return NULL;
    }
    result = ma_sound_init_from_data_source(
        &miniaudio_engine,
        (ma_data_source *)&stream->decoder,
        MA_SOUND_FLAG_NO_SPATIALIZATION, NULL, &stream->sound);
    if (result != MA_SUCCESS) {
        miniaudio_set_error(
            "could not create Miniaudio stream voice", result);
        miniaudio_backend_delete_stream(stream);
        return NULL;
    }
    stream->soundInitialized = qtrue;
    stream->baseRate = (int32_t)sampleRate;
    stream->playbackRate = stream->baseRate;
    stream->volume = 1.0f;
    stream->pan = 0.5f;
    ma_sound_set_volume(&stream->sound, 1.0f);
    ma_sound_set_pan(&stream->sound, 0.0f);
    stream->next = miniaudio_backend_streams;
    miniaudio_backend_streams = stream;
    return stream;
}

void miniaudio_close_stream(audio_stream_handle_t stream)
{
    if (stream == NULL)
        return;
    struct audio_stream_handle_s **link =
        &miniaudio_backend_streams;
    while (*link != NULL && *link != stream)
        link = &(*link)->next;
    if (*link == stream)
        *link = stream->next;
    miniaudio_backend_delete_stream(stream);
}

void miniaudio_pause_stream(
    audio_stream_handle_t stream, qboolean paused)
{
    if (stream == NULL || !stream->soundInitialized)
        return;
    if (paused) {
        (void)ma_sound_stop(&stream->sound);
    } else {
        if (ma_sound_at_end(&stream->sound))
            (void)ma_sound_seek_to_pcm_frame(&stream->sound, 0);
        if (ma_sound_start(&stream->sound) == MA_SUCCESS)
            stream->started = qtrue;
    }
}

int32_t miniaudio_stream_status(audio_stream_handle_t stream)
{
    return stream != NULL && stream->soundInitialized &&
                   stream->started && !ma_sound_at_end(&stream->sound)
               ? MINIAUDIO_BACKEND_STATUS_PLAYING
               : AUDIO_SAMPLE_STATUS_DONE;
}

int32_t miniaudio_stream_playback_rate(
    audio_stream_handle_t stream)
{
    return stream != NULL ? stream->playbackRate : 0;
}

void miniaudio_set_stream_playback_rate(
    audio_stream_handle_t stream, int32_t playbackRate)
{
    if (stream == NULL || !stream->soundInitialized ||
        playbackRate <= 0 || stream->baseRate <= 0) {
        return;
    }
    stream->playbackRate = playbackRate;
    ma_sound_set_pitch(
        &stream->sound,
        miniaudio_backend_clamp(
            (float)playbackRate / (float)stream->baseRate,
            0.01f, 4.0f));
}

void miniaudio_set_stream_volume_pan(
    audio_stream_handle_t stream, float volume, float pan)
{
    if (stream == NULL || !stream->soundInitialized)
        return;
    stream->volume = volume;
    stream->pan = pan;
    ma_sound_set_volume(
        &stream->sound,
        miniaudio_backend_clamp(volume, 0.0f, 1.0f));
    ma_sound_set_pan(
        &stream->sound,
        miniaudio_backend_clamp(pan, 0.0f, 1.0f) * 2.0f - 1.0f);
}

void miniaudio_stream_volume_pan(
    audio_stream_handle_t stream, float *volume, float *pan)
{
    if (stream == NULL)
        return;
    if (volume != NULL)
        *volume = stream->volume;
    if (pan != NULL)
        *pan = stream->pan;
}

void miniaudio_set_stream_loop_count(
    audio_stream_handle_t stream, int32_t loopCount)
{
    if (stream != NULL && stream->soundInitialized)
        ma_sound_set_looping(
            &stream->sound, loopCount == 0 ? MA_TRUE : MA_FALSE);
}

void miniaudio_set_stream_reverb_levels(
    audio_stream_handle_t stream, float dryLevel, float wetLevel)
{
    (void)stream;
    (void)dryLevel;
    (void)wetLevel;
}

void miniaudio_stream_ms_position(
    audio_stream_handle_t stream, int32_t *totalMsec,
    int32_t *currentMsec)
{
    if (stream == NULL)
        return;
    if (totalMsec != NULL) {
        const uint64_t duration = stream->baseRate > 0
                                      ? stream->frameCount * 1000u /
                                            (uint32_t)stream->baseRate
                                      : 0;
        *totalMsec = duration <= INT32_MAX ? (int32_t)duration
                                           : INT32_MAX;
    }
    if (currentMsec != NULL) {
        ma_uint64 cursor = 0;
        if (!stream->soundInitialized ||
            ma_sound_get_cursor_in_pcm_frames(
                &stream->sound, &cursor) != MA_SUCCESS ||
            stream->baseRate <= 0) {
            *currentMsec = 0;
        } else {
            const uint64_t position =
                cursor * 1000u / (uint32_t)stream->baseRate;
            *currentMsec = position <= INT32_MAX
                               ? (int32_t)position
                               : INT32_MAX;
        }
    }
}

void miniaudio_set_stream_ms_position(
    audio_stream_handle_t stream, int32_t positionMsec)
{
    if (stream == NULL || !stream->soundInitialized ||
        stream->baseRate <= 0) {
        return;
    }
    uint64_t frame = positionMsec > 0
                         ? (uint64_t)(uint32_t)positionMsec *
                               (uint32_t)stream->baseRate / 1000u
                         : 0;
    if (frame > stream->frameCount)
        frame = stream->frameCount;
    (void)ma_sound_seek_to_pcm_frame(&stream->sound, frame);
}

void miniaudio_start_stream(audio_stream_handle_t stream)
{
    if (stream == NULL || !stream->soundInitialized)
        return;
    if (ma_sound_at_end(&stream->sound))
        (void)ma_sound_seek_to_pcm_frame(&stream->sound, 0);
    if (ma_sound_start(&stream->sound) == MA_SUCCESS)
        stream->started = qtrue;
}

void miniaudio_init_sample(audio_sample_handle_t sample)
{
    miniaudio_backend_reset_sample(sample);
}

void miniaudio_set_sample_type(
    audio_sample_handle_t sample, audio_sample_type_t sampleType,
    int32_t flags)
{
    (void)flags;
    if (sample != NULL)
        sample->sampleType = sampleType;
}

void miniaudio_set_sample_address(
    audio_sample_handle_t sample, const void *data, uint32_t dataLength)
{
    if (sample == NULL)
        return;
    sample->data = data;
    sample->dataLength = dataLength;
}

void miniaudio_set_sample_adpcm_block_size(
    audio_sample_handle_t sample, uint32_t blockSize)
{
    if (sample != NULL)
        sample->blockSize = blockSize;
}

void miniaudio_bind_loaded_sample(
    audio_sample_handle_t sample,
    const snd_alias_sound_file_t *soundFile)
{
    if (sample == NULL || soundFile == NULL)
        return;
    if (!miniaudio_3d_sample_set_info(sample->voice, soundFile)) {
        sample->soundFile = NULL;
        sample->frameCount = 0;
        return;
    }
    sample->soundFile = soundFile;
    sample->sampleRate = (int32_t)soundFile->sampleRate;
    sample->frameCount =
        miniaudio_3d_sample_frame_count(sample->voice);
    miniaudio_backend_apply_2d_gains(sample);
}

void miniaudio_forget_loaded_sound(
    const snd_alias_sound_file_t *soundFile)
{
    for (struct audio_sample_handle_s *sample =
             miniaudio_backend_samples;
         sample != NULL; sample = sample->next) {
        if (sample->soundFile == soundFile) {
            miniaudio_3d_sample_reset(sample->voice);
            sample->soundFile = NULL;
            sample->frameCount = 0;
        }
    }
    for (struct audio_3d_sample_handle_s *sample =
             miniaudio_backend_3d_samples;
         sample != NULL; sample = sample->next) {
        if (sample->soundFile == soundFile) {
            miniaudio_3d_sample_reset(sample->voice);
            sample->soundFile = NULL;
            sample->frameCount = 0;
        }
    }
}

int32_t miniaudio_sample_playback_rate(
    audio_sample_handle_t sample)
{
    return sample != NULL ? sample->playbackRate : 0;
}

void miniaudio_set_sample_playback_rate(
    audio_sample_handle_t sample, int32_t playbackRate)
{
    if (sample == NULL)
        return;
    sample->playbackRate = playbackRate;
    if (sample->sampleRate == 0)
        sample->sampleRate = playbackRate;
    miniaudio_3d_sample_set_playback_rate(
        sample->voice, playbackRate, sample->sampleRate);
}

void miniaudio_set_sample_volume_pan(
    audio_sample_handle_t sample, float volume, float pan)
{
    if (sample == NULL)
        return;
    sample->volume = volume;
    sample->pan = pan;
    miniaudio_backend_apply_2d_gains(sample);
}

void miniaudio_sample_volume_pan(
    audio_sample_handle_t sample, float *volume, float *pan)
{
    if (sample == NULL)
        return;
    if (volume != NULL)
        *volume = sample->volume;
    if (pan != NULL)
        *pan = sample->pan;
}

void miniaudio_set_sample_loop_count(
    audio_sample_handle_t sample, int32_t loopCount)
{
    if (sample != NULL)
        miniaudio_3d_sample_set_loop_count(
            sample->voice, loopCount);
}

void miniaudio_set_sample_reverb_levels(
    audio_sample_handle_t sample, float dryLevel, float wetLevel)
{
    (void)sample;
    (void)dryLevel;
    (void)wetLevel;
}

void miniaudio_sample_ms_position(
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
        const uint32_t cursor =
            miniaudio_3d_sample_cursor_frame(sample->voice);
        *currentMsec = sample->playbackRate > 0
                           ? (int32_t)((uint64_t)cursor * 1000u /
                                       (uint32_t)sample->playbackRate)
                           : 0;
    }
}

void miniaudio_set_sample_ms_position(
    audio_sample_handle_t sample, int32_t positionMsec)
{
    if (sample != NULL && sample->playbackRate > 0) {
        const uint64_t frame = positionMsec > 0
                                   ? (uint64_t)(uint32_t)positionMsec *
                                         (uint32_t)sample->playbackRate /
                                         1000u
                                   : 0;
        miniaudio_3d_sample_seek_frame(
            sample->voice,
            frame <= UINT32_MAX ? (uint32_t)frame : UINT32_MAX);
    }
}

void miniaudio_start_sample(audio_sample_handle_t sample)
{
    if (sample != NULL)
        miniaudio_3d_sample_start(sample->voice);
}

void miniaudio_release_sample_handle(
    audio_sample_handle_t sample)
{
    if (sample == NULL)
        return;
    struct audio_sample_handle_s **link =
        &miniaudio_backend_samples;
    while (*link != NULL && *link != sample)
        link = &(*link)->next;
    if (*link == sample)
        *link = sample->next;
    miniaudio_backend_reset_sample(sample);
    miniaudio_3d_sample_destroy(sample->voice);
    free(sample);
}

int32_t miniaudio_minimum_sample_buffer_size(
    audio_driver_t driver, int32_t sampleRate,
    audio_sample_type_t sampleType)
{
    (void)driver;
    return sampleRate *
           miniaudio_backend_channels(sampleType) *
           miniaudio_backend_bytes_per_sample(sampleType) / 10;
}

uint32_t miniaudio_sample_position(audio_sample_handle_t sample)
{
    if (sample == NULL || sample->frameCount == 0)
        return 0;
    const uint32_t cursor =
        miniaudio_3d_sample_cursor_frame(sample->voice);
    return (uint32_t)((uint64_t)cursor * sample->dataLength /
                      sample->frameCount);
}

int32_t miniaudio_sample_buffer_ready(
    audio_sample_handle_t sample)
{
    if (sample == NULL)
        return -1;
    if (!sample->rawMode) {
        miniaudio_3d_sample_reset(sample->voice);
        sample->rawMode = qtrue;
        sample->rawNextStartFrame =
            ma_engine_get_time_in_pcm_frames(&miniaudio_engine);
    }
    for (int32_t index = 0; index < MSS_RAW_BUFFER_COUNT; ++index) {
        if (sample->rawVoices[index] == NULL)
            return index;
        if (!miniaudio_3d_sample_is_active(
                sample->rawVoices[index])) {
            miniaudio_3d_sample_destroy(
                sample->rawVoices[index]);
            sample->rawVoices[index] = NULL;
            return index;
        }
    }
    return -1;
}

void miniaudio_load_sample_buffer(
    audio_sample_handle_t sample, int32_t bufferIndex,
    const void *data, int32_t byteCount)
{
    if (sample == NULL || bufferIndex < 0 ||
        bufferIndex >= MSS_RAW_BUFFER_COUNT || data == NULL ||
        byteCount <= 0 || sample->sampleRate <= 0)
        return;
    const int32_t channels =
        miniaudio_backend_channels(sample->sampleType);
    const int32_t bytesPerSample =
        miniaudio_backend_bytes_per_sample(sample->sampleType);
    if (channels == 0 || bytesPerSample == 0 ||
        miniaudio_backend_is_adpcm(sample->sampleType))
        return;
    miniaudio_3d_sample_destroy(
        sample->rawVoices[bufferIndex]);
    sample->rawVoices[bufferIndex] =
        miniaudio_3d_sample_create();
    if (sample->rawVoices[bufferIndex] == NULL)
        return;
    snd_alias_sound_file_t soundFile;
    memset(&soundFile, 0, sizeof(soundFile));
    soundFile.formatTag = MINIAUDIO_WAVE_FORMAT_PCM;
    soundFile.data = (void *)data;
    soundFile.dataLength = (uint32_t)byteCount;
    soundFile.sampleRate = (uint32_t)sample->sampleRate;
    soundFile.bitsPerSample = (uint16_t)(bytesPerSample * 8);
    soundFile.channelCount = (uint16_t)channels;
    soundFile.sampleCount =
        (uint32_t)byteCount / (uint32_t)bytesPerSample;
    soundFile.initialData = (void *)data;
    miniaudio_3d_sample_t *const voice =
        sample->rawVoices[bufferIndex];
    if (!miniaudio_3d_sample_set_info(voice, &soundFile)) {
        miniaudio_3d_sample_destroy(voice);
        sample->rawVoices[bufferIndex] = NULL;
        return;
    }
    voice->soundFile = NULL;
    miniaudio_backend_apply_2d_gains(sample);
    const ma_uint64 now =
        ma_engine_get_time_in_pcm_frames(&miniaudio_engine);
    if (sample->rawNextStartFrame < now)
        sample->rawNextStartFrame = now;
    ma_sound_set_start_time_in_pcm_frames(
        &voice->sound, sample->rawNextStartFrame);
    const uint32_t sourceFrames =
        (uint32_t)byteCount /
        ((uint32_t)channels * (uint32_t)bytesPerSample);
    const uint32_t engineRate =
        ma_engine_get_sample_rate(&miniaudio_engine);
    sample->rawNextStartFrame +=
        (uint64_t)sourceFrames * engineRate /
        (uint32_t)sample->sampleRate;
    miniaudio_3d_sample_start(voice);
}

void miniaudio_set_3D_sample_info(
    audio_3d_sample_handle_t sample,
    const snd_alias_sound_file_t *soundFile)
{
    if (sample == NULL || soundFile == NULL)
        return;
    if (!miniaudio_3d_sample_set_info(
            sample->voice, soundFile)) {
        sample->soundFile = NULL;
        sample->frameCount = 0;
        return;
    }
    sample->soundFile = soundFile;
    sample->playbackRate = (int32_t)soundFile->sampleRate;
    sample->frameCount =
        miniaudio_3d_sample_frame_count(sample->voice);
    miniaudio_backend_apply_3d_gains(sample);
}

void miniaudio_set_3D_sample_volume(
    audio_3d_sample_handle_t sample, float volume)
{
    if (sample == NULL)
        return;
    sample->volume = volume;
    miniaudio_backend_apply_3d_gains(sample);
}

void miniaudio_set_3D_sample_distances(
    audio_3d_sample_handle_t sample, float maximumDistance,
    float minimumDistance)
{
    (void)sample;
    (void)maximumDistance;
    (void)minimumDistance;
}

int32_t miniaudio_3D_sample_playback_rate(
    audio_3d_sample_handle_t sample)
{
    return sample != NULL ? sample->playbackRate : 0;
}

uint32_t miniaudio_3D_sample_offset(
    audio_3d_sample_handle_t sample)
{
    if (sample == NULL || sample->soundFile == NULL ||
        sample->frameCount == 0)
        return 0;
    const uint32_t cursor =
        miniaudio_3d_sample_cursor_frame(sample->voice);
    return (uint32_t)((uint64_t)cursor *
                      sample->soundFile->dataLength /
                      sample->frameCount);
}

uint32_t miniaudio_3D_sample_length(
    audio_3d_sample_handle_t sample)
{
    return sample != NULL && sample->soundFile != NULL
               ? sample->soundFile->dataLength
               : 0;
}

float miniaudio_3D_sample_volume(
    audio_3d_sample_handle_t sample)
{
    return sample != NULL ? sample->volume : 0.0f;
}

void miniaudio_3D_position(
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

void miniaudio_set_3D_sample_playback_rate(
    audio_3d_sample_handle_t sample, int32_t playbackRate)
{
    if (sample == NULL)
        return;
    sample->playbackRate = playbackRate;
    if (sample->soundFile != NULL) {
        miniaudio_3d_sample_set_playback_rate(
            sample->voice, playbackRate,
            (int32_t)sample->soundFile->sampleRate);
    }
}

void miniaudio_set_3D_sample_loop_count(
    audio_3d_sample_handle_t sample, int32_t loopCount)
{
    if (sample != NULL)
        miniaudio_3d_sample_set_loop_count(
            sample->voice, loopCount);
}

void miniaudio_set_3D_sample_effects_level(
    audio_3d_sample_handle_t sample, float effectsLevel)
{
    (void)sample;
    (void)effectsLevel;
}

int32_t miniaudio_set_3D_sample_preference(
    audio_3d_sample_handle_t sample, const char *preferenceName,
    void *value)
{
    (void)sample;
    (void)preferenceName;
    (void)value;
    return 0;
}

void miniaudio_set_digital_master_room_type(
    audio_driver_t driver, int32_t roomType)
{
    (void)driver;
    (void)roomType;
}

void miniaudio_set_3D_room_type(
    audio_provider_t provider, int32_t roomType)
{
    (void)provider;
    (void)roomType;
}

void miniaudio_set_3D_sample_offset(
    audio_3d_sample_handle_t sample, int32_t byteOffset)
{
    if (sample == NULL || sample->soundFile == NULL ||
        sample->soundFile->dataLength == 0)
        return;
    const float fraction = miniaudio_backend_clamp(
        (float)byteOffset / (float)sample->soundFile->dataLength,
        0.0f, 1.0f);
    miniaudio_3d_sample_seek_frame(
        sample->voice,
        (uint32_t)(fraction * (float)sample->frameCount));
}

void miniaudio_start_3D_sample(
    audio_3d_sample_handle_t sample)
{
    if (sample != NULL)
        miniaudio_3d_sample_start(sample->voice);
}

const audio_backend_api_t miniaudio_backend = {
    .name = AUDIO_BACKEND_MINIAUDIO_NAME,
    .api_bind_loaded_sample = miniaudio_bind_loaded_sample,
    .api_forget_loaded_sound = miniaudio_forget_loaded_sound,
    .api_set_preference = miniaudio_set_preference,
    .api_set_file_callbacks = miniaudio_set_file_callbacks,
    .api_set_redist_directory = miniaudio_set_redist_directory,
    .api_startup = miniaudio_startup,
    .api_shutdown = miniaudio_shutdown,
    .api_close_3D_provider = miniaudio_close_3D_provider,
    .api_set_DirectSound_HWND = miniaudio_set_DirectSound_HWND,
    .api_WAV_info = miniaudio_WAV_info,
    .api_size_processed_digital_audio = miniaudio_size_processed_digital_audio,
    .api_process_digital_audio = miniaudio_process_digital_audio,
    .api_digital_CPU_percent = miniaudio_digital_CPU_percent,
    .api_open_digital_driver = miniaudio_open_digital_driver,
    .api_last_error = miniaudio_last_error,
    .api_enumerate_3D_providers = miniaudio_enumerate_3D_providers,
    .api_open_3D_provider = miniaudio_open_3D_provider,
    .api_3D_provider_attribute = miniaudio_3D_provider_attribute,
    .api_set_3D_provider_preference = miniaudio_set_3D_provider_preference,
    .api_set_3D_distance_factor = miniaudio_set_3D_distance_factor,
    .api_allocate_sample_handle = miniaudio_allocate_sample_handle,
    .api_allocate_3D_sample_handle = miniaudio_allocate_3D_sample_handle,
    .api_set_3D_position = miniaudio_set_3D_position,
    .api_end_sample = miniaudio_end_sample,
    .api_stop_sample = miniaudio_stop_sample,
    .api_resume_sample = miniaudio_resume_sample,
    .api_sample_status = miniaudio_sample_status,
    .api_end_3D_sample = miniaudio_end_3D_sample,
    .api_stop_3D_sample = miniaudio_stop_3D_sample,
    .api_resume_3D_sample = miniaudio_resume_3D_sample,
    .api_3D_sample_status = miniaudio_3D_sample_status,
    .api_open_stream = miniaudio_open_stream,
    .api_close_stream = miniaudio_close_stream,
    .api_pause_stream = miniaudio_pause_stream,
    .api_stream_status = miniaudio_stream_status,
    .api_stream_playback_rate = miniaudio_stream_playback_rate,
    .api_set_stream_playback_rate = miniaudio_set_stream_playback_rate,
    .api_set_stream_volume_pan = miniaudio_set_stream_volume_pan,
    .api_stream_volume_pan = miniaudio_stream_volume_pan,
    .api_set_stream_loop_count = miniaudio_set_stream_loop_count,
    .api_set_stream_reverb_levels = miniaudio_set_stream_reverb_levels,
    .api_stream_ms_position = miniaudio_stream_ms_position,
    .api_set_stream_ms_position = miniaudio_set_stream_ms_position,
    .api_start_stream = miniaudio_start_stream,
    .api_init_sample = miniaudio_init_sample,
    .api_set_sample_type = miniaudio_set_sample_type,
    .api_set_sample_address = miniaudio_set_sample_address,
    .api_set_sample_adpcm_block_size = miniaudio_set_sample_adpcm_block_size,
    .api_sample_playback_rate = miniaudio_sample_playback_rate,
    .api_set_sample_playback_rate = miniaudio_set_sample_playback_rate,
    .api_set_sample_volume_pan = miniaudio_set_sample_volume_pan,
    .api_sample_volume_pan = miniaudio_sample_volume_pan,
    .api_set_sample_loop_count = miniaudio_set_sample_loop_count,
    .api_set_sample_reverb_levels = miniaudio_set_sample_reverb_levels,
    .api_sample_ms_position = miniaudio_sample_ms_position,
    .api_set_sample_ms_position = miniaudio_set_sample_ms_position,
    .api_start_sample = miniaudio_start_sample,
    .api_release_sample_handle = miniaudio_release_sample_handle,
    .api_minimum_sample_buffer_size = miniaudio_minimum_sample_buffer_size,
    .api_sample_position = miniaudio_sample_position,
    .api_sample_buffer_ready = miniaudio_sample_buffer_ready,
    .api_load_sample_buffer = miniaudio_load_sample_buffer,
    .api_set_3D_sample_info = miniaudio_set_3D_sample_info,
    .api_set_3D_sample_volume = miniaudio_set_3D_sample_volume,
    .api_set_3D_sample_distances = miniaudio_set_3D_sample_distances,
    .api_3D_sample_playback_rate = miniaudio_3D_sample_playback_rate,
    .api_3D_sample_offset = miniaudio_3D_sample_offset,
    .api_3D_sample_length = miniaudio_3D_sample_length,
    .api_3D_sample_volume = miniaudio_3D_sample_volume,
    .api_3D_position = miniaudio_3D_position,
    .api_set_3D_sample_playback_rate = miniaudio_set_3D_sample_playback_rate,
    .api_set_3D_sample_loop_count = miniaudio_set_3D_sample_loop_count,
    .api_set_3D_sample_effects_level = miniaudio_set_3D_sample_effects_level,
    .api_set_3D_sample_preference = miniaudio_set_3D_sample_preference,
    .api_set_digital_master_room_type = miniaudio_set_digital_master_room_type,
    .api_set_3D_room_type = miniaudio_set_3D_room_type,
    .api_set_3D_sample_offset = miniaudio_set_3D_sample_offset,
    .api_start_3D_sample = miniaudio_start_3D_sample,
};

#endif
