#include "bg_bob.h"

#include "bg_bob_binding.h"
#include "compat/coduo_native_x87.h"
#include "compat/coduo_x87emu.h"

#if defined(LINUX_BEHAVIOR)
#include "compat/libm/coduo_libm.h"
#endif

#include <stdint.h>

#if defined(WINDOWS_BEHAVIOR) && defined(LINUX_BEHAVIOR)
#error "Select only one of WINDOWS_BEHAVIOR or LINUX_BEHAVIOR"
#elif !defined(WINDOWS_BEHAVIOR) && !defined(LINUX_BEHAVIOR)
#error "Select WINDOWS_BEHAVIOR or LINUX_BEHAVIOR"
#endif

/*
 * Complete BG bob-wave subsystem.  The Windows cgame/game bodies are
 * instruction-identical after relocating constants and cvar globals:
 *
 *   uo_cgame_mp_x86.dll  0x30014db0, 0x30014dd0, 0x30014e50
 *   uo_game_mp_x86.dll   0x20014cf0, 0x20014d10, 0x20014d90
 *
 * Linux game retains the same state tests and waveforms at RVAs 0x00037b2d,
 * 0x00037b7c, and 0x00037c5f.  Its original source/compiler boundary stores
 * the public results and intermediate sine terms as binary32, while Windows
 * returns live PC=53 x87 values.  Complete platform bodies preserve that real
 * computational difference; EMULATE_X87 remains an independent host axis.
 */

#if defined(WINDOWS_BEHAVIOR)

long double BG_GetBobCycle(const playerState_t *ps)
{
#if EMULATE_X87
    const x87f result = x87f_add(
        x87f_mul(x87f_load_i32((uint8_t)ps->bobCycle),
                 x87f_load_f32(0.0246399436f)),
        x87f_load_f32(6.2831854820f));
    return (long double)x87f_store_f64(result);
#else
    return (long double)(uint8_t)ps->bobCycle * 0.0246399436f +
           6.2831854820f;
#endif
}

long double BG_GetVerticalBobFactor(const playerState_t *ps, float phase,
                                    float amplitude, float maxAmplitude)
{
    float stanceScale;
    float clampedAmplitude;

    if (ps->viewHeightTarget == ps->proneViewHeight) {
        stanceScale = BG_BOB_AMPLITUDE_PRONE;
    } else if (ps->viewHeightTarget == ps->crouchViewHeight) {
        stanceScale = BG_BOB_AMPLITUDE_DUCKED;
    } else {
        stanceScale = BG_BOB_AMPLITUDE_STANDING;
    }

#if EMULATE_X87
    x87f amplitudeWide = x87f_mul(x87f_load_f32(stanceScale),
                                  x87f_load_f32(amplitude));
    clampedAmplitude = x87f_store_f32(amplitudeWide);
    if (x87f_lt(x87f_load_f32(maxAmplitude), amplitudeWide)) {
        clampedAmplitude = maxAmplitude;
    }

    const x87f harmonicArgument = x87f_add(
        x87f_mul(x87f_load_f32(phase), x87f_load_f32(4.0f)),
        x87f_load_f32(1.5707964f));
    const long double harmonicSine =
        coduo_x87_sinl((long double)x87f_store_f64(harmonicArgument));
    x87f harmonic = x87f_mul(x87f_load_f64((double)harmonicSine),
                             x87f_load_f32(0.2f));

    const x87f fundamentalArgument =
        x87f_add(x87f_load_f32(phase), x87f_load_f32(phase));
    const long double fundamentalSine =
        coduo_x87_sinl((long double)x87f_store_f64(fundamentalArgument));
    x87f result = x87f_add(harmonic,
                           x87f_load_f64((double)fundamentalSine));
    result = x87f_mul(result, x87f_load_f32(clampedAmplitude));
    result = x87f_mul(result, x87f_load_f32(0.75f));
    return (long double)x87f_store_f64(result);
#else
    const long double amplitudeWide =
        (long double)stanceScale * (long double)amplitude;
    clampedAmplitude = (float)amplitudeWide;
    if (amplitudeWide > (long double)maxAmplitude) {
        clampedAmplitude = maxAmplitude;
    }

    const long double harmonic =
        coduo_x87_sinl((long double)phase * 4.0f + 1.5707964f) * 0.2f;
    const long double fundamental =
        coduo_x87_sinl((long double)phase + (long double)phase);
    return (harmonic + fundamental) * clampedAmplitude * 0.75f;
#endif
}

