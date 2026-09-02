#include "xanim_eval.h"

#include "animation_private.h"
#include "xanim_asset_load.h"

#include <math.h>
#include <string.h>

#define XANIM_PACKED_SHORT_SCALE 3.0518509447574615e-05f

enum {
    XANIM_EVAL_TREE_ROOT_NODE = 0,
    XANIM_EVAL_SKIP_LAST_PART_BIT = 0x80
};

/* NOT_FROM_ORIGINAL_SOURCE: named lanes of the six-float carrier in the
 * retail XAnimCalcDelta* signatures.  Keeping the carrier an actual float
 * array preserves the canonical ABI without aliasing it through a struct. */
enum {
    XANIM_DELTA_ROTATION_0 = 0,
    XANIM_DELTA_ROTATION_1,
    XANIM_DELTA_TRANSLATION_WEIGHT,
    XANIM_DELTA_TRANSLATION_0,
    XANIM_DELTA_TRANSLATION_1,
    XANIM_DELTA_TRANSLATION_2,
    XANIM_DELTA_LANE_COUNT
};

typedef float xanim_delta_t[XANIM_DELTA_LANE_COUNT];

#if defined(WINDOWS_BEHAVIOR)
typedef long double xanim_eval_fraction_t;
#else
typedef float xanim_eval_fraction_t;
#endif

/* NOT_FROM_ORIGINAL_SOURCE: the original evaluator repeats these arithmetic
 * shapes at each lane.  These helpers preserve the target-specific x87 store
 * points while keeping the shared control flow readable. */
static float xanim_eval_madd_store(float destination, float left, float right)
{
#if defined(WINDOWS_BEHAVIOR)
    return (float)((long double)destination + (long double)left * (long double)right);
#elif EMULATE_X87
    return x87f_store_f32(x87f_add(x87f_load_f32(destination), x87f_mul(x87f_load_f32(left), x87f_load_f32(right))));
#else
    return destination + left * right;
#endif
}

static float xanim_eval_mul_store(float left, float right)
{
#if defined(WINDOWS_BEHAVIOR)
    return (float)((long double)left * (long double)right);
#elif EMULATE_X87
    return x87f_store_f32(x87f_mul(x87f_load_f32(left), x87f_load_f32(right)));
#else
    return left * right;
#endif
}

static float xanim_eval_sub_store(float left, float right)
{
#if defined(WINDOWS_BEHAVIOR)
    return (float)((long double)left - (long double)right);
#elif EMULATE_X87
    return x87f_store_f32(x87f_sub(x87f_load_f32(left), x87f_load_f32(right)));
#else
    return left - right;
#endif
}

static float xanim_eval_add_store(float left, float right)
{
#if defined(WINDOWS_BEHAVIOR)
    return (float)((long double)left + (long double)right);
#elif EMULATE_X87
    return x87f_store_f32(x87f_add(x87f_load_f32(left), x87f_load_f32(right)));
#else
    return left + right;
#endif
}

#if !defined(WINDOWS_BEHAVIOR)
static float xanim_eval_div_store(float numerator, float denominator)
{
#if EMULATE_X87
    return x87f_store_f32(x87f_div(x87f_load_f32(numerator), x87f_load_f32(denominator)));
#else
    return numerator / denominator;
#endif
}

static float xanim_eval_sumsq2_store(float lane0, float lane1)
{
#if EMULATE_X87
    return x87f_store_f32(
        x87f_add(x87f_mul(x87f_load_f32(lane0), x87f_load_f32(lane0)), x87f_mul(x87f_load_f32(lane1), x87f_load_f32(lane1))));
#else
    return lane0 * lane0 + lane1 * lane1;
#endif
}

static float xanim_eval_sumsq4_store(float lane0, float lane1, float lane2, float lane3)
{
#if EMULATE_X87
    return x87f_store_f32(x87f_add(
        x87f_add(x87f_add(x87f_mul(x87f_load_f32(lane0), x87f_load_f32(lane0)), x87f_mul(x87f_load_f32(lane1), x87f_load_f32(lane1))),
                 x87f_mul(x87f_load_f32(lane2), x87f_load_f32(lane2))),
        x87f_mul(x87f_load_f32(lane3), x87f_load_f32(lane3))));
#else
    return ((lane0 * lane0 + lane1 * lane1) + lane2 * lane2) + lane3 * lane3;
#endif
}

static float xanim_eval_mul3_store(float first, float second, float third)
{
#if EMULATE_X87
    return x87f_store_f32(x87f_mul(x87f_mul(x87f_load_f32(first), x87f_load_f32(second)), x87f_load_f32(third)));
#else
    return first * second * third;
#endif
}

static float xanim_eval_sub_two_products_store(float destination, float left0, float right0, float left1, float right1)
{
#if EMULATE_X87
    return x87f_store_f32(x87f_sub(x87f_load_f32(destination), x87f_add(x87f_mul(x87f_load_f32(left0), x87f_load_f32(right0)),
                                                                        x87f_mul(x87f_load_f32(left1), x87f_load_f32(right1)))));
#else
    return destination - (left0 * right0 + left1 * right1);
#endif
}

static float xanim_eval_reflection_lane0_store(float reflection01, float oldLane1, float reflection00, float oldLane0)
{
#if EMULATE_X87
    return x87f_store_f32(x87f_add(x87f_mul(x87f_load_f32(reflection01), x87f_load_f32(oldLane1)),
                                   x87f_mul(x87f_sub(x87f_load_f32(1.0f), x87f_load_f32(reflection00)), x87f_load_f32(oldLane0))));
#else
    return reflection01 * oldLane1 + (1.0f - reflection00) * oldLane0;
#endif
}

static float xanim_eval_rotation_cross_store(float destination, const vec2_t endRotation, const vec2_t startRotation, float scaledWeight)
{
#if EMULATE_X87
    return x87f_store_f32(
        x87f_add(x87f_load_f32(destination), x87f_mul(x87f_sub(x87f_mul(x87f_load_f32(endRotation[0]), x87f_load_f32(startRotation[1])),
                                                               x87f_mul(x87f_load_f32(endRotation[1]), x87f_load_f32(startRotation[0]))),
                                                      x87f_load_f32(scaledWeight))));
#else
    return destination + (endRotation[0] * startRotation[1] - endRotation[1] * startRotation[0]) * scaledWeight;
#endif
}

static float xanim_eval_rotation_dot_store(float destination, const vec2_t endRotation, const vec2_t startRotation, float scaledWeight)
{
#if EMULATE_X87
    return x87f_store_f32(
        x87f_add(x87f_load_f32(destination), x87f_mul(x87f_add(x87f_mul(x87f_load_f32(endRotation[1]), x87f_load_f32(startRotation[1])),
                                                               x87f_mul(x87f_load_f32(endRotation[0]), x87f_load_f32(startRotation[0]))),
                                                      x87f_load_f32(scaledWeight))));
#else
    return destination + (endRotation[1] * startRotation[1] + endRotation[0] * startRotation[0]) * scaledWeight;
#endif
}
#endif

static float xanim_eval_frame_time(int32_t frameCount, float time, int32_t *roundedFrame)
{
#if defined(WINDOWS_BEHAVIOR)
    const long double raw = (long double)frameCount * (long double)time;

    *roundedFrame = (int32_t)raw;
    return (float)raw;
#elif EMULATE_X87
    const x87f raw = x87f_mul(x87f_load_i32(frameCount), x87f_load_f32(time));
    const float stored = x87f_store_f32(raw);

    *roundedFrame = x87f_store_i32_trunc(x87f_load_f32(stored));
    return stored;
#else
    const float stored = time * (float)frameCount;

    *roundedFrame = (int32_t)stored;
    return stored;
#endif
}

static float xanim_eval_lerp_i32(int32_t first, int32_t second, xanim_eval_fraction_t fraction)
{
#if defined(WINDOWS_BEHAVIOR)
    return (float)((long double)(second - first) * fraction + (long double)first);
#elif EMULATE_X87
    return x87f_store_f32(x87f_add(x87f_mul(x87f_load_i32(second - first), x87f_load_f32(fraction)), x87f_load_i32(first)));
#else
    return (float)(second - first) * fraction + (float)first;
#endif
}

static float xanim_eval_lerp_f32(float first, float second, xanim_eval_fraction_t fraction)
{
#if defined(WINDOWS_BEHAVIOR)
    return (float)(((long double)second - (long double)first) * fraction + (long double)first);
#elif EMULATE_X87
    return x87f_store_f32(
        x87f_add(x87f_mul(x87f_sub(x87f_load_f32(second), x87f_load_f32(first)), x87f_load_f32(fraction)), x87f_load_f32(first)));
#else
    return (second - first) * fraction + first;
#endif
}

static float xanim_eval_accumulate_lerp_i32(float destination, int32_t first, int32_t second, xanim_eval_fraction_t fraction, float weight)
{
#if defined(WINDOWS_BEHAVIOR)
    return (float)((long double)destination + ((long double)(second - first) * fraction + (long double)first) * (long double)weight);
#elif EMULATE_X87
    const x87f interpolated = x87f_add(x87f_mul(x87f_load_i32(second - first), x87f_load_f32(fraction)), x87f_load_i32(first));

    return x87f_store_f32(x87f_add(x87f_load_f32(destination), x87f_mul(interpolated, x87f_load_f32(weight))));
#else
    return destination + ((float)(second - first) * fraction + (float)first) * weight;
#endif
}

