#include "bg_pmove.h"

#include "compat/coduo_x87emu.h"
#include "math/q_math.h"

#include <math.h>
#include <stdint.h>

#define PM_SHORT_TO_ANGLE 0.0054931640625f

/*
 * The authoritative Windows cgame/game bodies are instruction-identical at
 * 0x30013a90 and 0x200139d0.  The Linux game body at RVA 0x00035dcb retains
 * the same source-level decisions and formula, but its unoptimized compiler
 * spills float assignments which MSVC keeps in x87 registers or folds into
 * constants.  Those lowering choices do not represent separate Windows and
 * Linux source implementations; the ambient/emulated x87 precision remains
 * selected independently by the build.
 */
void PM_AdjustAimSpreadScale(void)
{
    const weaponInfo_t *const weaponInfo = pml.weaponInfo;
    playerState_t *const ps = pm->ps;
    float decayRate = weaponInfo->aimSpreadDecayRate;
    float decay;
    float growth;

    if (decayRate != 0.0f) {
        if (ps->groundEntityNum == ENTITYNUM_NONE && ps->pmType != PM_TYPE_LINKED) {
#if EMULATE_X87
            decayRate = x87f_store_f32(x87f_mul(x87f_load_f32(decayRate), x87f_load_f32(0.5f)));
#else
            decayRate = (float)((long double)decayRate * 0.5L);
#endif
        } else if ((ps->entityStateFlags & EF_PRONE) != 0) {
#if EMULATE_X87
            decayRate = x87f_store_f32(x87f_mul(x87f_load_f32(decayRate), x87f_load_f32(weaponInfo->aimSpreadProneScale)));
#else
            decayRate = (float)((long double)decayRate * (long double)weaponInfo->aimSpreadProneScale);
#endif
        } else if ((ps->entityStateFlags & EF_CROUCHING) != 0) {
#if EMULATE_X87
            decayRate = x87f_store_f32(x87f_mul(x87f_load_f32(decayRate), x87f_load_f32(weaponInfo->aimSpreadCrouchScale)));
#else
            decayRate = (float)((long double)decayRate * (long double)weaponInfo->aimSpreadCrouchScale);
#endif
        }

#if EMULATE_X87
        decay = x87f_store_f32(x87f_mul(x87f_load_f32(decayRate), x87f_load_f32(pml.frametime)));
#else
        decay = (float)((long double)decayRate * (long double)pml.frametime);
#endif

        if (ps->adsFraction == 1.0f) {
            growth = 0.0f;
        } else {
            float growthRate = 0.0f;

            if (weaponInfo->aimSpreadTurnRate != 0.0f) {
                for (int32_t axis = 0; axis < 2; ++axis) {
                    float currentAngle;
                    float previousAngle;
                    float delta;

#if EMULATE_X87
                    currentAngle = x87f_store_f32(x87f_mul(x87f_load_i32(pm->command.angles[axis]), x87f_load_f32(PM_SHORT_TO_ANGLE)));
                    previousAngle = x87f_store_f32(x87f_mul(x87f_load_i32(pm->oldCommand.angles[axis]), x87f_load_f32(PM_SHORT_TO_ANGLE)));
#else
                    currentAngle = (float)((long double)pm->command.angles[axis] * (long double)PM_SHORT_TO_ANGLE);
                    previousAngle = (float)((long double)pm->oldCommand.angles[axis] * (long double)PM_SHORT_TO_ANGLE);
#endif
                    delta = fabsf(AngleSubtract(currentAngle, previousAngle));

#if EMULATE_X87
                    growthRate = x87f_store_f32(
                        x87f_add(x87f_load_f32(growthRate), x87f_div(x87f_mul(x87f_mul(x87f_load_f32(delta), x87f_load_f32(0.01f)),
                                                                              x87f_load_f32(weaponInfo->aimSpreadTurnRate)),
                                                                     x87f_load_f32(pml.frametime))));
#else
                    growthRate = (float)((long double)growthRate + (long double)delta * 0.01L * (long double)weaponInfo->aimSpreadTurnRate /
                                                                       (long double)pml.frametime);
#endif
                }
            }

            if (weaponInfo->aimSpreadMoveAdd != 0.0f && (pm->command.forwardmove != 0 || pm->command.rightmove != 0)) {
#if EMULATE_X87
                growthRate = x87f_store_f32(x87f_add(x87f_load_f32(growthRate), x87f_load_f32(weaponInfo->aimSpreadMoveAdd)));
#else
                growthRate = (float)((long double)growthRate + (long double)weaponInfo->aimSpreadMoveAdd);
#endif
            }

            if (ps->groundEntityNum == ENTITYNUM_NONE && ps->pmType != PM_TYPE_LINKED) {
                for (int32_t step = 0; step < 2; ++step) {
#if EMULATE_X87
                    growthRate = x87f_store_f32(x87f_add(x87f_load_f32(growthRate), x87f_mul(x87f_load_f32(0.01f), x87f_load_f32(128.0f))));
#else
                    growthRate = (float)((long double)growthRate + (long double)0.01f * 128.0L);
#endif
                }
            }

#if EMULATE_X87
            growth = x87f_store_f32(x87f_mul(x87f_load_f32(growthRate), x87f_load_f32(pml.frametime)));
#else
            growth = (float)((long double)growthRate * (long double)pml.frametime);
#endif
        }
    } else {
        growth = 0.0f;
        decay = 1.0f;
    }

#if EMULATE_X87
    ps->aimSpreadScale =
        x87f_store_f32(x87f_add(x87f_load_f32(ps->aimSpreadScale),
                                x87f_mul(x87f_sub(x87f_load_f32(growth), x87f_load_f32(decay)), x87f_load_i32(PM_AIM_SPREAD_SCALE_MAX))));
#else
    ps->aimSpreadScale =
        (float)((long double)ps->aimSpreadScale + ((long double)growth - (long double)decay) * (long double)PM_AIM_SPREAD_SCALE_MAX);
#endif

    if (ps->aimSpreadScale < 0.0f) {
        ps->aimSpreadScale = 0.0f;
    } else if (ps->aimSpreadScale > (float)PM_AIM_SPREAD_SCALE_MAX) {
        ps->aimSpreadScale = (float)PM_AIM_SPREAD_SCALE_MAX;
    }
}