long double BG_GetHorizontalBobFactor(const playerState_t *ps, float phase,
                                      float amplitude, float maxAmplitude)
{
    float stanceScale;

    if (ps->viewHeightTarget == ps->proneViewHeight) {
        stanceScale = BG_BOB_AMPLITUDE_PRONE;
    } else if (ps->viewHeightTarget == ps->crouchViewHeight) {
        stanceScale = BG_BOB_AMPLITUDE_DUCKED;
    } else {
        stanceScale = BG_BOB_AMPLITUDE_STANDING;
    }

#if EMULATE_X87
    x87f clampedAmplitude = x87f_mul(x87f_load_f32(stanceScale),
                                     x87f_load_f32(amplitude));
    if (x87f_lt(x87f_load_f32(maxAmplitude), clampedAmplitude)) {
        clampedAmplitude = x87f_load_f32(maxAmplitude);
    }
    const long double sine = coduo_x87_sinl((long double)phase);
    const x87f result = x87f_mul(x87f_load_f64((double)sine),
                                 clampedAmplitude);
    return (long double)x87f_store_f64(result);
#else
    long double clampedAmplitude =
        (long double)stanceScale * (long double)amplitude;
    if (clampedAmplitude > (long double)maxAmplitude) {
        clampedAmplitude = maxAmplitude;
    }
    return coduo_x87_sinl((long double)phase) * clampedAmplitude;
#endif
}

#else

float BG_GetBobCycle(const playerState_t *ps)
{
#if EMULATE_X87
    x87f phase = x87f_mul(
        x87f_div(x87f_load_i32((uint8_t)ps->bobCycle),
                 x87f_load_f32(255.0f)),
        x87f_load_f64(3.141592653589793));
    return x87f_store_f32(
        x87f_add(x87f_add(phase, phase),
                 x87f_load_f64(6.283185307179586)));
#else
    const long double phase =
        (long double)(uint8_t)ps->bobCycle / 255.0f *
        3.141592653589793L;
    return (float)(phase + phase + 6.283185307179586L);
#endif
}

float BG_GetVerticalBobFactor(const playerState_t *ps, float phase,
                              float amplitude, float maxAmplitude)
{
    float clampedAmplitude;
    float fundamental;
    float harmonic;

    if (ps->viewHeightTarget == ps->proneViewHeight) {
        clampedAmplitude = amplitude * BG_BOB_AMPLITUDE_PRONE;
    } else if (ps->viewHeightTarget == ps->crouchViewHeight) {
        clampedAmplitude = amplitude * BG_BOB_AMPLITUDE_DUCKED;
    } else {
        clampedAmplitude = amplitude * BG_BOB_AMPLITUDE_STANDING;
    }
    if (maxAmplitude < clampedAmplitude) {
        clampedAmplitude = maxAmplitude;
    }

#if EMULATE_X87
    const double fundamentalArgument = x87f_store_f64(
        x87f_add(x87f_load_f32(phase), x87f_load_f32(phase)));
    fundamental = (float)CoduoLibm_Sin(fundamentalArgument);
    const double harmonicArgument = x87f_store_f64(x87f_add(
        x87f_mul(x87f_load_f32(phase), x87f_load_f32(4.0f)),
        x87f_load_f64(1.5707963267948966)));
    harmonic = (float)CoduoLibm_Sin(harmonicArgument);
    fundamental = x87f_store_f32(x87f_add(
        x87f_load_f32(fundamental),
        x87f_mul(x87f_load_f32(harmonic), x87f_load_f32(0.2f))));
    fundamental = x87f_store_f32(
        x87f_mul(x87f_load_f32(0.75f), x87f_load_f32(fundamental)));
    return x87f_store_f32(x87f_mul(x87f_load_f32(fundamental),
                                   x87f_load_f32(clampedAmplitude)));
#else
    fundamental = (float)CoduoLibm_Sin((double)(phase + phase));
    harmonic = (float)CoduoLibm_Sin(
        (double)((long double)phase * 4.0f + 1.5707963267948966L));
    fundamental = fundamental + harmonic * 0.2f;
    fundamental = 0.75f * fundamental;
    return fundamental * clampedAmplitude;
#endif
}

float BG_GetHorizontalBobFactor(const playerState_t *ps, float phase,
                                float amplitude, float maxAmplitude)
{
    float clampedAmplitude;

    if (ps->viewHeightTarget == ps->proneViewHeight) {
        clampedAmplitude = amplitude * BG_BOB_AMPLITUDE_PRONE;
    } else if (ps->viewHeightTarget == ps->crouchViewHeight) {
        clampedAmplitude = amplitude * BG_BOB_AMPLITUDE_DUCKED;
    } else {
        clampedAmplitude = amplitude * BG_BOB_AMPLITUDE_STANDING;
    }
    if (maxAmplitude < clampedAmplitude) {
        clampedAmplitude = maxAmplitude;
    }

#if EMULATE_X87
    return x87f_store_f32(x87f_mul(
        x87f_load_f64(CoduoLibm_Sin((double)phase)),
        x87f_load_f32(clampedAmplitude)));
#else
    return (float)CoduoLibm_Sin((double)phase) * clampedAmplitude;
#endif
}

#endif

#undef BG_BOB_AMPLITUDE_STANDING
#undef BG_BOB_AMPLITUDE_DUCKED
#undef BG_BOB_AMPLITUDE_PRONE