static float xanim_eval_accumulate_lerp_f32(float destination, float first, float second, xanim_eval_fraction_t fraction, float weight)
{
#if defined(WINDOWS_BEHAVIOR)
    return (float)((long double)destination +
                   (((long double)second - (long double)first) * fraction + (long double)first) * (long double)weight);
#elif EMULATE_X87
    const x87f interpolated =
        x87f_add(x87f_mul(x87f_sub(x87f_load_f32(second), x87f_load_f32(first)), x87f_load_f32(fraction)), x87f_load_f32(first));

    return x87f_store_f32(x87f_add(x87f_load_f32(destination), x87f_mul(interpolated, x87f_load_f32(weight))));
#else
    return destination + ((second - first) * fraction + first) * weight;
#endif
}

/* NOT_FROM_ORIGINAL_SOURCE: source-level factoring of the terminal primary
 * sample path in XAnimCalcDeltaParts. */
static void xanim_eval_sample_optional_primary_last_frame(const xanim_rotation_stream_t *primary, vec2_t rotation)
{
    if (primary == NULL) {
        rotation[0] = 0.0f;
        rotation[1] = 32767.0f;
    } else if (primary->frameIndex == 0) {
        rotation[0] = (float)primary->data.inlinePrefix.lane0;
        rotation[1] = (float)primary->data.inlinePrefix.lane1;
    } else {
        const xanim_int16_vec2_t *frame = &primary->data.frames2[primary->frameIndex];
        rotation[0] = (float)frame->components[0];
        rotation[1] = (float)frame->components[1];
    }
}

/* NOT_FROM_ORIGINAL_SOURCE: source-level factoring of the terminal secondary
 * sample path in XAnimCalcDeltaParts. */
static void xanim_eval_sample_optional_secondary_last_frame(const xanim_translation_stream_t *secondary, vec3_t move)
{
    if (secondary == NULL) {
        move[0] = 0.0f;
        move[1] = 0.0f;
        move[2] = 0.0f;
    } else if (secondary->frameIndex == 0) {
        move[0] = secondary->data.inlineLane0;
        move[1] = secondary->inlineLanes.lane1;
        move[2] = secondary->inlineLanes.lane2;
    } else {
        const float *frame = secondary->data.frames[secondary->frameIndex];
        move[0] = frame[0];
        move[1] = frame[1];
        move[2] = frame[2];
    }
}

/* NOT_FROM_ORIGINAL_SOURCE: source-level factoring of byte-key interval
 * selection and interpolation in XAnimCalcDeltaParts. */
static xanim_eval_fraction_t xanim_eval_byte_key_frame_fraction(float time, float frameTime, const uint8_t *keys, int32_t keyCount,
                                                                int32_t targetKey, int32_t *frameIndex)
{
    *frameIndex = XAnimFindByteKey(time, keys, keyCount, targetKey);
#if defined(WINDOWS_BEHAVIOR)
    return ((long double)frameTime - (long double)keys[*frameIndex]) /
           (long double)((int32_t)keys[*frameIndex + 1] - (int32_t)keys[*frameIndex]);
#elif EMULATE_X87
    return x87f_store_f32(x87f_div(x87f_sub(x87f_load_f32(frameTime), x87f_load_i32(keys[*frameIndex])),
                                   x87f_load_i32((int32_t)keys[*frameIndex + 1] - (int32_t)keys[*frameIndex])));
#else
    return (frameTime - (float)keys[*frameIndex]) / (float)((int32_t)keys[*frameIndex + 1] - (int32_t)keys[*frameIndex]);
#endif
}

/* NOT_FROM_ORIGINAL_SOURCE: source-level factoring of short-key interval
 * selection and interpolation in XAnimCalcDeltaParts. */
static xanim_eval_fraction_t xanim_eval_short_key_frame_fraction(float time, float frameTime, const uint16_t *keys, int32_t keyCount,
                                                                 int32_t targetKey, int32_t *frameIndex)
{
    *frameIndex = XAnimFindShortKey(time, keys, keyCount, targetKey);
#if defined(WINDOWS_BEHAVIOR)
    return ((long double)frameTime - (long double)keys[*frameIndex]) /
           (long double)((int32_t)keys[*frameIndex + 1] - (int32_t)keys[*frameIndex]);
#elif EMULATE_X87
    return x87f_store_f32(x87f_div(x87f_sub(x87f_load_f32(frameTime), x87f_load_i32(keys[*frameIndex])),
                                   x87f_load_i32((int32_t)keys[*frameIndex + 1] - (int32_t)keys[*frameIndex])));
#else
    return (frameTime - (float)keys[*frameIndex]) / (float)((int32_t)keys[*frameIndex + 1] - (int32_t)keys[*frameIndex]);
#endif
}

static xanim_eval_fraction_t xanim_eval_unkeyed_frame_fraction(float frameTime, int32_t roundedFrame)
{
#if defined(WINDOWS_BEHAVIOR)
    return (long double)frameTime - (long double)roundedFrame;
#elif EMULATE_X87
    return x87f_store_f32(x87f_sub(x87f_load_f32(frameTime), x87f_load_i32(roundedFrame)));
#else
    return frameTime - (float)roundedFrame;
#endif
}

/* NOT_FROM_ORIGINAL_SOURCE: source-level factoring of primary-stream
 * interpolation in XAnimCalcDeltaParts. */
static void xanim_eval_sample_optional_primary(const xanim_rotation_stream_t *primary, int32_t frameCount, float time, float frameTime,
                                               int32_t roundedFrame, qboolean useShortKeys, vec2_t rotation)
{
    int32_t frameIndex = roundedFrame;
    xanim_eval_fraction_t frameFraction;

    if (primary == NULL) {
        rotation[0] = 0.0f;
        rotation[1] = 32767.0f;
        return;
    }
    if (primary->frameIndex == 0) {
        rotation[0] = (float)primary->data.inlinePrefix.lane0;
        rotation[1] = (float)primary->data.inlinePrefix.lane1;
        return;
    }

    if (primary->frameIndex < frameCount) {
        frameFraction = useShortKeys != qfalse ? xanim_eval_short_key_frame_fraction(time, frameTime, primary->tail.shortKeys,
                                                                                     primary->frameIndex, roundedFrame, &frameIndex)
                                               : xanim_eval_byte_key_frame_fraction(time, frameTime, primary->tail.byteKeys,
                                                                                    primary->frameIndex, roundedFrame, &frameIndex);
    } else {
        frameFraction = xanim_eval_unkeyed_frame_fraction(frameTime, roundedFrame);
    }

    const xanim_int16_vec2_t *frame = &primary->data.frames2[frameIndex];
    const xanim_int16_vec2_t *nextFrame = &primary->data.frames2[frameIndex + 1];
    rotation[0] = xanim_eval_lerp_i32(frame->components[0], nextFrame->components[0], frameFraction);
    rotation[1] = xanim_eval_lerp_i32(frame->components[1], nextFrame->components[1], frameFraction);
}

/* NOT_FROM_ORIGINAL_SOURCE: source-level factoring of secondary-stream
 * interpolation in XAnimCalcDeltaParts. */
static void xanim_eval_sample_optional_secondary(const xanim_translation_stream_t *secondary, int32_t frameCount, float time,
                                                 float frameTime, int32_t roundedFrame, qboolean useShortKeys, vec3_t move)
{
    int32_t frameIndex = roundedFrame;
    xanim_eval_fraction_t frameFraction;

    if (secondary == NULL) {
        move[0] = 0.0f;
        move[1] = 0.0f;
        move[2] = 0.0f;
        return;
    }
    if (secondary->frameIndex == 0) {
        move[0] = secondary->data.inlineLane0;
        move[1] = secondary->inlineLanes.lane1;
        move[2] = secondary->inlineLanes.lane2;
        return;
    }

    if (secondary->frameIndex < frameCount) {
        frameFraction = useShortKeys != qfalse ? xanim_eval_short_key_frame_fraction(time, frameTime, secondary->key.shortKeys,
                                                                                     secondary->frameIndex, roundedFrame, &frameIndex)
                                               : xanim_eval_byte_key_frame_fraction(time, frameTime, secondary->key.byteKeys,
                                                                                    secondary->frameIndex, roundedFrame, &frameIndex);
    } else {
        frameFraction = xanim_eval_unkeyed_frame_fraction(frameTime, roundedFrame);
    }

    const float *frame = secondary->data.frames[frameIndex];
    const float *nextFrame = secondary->data.frames[frameIndex + 1];
    move[0] = xanim_eval_lerp_f32(frame[0], nextFrame[0], frameFraction);
    move[1] = xanim_eval_lerp_f32(frame[1], nextFrame[1], frameFraction);
    move[2] = xanim_eval_lerp_f32(frame[2], nextFrame[2], frameFraction);
}

/* Source: CoDUOMP.exe 0x00497b10..0x00497eee.
 * Name: exact same-module Mac symbol XAnimCalcDeltaParts. */
