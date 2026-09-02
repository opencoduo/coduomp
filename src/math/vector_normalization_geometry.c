#include "q_math.h"

#include "compat/coduo_int32_bits.h"

#include <math.h>
#include <stdint.h>
#include <string.h>

#if defined(WINDOWS_BEHAVIOR) && defined(LINUX_BEHAVIOR)
#error "Select only one of WINDOWS_BEHAVIOR or LINUX_BEHAVIOR"
#elif !defined(WINDOWS_BEHAVIOR) && !defined(LINUX_BEHAVIOR)
#error "Select WINDOWS_BEHAVIOR or LINUX_BEHAVIOR"
#endif

#include "compat/coduo_x87emu.h"

#define Q_RSQRT_MAGIC UINT32_C(0x5f3759df)
#define Q_FLOAT_SIGN_BIT UINT32_C(0x80000000)

/*
 * The four Windows Q_rsqrt bodies are instruction-identical except for the
 * relocated 0.5f and 1.5f addresses:
 *
 *   CoDUOMP.exe                 0x00431020
 *   uo_cgame_mp_x86.dll        0x30001150
 *   uo_ui_mp_x86.dll           0x40001150
 *   uo_game_mp_x86.dll         0x20001150
 *
 * They return the Newton result directly in x87 ST0 under the Windows PC=53
 * policy.  The two Linux bodies use the same bit seed and Newton graph, but
 * store the half-value and final estimate to binary32 under PC=64 before
 * reloading the return value (coduo_lnxded 0x080660af and
 * game.mp.uo.i386.so RVA 0x00039a6f).  The supporting PowerPC executable,
 * cgame, and game modules retain the canonical Q_rsqrt symbol.
 */
#if defined(WINDOWS_BEHAVIOR)
float Q_rsqrt(float value)
{
    const float half = 0.5f;
    const float threeHalves = 1.5f;
    uint32_t bits;
    float estimate;

    memcpy(&bits, &value, sizeof(bits));
    bits = Q_RSQRT_MAGIC - coduo_int32_sar_bits(bits, 1U);
    memcpy(&estimate, &bits, sizeof(estimate));

#if EMULATE_X87
    {
        const x87f estimateWide = x87f_load_f32(estimate);
        const x87f result = x87f_mul(
            estimateWide, x87f_sub(x87f_load_f32(threeHalves),
                                   x87f_mul(x87f_mul(x87f_mul(x87f_load_f32(value), x87f_load_f32(half)), estimateWide), estimateWide)));

        /* A non-x87 ABI cannot carry the original live ST0 result.  Narrow at
         * the public binary32 boundary after reproducing its PC=53 graph. */
        return x87f_store_f32(result);
    }
#elif (defined(__i386__) || defined(__x86_64__)) && (defined(__GNUC__) || defined(__clang__))
    {
        register float result;

        /* Keep the retained value*0.5, two estimate factors, reverse
         * subtraction, and final estimate factor in ST0 through the return. */
        __asm__ __volatile__("flds %[value]\n\t"
                             "fmuls %[half]\n\t"
                             "fmuls %[estimate]\n\t"
                             "fmuls %[estimate]\n\t"
                             "fsubrs %[threeHalves]\n\t"
                             "fmuls %[estimate]"
                             : "=t"(result)
                             : [value] "m"(value), [half] "m"(half), [estimate] "m"(estimate), [threeHalves] "m"(threeHalves));
        return result;
    }
#else
    /* The i386 excess-precision return intentionally has no binary32 spill. */
    return estimate * (threeHalves - value * half * estimate * estimate);
#endif
}
#else
float Q_rsqrt(float value)
{
    const float half = 0.5f;
    const float threeHalves = 1.5f;
    uint32_t bits;
    float estimate;
    float halfValue;

#if EMULATE_X87
    halfValue = x87f_store_f32(x87f_mul(x87f_load_f32(value), x87f_load_f32(half)));
#elif (defined(__i386__) || defined(__x86_64__)) && (defined(__GNUC__) || defined(__clang__))
    __asm__ __volatile__("flds %1\n\t"
                         "fmuls %2\n\t"
                         "fstps %0"
                         : "=m"(halfValue)
                         : "m"(value), "m"(half)
                         : "st", "memory");
#else
    halfValue = value * half;
#endif

    memcpy(&bits, &value, sizeof(bits));
    bits = Q_RSQRT_MAGIC - coduo_int32_sar_bits(bits, 1U);
    memcpy(&estimate, &bits, sizeof(estimate));

#if EMULATE_X87
    {
        const x87f estimateWide = x87f_load_f32(estimate);

        estimate = x87f_store_f32(x87f_mul(
            estimateWide, x87f_sub(x87f_load_f32(threeHalves), x87f_mul(x87f_mul(x87f_load_f32(halfValue), estimateWide), estimateWide))));
    }
#elif (defined(__i386__) || defined(__x86_64__)) && (defined(__GNUC__) || defined(__clang__))
    __asm__ __volatile__("flds %[halfValue]\n\t"
                         "fmuls %[estimate]\n\t"
                         "fmuls %[estimate]\n\t"
                         "flds %[threeHalves]\n\t"
                         "fsubp %%st, %%st(1)\n\t"
                         "flds %[estimate]\n\t"
                         "fmulp %%st, %%st(1)\n\t"
                         "fstps %[estimate]"
                         : [estimate] "+m"(estimate)
                         : [halfValue] "m"(halfValue), [threeHalves] "m"(threeHalves)
                         : "st", "st(1)", "memory");
#else
    estimate = estimate * (threeHalves - halfValue * estimate * estimate);
#endif
    return estimate;
}
#endif

