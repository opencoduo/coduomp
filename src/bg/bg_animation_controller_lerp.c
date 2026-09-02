#include "bg_animation.h"

#include "compat/coduo_int32_bits.h"
#include "compat/coduo_x87emu.h"
#include "math/q_math.h"

#include <stdint.h>
#include <string.h>

#if defined(WINDOWS_BEHAVIOR) && defined(LINUX_BEHAVIOR)
#error "Select only one of WINDOWS_BEHAVIOR or LINUX_BEHAVIOR"
#elif !defined(WINDOWS_BEHAVIOR) && !defined(LINUX_BEHAVIOR)
#error "Select WINDOWS_BEHAVIOR or LINUX_BEHAVIOR"
#endif

/*
 * The Windows cgame/game bodies are instruction-identical after relocated
 * constants are normalized:
 *
 *   BG_LerpAngles  0x300055b0 / 0x200053e0
 *   BG_LerpOffset  0x30005670 / 0x200054a0
 *
 * Supporting Mac cgame/game retain both canonical names with equal sizes and
 * call graphs. Linux game retains BG_LerpAngles at RVA 0x0001f107 and
 * BG_LerpOffset at RVA 0x0001f1e7. The latter contains a genuine malformed
 * Q_rsqrt call/return ABI, so BG_LerpOffset has whole-function platform bodies.
 */

void BG_LerpAngles(const vec3_t target, float maxStep, vec3_t current)
{
    int32_t lane;

    for (lane = 0; lane < 3; ++lane) {
#if EMULATE_X87
#if defined(WINDOWS_BEHAVIOR)
        const x87f delta = x87f_sub(x87f_load_f32(target[lane]), x87f_load_f32(current[lane]));
        const x87f step = x87f_load_f32(maxStep);

        if (x87f_lt_signaling(step, delta)) {
            current[lane] = x87f_store_f32(x87f_add(step, x87f_load_f32(current[lane])));
        } else if (x87f_lt_signaling(delta, x87f_neg(step))) {
            current[lane] = x87f_store_f32(x87f_sub(x87f_load_f32(current[lane]), step));
        } else {
            current[lane] = target[lane];
        }
#else
        const float delta = x87f_store_f32(x87f_sub(x87f_load_f32(target[lane]), x87f_load_f32(current[lane])));
        const x87f deltaWide = x87f_load_f32(delta);
        const x87f step = x87f_load_f32(maxStep);

        if (x87f_lt(step, deltaWide)) {
            current[lane] = x87f_store_f32(x87f_add(x87f_load_f32(current[lane]), step));
        } else if (x87f_lt(deltaWide, x87f_neg(step))) {
            current[lane] = x87f_store_f32(x87f_sub(x87f_load_f32(current[lane]), step));
        } else {
            current[lane] = target[lane];
        }
#endif
#elif defined(WINDOWS_BEHAVIOR)
        const double delta = (double)target[lane] - current[lane];

        if (delta > maxStep) {
            current[lane] = (float)((double)current[lane] + maxStep);
        } else if (delta < -(double)maxStep) {
            current[lane] = (float)((double)current[lane] - maxStep);
        } else {
            current[lane] = target[lane];
        }
#else
        const float delta = target[lane] - current[lane];

        if (delta > maxStep) {
            current[lane] = current[lane] + maxStep;
        } else if (delta < -maxStep) {
            current[lane] = current[lane] - maxStep;
        } else {
            current[lane] = target[lane];
        }
#endif
    }
}