void XAnimCalcDeltaParts(XAnimParts *record, vec2_t rotation, vec3_t move, float time)
{
    int32_t frameCount = record->frameCountMinusOne;
    xanim_rotation_stream_t *primary = record->deltaMotion->rotation;
    xanim_translation_stream_t *secondary = record->deltaMotion->translation;

    if (time == 1.0f || frameCount == 0) {
        xanim_eval_sample_optional_primary_last_frame(primary, rotation);
        xanim_eval_sample_optional_secondary_last_frame(secondary, move);
        return;
    }

    /* Windows 0x00497b4b stores the scaled time for interpolation but retains
     * the PC=53 product for its integer conversion.  Linux
     * 0x080bacd9..0x080bad02 converts the stored binary32 value instead. */
    int32_t roundedFrame;
    float frameTime = xanim_eval_frame_time(frameCount, time, &roundedFrame);
    qboolean useShortKeys = frameCount >= XANIM_SMALL_FRAME_KEY_LIMIT;
    xanim_eval_sample_optional_primary(primary, frameCount, time, frameTime, roundedFrame, useShortKeys, rotation);
    xanim_eval_sample_optional_secondary(secondary, frameCount, time, frameTime, roundedFrame, useShortKeys, move);
}

/* Source: CoDUOMP.exe 0x00497a60..0x00497b0f.
 * Name: exact same-module Mac symbol XAnimCalcData. */
void XAnimCalcData(fileData_t *entry, XAnimToXModel *partRemap, float weight, DObjAnimMat *parts, float time)
{
    for (int32_t word = 0; word < DOBJ_PART_BITSET_WORD_COUNT; ++word) {
        uint32_t sourcePartBits;

        memcpy(&sourcePartBits, &partRemap->partBits[(size_t)word * sizeof(sourcePartBits)], sizeof(sourcePartBits));
        xanim_evalPartBits[word] |= sourcePartBits & ~xanim_evalSkipBits[word];
    }

    XAnimParts *record = entry->data.xanimParts;
    if (time == 1.0f || record->frameCountMinusOne == 0) {
        XAnimCalcNonLoopEnd(record, partRemap->boneIndex, weight, parts);
    } else if (record->frameCountMinusOne < XANIM_SMALL_FRAME_KEY_LIMIT) {
        XAnimCalcPartsSmallIndices(record, partRemap->boneIndex, time, weight, parts);
    } else {
        XAnimCalcPartsLargeIndices(record, partRemap->boneIndex, time, weight, parts);
    }
}

/* NOT_FROM_ORIGINAL_SOURCE: shared readable bit test used by the three
 * original part-sampling functions. */
static qboolean xanim_eval_part_bit_is_set(const uint8_t *bits, int32_t partIndex)
{
    return (bits[partIndex >> 3] & (uint8_t)(1U << (partIndex & 7))) != 0 ? qtrue : qfalse;
}

/* NOT_FROM_ORIGINAL_SOURCE: the byte-key and short-key original functions
 * are instruction-for-instruction structural twins apart from key width;
 * this helper keeps their shared interpolation behavior in one typed body. */
static void xanim_eval_accumulate_interpolated_parts(XAnimParts *record, const uint8_t *partRemap, float time, float weight,
                                                     DObjAnimMat *parts, qboolean useShortKeys)
{
    const float packedShortScale = XANIM_PACKED_SHORT_SCALE;
    const uint8_t *skipBits = (const uint8_t *)xanim_evalSkipBits;
    int32_t frameCount = record->frameCountMinusOne;
    float scaledWeight = xanim_eval_mul_store(weight, packedShortScale);
    /* Windows 0x00497026/0x00497416 retain the scaled-time product for the
     * integer conversion.  Linux 0x080b9b91..0x080b9bc1 and
     * 0x080ba199..0x080ba1c9 convert the stored binary32 value. */
    int32_t roundedFrame;
    float frameTime = xanim_eval_frame_time(frameCount, time, &roundedFrame);
    int32_t partCount = (int16_t)record->partNameHandles[0];

    for (int32_t targetPart = 0; targetPart < partCount; ++targetPart) {
        int32_t sourcePart = partRemap[targetPart];
        if (xanim_eval_part_bit_is_set(skipBits, sourcePart)) {
            continue;
        }

        DObjAnimMat *part = &parts[sourcePart];
        xanim_part_stream_pair_t *streams = &record->partStreamPairs[targetPart];
        xanim_rotation_stream_t *primary = streams->rotation;
        int32_t frameIndex = roundedFrame;
        xanim_eval_fraction_t frameFraction;

        if (xanim_eval_part_bit_is_set(record->compressedRotationBits, targetPart)) {
            if (primary == NULL) {
                part->quat[3] += weight;
            } else if (primary->frameIndex == 0) {
                part->quat[2] = xanim_eval_madd_store(part->quat[2], (float)primary->data.inlinePrefix.lane0, scaledWeight);
                part->quat[3] = xanim_eval_madd_store(part->quat[3], (float)primary->data.inlinePrefix.lane1, scaledWeight);
            } else {
                if (primary->frameIndex < frameCount) {
                    frameFraction = useShortKeys != qfalse
                                        ? xanim_eval_short_key_frame_fraction(time, frameTime, primary->tail.shortKeys, primary->frameIndex,
                                                                              roundedFrame, &frameIndex)
                                        : xanim_eval_byte_key_frame_fraction(time, frameTime, primary->tail.byteKeys, primary->frameIndex,
                                                                             roundedFrame, &frameIndex);
                } else {
                    frameFraction = xanim_eval_unkeyed_frame_fraction(frameTime, roundedFrame);
                }

                xanim_int16_vec2_t *frame = &primary->data.frames2[frameIndex];
                xanim_int16_vec2_t *nextFrame = &primary->data.frames2[frameIndex + 1];
                part->quat[2] = xanim_eval_accumulate_lerp_i32(part->quat[2], frame->components[0], nextFrame->components[0], frameFraction,
                                                               scaledWeight);
                part->quat[3] = xanim_eval_accumulate_lerp_i32(part->quat[3], frame->components[1], nextFrame->components[1], frameFraction,
                                                               scaledWeight);
            }
        } else if (primary->frameIndex == 0) {
            part->quat[0] = xanim_eval_madd_store(part->quat[0], (float)primary->data.inlinePrefix.lane0, scaledWeight);
            part->quat[1] = xanim_eval_madd_store(part->quat[1], (float)primary->data.inlinePrefix.lane1, scaledWeight);
            part->quat[2] = xanim_eval_madd_store(part->quat[2], (float)primary->tail.inlineFull.lane2, scaledWeight);
            part->quat[3] = xanim_eval_madd_store(part->quat[3], (float)primary->tail.inlineFull.lane3, scaledWeight);
        } else {
            if (primary->frameIndex < frameCount) {
                frameFraction = useShortKeys != qfalse ? xanim_eval_short_key_frame_fraction(time, frameTime, primary->tail.shortKeys,
                                                                                             primary->frameIndex, roundedFrame, &frameIndex)
                                                       : xanim_eval_byte_key_frame_fraction(time, frameTime, primary->tail.byteKeys,
                                                                                            primary->frameIndex, roundedFrame, &frameIndex);
            } else {
                frameFraction = xanim_eval_unkeyed_frame_fraction(frameTime, roundedFrame);
            }

            xanim_int16_vec4_t *frame = &primary->data.frames4[frameIndex];
            xanim_int16_vec4_t *nextFrame = &primary->data.frames4[frameIndex + 1];
            for (int32_t lane = 0; lane < 4; ++lane) {
                part->quat[lane] = xanim_eval_accumulate_lerp_i32(part->quat[lane], frame->components[lane], nextFrame->components[lane],
                                                                  frameFraction, scaledWeight);
            }
        }

        xanim_translation_stream_t *secondary = streams->translation;
        if (secondary != NULL) {
            if (secondary->frameIndex == 0) {
                part->translation[0] = xanim_eval_madd_store(part->translation[0], weight, secondary->data.inlineLane0);
                part->translation[1] = xanim_eval_madd_store(part->translation[1], weight, secondary->inlineLanes.lane1);
                part->translation[2] = xanim_eval_madd_store(part->translation[2], weight, secondary->inlineLanes.lane2);
            } else {
                if (secondary->frameIndex < frameCount) {
                    frameFraction = useShortKeys != qfalse
                                        ? xanim_eval_short_key_frame_fraction(time, frameTime, secondary->key.shortKeys,
                                                                              secondary->frameIndex, roundedFrame, &frameIndex)
                                        : xanim_eval_byte_key_frame_fraction(time, frameTime, secondary->key.byteKeys,
                                                                             secondary->frameIndex, roundedFrame, &frameIndex);
                } else {
                    frameIndex = roundedFrame;
                    frameFraction = xanim_eval_unkeyed_frame_fraction(frameTime, roundedFrame);
                }

                float *frame = secondary->data.frames[frameIndex];
                float *nextFrame = secondary->data.frames[frameIndex + 1];
                for (int32_t lane = 0; lane < 3; ++lane) {
                    part->translation[lane] =
                        xanim_eval_accumulate_lerp_f32(part->translation[lane], frame[lane], nextFrame[lane], frameFraction, weight);
                }
            }
        }

        part->accumulatedWeight += weight;
    }
}