/*
 * Each Windows function below is byte-identical across CoDUOMP.exe, cgame,
 * UI, and game after normalizing only relocated calls/constants.  Addresses:
 *
 *                         EXE          cgame       UI          game
 *   VectorNormalize       0x004315a0   0x30049700  0x400016d0  0x20016750
 *   VectorNormalize2D     0x00431620   0x30049780  0x40001750  0x200167d0
 *   VectorNormalize4D     0x00431690   0x300497f0  0x400017c0  0x20016840
 *   VectorNormalizeFast   0x00431730   0x30049890  0x40001860  0x200168e0
 *   VectorNormalize2      0x004317c0   0x30049920  0x400018f0  0x20016970
 *
 * VectorNormalize and VectorNormalize2 round the complete three-term squared
 * sum to binary32, pass that value through sqrt, round the length to binary32,
 * and round the reciprocal and each scaled lane to binary32.  A NaN length
 * follows the nonzero branch.  VectorNormalize2 zeroes all output lanes only
 * for an exactly zero length.
 *
 * Linux retains the same store boundaries and branch behavior at engine
 * addresses 0x08066755/0x080667f2/0x0806686a/0x0806692c/0x080669a0 and game
 * RVAs 0x0003a1c3/0x0003a278/0x0003a308/0x0003a3e2/0x0003a466.  The Linux
 * symbols establish VectorNormalize4D as the canonical name; UI's
 * Vector4Normalize and the engine reconstruction's VectorNormalize4 were
 * local naming drift.  Every dimensional body rounds the complete squared
 * sum, square root, reciprocal, and scaled lanes to binary32 at the same
 * points.  Returning float is therefore the common contract even though the
 * i386 ABI carries that already-rounded value in x87 ST0.
 *
 * Linux native bodies evaluate algebra under PC=64; the Windows bodies use
 * PC=53.  Linux VectorNormalizeFast calls its Q_rsqrt body while the optimized
 * Windows bodies inline it.  Both store the half value and final estimate, but
 * the retained multiply chain is genuinely reassociated: Windows uses
 * (estimate * estimate) * halfValue, while Linux uses
 * (halfValue * estimate) * estimate.  The narrow behavior gates below preserve
 * that difference without duplicating the surrounding function.
 */
float VectorNormalize2D(vec2_t vector)
{
    float lengthSquared;
    float length;

#if EMULATE_X87
    lengthSquared = x87f_store_f32(x87f_add(x87f_mul(x87f_load_f32(vector[0]), x87f_load_f32(vector[0])),
                                            x87f_mul(x87f_load_f32(vector[1]), x87f_load_f32(vector[1]))));
    length = (float)sqrt(x87f_store_f64(x87f_load_f32(lengthSquared)));
    if (length != 0.0f) {
        const float inverseLength = x87f_store_f32(x87f_div(x87f_load_f32(1.0f), x87f_load_f32(length)));

        vector[0] = x87f_store_f32(x87f_mul(x87f_load_f32(vector[0]), x87f_load_f32(inverseLength)));
        vector[1] = x87f_store_f32(x87f_mul(x87f_load_f32(vector[1]), x87f_load_f32(inverseLength)));
    }
#else
    lengthSquared = vector[0] * vector[0] + vector[1] * vector[1];
    length = (float)sqrt((double)lengthSquared);
    if (length != 0.0f) {
        const float inverseLength = 1.0f / length;

        vector[0] = vector[0] * inverseLength;
        vector[1] = vector[1] * inverseLength;
    }
#endif
    return length;
}

float VectorNormalize(vec3_t vector)
{
    float lengthSquared;
    float length;

#if EMULATE_X87
    lengthSquared = x87f_store_f32(x87f_add(x87f_add(x87f_mul(x87f_load_f32(vector[0]), x87f_load_f32(vector[0])),
                                                     x87f_mul(x87f_load_f32(vector[1]), x87f_load_f32(vector[1]))),
                                            x87f_mul(x87f_load_f32(vector[2]), x87f_load_f32(vector[2]))));
    length = (float)sqrt(x87f_store_f64(x87f_load_f32(lengthSquared)));
    if (length != 0.0f) {
        const float inverseLength = x87f_store_f32(x87f_div(x87f_load_f32(1.0f), x87f_load_f32(length)));

        vector[0] = x87f_store_f32(x87f_mul(x87f_load_f32(vector[0]), x87f_load_f32(inverseLength)));
        vector[1] = x87f_store_f32(x87f_mul(x87f_load_f32(vector[1]), x87f_load_f32(inverseLength)));
        vector[2] = x87f_store_f32(x87f_mul(x87f_load_f32(vector[2]), x87f_load_f32(inverseLength)));
    }
#else
    lengthSquared = ((vector[0] * vector[0]) + (vector[1] * vector[1])) + (vector[2] * vector[2]);
    length = (float)sqrt((double)lengthSquared);
    if (length != 0.0f) {
        const float inverseLength = 1.0f / length;

        vector[0] = vector[0] * inverseLength;
        vector[1] = vector[1] * inverseLength;
        vector[2] = vector[2] * inverseLength;
    }
#endif
    return length;
}