#if defined(WINDOWS_BEHAVIOR)
void BG_LerpOffset(const vec3_t target, float scale, vec3_t current)
{
    const float zero = 0.0f;
    const float half = 0.5f;
    const float one = 1.0f;
    const float threeHalves = 1.5f;
    float delta[3];
    uint32_t bits;
    float estimate;

#if EMULATE_X87
    x87f delta2Wide;
    x87f lengthSquared;
    x87f factor;

    delta[0] = x87f_store_f32(x87f_sub(x87f_load_f32(target[0]), x87f_load_f32(current[0])));
    delta[1] = x87f_store_f32(x87f_sub(x87f_load_f32(target[1]), x87f_load_f32(current[1])));
    delta2Wide = x87f_sub(x87f_load_f32(target[2]), x87f_load_f32(current[2]));
    delta[2] = x87f_store_f32(delta2Wide);

    lengthSquared =
        x87f_add(x87f_add(x87f_mul(delta2Wide, x87f_load_f32(delta[2])), x87f_mul(x87f_load_f32(delta[1]), x87f_load_f32(delta[1]))),
                 x87f_mul(x87f_load_f32(delta[0]), x87f_load_f32(delta[0])));
    if (x87f_eq(lengthSquared, x87f_load_f32(zero))) {
        return;
    }

    {
        const float storedLengthSquared = x87f_store_f32(lengthSquared);

        memcpy(&bits, &storedLengthSquared, sizeof(bits));
    }
    bits = UINT32_C(0x5f3759df) - coduo_int32_sar_bits(bits, 1U);
    memcpy(&estimate, &bits, sizeof(estimate));

    factor = x87f_mul(
        x87f_sub(x87f_load_f32(threeHalves),
                 x87f_mul(x87f_mul(x87f_mul(lengthSquared, x87f_load_f32(half)), x87f_load_f32(estimate)), x87f_load_f32(estimate))),
        x87f_load_f32(estimate));
    factor = x87f_mul(factor, x87f_load_f32(scale));

    if (!x87f_lt_signaling(factor, x87f_load_f32(one))) {
        current[0] = target[0];
        current[1] = target[1];
        current[2] = target[2];
        return;
    }

    current[0] = x87f_store_f32(x87f_add(x87f_mul(x87f_load_f32(delta[0]), factor), x87f_load_f32(current[0])));
    current[1] = x87f_store_f32(x87f_add(x87f_mul(x87f_load_f32(delta[1]), factor), x87f_load_f32(current[1])));
    current[2] = x87f_store_f32(x87f_add(x87f_mul(factor, x87f_load_f32(delta[2])), x87f_load_f32(current[2])));
#else
    double delta2Wide;
    double lengthSquared;
    double factor;

    delta[0] = (float)((double)target[0] - current[0]);
    delta[1] = (float)((double)target[1] - current[1]);
    delta2Wide = (double)target[2] - current[2];
    delta[2] = (float)delta2Wide;
    lengthSquared = (delta2Wide * delta[2] + (double)delta[1] * delta[1]) + (double)delta[0] * delta[0];
    if (lengthSquared == zero) {
        return;
    }

    {
        const float storedLengthSquared = (float)lengthSquared;

        memcpy(&bits, &storedLengthSquared, sizeof(bits));
    }
    bits = UINT32_C(0x5f3759df) - coduo_int32_sar_bits(bits, 1U);
    memcpy(&estimate, &bits, sizeof(estimate));

    factor = ((double)threeHalves - ((lengthSquared * half * (double)estimate) * estimate)) * (double)estimate;
    factor *= scale;
    if (!(factor < one)) {
        current[0] = target[0];
        current[1] = target[1];
        current[2] = target[2];
        return;
    }

    current[0] = (float)((double)current[0] + (double)delta[0] * factor);
    current[1] = (float)((double)current[1] + (double)delta[1] * factor);
    current[2] = (float)((double)current[2] + (double)delta[2] * factor);
#endif
}
#else
/* NOT_FROM_ORIGINAL_SOURCE: reproduce the Linux i386 BG_LerpOffset call-site
 * ABI defect. The original passes a double to Q_rsqrt, then treats EAX's
 * retained float bits as a signed integer instead of consuming x87 ST0. */
static float bg_compat_lerp_offset_qrsqrt_scale(float lengthSquared, float scale)
{
    const double argument = (double)lengthSquared;
    uint32_t lowWord;
    float qrsqrtArgument;
    float qrsqrtResult;
    int32_t returnWord;

    memcpy(&lowWord, &argument, sizeof(lowWord));
    memcpy(&qrsqrtArgument, &lowWord, sizeof(qrsqrtArgument));
    qrsqrtResult = Q_rsqrt(qrsqrtArgument);
    memcpy(&returnWord, &qrsqrtResult, sizeof(returnWord));

#if EMULATE_X87
    return x87f_store_f32(x87f_mul(x87f_load_i32(returnWord), x87f_load_f32(scale)));
#else
    return (float)((double)returnWord * scale);
#endif
}

void BG_LerpOffset(const vec3_t target, float scale, vec3_t current)
{
    float delta[3];
    float lengthSquared;
    float step;

#if EMULATE_X87
    delta[0] = x87f_store_f32(x87f_sub(x87f_load_f32(target[0]), x87f_load_f32(current[0])));
    delta[1] = x87f_store_f32(x87f_sub(x87f_load_f32(target[1]), x87f_load_f32(current[1])));
    delta[2] = x87f_store_f32(x87f_sub(x87f_load_f32(target[2]), x87f_load_f32(current[2])));
    lengthSquared = x87f_store_f32(x87f_add(
        x87f_add(x87f_mul(x87f_load_f32(delta[0]), x87f_load_f32(delta[0])), x87f_mul(x87f_load_f32(delta[1]), x87f_load_f32(delta[1]))),
        x87f_mul(x87f_load_f32(delta[2]), x87f_load_f32(delta[2]))));
#else
    delta[0] = target[0] - current[0];
    delta[1] = target[1] - current[1];
    delta[2] = target[2] - current[2];
    lengthSquared = delta[0] * delta[0] + delta[1] * delta[1] + delta[2] * delta[2];
#endif

    if (lengthSquared == 0.0f) {
        return;
    }

    /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
    step = bg_compat_lerp_offset_qrsqrt_scale(lengthSquared, scale);
    if (step < 1.0f) {
#if EMULATE_X87
        current[0] = x87f_store_f32(x87f_add(x87f_mul(x87f_load_f32(delta[0]), x87f_load_f32(step)), x87f_load_f32(current[0])));
        current[1] = x87f_store_f32(x87f_add(x87f_mul(x87f_load_f32(delta[1]), x87f_load_f32(step)), x87f_load_f32(current[1])));
        current[2] = x87f_store_f32(x87f_add(x87f_mul(x87f_load_f32(delta[2]), x87f_load_f32(step)), x87f_load_f32(current[2])));
#else
        current[0] = current[0] + delta[0] * step;
        current[1] = current[1] + delta[1] * step;
        current[2] = current[2] + delta[2] * step;
#endif
    } else {
        current[0] = target[0];
        current[1] = target[1];
        current[2] = target[2];
    }
}
#endif