/* Source: CoDUOMP.exe 0x00497000..0x004973e1.
 * Name: same-module Mac symbol XAnimCalcPartsSmallIndices. */
void XAnimCalcPartsSmallIndices(XAnimParts *record, const uint8_t *partRemap, float time, float weight, DObjAnimMat *parts)
{
    xanim_eval_accumulate_interpolated_parts(record, partRemap, time, weight, parts, qfalse);
}

/* Source: CoDUOMP.exe 0x004973f0..0x004977d1.
 * Name: same-module Mac symbol XAnimCalcPartsLargeIndices. */
void XAnimCalcPartsLargeIndices(XAnimParts *record, const uint8_t *partRemap, float time, float weight, DObjAnimMat *parts)
{
    xanim_eval_accumulate_interpolated_parts(record, partRemap, time, weight, parts, qtrue);
}

/* Source: CoDUOMP.exe 0x004977e0..0x00497a00.
 * Name: same-module Mac symbol XAnimCalcNonLoopEnd. */
void XAnimCalcNonLoopEnd(XAnimParts *record, const uint8_t *partRemap, float weight, DObjAnimMat *parts)
{
    const float packedShortScale = XANIM_PACKED_SHORT_SCALE;
    const uint8_t *skipBits = (const uint8_t *)xanim_evalSkipBits;
    float scaledWeight = xanim_eval_mul_store(weight, packedShortScale);
    int32_t partCount = (int16_t)record->partNameHandles[0];

    for (int32_t targetPart = 0; targetPart < partCount; ++targetPart) {
        int32_t sourcePart = partRemap[targetPart];
        if (xanim_eval_part_bit_is_set(skipBits, sourcePart)) {
            continue;
        }

        DObjAnimMat *part = &parts[sourcePart];
        xanim_part_stream_pair_t *streams = &record->partStreamPairs[targetPart];
        xanim_rotation_stream_t *primary = streams->rotation;

        if (xanim_eval_part_bit_is_set(record->compressedRotationBits, targetPart)) {
            if (primary == NULL) {
                part->quat[3] += weight;
            } else if (primary->frameIndex == 0) {
                part->quat[2] = xanim_eval_madd_store(part->quat[2], (float)primary->data.inlinePrefix.lane0, scaledWeight);
                part->quat[3] = xanim_eval_madd_store(part->quat[3], (float)primary->data.inlinePrefix.lane1, scaledWeight);
            } else {
                xanim_int16_vec2_t *frame = &primary->data.frames2[primary->frameIndex];
                part->quat[2] = xanim_eval_madd_store(part->quat[2], (float)frame->components[0], scaledWeight);
                part->quat[3] = xanim_eval_madd_store(part->quat[3], (float)frame->components[1], scaledWeight);
            }
        } else if (primary->frameIndex == 0) {
            part->quat[0] = xanim_eval_madd_store(part->quat[0], (float)primary->data.inlinePrefix.lane0, scaledWeight);
            part->quat[1] = xanim_eval_madd_store(part->quat[1], (float)primary->data.inlinePrefix.lane1, scaledWeight);
            part->quat[2] = xanim_eval_madd_store(part->quat[2], (float)primary->tail.inlineFull.lane2, scaledWeight);
            part->quat[3] = xanim_eval_madd_store(part->quat[3], (float)primary->tail.inlineFull.lane3, scaledWeight);
        } else {
            xanim_int16_vec4_t *frame = &primary->data.frames4[primary->frameIndex];
            for (int32_t lane = 0; lane < 4; ++lane) {
                part->quat[lane] = xanim_eval_madd_store(part->quat[lane], (float)frame->components[lane], scaledWeight);
            }
        }

        xanim_translation_stream_t *secondary = streams->translation;
        if (secondary != NULL) {
            if (secondary->frameIndex == 0) {
                part->translation[0] = xanim_eval_madd_store(part->translation[0], weight, secondary->data.inlineLane0);
                part->translation[1] = xanim_eval_madd_store(part->translation[1], weight, secondary->inlineLanes.lane1);
                part->translation[2] = xanim_eval_madd_store(part->translation[2], weight, secondary->inlineLanes.lane2);
            } else {
                float *frame = secondary->data.frames[secondary->frameIndex];
                part->translation[0] = xanim_eval_madd_store(part->translation[0], weight, frame[0]);
                part->translation[1] = xanim_eval_madd_store(part->translation[1], weight, frame[1]);
                part->translation[2] = xanim_eval_madd_store(part->translation[2], weight, frame[2]);
            }
        }

        part->accumulatedWeight += weight;
    }
}

/* NOT_FROM_ORIGINAL_SOURCE: source-level factoring of the terminal
 * normalization loop in XAnimCalc. */
static void xanim_eval_normalize_eval_parts(DObjAnimMat *parts)
{
    const uint8_t *skipBits = (const uint8_t *)xanim_evalSkipBits;

    for (int32_t partIndex = 0; partIndex < xanim_evalPartCount; ++partIndex) {
        DObjAnimMat *part = &parts[partIndex];
        if (xanim_eval_part_bit_is_set(skipBits, partIndex) || part->accumulatedWeight == 0.0f) {
            continue;
        }

#if defined(WINDOWS_BEHAVIOR)
        const long double scale = (long double)1.0f / (long double)part->accumulatedWeight;
        for (int32_t lane = 0; lane < 4; ++lane) {
            part->quat[lane] = (float)(scale * (long double)part->quat[lane]);
        }
        for (int32_t lane = 0; lane < 3; ++lane) {
            part->translation[lane] = (float)(scale * (long double)part->translation[lane]);
        }
#else
        const float scale = xanim_eval_div_store(1.0f, part->accumulatedWeight);
        for (int32_t lane = 0; lane < 4; ++lane) {
            part->quat[lane] = xanim_eval_mul_store(part->quat[lane], scale);
        }
        for (int32_t lane = 0; lane < 3; ++lane) {
            part->translation[lane] = xanim_eval_mul_store(part->translation[lane], scale);
        }
#endif
    }
}

/* NOT_FROM_ORIGINAL_SOURCE: source-level factoring of XAnimCalc's merge of a
 * normalized multi-child result into its caller's accumulation. */
static void xanim_eval_merge_normalized_eval_parts(DObjAnimMat *dest, DObjAnimMat *child, float weight)
{
    const uint8_t *skipBits = (const uint8_t *)xanim_evalSkipBits;

    for (int32_t partIndex = 0; partIndex < xanim_evalPartCount; ++partIndex) {
        if (xanim_eval_part_bit_is_set(skipBits, partIndex)) {
            continue;
        }

#if defined(WINDOWS_BEHAVIOR)
        const long double quatLengthSquared = (((long double)child[partIndex].quat[3] * (long double)child[partIndex].quat[3] +
                                                (long double)child[partIndex].quat[1] * (long double)child[partIndex].quat[1]) +
                                               (long double)child[partIndex].quat[0] * (long double)child[partIndex].quat[0]) +
                                              (long double)child[partIndex].quat[2] * (long double)child[partIndex].quat[2];
        if (quatLengthSquared != (long double)0.0f) {
            const long double scale = (long double)weight / sqrtl(quatLengthSquared);
            for (int32_t lane = 0; lane < 4; ++lane) {
                dest[partIndex].quat[lane] =
                    (float)((long double)dest[partIndex].quat[lane] + (long double)child[partIndex].quat[lane] * scale);
            }
        }

        if (child[partIndex].accumulatedWeight != 0.0f) {
            const long double scale = (long double)weight / (long double)child[partIndex].accumulatedWeight;
            dest[partIndex].accumulatedWeight += weight;
            for (int32_t lane = 0; lane < 3; ++lane) {
                dest[partIndex].translation[lane] =
                    (float)((long double)dest[partIndex].translation[lane] + (long double)child[partIndex].translation[lane] * scale);
            }
        }
#else
        const float quatLengthSquared =
            xanim_eval_sumsq4_store(child[partIndex].quat[0], child[partIndex].quat[1], child[partIndex].quat[2], child[partIndex].quat[3]);
        if (quatLengthSquared != 0.0f) {
            const float scale = xanim_eval_div_store(weight, (float)sqrt((double)quatLengthSquared));
            for (int32_t lane = 0; lane < 4; ++lane) {
                dest[partIndex].quat[lane] = xanim_eval_madd_store(dest[partIndex].quat[lane], child[partIndex].quat[lane], scale);
            }
        }

        if (child[partIndex].accumulatedWeight != 0.0f) {
            const float scale = xanim_eval_div_store(weight, child[partIndex].accumulatedWeight);
            dest[partIndex].accumulatedWeight += weight;
            for (int32_t lane = 0; lane < 3; ++lane) {
                dest[partIndex].translation[lane] =
                    xanim_eval_madd_store(dest[partIndex].translation[lane], child[partIndex].translation[lane], scale);
            }
        }
#endif
    }
}

/* NOT_FROM_ORIGINAL_SOURCE: source-level factoring of XAnimCalc's weighted
 * normalization of a newly cleared child accumulation. */