float VectorNormalize4D(vec4_t vector)
{
    float lengthSquared;
    float length;

#if EMULATE_X87
    x87f sum = x87f_add(x87f_mul(x87f_load_f32(vector[0]), x87f_load_f32(vector[0])),
                        x87f_mul(x87f_load_f32(vector[1]), x87f_load_f32(vector[1])));
    sum = x87f_add(sum, x87f_mul(x87f_load_f32(vector[2]), x87f_load_f32(vector[2])));
    sum = x87f_add(sum, x87f_mul(x87f_load_f32(vector[3]), x87f_load_f32(vector[3])));
    lengthSquared = x87f_store_f32(sum);
    length = (float)sqrt(x87f_store_f64(x87f_load_f32(lengthSquared)));
    if (length != 0.0f) {
        const float inverseLength = x87f_store_f32(x87f_div(x87f_load_f32(1.0f), x87f_load_f32(length)));

        vector[0] = x87f_store_f32(x87f_mul(x87f_load_f32(vector[0]), x87f_load_f32(inverseLength)));
        vector[1] = x87f_store_f32(x87f_mul(x87f_load_f32(vector[1]), x87f_load_f32(inverseLength)));
        vector[2] = x87f_store_f32(x87f_mul(x87f_load_f32(vector[2]), x87f_load_f32(inverseLength)));
        vector[3] = x87f_store_f32(x87f_mul(x87f_load_f32(vector[3]), x87f_load_f32(inverseLength)));
    }
#else
    lengthSquared = (((vector[0] * vector[0]) + (vector[1] * vector[1])) + (vector[2] * vector[2])) + (vector[3] * vector[3]);
    length = (float)sqrt((double)lengthSquared);
    if (length != 0.0f) {
        const float inverseLength = 1.0f / length;

        vector[0] = vector[0] * inverseLength;
        vector[1] = vector[1] * inverseLength;
        vector[2] = vector[2] * inverseLength;
        vector[3] = vector[3] * inverseLength;
    }
#endif
    return length;
}

void VectorNormalizeFast(vec3_t vector)
{
    const float half = 0.5f;
    const float threeHalves = 1.5f;
    float lengthSquared;
    float halfLengthSquared;
    uint32_t bits;
    float inverseLength;

#if EMULATE_X87
    lengthSquared = x87f_store_f32(x87f_add(x87f_add(x87f_mul(x87f_load_f32(vector[0]), x87f_load_f32(vector[0])),
                                                     x87f_mul(x87f_load_f32(vector[1]), x87f_load_f32(vector[1]))),
                                            x87f_mul(x87f_load_f32(vector[2]), x87f_load_f32(vector[2]))));
    halfLengthSquared = x87f_store_f32(x87f_mul(x87f_load_f32(lengthSquared), x87f_load_f32(half)));
    memcpy(&bits, &lengthSquared, sizeof(bits));
    bits = Q_RSQRT_MAGIC - coduo_int32_sar_bits(bits, 1U);
    memcpy(&inverseLength, &bits, sizeof(inverseLength));
    {
        const x87f estimate = x87f_load_f32(inverseLength);

        inverseLength =
            x87f_store_f32(x87f_mul(estimate, x87f_sub(x87f_load_f32(threeHalves),
#if defined(WINDOWS_BEHAVIOR)
                                                       x87f_mul(x87f_mul(estimate, estimate), x87f_load_f32(halfLengthSquared)))));
#else
                                                       x87f_mul(x87f_mul(x87f_load_f32(halfLengthSquared), estimate), estimate))));
#endif
    }
    vector[0] = x87f_store_f32(x87f_mul(x87f_load_f32(vector[0]), x87f_load_f32(inverseLength)));
    vector[1] = x87f_store_f32(x87f_mul(x87f_load_f32(vector[1]), x87f_load_f32(inverseLength)));
    vector[2] = x87f_store_f32(x87f_mul(x87f_load_f32(vector[2]), x87f_load_f32(inverseLength)));
#elif (defined(__i386__) || defined(__x86_64__)) && (defined(__GNUC__) || defined(__clang__))
    lengthSquared = ((vector[0] * vector[0]) + (vector[1] * vector[1])) + (vector[2] * vector[2]);
    __asm__ __volatile__("flds %[lengthSquared]\n\t"
                         "fmuls %[half]\n\t"
                         "fstps %[halfLengthSquared]"
                         : [halfLengthSquared] "=m"(halfLengthSquared)
                         : [lengthSquared] "m"(lengthSquared), [half] "m"(half)
                         : "st", "memory");
    memcpy(&bits, &lengthSquared, sizeof(bits));
    bits = Q_RSQRT_MAGIC - coduo_int32_sar_bits(bits, 1U);
    memcpy(&inverseLength, &bits, sizeof(inverseLength));
