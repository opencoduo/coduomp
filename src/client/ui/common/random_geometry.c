#include "../module/ui_functions.h"
#include "compat/coduo_native_x87.h"

#include <math.h>

enum {
    MSVC_RAND_MULTIPLIER = 214013,
    MSVC_RAND_INCREMENT = 2531011,
    MSVC_RAND_MAXIMUM = 32767
};

/* Portable carrier for the linked MSVC per-thread rand state used only by
 * gunrandom in this DLL. FUN_4001f5f0 returns the calling thread's CRT record,
 * whose +0x14 seed is initialized to one at 0x4001f638. */
/* NOT_FROM_ORIGINAL_SOURCE: VM dispatch is serialized on every supported
 * platform, so a module-local carrier preserves the linked MSVC sequence
 * without making module unloading depend on native TLS lifetime rules. */
static uint32_t gunrandomSeed = 1;

/* NOT_FROM_ORIGINAL_SOURCE: restore the linked CRT's per-image initial seed at
 * the portable dllEntry lifecycle boundary. */
void ui_compat_reset_random_geometry_state(void)
{
    gunrandomSeed = 1;
}

// Source: uo_ui_mp_x86.dll 0x40001240..0x400012df
// Evidence: cgame_mp/mcode/uo_ui_mp_x86/FUN_40001240_400012df.mcode
// Exact same-module PPC symbol: gunrandom.
void gunrandom(float *x, float *y)
{
    const float randomDivisor = 32768.0f;
    const float fullCircleDegrees = 360.0f;
    const float pi = 3.1415927f;
    const float degreesPerHalfCircle = 180.0f;
    float angle;
    float magnitude;
    int32_t angleRandom;
    int32_t magnitudeRandom;

    gunrandomSeed = gunrandomSeed * MSVC_RAND_MULTIPLIER + MSVC_RAND_INCREMENT;
    angleRandom = (int32_t)((gunrandomSeed >> 16) & MSVC_RAND_MAXIMUM);
    gunrandomSeed = gunrandomSeed * MSVC_RAND_MULTIPLIER + MSVC_RAND_INCREMENT;
    magnitudeRandom = (int32_t)((gunrandomSeed >> 16) & MSVC_RAND_MAXIMUM);

    angle = (float)((long double)(float)angleRandom / randomDivisor * fullCircleDegrees);
    magnitude = (float)((long double)(float)magnitudeRandom / randomDivisor);
    angle = (float)((long double)angle * pi / degreesPerHalfCircle);
    {
        float sine;
        float cosine;

        /* 0x400012ac: the DLL performs one FSINCOS and stores cosine to the
         * first local, sine to the second before the two scaled stores. */
        coduo_x87_sincosf(angle, &sine, &cosine);
        *x = (float)((long double)cosine * magnitude);
        *y = (float)((long double)sine * magnitude);
    }
}