static void xanim_eval_scale_normalized_eval_parts(DObjAnimMat *parts, float weight)
{
    const uint8_t *skipBits = (const uint8_t *)xanim_evalSkipBits;

    for (int32_t partIndex = 0; partIndex < xanim_evalPartCount; ++partIndex) {
        DObjAnimMat *part = &parts[partIndex];
        if (xanim_eval_part_bit_is_set(skipBits, partIndex)) {
            continue;
        }

#if defined(WINDOWS_BEHAVIOR)
        const long double quatLengthSquared =
            (((long double)part->quat[3] * (long double)part->quat[3] + (long double)part->quat[1] * (long double)part->quat[1]) +
             (long double)part->quat[0] * (long double)part->quat[0]) +
            (long double)part->quat[2] * (long double)part->quat[2];
        if (quatLengthSquared != (long double)0.0f) {
            const long double scale = (long double)weight / sqrtl(quatLengthSquared);
            for (int32_t lane = 0; lane < 4; ++lane) {
                part->quat[lane] = (float)(scale * (long double)part->quat[lane]);
            }
        }

        if (part->accumulatedWeight != 0.0f) {
            const long double scale = (long double)weight / (long double)part->accumulatedWeight;
            part->accumulatedWeight = weight;
            for (int32_t lane = 0; lane < 3; ++lane) {
                part->translation[lane] = (float)(scale * (long double)part->translation[lane]);
            }
        }
#else
        const float quatLengthSquared = xanim_eval_sumsq4_store(part->quat[0], part->quat[1], part->quat[2], part->quat[3]);
        if (quatLengthSquared != 0.0f) {
            const float scale = xanim_eval_div_store(weight, (float)sqrt((double)quatLengthSquared));
            for (int32_t lane = 0; lane < 4; ++lane) {
                part->quat[lane] = xanim_eval_mul_store(part->quat[lane], scale);
            }
        }

        if (part->accumulatedWeight != 0.0f) {
            const float scale = xanim_eval_div_store(weight, part->accumulatedWeight);
            part->accumulatedWeight = weight;
            for (int32_t lane = 0; lane < 3; ++lane) {
                part->translation[lane] = xanim_eval_mul_store(part->translation[lane], scale);
            }
        }
#endif
    }
}

/* Source: CoDUOMP.exe 0x00499d50..0x0049a45f.
 * Name: same-module Mac symbol XAnimCalc. */
void XAnimCalc(uint32_t animIndex, float weight, DObjAnimMat *parts, qboolean clear, qboolean normalize)
{
    XAnimEntry *entry = &xanim_currentTree->sourceTree->entries[animIndex];

    if (entry->childCount == 0) {
        if (clear != qfalse) {
            XAnimClearData(parts);
        }

        size_t nodeCount = xanim_currentTree->sourceTree->nodeCount;
        uint8_t *remapTable = xanim_currentEvalState->partRemapTable;
        uint8_t *remapGeneration = coduo_xanim_part_remap_generation_bytes(remapTable, nodeCount);
        uint16_t remapHandle = coduo_xanim_part_remap_handle_load(remapTable, (size_t)animIndex);

        if (remapHandle == 0) {
            remapGeneration[animIndex + 1] = remapGeneration[0];
            remapHandle = XAnimSetModel(entry, xanim_evalChildRefs, xanim_evalChildCount);
            coduo_xanim_part_remap_handle_store(remapTable, (size_t)animIndex, remapHandle);
        } else if (remapGeneration[animIndex + 1] != remapGeneration[0]) {
            XAnimParts *record = entry->payload.leafAsset->data.xanimParts;
            remapGeneration[animIndex + 1] = remapGeneration[0];
            SL_RemoveRefToStringOfLen(remapHandle, (uint32_t)(int16_t)record->partNameHandles[0] + DOBJ_PART_REMAP_PREFIX_SIZE);
            remapHandle = XAnimSetModel(entry, xanim_evalChildRefs, xanim_evalChildCount);
            coduo_xanim_part_remap_handle_store(remapTable, (size_t)animIndex, remapHandle);
        }

        XAnimToXModel *partRemap = (XAnimToXModel *)(void *)SL_ConvertToString(remapHandle);
        uint16_t handle = xanim_currentTree->poolNodeHandles[animIndex];
        float time = xanim_pool[handle].states[xanim_activePoolPayloadSlot].time;
        XAnimCalcData(entry->payload.leafAsset, partRemap, weight, parts, time);
        return;
    }

    int32_t firstActiveChild = -1;
    float firstWeight = 0.0f;
    for (int32_t child = 0; child < entry->childCount; ++child) {
        int32_t childIndex = entry->payload.parent.firstChildIndex + child;
        uint16_t handle = xanim_currentTree->poolNodeHandles[childIndex];
        if (handle != 0) {
            float childWeight = xanim_pool[handle].states[xanim_activePoolPayloadSlot].currentWeight;
            if (childWeight != 0.0f) {
                firstActiveChild = child;
                firstWeight = childWeight;
                break;
            }
        }
    }

    if (firstActiveChild < 0) {
        if (clear != qfalse) {
            XAnimClearData(parts);
        }
        return;
    }

    int32_t secondActiveChild = -1;
    float secondWeight = 0.0f;
    for (int32_t child = firstActiveChild + 1; child < entry->childCount; ++child) {
        int32_t childIndex = entry->payload.parent.firstChildIndex + child;
        uint16_t handle = xanim_currentTree->poolNodeHandles[childIndex];
        if (handle != 0) {
            float childWeight = xanim_pool[handle].states[xanim_activePoolPayloadSlot].currentWeight;
            if (childWeight != 0.0f) {
                secondActiveChild = child;
                secondWeight = childWeight;
                break;
            }
        }
    }

    if (secondActiveChild < 0) {
        XAnimCalc(entry->payload.parent.firstChildIndex + firstActiveChild, weight, parts, clear, normalize);
        return;
    }

    DObjAnimMat scratch[xanim_evalPartCount];
    DObjAnimMat *blendParts = clear != qfalse ? parts : scratch;
    XAnimCalc(entry->payload.parent.firstChildIndex + firstActiveChild, firstWeight, blendParts, qtrue, qtrue);
    XAnimCalc(entry->payload.parent.firstChildIndex + secondActiveChild, secondWeight, blendParts, qfalse, qtrue);

    for (int32_t child = secondActiveChild + 1; child < entry->childCount; ++child) {
        int32_t childIndex = entry->payload.parent.firstChildIndex + child;
        uint16_t handle = xanim_currentTree->poolNodeHandles[childIndex];
        if (handle != 0) {
            float childWeight = xanim_pool[handle].states[xanim_activePoolPayloadSlot].currentWeight;
            if (childWeight != 0.0f) {
                XAnimCalc(childIndex, childWeight, blendParts, qfalse, qtrue);
            }
        }
    }

    if (normalize == qfalse) {
        xanim_eval_normalize_eval_parts(parts);
    } else if (clear == qfalse) {
        xanim_eval_merge_normalized_eval_parts(parts, blendParts, weight);
    } else {
        xanim_eval_scale_normalized_eval_parts(parts, weight);
    }
}

/* NOT_FROM_ORIGINAL_SOURCE: resolves the model part-name/base-pose chain
 * repeatedly inlined in DObjCalcAnim. */
static const XModelPartsData *xanim_eval_model_part_payload(const XModel *model)
{
    return model->info->parts->data.xmodelParts;
}

/* Source: CoDUOMP.exe 0x0049adf0..0x0049b036.
 * Name: same-module Mac symbol DObjCalcAnim. */