#if defined(WINDOWS_BEHAVIOR)
    __asm__ __volatile__("flds %[estimate]\n\t"
                         "fmuls %[estimate]\n\t"
                         "fmuls %[halfLengthSquared]\n\t"
                         "fsubrs %[threeHalves]\n\t"
                         "fmuls %[estimate]\n\t"
                         "fstps %[estimate]"
                         : [estimate] "+m"(inverseLength)
                         : [halfLengthSquared] "m"(halfLengthSquared), [threeHalves] "m"(threeHalves)
                         : "st", "memory");
#else
    __asm__ __volatile__("flds %[halfLengthSquared]\n\t"
                         "fmuls %[estimate]\n\t"
                         "fmuls %[estimate]\n\t"
                         "fsubrs %[threeHalves]\n\t"
                         "fmuls %[estimate]\n\t"
                         "fstps %[estimate]"
                         : [estimate] "+m"(inverseLength)
                         : [halfLengthSquared] "m"(halfLengthSquared), [threeHalves] "m"(threeHalves)
                         : "st", "memory");
#endif
    vector[0] = vector[0] * inverseLength;
    vector[1] = vector[1] * inverseLength;
    vector[2] = vector[2] * inverseLength;
#else
    lengthSquared = ((vector[0] * vector[0]) + (vector[1] * vector[1])) + (vector[2] * vector[2]);
    halfLengthSquared = lengthSquared * half;
    memcpy(&bits, &lengthSquared, sizeof(bits));
    bits = Q_RSQRT_MAGIC - coduo_int32_sar_bits(bits, 1U);
    memcpy(&inverseLength, &bits, sizeof(inverseLength));
#if defined(WINDOWS_BEHAVIOR)
    inverseLength = inverseLength * (threeHalves - (inverseLength * inverseLength) * halfLengthSquared);
#else
    inverseLength = inverseLength * (threeHalves - (halfLengthSquared * inverseLength) * inverseLength);
#endif
    vector[0] = vector[0] * inverseLength;
    vector[1] = vector[1] * inverseLength;
    vector[2] = vector[2] * inverseLength;
#endif
}

float VectorNormalize2(const vec3_t input, vec3_t output)
{
    float lengthSquared;
    float length;

#if EMULATE_X87
    lengthSquared = x87f_store_f32(x87f_add(
        x87f_add(x87f_mul(x87f_load_f32(input[0]), x87f_load_f32(input[0])), x87f_mul(x87f_load_f32(input[1]), x87f_load_f32(input[1]))),
        x87f_mul(x87f_load_f32(input[2]), x87f_load_f32(input[2]))));
    length = (float)sqrt(x87f_store_f64(x87f_load_f32(lengthSquared)));
    if (length != 0.0f) {
        const float inverseLength = x87f_store_f32(x87f_div(x87f_load_f32(1.0f), x87f_load_f32(length)));

        output[0] = x87f_store_f32(x87f_mul(x87f_load_f32(input[0]), x87f_load_f32(inverseLength)));
        output[1] = x87f_store_f32(x87f_mul(x87f_load_f32(input[1]), x87f_load_f32(inverseLength)));
        output[2] = x87f_store_f32(x87f_mul(x87f_load_f32(input[2]), x87f_load_f32(inverseLength)));
    } else {
        output[2] = 0.0f;
        output[1] = 0.0f;
        output[0] = 0.0f;
    }
#else
    lengthSquared = ((input[0] * input[0]) + (input[1] * input[1])) + (input[2] * input[2]);
    length = (float)sqrt((double)lengthSquared);
    if (length != 0.0f) {
        const float inverseLength = 1.0f / length;

        output[0] = input[0] * inverseLength;
        output[1] = input[1] * inverseLength;
        output[2] = input[2] * inverseLength;
    } else {
        output[2] = 0.0f;
        output[1] = 0.0f;
        output[0] = 0.0f;
    }
#endif
    return length;
}
/*
 * The four Windows MakeNormalVectors bodies are byte-identical at
 * 0x00431be0/0x30049d40/0x40001d10/0x20016d90.  Windows folds the dot in
 * x,z,y order under PC=53 and negates the seed through x87.  Linux folds in
 * x,y,z order under PC=64 and toggles the seed sign bit as an integer at
 * coduo_lnxded 0x08066ef7 and game RVA 0x0003a9f5.  Both then perform three
 * independently stored Gram-Schmidt updates, normalize right, and compute
 * CrossProduct(right, forward, up).
 */
