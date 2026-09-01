#include "bg_pmove.h"

#include <stdint.h>
#include <string.h>

/*
 * Shared pmove scalar/state helpers.  Windows cgame and game bodies are
 * instruction-identical apart from relocations:
 *
 *   uo_cgame_mp_x86.dll  PM_FloatAbs         0x30008230
 *   uo_game_mp_x86.dll   PM_FloatAbs         0x20007fe0
 *   uo_cgame_mp_x86.dll  PM_FloatIsNegative 0x30008250
 *   uo_game_mp_x86.dll   PM_FloatIsNegative 0x20008000
 *   uo_cgame_mp_x86.dll  PM_FloatSign        0x30008260
 *   uo_game_mp_x86.dll   PM_FloatSign        0x20008010
 *
 * Linux game retains the canonical symbols at RVAs 0x0002e342,
 * 0x0002e381, and 0x0002e35b.  Its PM_GetEffectiveStance is at 0x000233e1;
 * the optimized Windows modules inline the same two field comparisons.
 */

float PM_FloatAbs(float value)
{
    uint32_t bits;

    memcpy(&bits, &value, sizeof(bits));
    bits &= UINT32_C(0x7fffffff);
    memcpy(&value, &bits, sizeof(value));
    return value;
}

int32_t PM_FloatIsNegative(float value)
{
    uint32_t bits;

    /* The Windows bodies test the stored binary32 sign bit. Linux RVA
     * 0x0002e381 instead compares with zero, but that differs only for
     * negative zero and negative NaNs. Those are not intentional pmove input
     * domains, so retain the main-platform Windows behavior for all builds. */
    memcpy(&bits, &value, sizeof(bits));
    return (int32_t)(bits >> 31);
}

int32_t PM_FloatSign(float value)
{
    return 1 - PM_FloatIsNegative(value) * 2;
}

effectiveStance_t PM_GetEffectiveStance(const playerState_t *ps)
{
    if (ps->viewHeightTarget == (int32_t)ps->crouchViewHeight) {
        return EFFECTIVE_STANCE_CROUCH;
    }
    if (ps->viewHeightTarget == (int32_t)ps->proneViewHeight) {
        return EFFECTIVE_STANCE_PRONE;
    }
    return EFFECTIVE_STANCE_STAND;
}