void DObjCalcAnim(DObj *obj, const uint32_t *partBits)
{
    dobj_eval_storage_t *storage = obj->evaluationStorage;
    qboolean allEvaluated = qtrue;

    for (int32_t word = 0; word < DOBJ_PART_BITSET_WORD_COUNT; ++word) {
        xanim_evalPartBits[word] = ~partBits[word] | storage->evaluatedPartBits[word];
        if (xanim_evalPartBits[word] != UINT32_MAX) {
            allEvaluated = qfalse;
        }
    }
    if (allEvaluated != qfalse) {
        return;
    }

    for (int32_t word = 0; word < DOBJ_PART_BITSET_WORD_COUNT; ++word) {
        storage->evaluatedPartBits[word] |= partBits[word];
        xanim_evalSkipBits[word] = xanim_evalPartBits[word];
    }

    xanim_currentEvalState = obj;
    xanim_currentTree = obj->runtimeTree;
    xanim_evalChildCount = obj->modelCount;
    xanim_evalChildRefs = obj->models;
    xanim_evalPartCount = obj->boneCount;

    DObjAnimMat *output = &storage->partSpans[obj->boneCount].evalParts.parts[0];
    if (xanim_currentTree != NULL) {
        xanim_evalPartBytes = (size_t)obj->boneCount * sizeof(DObjAnimMat);
        (void)xanim_evalPartBytes;
        ((uint8_t *)xanim_evalSkipBits)[sizeof(xanim_evalSkipBits) - 1U] |= XANIM_EVAL_SKIP_LAST_PART_BIT;
        XAnimCalc(XANIM_EVAL_TREE_ROOT_NODE, 1.0f, output, qtrue, qfalse);
    }

    const uint8_t *evaluatedBytes = (const uint8_t *)xanim_evalPartBits;
    int32_t globalPart = 0;
    for (int32_t modelIndex = 0; modelIndex < obj->modelCount; ++modelIndex) {
        const XModelPartsData *payload = xanim_eval_model_part_payload(obj->models[modelIndex]);
        const XModelPartNameTable *partNames = payload->partNameTableSlot->partNameTable;

        int32_t rootPartsRemaining = payload->rootPartCount;
        /* NOT_FROM_ORIGINAL_SOURCE: model loading establishes a nonnegative
         * root count; retain a positive sink countdown. */
        while (rootPartsRemaining > 0) {
            if (!xanim_eval_part_bit_is_set(evaluatedBytes, globalPart)) {
                output->quat[0] = 0.0f;
                output->quat[1] = 0.0f;
                output->quat[2] = 0.0f;
                output->quat[3] = 1.0f;
                output->translation[2] = 0.0f;
                output->translation[1] = 0.0f;
                output->translation[0] = 0.0f;
            }
            ++output;
            ++globalPart;
            --rootPartsRemaining;
        }

        xanim_int16_vec4_t *baseRotation = payload->baseRotations;
        int32_t remainingParts = partNames->count - payload->rootPartCount;
        while (remainingParts != 0) {
            if (!xanim_eval_part_bit_is_set(evaluatedBytes, globalPart)) {
                const float packedShortScale = XANIM_PACKED_SHORT_SCALE;
                output->quat[0] = xanim_eval_mul_store((float)baseRotation->components[0], packedShortScale);
                output->quat[1] = xanim_eval_mul_store((float)baseRotation->components[1], packedShortScale);
                output->quat[2] = xanim_eval_mul_store((float)baseRotation->components[2], packedShortScale);
                output->translation[2] = 0.0f;
                output->translation[1] = 0.0f;
                output->translation[0] = 0.0f;
                output->quat[3] = xanim_eval_mul_store((float)baseRotation->components[3], packedShortScale);
            }
            ++output;
            ++globalPart;
            ++baseRotation;
            --remainingParts;
        }
    }
}

/* NOT_FROM_ORIGINAL_SOURCE: shared typed formatting of the six-lane delta
 * carrier used by the four public delta-motion entry points. */
static void xanim_eval_store_delta_output(const float *delta, vec2_t rotationDelta, vec3_t moveDelta, qboolean identityOnEitherZero)
{
    qboolean useIdentity = identityOnEitherZero ? delta[XANIM_DELTA_ROTATION_0] == 0.0f || delta[XANIM_DELTA_ROTATION_1] == 0.0f
                                                : delta[XANIM_DELTA_ROTATION_0] == 0.0f && delta[XANIM_DELTA_ROTATION_1] == 0.0f;

    if (useIdentity) {
        rotationDelta[0] = 0.0f;
        rotationDelta[1] = 1.0f;
    } else {
        rotationDelta[0] = delta[XANIM_DELTA_ROTATION_0];
        rotationDelta[1] = delta[XANIM_DELTA_ROTATION_1];
    }

    moveDelta[0] = delta[XANIM_DELTA_TRANSLATION_0];
    moveDelta[1] = delta[XANIM_DELTA_TRANSLATION_1];
    moveDelta[2] = delta[XANIM_DELTA_TRANSLATION_2];
}

/* NOT_FROM_ORIGINAL_SOURCE: typed factoring shared by XAnimCalcDeltaTree's
 * leaf and empty-subtree paths. */
static void xanim_eval_clear_delta(float *delta)
{
    delta[XANIM_DELTA_ROTATION_0] = 0.0f;
    delta[XANIM_DELTA_ROTATION_1] = 0.0f;
    delta[XANIM_DELTA_TRANSLATION_WEIGHT] = 0.0f;
    delta[XANIM_DELTA_TRANSLATION_0] = 0.0f;
    delta[XANIM_DELTA_TRANSLATION_1] = 0.0f;
    delta[XANIM_DELTA_TRANSLATION_2] = 0.0f;
}

/* NOT_FROM_ORIGINAL_SOURCE: readable selection of the current or target
 * weight lane proven by the 0x00b8cd10 selector accesses. */
static float xanim_eval_selected_delta_weight(XAnimState *payload)
{
    return xanim_evalPoolWeightSelector == 0 ? payload->currentWeight : payload->targetWeight;
}

/* NOT_FROM_ORIGINAL_SOURCE: source-level factoring of the final translation
 * normalization branch in XAnimCalcDeltaTree. */
static void xanim_eval_normalize_delta_translation(float *delta)
{
    if (delta[XANIM_DELTA_TRANSLATION_WEIGHT] != 0.0f) {
#if defined(WINDOWS_BEHAVIOR)
        const long double scale = (long double)1.0f / (long double)delta[XANIM_DELTA_TRANSLATION_WEIGHT];
        delta[XANIM_DELTA_TRANSLATION_0] = (float)(scale * (long double)delta[XANIM_DELTA_TRANSLATION_0]);
        delta[XANIM_DELTA_TRANSLATION_1] = (float)(scale * (long double)delta[XANIM_DELTA_TRANSLATION_1]);
        delta[XANIM_DELTA_TRANSLATION_2] = (float)(scale * (long double)delta[XANIM_DELTA_TRANSLATION_2]);
#else
        const float scale = xanim_eval_div_store(1.0f, delta[XANIM_DELTA_TRANSLATION_WEIGHT]);
        delta[XANIM_DELTA_TRANSLATION_0] = xanim_eval_mul_store(delta[XANIM_DELTA_TRANSLATION_0], scale);
        delta[XANIM_DELTA_TRANSLATION_1] = xanim_eval_mul_store(delta[XANIM_DELTA_TRANSLATION_1], scale);
        delta[XANIM_DELTA_TRANSLATION_2] = xanim_eval_mul_store(delta[XANIM_DELTA_TRANSLATION_2], scale);
#endif
    }
}

/* NOT_FROM_ORIGINAL_SOURCE: source-level factoring of the weighted merge
 * branch in XAnimCalcDeltaTree. */
static void xanim_eval_merge_weighted_delta(float *dest, float *source, float weight)
{
#if defined(WINDOWS_BEHAVIOR)
    const long double rotationLengthSquared = (long double)source[XANIM_DELTA_ROTATION_0] * (long double)source[XANIM_DELTA_ROTATION_0] +
                                              (long double)source[XANIM_DELTA_ROTATION_1] * (long double)source[XANIM_DELTA_ROTATION_1];
    if (rotationLengthSquared != (long double)0.0f) {
        const long double scale = (long double)weight / sqrtl(rotationLengthSquared);
        dest[XANIM_DELTA_ROTATION_0] =
            (float)((long double)dest[XANIM_DELTA_ROTATION_0] + (long double)source[XANIM_DELTA_ROTATION_0] * scale);
        dest[XANIM_DELTA_ROTATION_1] =
            (float)((long double)dest[XANIM_DELTA_ROTATION_1] + (long double)source[XANIM_DELTA_ROTATION_1] * scale);
    }

    if (source[XANIM_DELTA_TRANSLATION_WEIGHT] != 0.0f) {
        const long double scale = (long double)weight / (long double)source[XANIM_DELTA_TRANSLATION_WEIGHT];
        dest[XANIM_DELTA_TRANSLATION_WEIGHT] += weight;
        dest[XANIM_DELTA_TRANSLATION_0] =
            (float)((long double)dest[XANIM_DELTA_TRANSLATION_0] + (long double)source[XANIM_DELTA_TRANSLATION_0] * scale);
        dest[XANIM_DELTA_TRANSLATION_1] =
            (float)((long double)dest[XANIM_DELTA_TRANSLATION_1] + (long double)source[XANIM_DELTA_TRANSLATION_1] * scale);
        dest[XANIM_DELTA_TRANSLATION_2] =
            (float)((long double)dest[XANIM_DELTA_TRANSLATION_2] + (long double)source[XANIM_DELTA_TRANSLATION_2] * scale);
    }
#else
    const float rotationLengthSquared = xanim_eval_sumsq2_store(source[XANIM_DELTA_ROTATION_1], source[XANIM_DELTA_ROTATION_0]);
    if (rotationLengthSquared != 0.0f) {
        const float scale = xanim_eval_div_store(weight, (float)sqrt((double)rotationLengthSquared));
        dest[XANIM_DELTA_ROTATION_0] = xanim_eval_madd_store(dest[XANIM_DELTA_ROTATION_0], source[XANIM_DELTA_ROTATION_0], scale);
        dest[XANIM_DELTA_ROTATION_1] = xanim_eval_madd_store(dest[XANIM_DELTA_ROTATION_1], source[XANIM_DELTA_ROTATION_1], scale);
    }

    if (source[XANIM_DELTA_TRANSLATION_WEIGHT] != 0.0f) {
        const float scale = xanim_eval_div_store(weight, source[XANIM_DELTA_TRANSLATION_WEIGHT]);
        dest[XANIM_DELTA_TRANSLATION_WEIGHT] += weight;
        dest[XANIM_DELTA_TRANSLATION_0] = xanim_eval_madd_store(dest[XANIM_DELTA_TRANSLATION_0], source[XANIM_DELTA_TRANSLATION_0], scale);
        dest[XANIM_DELTA_TRANSLATION_1] = xanim_eval_madd_store(dest[XANIM_DELTA_TRANSLATION_1], source[XANIM_DELTA_TRANSLATION_1], scale);
        dest[XANIM_DELTA_TRANSLATION_2] = xanim_eval_madd_store(dest[XANIM_DELTA_TRANSLATION_2], source[XANIM_DELTA_TRANSLATION_2], scale);
    }
#endif
}