#if defined(WINDOWS_BEHAVIOR)
void MakeNormalVectors(const vec3_t forward, vec3_t right, vec3_t up)
{
    float dot;
    float projection;

    right[1] = -forward[0];
    right[2] = forward[1];
    right[0] = forward[2];
#if (defined(__i386__) || defined(__x86_64__)) && (defined(__GNUC__) || defined(__clang__))
    __asm__ __volatile__("flds %[forwardX]\n\t"
                         "fmuls %[rightX]\n\t"
                         "flds %[forwardZ]\n\t"
                         "fmuls %[rightZ]\n\t"
                         "faddp %%st, %%st(1)\n\t"
                         "flds %[rightY]\n\t"
                         "fmuls %[forwardY]\n\t"
                         "faddp %%st, %%st(1)\n\t"
                         "fstps %[dot]"
                         : [dot] "=m"(dot)
                         : [forwardX] "m"(forward[0]), [forwardY] "m"(forward[1]), [forwardZ] "m"(forward[2]), [rightX] "m"(right[0]),
                           [rightY] "m"(right[1]), [rightZ] "m"(right[2])
                         : "st", "st(1)", "memory");
#else
    dot = (float)(((double)forward[0] * right[0] + (double)forward[2] * right[2]) + (double)right[1] * forward[1]);
#endif
    projection = -dot;
#if (defined(__i386__) || defined(__x86_64__)) && (defined(__GNUC__) || defined(__clang__))
    __asm__ __volatile__("flds %[projection]\n\t"
                         "fmuls %[forward]\n\t"
                         "fadds %[right]\n\t"
                         "fstps %[right]"
                         : [right] "+m"(right[0])
                         : [projection] "m"(projection), [forward] "m"(forward[0])
                         : "st", "memory");
    __asm__ __volatile__("flds %[projection]\n\t"
                         "fmuls %[forward]\n\t"
                         "fadds %[right]\n\t"
                         "fstps %[right]"
                         : [right] "+m"(right[1])
                         : [projection] "m"(projection), [forward] "m"(forward[1])
                         : "st", "memory");
    __asm__ __volatile__("flds %[projection]\n\t"
                         "fmuls %[forward]\n\t"
                         "fadds %[right]\n\t"
                         "fstps %[right]"
                         : [right] "+m"(right[2])
                         : [projection] "m"(projection), [forward] "m"(forward[2])
                         : "st", "memory");
#else
    right[0] = (float)((double)projection * forward[0] + right[0]);
    right[1] = (float)((double)projection * forward[1] + right[1]);
    right[2] = (float)((double)projection * forward[2] + right[2]);
#endif
    (void)VectorNormalize(right);
    CrossProduct(right, forward, up);
}
#else
void MakeNormalVectors(const vec3_t forward, vec3_t right, vec3_t up)
{
    uint32_t bits;
    float dot;

    memcpy(&bits, &forward[0], sizeof(bits));
    bits ^= Q_FLOAT_SIGN_BIT;
    memcpy(&right[1], &bits, sizeof(bits));
    memcpy(&right[2], &forward[1], sizeof(right[2]));
    memcpy(&right[0], &forward[2], sizeof(right[0]));

#if EMULATE_X87
    dot = x87f_store_f32(x87f_add(x87f_add(x87f_mul(x87f_load_f32(right[0]), x87f_load_f32(forward[0])),
                                           x87f_mul(x87f_load_f32(right[1]), x87f_load_f32(forward[1]))),
                                  x87f_mul(x87f_load_f32(right[2]), x87f_load_f32(forward[2]))));
    right[0] = x87f_store_f32(x87f_add(x87f_mul(x87f_neg(x87f_load_f32(dot)), x87f_load_f32(forward[0])), x87f_load_f32(right[0])));
    right[1] = x87f_store_f32(x87f_add(x87f_mul(x87f_neg(x87f_load_f32(dot)), x87f_load_f32(forward[1])), x87f_load_f32(right[1])));
    right[2] = x87f_store_f32(x87f_add(x87f_mul(x87f_neg(x87f_load_f32(dot)), x87f_load_f32(forward[2])), x87f_load_f32(right[2])));
#elif (defined(__i386__) || defined(__x86_64__)) && (defined(__GNUC__) || defined(__clang__))
    __asm__ __volatile__("flds %[rightX]\n\t"
                         "fmuls %[forwardX]\n\t"
                         "flds %[rightY]\n\t"
                         "fmuls %[forwardY]\n\t"
                         "faddp %%st, %%st(1)\n\t"
                         "flds %[rightZ]\n\t"
                         "fmuls %[forwardZ]\n\t"
                         "faddp %%st, %%st(1)\n\t"
                         "fstps %[dot]"
                         : [dot] "=m"(dot)
                         : [forwardX] "m"(forward[0]), [forwardY] "m"(forward[1]), [forwardZ] "m"(forward[2]), [rightX] "m"(right[0]),
                           [rightY] "m"(right[1]), [rightZ] "m"(right[2])
                         : "st", "st(1)", "memory");
    __asm__ __volatile__("flds %[dot]\n\t"
                         "fchs\n\t"
                         "fmuls %[forward]\n\t"
                         "flds %[right]\n\t"
                         "faddp %%st, %%st(1)\n\t"
                         "fstps %[right]"
                         : [right] "+m"(right[0])
                         : [dot] "m"(dot), [forward] "m"(forward[0])
                         : "st", "st(1)", "memory");
    __asm__ __volatile__("flds %[dot]\n\t"
                         "fchs\n\t"
                         "fmuls %[forward]\n\t"
                         "flds %[right]\n\t"
                         "faddp %%st, %%st(1)\n\t"
                         "fstps %[right]"
                         : [right] "+m"(right[1])
                         : [dot] "m"(dot), [forward] "m"(forward[1])
                         : "st", "st(1)", "memory");
    __asm__ __volatile__("flds %[dot]\n\t"
                         "fchs\n\t"
                         "fmuls %[forward]\n\t"
                         "flds %[right]\n\t"
                         "faddp %%st, %%st(1)\n\t"
                         "fstps %[right]"
                         : [right] "+m"(right[2])
                         : [dot] "m"(dot), [forward] "m"(forward[2])
                         : "st", "st(1)", "memory");
#else
    dot = ((right[0] * forward[0]) + (right[1] * forward[1])) + (right[2] * forward[2]);
    right[0] = (-dot) * forward[0] + right[0];
    right[1] = (-dot) * forward[1] + right[1];
    right[2] = (-dot) * forward[2] + right[2];
#endif
    (void)VectorNormalize(right);
    CrossProduct(right, forward, up);
}
#endif

