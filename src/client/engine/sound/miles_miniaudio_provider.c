#include "miles_miniaudio_provider.h"

#if !defined(CODUOMP_DISABLE_AUDIO) && \
    (defined(__APPLE__) || defined(__linux__))

/* NOT_FROM_ORIGINAL_SOURCE: Miniaudio is compiled into the client. The
 * provider consumes engine-owned PCM/IMA alias payloads directly, so its
 * optional file decoders are not part of this adapter. */
#define MA_NO_FLAC
#define MA_NO_MP3
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

enum {
    CODUOMP_MINIAUDIO_WAVE_FORMAT_PCM = 1,
    CODUOMP_MINIAUDIO_WAVE_FORMAT_IMA_ADPCM = 17,
    CODUOMP_MINIAUDIO_IMA_HEADER_BYTES = 4
};

typedef struct coduomp_miniaudio_pan_node_s {
    ma_node_base base;
    ma_atomic_float leftGain;
    ma_atomic_float rightGain;
} coduomp_miniaudio_pan_node_t;

struct coduomp_miniaudio_3d_sample_s {
    ma_audio_buffer buffer;
    coduomp_miniaudio_pan_node_t panNode;
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

static ma_engine coduomp_miniaudio_engine;
static qboolean coduomp_miniaudio_engine_initialized;
static char coduomp_miniaudio_error[256] = "no Miniaudio error";

/* NOT_FROM_ORIGINAL_SOURCE: publish a stable error string through the Miles
 * compatibility boundary. */
static void coduomp_miniaudio_set_error(const char *operation,
                                        ma_result result)
{
    (void)snprintf(coduomp_miniaudio_error,
                   sizeof(coduomp_miniaudio_error), "%s: %s",
                   operation, ma_result_description(result));
}

static uint16_t coduomp_miniaudio_read_u16(const uint8_t *bytes)
{
    return (uint16_t)((uint16_t)bytes[0] |
                      ((uint16_t)bytes[1] << 8));
}

/* NOT_FROM_ORIGINAL_SOURCE: decode the mono Microsoft IMA blocks already
 * accepted by the recovered Miles load path. Fast 2D itself accepts only
 * mono input, matching msssoft.m3d's sample setup. */
static int16_t coduomp_miniaudio_decode_ima_nibble(
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

static qboolean coduomp_miniaudio_decode_ima(
    const snd_alias_sound_file_t *soundFile, void **outPcm,
    uint32_t *outFrameCount)
{
    const uint32_t blockSize = soundFile->blockSize;
    if (soundFile->data == NULL || soundFile->dataLength == 0 ||
        blockSize < CODUOMP_MINIAUDIO_IMA_HEADER_BYTES)
        return qfalse;

    const uint32_t framesPerBlock =
        1u + (blockSize - CODUOMP_MINIAUDIO_IMA_HEADER_BYTES) * 2u;
    const uint32_t blockCount =
        (soundFile->dataLength + blockSize - 1u) / blockSize;
    const uint64_t capacityFrames =
        (uint64_t)framesPerBlock * blockCount;
    if (capacityFrames == 0 ||
        capacityFrames > (uint64_t)UINT32_MAX / sizeof(int16_t))
        return qfalse;

    int16_t *const output =
        malloc((size_t)capacityFrames * sizeof(*output));
    if (output == NULL)
        return qfalse;

    const uint8_t *const input = soundFile->data;
    uint32_t inputOffset = 0;
    uint32_t outputFrames = 0;
    while (inputOffset + CODUOMP_MINIAUDIO_IMA_HEADER_BYTES <=
           soundFile->dataLength) {
        uint32_t currentBlockSize = soundFile->dataLength - inputOffset;
        if (currentBlockSize > blockSize)
            currentBlockSize = blockSize;
        const uint8_t *const block = input + inputOffset;
        int32_t predictor = (int16_t)coduomp_miniaudio_read_u16(block);
        int32_t stepIndex = block[2];
        if (stepIndex > 88)
            stepIndex = 88;
        output[outputFrames++] = (int16_t)predictor;
        for (uint32_t offset = CODUOMP_MINIAUDIO_IMA_HEADER_BYTES;
             offset < currentBlockSize; ++offset) {
            const uint8_t packed = block[offset];
            if (outputFrames < capacityFrames) {
                output[outputFrames++] =
                    coduomp_miniaudio_decode_ima_nibble(
                        packed & 15u, &predictor, &stepIndex);
            }
            if (outputFrames < capacityFrames) {
                output[outputFrames++] =
                    coduomp_miniaudio_decode_ima_nibble(
                        packed >> 4, &predictor, &stepIndex);
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
static void coduomp_miniaudio_pan_process(
    ma_node *node, const float **inputFrames, ma_uint32 *inputFrameCount,
    float **outputFrames, ma_uint32 *outputFrameCount)
{
    coduomp_miniaudio_pan_node_t *const pan =
        (coduomp_miniaudio_pan_node_t *)node;
    ma_uint32 frameCount = *inputFrameCount;
    if (frameCount > *outputFrameCount)
        frameCount = *outputFrameCount;
    const float left = ma_atomic_float_get(&pan->leftGain);
    const float right = ma_atomic_float_get(&pan->rightGain);
    const float *const input = inputFrames[0];
    float *const output = outputFrames[0];
    for (ma_uint32 frame = 0; frame < frameCount; ++frame) {
        output[frame * 2u] = input[frame] * left;
        output[frame * 2u + 1u] = input[frame] * right;
    }
    *inputFrameCount = frameCount;
    *outputFrameCount = frameCount;
}

static ma_node_vtable coduomp_miniaudio_pan_vtable = {
    coduomp_miniaudio_pan_process,
    NULL,
    1,
    1,
    0
};

/* NOT_FROM_ORIGINAL_SOURCE: tear down one reusable provider voice without
 * releasing the small outer handle allocated for the engine channel. */
static void coduomp_miniaudio_3d_sample_reset(
    coduomp_miniaudio_3d_sample_t *sample)
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

qboolean coduomp_miniaudio_provider_init(int32_t sampleRate)
{
    if (coduomp_miniaudio_engine_initialized)
        return qtrue;
    ma_engine_config config = ma_engine_config_init();
    config.channels = 2;
    if (sampleRate > 0)
        config.sampleRate = (ma_uint32)sampleRate;
    config.defaultVolumeSmoothTimeInPCMFrames = 0;
    const ma_result result =
        ma_engine_init(&config, &coduomp_miniaudio_engine);
    if (result != MA_SUCCESS) {
        coduomp_miniaudio_set_error("could not open Miniaudio device",
                                    result);
        return qfalse;
    }
    coduomp_miniaudio_engine_initialized = qtrue;
    (void)snprintf(coduomp_miniaudio_error,
                   sizeof(coduomp_miniaudio_error),
                   "%s", "no Miniaudio error");
    return qtrue;
}

void coduomp_miniaudio_provider_shutdown(void)
{
    if (!coduomp_miniaudio_engine_initialized)
        return;
    ma_engine_uninit(&coduomp_miniaudio_engine);
    memset(&coduomp_miniaudio_engine, 0,
           sizeof(coduomp_miniaudio_engine));
    coduomp_miniaudio_engine_initialized = qfalse;
}

const char *coduomp_miniaudio_provider_last_error(void)
{
    return coduomp_miniaudio_error;
}

coduomp_miniaudio_3d_sample_t *coduomp_miniaudio_3d_sample_create(void)
{
    if (!coduomp_miniaudio_engine_initialized)
        return NULL;
    coduomp_miniaudio_3d_sample_t *const sample =
        calloc(1, sizeof(*sample));
    if (sample != NULL) {
        sample->leftGain = 0.5f;
        sample->rightGain = 0.5f;
    }
    return sample;
}

void coduomp_miniaudio_3d_sample_destroy(
    coduomp_miniaudio_3d_sample_t *sample)
{
    if (sample == NULL)
        return;
    coduomp_miniaudio_3d_sample_reset(sample);
    free(sample);
}

qboolean coduomp_miniaudio_3d_sample_set_info(
    coduomp_miniaudio_3d_sample_t *sample,
    const snd_alias_sound_file_t *soundFile)
{
    if (sample == NULL || soundFile == NULL ||
        !coduomp_miniaudio_engine_initialized ||
        soundFile->channelCount != 1 || soundFile->sampleRate == 0 ||
        soundFile->data == NULL || soundFile->dataLength == 0)
        return qfalse;

    coduomp_miniaudio_3d_sample_reset(sample);
    ma_format format;
    const void *pcmData = soundFile->data;
    if (soundFile->formatTag == CODUOMP_MINIAUDIO_WAVE_FORMAT_PCM &&
        soundFile->bitsPerSample == 8) {
        format = ma_format_u8;
        sample->frameCount = soundFile->dataLength;
    } else if (
        soundFile->formatTag == CODUOMP_MINIAUDIO_WAVE_FORMAT_PCM &&
        soundFile->bitsPerSample == 16 &&
        soundFile->dataLength % sizeof(int16_t) == 0) {
        format = ma_format_s16;
        sample->frameCount =
            soundFile->dataLength / (uint32_t)sizeof(int16_t);
    } else if (
        soundFile->formatTag ==
            CODUOMP_MINIAUDIO_WAVE_FORMAT_IMA_ADPCM) {
        format = ma_format_s16;
        if (!coduomp_miniaudio_decode_ima(
                soundFile, &sample->ownedPcm, &sample->frameCount)) {
            (void)snprintf(coduomp_miniaudio_error,
                           sizeof(coduomp_miniaudio_error), "%s",
                           "could not decode Miniaudio 3D sample");
            return qfalse;
        }
        pcmData = sample->ownedPcm;
    } else {
        (void)snprintf(coduomp_miniaudio_error,
                       sizeof(coduomp_miniaudio_error), "%s",
                       "unsupported Miniaudio 3D sample format");
        return qfalse;
    }

    ma_audio_buffer_config bufferConfig = ma_audio_buffer_config_init(
        format, 1, sample->frameCount, pcmData, NULL);
    bufferConfig.sampleRate = soundFile->sampleRate;
    ma_result result = ma_audio_buffer_init(
        &bufferConfig, &sample->buffer);
    if (result != MA_SUCCESS) {
        coduomp_miniaudio_set_error(
            "could not initialize Miniaudio sample buffer", result);
        coduomp_miniaudio_3d_sample_reset(sample);
        return qfalse;
    }
    sample->bufferInitialized = qtrue;

    const ma_uint32 inputChannels[1] = {1};
    const ma_uint32 outputChannels[1] = {2};
    ma_node_config nodeConfig = ma_node_config_init();
    nodeConfig.vtable = &coduomp_miniaudio_pan_vtable;
    nodeConfig.pInputChannels = inputChannels;
    nodeConfig.pOutputChannels = outputChannels;
    result = ma_node_init(
        ma_engine_get_node_graph(&coduomp_miniaudio_engine),
        &nodeConfig, NULL, (ma_node *)&sample->panNode);
    if (result != MA_SUCCESS) {
        coduomp_miniaudio_set_error(
            "could not initialize Miniaudio Fast 2D node", result);
        coduomp_miniaudio_3d_sample_reset(sample);
        return qfalse;
    }
    sample->nodeInitialized = qtrue;
    ma_atomic_float_set(&sample->panNode.leftGain, sample->leftGain);
    ma_atomic_float_set(&sample->panNode.rightGain, sample->rightGain);
    result = ma_node_attach_output_bus(
        (ma_node *)&sample->panNode, 0,
        ma_engine_get_endpoint(&coduomp_miniaudio_engine), 0);
    if (result != MA_SUCCESS) {
        coduomp_miniaudio_set_error(
            "could not attach Miniaudio Fast 2D node", result);
        coduomp_miniaudio_3d_sample_reset(sample);
        return qfalse;
    }

    ma_sound_config soundConfig =
        ma_sound_config_init_2(&coduomp_miniaudio_engine);
    soundConfig.pDataSource = (ma_data_source *)&sample->buffer;
    soundConfig.pInitialAttachment = (ma_node *)&sample->panNode;
    soundConfig.channelsOut = MA_SOUND_SOURCE_CHANNEL_COUNT;
    soundConfig.flags = MA_SOUND_FLAG_NO_SPATIALIZATION;
    result = ma_sound_init_ex(
        &coduomp_miniaudio_engine, &soundConfig, &sample->sound);
    if (result != MA_SUCCESS) {
        coduomp_miniaudio_set_error(
            "could not initialize Miniaudio 3D voice", result);
        coduomp_miniaudio_3d_sample_reset(sample);
        return qfalse;
    }
    sample->soundInitialized = qtrue;
    sample->soundFile = soundFile;
    return qtrue;
}

void coduomp_miniaudio_3d_sample_forget_sound(
    coduomp_miniaudio_3d_sample_t *sample,
    const snd_alias_sound_file_t *soundFile)
{
    if (sample != NULL && sample->soundFile == soundFile)
        coduomp_miniaudio_3d_sample_reset(sample);
}

void coduomp_miniaudio_3d_sample_set_channel_gains(
    coduomp_miniaudio_3d_sample_t *sample, float left, float right)
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

void coduomp_miniaudio_3d_sample_set_playback_rate(
    coduomp_miniaudio_3d_sample_t *sample, int32_t playbackRate,
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

void coduomp_miniaudio_3d_sample_set_loop_count(
    coduomp_miniaudio_3d_sample_t *sample, int32_t loopCount)
{
    if (sample != NULL && sample->soundInitialized)
        ma_sound_set_looping(&sample->sound,
                             loopCount == 0 ? MA_TRUE : MA_FALSE);
}

void coduomp_miniaudio_3d_sample_end(
    coduomp_miniaudio_3d_sample_t *sample)
{
    if (sample == NULL || !sample->soundInitialized)
        return;
    (void)ma_sound_stop(&sample->sound);
    (void)ma_sound_seek_to_pcm_frame(&sample->sound, 0);
    sample->active = qfalse;
}

void coduomp_miniaudio_3d_sample_stop(
    coduomp_miniaudio_3d_sample_t *sample)
{
    if (sample != NULL && sample->soundInitialized)
        (void)ma_sound_stop(&sample->sound);
}

void coduomp_miniaudio_3d_sample_resume(
    coduomp_miniaudio_3d_sample_t *sample)
{
    if (sample != NULL && sample->soundInitialized && sample->active)
        (void)ma_sound_start(&sample->sound);
}

void coduomp_miniaudio_3d_sample_start(
    coduomp_miniaudio_3d_sample_t *sample)
{
    if (sample == NULL || !sample->soundInitialized)
        return;
    if (ma_sound_at_end(&sample->sound))
        (void)ma_sound_seek_to_pcm_frame(&sample->sound, 0);
    if (ma_sound_start(&sample->sound) == MA_SUCCESS)
        sample->active = qtrue;
}

qboolean coduomp_miniaudio_3d_sample_is_active(
    coduomp_miniaudio_3d_sample_t *sample)
{
    return sample != NULL && sample->soundInitialized && sample->active &&
                   !ma_sound_at_end(&sample->sound)
               ? qtrue
               : qfalse;
}

uint32_t coduomp_miniaudio_3d_sample_frame_count(
    const coduomp_miniaudio_3d_sample_t *sample)
{
    return sample != NULL ? sample->frameCount : 0;
}

uint32_t coduomp_miniaudio_3d_sample_cursor_frame(
    const coduomp_miniaudio_3d_sample_t *sample)
{
    if (sample == NULL || !sample->soundInitialized)
        return 0;
    ma_uint64 cursor = 0;
    if (ma_sound_get_cursor_in_pcm_frames(&sample->sound, &cursor) !=
        MA_SUCCESS)
        return 0;
    return cursor <= UINT32_MAX ? (uint32_t)cursor : UINT32_MAX;
}

void coduomp_miniaudio_3d_sample_seek_frame(
    coduomp_miniaudio_3d_sample_t *sample, uint32_t frame)
{
    if (sample == NULL || !sample->soundInitialized)
        return;
    if (frame > sample->frameCount)
        frame = sample->frameCount;
    (void)ma_sound_seek_to_pcm_frame(&sample->sound, frame);
}

#endif