/* NOT_FROM_ORIGINAL_SOURCE: source-level factoring of the weighted scale
 * branch in XAnimCalcDeltaTree. */
static void xanim_eval_scale_weighted_delta(float *delta, float weight)
{
#if defined(WINDOWS_BEHAVIOR)
    const long double rotationLengthSquared = (long double)delta[XANIM_DELTA_ROTATION_0] * (long double)delta[XANIM_DELTA_ROTATION_0] +
                                              (long double)delta[XANIM_DELTA_ROTATION_1] * (long double)delta[XANIM_DELTA_ROTATION_1];
    if (rotationLengthSquared != (long double)0.0f) {
        const long double scale = (long double)weight / sqrtl(rotationLengthSquared);
        delta[XANIM_DELTA_ROTATION_0] = (float)(scale * (long double)delta[XANIM_DELTA_ROTATION_0]);
        delta[XANIM_DELTA_ROTATION_1] = (float)(scale * (long double)delta[XANIM_DELTA_ROTATION_1]);
    }

    if (delta[XANIM_DELTA_TRANSLATION_WEIGHT] != 0.0f) {
        const long double scale = (long double)weight / (long double)delta[XANIM_DELTA_TRANSLATION_WEIGHT];
        delta[XANIM_DELTA_TRANSLATION_WEIGHT] = weight;
        delta[XANIM_DELTA_TRANSLATION_0] = (float)(scale * (long double)delta[XANIM_DELTA_TRANSLATION_0]);
        delta[XANIM_DELTA_TRANSLATION_1] = (float)(scale * (long double)delta[XANIM_DELTA_TRANSLATION_1]);
        delta[XANIM_DELTA_TRANSLATION_2] = (float)(scale * (long double)delta[XANIM_DELTA_TRANSLATION_2]);
    }
#else
    const float rotationLengthSquared = xanim_eval_sumsq2_store(delta[XANIM_DELTA_ROTATION_1], delta[XANIM_DELTA_ROTATION_0]);
    if (rotationLengthSquared != 0.0f) {
        const float scale = xanim_eval_div_store(weight, (float)sqrt((double)rotationLengthSquared));
        delta[XANIM_DELTA_ROTATION_0] = xanim_eval_mul_store(delta[XANIM_DELTA_ROTATION_0], scale);
        delta[XANIM_DELTA_ROTATION_1] = xanim_eval_mul_store(delta[XANIM_DELTA_ROTATION_1], scale);
    }

    if (delta[XANIM_DELTA_TRANSLATION_WEIGHT] != 0.0f) {
        const float scale = xanim_eval_div_store(weight, delta[XANIM_DELTA_TRANSLATION_WEIGHT]);
        delta[XANIM_DELTA_TRANSLATION_WEIGHT] = weight;
        delta[XANIM_DELTA_TRANSLATION_0] = xanim_eval_mul_store(delta[XANIM_DELTA_TRANSLATION_0], scale);
        delta[XANIM_DELTA_TRANSLATION_1] = xanim_eval_mul_store(delta[XANIM_DELTA_TRANSLATION_1], scale);
        delta[XANIM_DELTA_TRANSLATION_2] = xanim_eval_mul_store(delta[XANIM_DELTA_TRANSLATION_2], scale);
    }
#endif
}

/* NOT_FROM_ORIGINAL_SOURCE: source-level factoring of the leaf path in
 * XAnimCalcDeltaTree. */
static void xanim_eval_calc_leaf_delta(XAnimTree *tree, uint32_t animIndex, float weight, float *delta, qboolean clear)
{
    if (clear != qfalse) {
        xanim_eval_clear_delta(delta);
    }

    XAnimEntry *entry = &tree->sourceTree->entries[animIndex];
    XAnimParts *record = entry->payload.leafAsset->data.xanimParts;
    if (record->hasDeltaMotion == 0) {
        return;
    }

    uint16_t handle = tree->poolNodeHandles[animIndex];
    if (handle == 0) {
        return;
    }

    XAnimState *payload = &xanim_pool[handle].states[xanim_activePoolPayloadSlot];
    if (xanim_evalLeafOutputMode != qfalse) {
        XAnimCalcAbsDeltaParts(record, weight, delta, payload->time);
    } else {
        XAnimCalcRelDeltaParts(record, weight, delta, payload->oldTime, payload->time);
    }
}

/* Source: CoDUOMP.exe 0x0049a790..0x0049ab74.
 * Name: same-module Mac symbol XAnimCalcDeltaTree. */
void XAnimCalcDeltaTree(XAnimTree *tree, uint32_t animIndex, float weight, float *deltaLanes, qboolean clear, qboolean normalize)
{
    float *delta = deltaLanes;
    XAnimEntry *entry = &tree->sourceTree->entries[animIndex];
    if (entry->childCount == 0) {
        xanim_eval_calc_leaf_delta(tree, animIndex, weight, delta, clear);
        return;
    }

    int32_t firstActiveChild = -1;
    float firstWeight = 0.0f;
    for (int32_t child = 0; child < entry->childCount; ++child) {
        int32_t childIndex = entry->payload.parent.firstChildIndex + child;
        uint16_t handle = tree->poolNodeHandles[childIndex];
        if (handle != 0) {
            float childWeight = xanim_eval_selected_delta_weight(&xanim_pool[handle].states[xanim_activePoolPayloadSlot]);
            if (childWeight != 0.0f) {
                firstActiveChild = child;
                firstWeight = childWeight;
                break;
            }
        }
    }

    if (firstActiveChild < 0) {
        if (clear != qfalse) {
            xanim_eval_clear_delta(delta);
        }
        return;
    }

    int32_t secondActiveChild = -1;
    float secondWeight = 0.0f;
    for (int32_t child = firstActiveChild + 1; child < entry->childCount; ++child) {
        int32_t childIndex = entry->payload.parent.firstChildIndex + child;
        uint16_t handle = tree->poolNodeHandles[childIndex];
        if (handle != 0) {
            float childWeight = xanim_eval_selected_delta_weight(&xanim_pool[handle].states[xanim_activePoolPayloadSlot]);
            if (childWeight != 0.0f) {
                secondActiveChild = child;
                secondWeight = childWeight;
                break;
            }
        }
    }

    if (secondActiveChild < 0) {
        XAnimCalcDeltaTree(tree, entry->payload.parent.firstChildIndex + firstActiveChild, weight, delta, clear, normalize);
        return;
    }

    xanim_delta_t scratch;
    float *blendDelta = clear != qfalse ? delta : scratch;
    XAnimCalcDeltaTree(tree, entry->payload.parent.firstChildIndex + firstActiveChild, firstWeight, blendDelta, qtrue, qtrue);
    XAnimCalcDeltaTree(tree, entry->payload.parent.firstChildIndex + secondActiveChild, secondWeight, blendDelta, qfalse, qtrue);

    for (int32_t child = secondActiveChild + 1; child < entry->childCount; ++child) {
        int32_t childIndex = entry->payload.parent.firstChildIndex + child;
        uint16_t handle = tree->poolNodeHandles[childIndex];
        if (handle != 0) {
            float childWeight = xanim_eval_selected_delta_weight(&xanim_pool[handle].states[xanim_activePoolPayloadSlot]);
            if (childWeight != 0.0f) {
                XAnimCalcDeltaTree(tree, childIndex, childWeight, blendDelta, qfalse, qtrue);
            }
        }
    }

    if (normalize == qfalse) {
        xanim_eval_normalize_delta_translation(delta);
    } else if (clear == qfalse) {
        xanim_eval_merge_weighted_delta(delta, blendDelta, weight);
    } else {
        xanim_eval_scale_weighted_delta(delta, weight);
    }
}

/* Source: CoDUOMP.exe 0x0049b070..0x0049b0f3.
 * Name: same-module Mac symbol XAnimCalcDelta. */
void XAnimCalcDelta(XAnimTree *tree, uint32_t animIndex, vec2_t rotationDelta, vec3_t moveDelta, int32_t weightSelector)
{
    xanim_delta_t delta;

    xanim_evalLeafOutputMode = qfalse;
    xanim_evalPoolWeightSelector = weightSelector;
    XAnimCalcDeltaTree(tree, animIndex, 1.0f, delta, qtrue, qfalse);
    xanim_eval_store_delta_output(delta, rotationDelta, moveDelta, qtrue);
}

/* Source: CoDUOMP.exe 0x0049b100..0x0049b181.
 * Name: same-module Mac symbol XAnimCalcAbsDelta. */