/*
 * PerpendicularVector is byte-identical within the four Windows targets at
 * 0x00432270/0x3004a3d0/0x400023a0/0x20017420.  Those bodies unroll the scan
 * and widen exact binary32 absolute values into binary64 scratch slots.  Linux
 * scans the same three lanes in a loop at engine 0x080676b4 and game RVA
 * 0x0003b28f.  Both choose only a strict decrease, then call the shared
 * projection and normalization functions; neither detail changes the result.
 */
void PerpendicularVector(vec3_t output, const vec3_t source)
{
    float minimum = 1.0f;
    int32_t position = 0;
    vec3_t axis = {0.0f, 0.0f, 0.0f};

    for (int32_t lane = 0; lane < 3; ++lane) {
#if EMULATE_X87
        if (x87f_lt(x87f_abs(x87f_load_f32(source[lane])), x87f_load_f32(minimum))) {
            minimum = x87f_store_f32(x87f_abs(x87f_load_f32(source[lane])));
            position = lane;
        }
#else
        if (isless(fabsf(source[lane]), minimum)) {
            minimum = fabsf(source[lane]);
            position = lane;
        }
#endif
    }
    axis[position] = 1.0f;
    ProjectPointOnPlane(output, axis, source);
    (void)VectorNormalize(output);
}

/*
 * PlaneFromPoints is byte-identical within Windows at
 * 0x00434530/0x3004c690/0x400046a0/0x200196e0.  Linux retains the same edge,
 * cross-product, zero-test, and distance behavior at engine 0x0806a964 and
 * game RVA 0x0003e7b8.  Windows inlines the same CrossProduct graph that Linux
 * calls; the shared x87 backend supplies PC=53 or PC=64.  On the false path
 * every original leaves plane[3] untouched.  NaN normalization follows the
 * true path.
 */
int32_t PlaneFromPoints(vec4_t plane, const vec3_t point0, const vec3_t point1, const vec3_t point2)
{
    vec3_t edge0;
    vec3_t edge1;

#if EMULATE_X87
    for (int32_t lane = 0; lane < 3; ++lane) {
        edge0[lane] = x87f_store_f32(x87f_sub(x87f_load_f32(point1[lane]), x87f_load_f32(point0[lane])));
        edge1[lane] = x87f_store_f32(x87f_sub(x87f_load_f32(point2[lane]), x87f_load_f32(point0[lane])));
    }
#else
    edge0[0] = point1[0] - point0[0];
    edge0[1] = point1[1] - point0[1];
    edge0[2] = point1[2] - point0[2];
    edge1[0] = point2[0] - point0[0];
    edge1[1] = point2[1] - point0[1];
    edge1[2] = point2[2] - point0[2];
#endif
    CrossProduct(edge1, edge0, plane);
    if (VectorNormalize(plane) == 0.0f) {
        return 0;
    }
#if EMULATE_X87
    plane[3] = x87f_store_f32(x87f_add(
        x87f_add(x87f_mul(x87f_load_f32(point0[0]), x87f_load_f32(plane[0])), x87f_mul(x87f_load_f32(point0[1]), x87f_load_f32(plane[1]))),
        x87f_mul(x87f_load_f32(point0[2]), x87f_load_f32(plane[2]))));
#else
    plane[3] = ((point0[0] * plane[0]) + (point0[1] * plane[1])) + (point0[2] * plane[2]);
#endif
    return 1;
}

/* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
#if defined(WINDOWS_BEHAVIOR)
void ProjectPointOnPlane(vec3_t output, const vec3_t point, const vec3_t normal)
{
    float lengthSquared;
    float pointScale;
    vec3_t scaledNormal;

#if (defined(__i386__) || defined(__x86_64__)) && (defined(__GNUC__) || defined(__clang__))
    __asm__ __volatile__("flds %[x]\n\t"
                         "fmuls %[x]\n\t"
                         "flds %[y]\n\t"
                         "fmuls %[y]\n\t"
                         "faddp %%st, %%st(1)\n\t"
                         "flds %[z]\n\t"
                         "fmuls %[z]\n\t"
                         "faddp %%st, %%st(1)\n\t"
                         "fstps %[result]"
                         : [result] "=m"(lengthSquared)
                         : [x] "m"(normal[0]), [y] "m"(normal[1]), [z] "m"(normal[2])
                         : "st", "st(1)", "memory");
#else
    lengthSquared = (float)(((double)normal[0] * normal[0] + (double)normal[1] * normal[1]) + (double)normal[2] * normal[2]);
#endif
    const float inverseLengthSquared = (float)((double)1.0f / lengthSquared);

#if (defined(__i386__) || defined(__x86_64__)) && (defined(__GNUC__) || defined(__clang__))
    __asm__ __volatile__("flds %[pointZ]\n\t"
                         "fmuls %[normalZ]\n\t"
                         "flds %[pointY]\n\t"
                         "fmuls %[normalY]\n\t"
                         "faddp %%st, %%st(1)\n\t"
                         "flds %[pointX]\n\t"
                         "fmuls %[normalX]\n\t"
                         "faddp %%st, %%st(1)\n\t"
                         "fmuls %[inverse]\n\t"
                         "fstps %[result]"
                         : [result] "=m"(pointScale)
                         : [pointX] "m"(point[0]), [pointY] "m"(point[1]), [pointZ] "m"(point[2]), [normalX] "m"(normal[0]),
                           [normalY] "m"(normal[1]), [normalZ] "m"(normal[2]), [inverse] "m"(inverseLengthSquared)
                         : "st", "st(1)", "memory");

    __asm__ __volatile__("flds %[inverse]\n\t"
                         "fmuls %[normal]\n\t"
                         "fstps %[result]"
                         : [result] "=m"(scaledNormal[0])
                         : [inverse] "m"(inverseLengthSquared), [normal] "m"(normal[0])
                         : "st", "memory");
    __asm__ __volatile__("flds %[inverse]\n\t"
                         "fmuls %[normal]\n\t"
                         "fstps %[result]"
                         : [result] "=m"(scaledNormal[1])
                         : [inverse] "m"(inverseLengthSquared), [normal] "m"(normal[1])
                         : "st", "memory");
    __asm__ __volatile__("flds %[inverse]\n\t"
                         "fmuls %[normal]\n\t"
                         "fstps %[result]"
                         : [result] "=m"(scaledNormal[2])
                         : [inverse] "m"(inverseLengthSquared), [normal] "m"(normal[2])
                         : "st", "memory");

    __asm__ __volatile__("flds %[scaled]\n\t"
                         "fmuls %[scale]\n\t"
                         "fsubrs %[point]\n\t"
                         "fstps %[result]"
                         : [result] "=m"(output[0])
                         : [scaled] "m"(scaledNormal[0]), [scale] "m"(pointScale), [point] "m"(point[0])
                         : "st", "memory");
    __asm__ __volatile__("flds %[scaled]\n\t"
                         "fmuls %[scale]\n\t"
                         "fsubrs %[point]\n\t"
                         "fstps %[result]"
                         : [result] "=m"(output[1])
                         : [scaled] "m"(scaledNormal[1]), [scale] "m"(pointScale), [point] "m"(point[1])
                         : "st", "memory");
    __asm__ __volatile__("flds %[scaled]\n\t"
                         "fmuls %[scale]\n\t"
                         "fsubrs %[point]\n\t"
                         "fstps %[result]"
                         : [result] "=m"(output[2])
                         : [scaled] "m"(scaledNormal[2]), [scale] "m"(pointScale), [point] "m"(point[2])
                         : "st", "memory");
#else
    pointScale =
        (float)((((double)point[2] * normal[2] + (double)point[1] * normal[1]) + (double)point[0] * normal[0]) * inverseLengthSquared);

    scaledNormal[0] = (float)((double)inverseLengthSquared * normal[0]);
    scaledNormal[1] = (float)((double)inverseLengthSquared * normal[1]);
    scaledNormal[2] = (float)((double)inverseLengthSquared * normal[2]);
    output[0] = (float)((double)point[0] - (double)scaledNormal[0] * pointScale);
    output[1] = (float)((double)point[1] - (double)scaledNormal[1] * pointScale);
    output[2] = (float)((double)point[2] - (double)scaledNormal[2] * pointScale);
#endif
}
#else
void ProjectPointOnPlane(vec3_t output, const vec3_t point, const vec3_t normal)
{
#if EMULATE_X87
    const float lengthSquared = x87f_store_f32(x87f_add(x87f_add(x87f_mul(x87f_load_f32(normal[0]), x87f_load_f32(normal[0])),
                                                                 x87f_mul(x87f_load_f32(normal[1]), x87f_load_f32(normal[1]))),
                                                        x87f_mul(x87f_load_f32(normal[2]), x87f_load_f32(normal[2]))));
    const float inverseLengthSquared = x87f_store_f32(x87f_div(x87f_load_f32(1.0f), x87f_load_f32(lengthSquared)));
    const float pointScale = x87f_store_f32(x87f_mul(x87f_add(x87f_add(x87f_mul(x87f_load_f32(normal[0]), x87f_load_f32(point[0])),
                                                                       x87f_mul(x87f_load_f32(normal[1]), x87f_load_f32(point[1]))),
                                                              x87f_mul(x87f_load_f32(normal[2]), x87f_load_f32(point[2]))),
                                                     x87f_load_f32(inverseLengthSquared)));
    const float scaled0 = x87f_store_f32(x87f_mul(x87f_load_f32(normal[0]), x87f_load_f32(inverseLengthSquared)));
    const float scaled1 = x87f_store_f32(x87f_mul(x87f_load_f32(normal[1]), x87f_load_f32(inverseLengthSquared)));
    const float scaled2 = x87f_store_f32(x87f_mul(x87f_load_f32(normal[2]), x87f_load_f32(inverseLengthSquared)));

    output[0] = x87f_store_f32(x87f_sub(x87f_load_f32(point[0]), x87f_mul(x87f_load_f32(pointScale), x87f_load_f32(scaled0))));
    output[1] = x87f_store_f32(x87f_sub(x87f_load_f32(point[1]), x87f_mul(x87f_load_f32(pointScale), x87f_load_f32(scaled1))));
    output[2] = x87f_store_f32(x87f_sub(x87f_load_f32(point[2]), x87f_mul(x87f_load_f32(pointScale), x87f_load_f32(scaled2))));
#elif (defined(__i386__) || defined(__x86_64__)) && (defined(__GNUC__) || defined(__clang__))
    float lengthSquared;
    float inverseLengthSquared;
    float pointScale;
    vec3_t scaledNormal;

    __asm__ __volatile__("flds 0(%[normal])\n\t"
                         "fmuls 0(%[normal])\n\t"
                         "flds 4(%[normal])\n\t"
                         "fmuls 4(%[normal])\n\t"
                         "faddp %%st, %%st(1)\n\t"
                         "flds 8(%[normal])\n\t"
                         "fmuls 8(%[normal])\n\t"
                         "faddp %%st, %%st(1)\n\t"
                         "fstps %[result]"
                         : [result] "=m"(lengthSquared)
                         : [normal] "r"(normal)
                         : "st", "st(1)", "memory");
    __asm__ __volatile__("fld1\n\t"
                         "fdivs %[lengthSquared]\n\t"
                         "fstps %[inverse]"
                         : [inverse] "=m"(inverseLengthSquared)
                         : [lengthSquared] "m"(lengthSquared)
                         : "st", "memory");
    __asm__ __volatile__("flds 0(%[normal])\n\t"
                         "fmuls 0(%[point])\n\t"
                         "flds 4(%[normal])\n\t"
                         "fmuls 4(%[point])\n\t"
                         "faddp %%st, %%st(1)\n\t"
                         "flds 8(%[normal])\n\t"
                         "fmuls 8(%[point])\n\t"
                         "faddp %%st, %%st(1)\n\t"
                         "fmuls %[inverse]\n\t"
                         "fstps %[scale]"
                         : [scale] "=m"(pointScale)
                         : [normal] "r"(normal), [point] "r"(point), [inverse] "m"(inverseLengthSquared)
                         : "st", "st(1)", "memory");
    __asm__ __volatile__("flds 0(%[normal])\n\t"
                         "fmuls %[inverse]\n\t"
                         "fstps 0(%[scaled])\n\t"
                         "flds 4(%[normal])\n\t"
                         "fmuls %[inverse]\n\t"
                         "fstps 4(%[scaled])\n\t"
                         "flds 8(%[normal])\n\t"
                         "fmuls %[inverse]\n\t"
                         "fstps 8(%[scaled])"
                         :
                         : [normal] "r"(normal), [scaled] "r"(scaledNormal), [inverse] "m"(inverseLengthSquared)
                         : "st", "memory");
    __asm__ __volatile__("flds %[scale]\n\t"
                         "fmuls 0(%[scaled])\n\t"
                         "flds 0(%[point])\n\t"
                         "fsubp %%st, %%st(1)\n\t"
                         "fstps 0(%[output])\n\t"
                         "flds %[scale]\n\t"
                         "fmuls 4(%[scaled])\n\t"
                         "flds 4(%[point])\n\t"
                         "fsubp %%st, %%st(1)\n\t"
                         "fstps 4(%[output])\n\t"
                         "flds %[scale]\n\t"
                         "fmuls 8(%[scaled])\n\t"
                         "flds 8(%[point])\n\t"
                         "fsubp %%st, %%st(1)\n\t"
                         "fstps 8(%[output])"
                         :
                         : [scale] "m"(pointScale), [scaled] "r"(scaledNormal), [point] "r"(point), [output] "r"(output)
                         : "st", "st(1)", "memory");
#else
    const float lengthSquared = ((normal[0] * normal[0]) + (normal[1] * normal[1])) + (normal[2] * normal[2]);
    const float inverseLengthSquared = 1.0f / lengthSquared;
    const float pointScale = (((normal[0] * point[0]) + (normal[1] * point[1])) + (normal[2] * point[2])) * inverseLengthSquared;
    const float scaled0 = normal[0] * inverseLengthSquared;
    const float scaled1 = normal[1] * inverseLengthSquared;
    const float scaled2 = normal[2] * inverseLengthSquared;

    output[0] = point[0] - pointScale * scaled0;
    output[1] = point[1] - pointScale * scaled1;
    output[2] = point[2] - pointScale * scaled2;
#endif
}
#endif