void XAnimCalcAbsDelta(XAnimTree *tree, uint32_t animIndex, vec2_t rotationDelta, vec3_t moveDelta)
{
    xanim_delta_t delta;

    xanim_evalLeafOutputMode = qtrue;
    xanim_evalPoolWeightSelector = 1;
    XAnimCalcDeltaTree(tree, animIndex, 1.0f, delta, qtrue, qfalse);
    xanim_eval_store_delta_output(delta, rotationDelta, moveDelta, qfalse);
}

/* Source: CoDUOMP.exe 0x0049b190..0x0049b262.
 * Name: same-module Mac symbol XAnimGetRelDelta. */
void XAnimGetRelDelta(XAnim *tree, uint32_t animIndex, vec2_t rotationDelta, vec3_t moveDelta, float startTime, float endTime)
{
    xanim_delta_t delta = {0};
    XAnimEntry *entry = &tree->entries[animIndex];

    if (entry->childCount == 0) {
        XAnimParts *record = entry->payload.leafAsset->data.xanimParts;
        if (record->hasDeltaMotion != 0) {
            XAnimCalcRelDeltaParts(record, 1.0f, delta, startTime, endTime);
        }
    }

    xanim_eval_store_delta_output(delta, rotationDelta, moveDelta, qfalse);
}

/* Source: CoDUOMP.exe 0x0049b270..0x0049b361.
 * Name: same-module Mac symbol XAnimGetAbsDelta. */
void XAnimGetAbsDelta(XAnim *tree, uint32_t animIndex, vec2_t rotationDelta, vec3_t moveDelta, float time)
{
    xanim_delta_t delta = {0};
    XAnimEntry *entry = &tree->entries[animIndex];

    if (entry->childCount == 0) {
        XAnimParts *record = entry->payload.leafAsset->data.xanimParts;
        if (record->hasDeltaMotion != 0) {
            XAnimCalcAbsDeltaParts(record, 1.0f, delta, time);
        }
    }

    xanim_eval_store_delta_output(delta, rotationDelta, moveDelta, qfalse);
}

/* Source: CoDUOMP.exe 0x00497ef0..0x00497f58.
 * Name: exact same-module Mac symbol TransformToQuatRefFrame. */
void TransformToQuatRefFrame(const vec2_t rotation, vec2_t vector)
{
#if defined(WINDOWS_BEHAVIOR)
    const long double lane0Squared = (long double)rotation[0] * (long double)rotation[0];
    const long double lengthSquared = (long double)rotation[1] * (long double)rotation[1] + lane0Squared;

    if (lengthSquared != (long double)0.0f) {
        const long double inverseScale = (long double)2.0f / lengthSquared;
        /* 0x00497f1b is the one intentional intermediate binary32 store. */
        const float reflection00 = (float)(lane0Squared * inverseScale);
        const long double reflection01 = ((long double)rotation[0] * (long double)rotation[1]) * inverseScale;
        const float oldLane1 = vector[1];
        const long double newLane0 =
            reflection01 * (long double)oldLane1 + ((long double)1.0f - (long double)reflection00) * (long double)vector[0];
        const long double reflectedLane1 = (long double)reflection00 * (long double)oldLane1 + reflection01 * (long double)vector[0];

        vector[1] = (float)((long double)oldLane1 - reflectedLane1);
        vector[0] = (float)newLane0;
    }
#else
    const float lane0Squared = xanim_eval_mul_store(rotation[0], rotation[0]);
    const float lengthSquared = xanim_eval_madd_store(lane0Squared, rotation[1], rotation[1]);

    if (lengthSquared != 0.0f) {
        const float inverseScale = xanim_eval_div_store(2.0f, lengthSquared);
        const float reflection00 = xanim_eval_mul_store(lane0Squared, inverseScale);
        const float reflection01 = xanim_eval_mul3_store(rotation[0], rotation[1], inverseScale);
        const float oldLane0 = vector[0];
        const float oldLane1 = vector[1];

        vector[1] = xanim_eval_sub_two_products_store(oldLane1, reflection00, oldLane1, reflection01, oldLane0);
        vector[0] = xanim_eval_reflection_lane0_store(reflection01, oldLane1, reflection00, oldLane0);
    }
#endif
}

/* Source: CoDUOMP.exe 0x004980b0..0x00498124.
 * Name: same-module Mac symbol XAnimCalcAbsDeltaParts. */
void XAnimCalcAbsDeltaParts(XAnimParts *record, float weight, float *deltaLanes, float time)
{
    float *delta = deltaLanes;
    const float packedShortScale = XANIM_PACKED_SHORT_SCALE;
    vec2_t rotation;
    vec3_t move;

    XAnimCalcDeltaParts(record, rotation, move, time);
    float scaledWeight = xanim_eval_mul_store(weight, packedShortScale);
    delta[XANIM_DELTA_ROTATION_0] = xanim_eval_madd_store(delta[XANIM_DELTA_ROTATION_0], scaledWeight, rotation[0]);
    delta[XANIM_DELTA_ROTATION_1] = xanim_eval_madd_store(delta[XANIM_DELTA_ROTATION_1], scaledWeight, rotation[1]);
    delta[XANIM_DELTA_TRANSLATION_WEIGHT] += weight;
    delta[XANIM_DELTA_TRANSLATION_0] = xanim_eval_madd_store(delta[XANIM_DELTA_TRANSLATION_0], weight, move[0]);
    delta[XANIM_DELTA_TRANSLATION_1] = xanim_eval_madd_store(delta[XANIM_DELTA_TRANSLATION_1], weight, move[1]);
    delta[XANIM_DELTA_TRANSLATION_2] = xanim_eval_madd_store(delta[XANIM_DELTA_TRANSLATION_2], weight, move[2]);
}

/* Source: CoDUOMP.exe 0x00497f60..0x004980a0.
 * Name: same-module Mac symbol XAnimCalcRelDeltaParts. */
void XAnimCalcRelDeltaParts(XAnimParts *record, float weight, float *deltaLanes, float startTime, float endTime)
{
    float *delta = deltaLanes;
    const float packedShortScale = XANIM_PACKED_SHORT_SCALE;
    vec2_t startRotation;
    vec2_t endRotation;
    vec3_t startMove;
    vec3_t endMove;
    vec3_t moveDelta;

    XAnimCalcDeltaParts(record, startRotation, startMove, startTime);
    XAnimCalcDeltaParts(record, endRotation, endMove, endTime);

    if (record->looped != 0 && endTime < startTime) {
        xanim_translation_stream_t *translation = record->deltaMotion->translation;

        if (translation != NULL) {
            if (translation->frameIndex == 0) {
                endMove[0] = xanim_eval_add_store(endMove[0], translation->data.inlineLane0);
                endMove[1] = xanim_eval_add_store(endMove[1], translation->inlineLanes.lane1);
                endMove[2] = xanim_eval_add_store(endMove[2], translation->inlineLanes.lane2);
            } else {
                const float *lastFrame = translation->data.frames[translation->frameIndex];
                endMove[0] = xanim_eval_add_store(endMove[0], lastFrame[0]);
                endMove[1] = xanim_eval_add_store(endMove[1], lastFrame[1]);
                endMove[2] = xanim_eval_add_store(endMove[2], lastFrame[2]);
            }
        }
    }

    float scaledWeight = xanim_eval_mul_store(weight, packedShortScale);
#if defined(WINDOWS_BEHAVIOR)
    delta[XANIM_DELTA_ROTATION_0] =
        (float)((long double)delta[XANIM_DELTA_ROTATION_0] + (((long double)endRotation[0] * (long double)startRotation[1] -
                                                               (long double)endRotation[1] * (long double)startRotation[0]) *
                                                              (long double)scaledWeight));
    delta[XANIM_DELTA_ROTATION_1] =
        (float)((long double)delta[XANIM_DELTA_ROTATION_1] + (((long double)endRotation[1] * (long double)startRotation[1] +
                                                               (long double)endRotation[0] * (long double)startRotation[0]) *
                                                              (long double)scaledWeight));
#else
    delta[XANIM_DELTA_ROTATION_0] =
        xanim_eval_rotation_cross_store(delta[XANIM_DELTA_ROTATION_0], endRotation, startRotation, scaledWeight);
    delta[XANIM_DELTA_ROTATION_1] = xanim_eval_rotation_dot_store(delta[XANIM_DELTA_ROTATION_1], endRotation, startRotation, scaledWeight);
#endif

    moveDelta[0] = xanim_eval_sub_store(endMove[0], startMove[0]);
    moveDelta[1] = xanim_eval_sub_store(endMove[1], startMove[1]);
    moveDelta[2] = xanim_eval_sub_store(endMove[2], startMove[2]);
    TransformToQuatRefFrame(endRotation, moveDelta);

    delta[XANIM_DELTA_TRANSLATION_WEIGHT] += weight;
    delta[XANIM_DELTA_TRANSLATION_0] = xanim_eval_madd_store(delta[XANIM_DELTA_TRANSLATION_0], weight, moveDelta[0]);
    delta[XANIM_DELTA_TRANSLATION_1] = xanim_eval_madd_store(delta[XANIM_DELTA_TRANSLATION_1], weight, moveDelta[1]);
    delta[XANIM_DELTA_TRANSLATION_2] = xanim_eval_madd_store(delta[XANIM_DELTA_TRANSLATION_2], weight, moveDelta[2]);
}
