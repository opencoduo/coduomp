#include "bg_pmove.h"

#include "compat/coduo_native_x87.h"
#include "compat/coduo_x87emu.h"

#if defined(LINUX_BEHAVIOR)
#include "compat/libm/coduo_libm.h"
#endif

#include <math.h>
#include <stdint.h>

/*
 * The Windows cgame/game PM_CmdScale bodies are instruction-identical apart
 * from relocations (0x30008690 and 0x20008440).  They keep the complete scale
 * in x87 through the stance and free-move multipliers.  All five callers in
 * each DLL immediately store ST0 as binary32, so float is the common observable
 * interface despite the Windows callee's excess precision.
 *
 * Linux game RVA 0x00023905 has the same integer inputs and branch decisions,
 * but calls glibc sqrt through a binary64 argument and stores the length, base
 * scale, and each subsequent multiplier result as binary32.  The whole bodies
 * below preserve those proven spill graphs; see the discrepancy record.
 */

#if defined(WINDOWS_BEHAVIOR)
float PM_CmdScale(const usercmd_t *command)
{
    int32_t forward = command->forwardmove;
    int32_t right = command->rightmove;
    int32_t up = command->upmove;
    int32_t maximum;
    int32_t lengthSquared;
    const playerState_t *ps;

    if (forward < 0) {
        forward = -forward;
    }
    if (right < 0) {
        right = -right;
    }
    if (up < 0) {
        up = -up;
    }
    maximum = forward;
    if (maximum < right) {
        maximum = right;
    }
    if (maximum < up) {
        maximum = up;
    }

    if (maximum == 0) {
        return 0.0f;
    }

    forward = command->forwardmove;
    right = command->rightmove;
    up = command->upmove;
    lengthSquared = forward * forward + right * right + up * up;
    ps = pm->ps;

#if EMULATE_X87
    {
        x87f scale = x87f_div(
            x87f_mul(x87f_load_i32(ps->speed), x87f_load_i32(maximum)),
            x87f_mul(x87f_sqrt(x87f_load_i32(lengthSquared)),
                     x87f_load_f32(127.0f)));

        if ((ps->playerStateFlags & PMF_WALKING) != 0 ||
            ps->leanFraction != 0.0f) {
            scale = x87f_mul(scale, x87f_load_f32(ps->walkSpeedScale));
        } else if ((ps->playerStateFlags & PMF_SPRINTING) != 0) {
            scale = x87f_mul(scale, x87f_load_f32(ps->sprintSpeedScale));
        } else {
            scale = x87f_mul(scale, x87f_load_f32(ps->runSpeedScale));
        }

        if (ps->pmType == PM_TYPE_NOCLIP) {
            scale = x87f_mul(scale, x87f_load_f32(3.0f));
        }
        if (ps->pmType == PM_TYPE_UFO) {
            scale = x87f_mul(scale, x87f_load_f32(6.0f));
        }
        return x87f_store_f32(scale);
    }
#else
    {
        long double scale =
            ((long double)ps->speed * (long double)maximum) /
            (coduo_x87_sqrtl((long double)lengthSquared) * 127.0L);

        if ((ps->playerStateFlags & PMF_WALKING) != 0 ||
            ps->leanFraction != 0.0f) {
            scale *= (long double)ps->walkSpeedScale;
        } else if ((ps->playerStateFlags & PMF_SPRINTING) != 0) {
            scale *= (long double)ps->sprintSpeedScale;
        } else {
            scale *= (long double)ps->runSpeedScale;
        }

        if (ps->pmType == PM_TYPE_NOCLIP) {
            scale *= 3.0L;
        }
        if (ps->pmType == PM_TYPE_UFO) {
            scale *= 6.0L;
        }
        return (float)scale;
    }
#endif
}
#else
float PM_CmdScale(const usercmd_t *command)
{
    int32_t forward = command->forwardmove;
    int32_t right = command->rightmove;
    int32_t up = command->upmove;
    int32_t maximum;
    int32_t lengthSquared;
    const playerState_t *ps;
    float length;
    float scale;

    if (forward < 0) {
        forward = -forward;
    }
    if (right < 0) {
        right = -right;
    }
    if (up < 0) {
        up = -up;
    }
    maximum = forward;
    if (maximum < right) {
        maximum = right;
    }
    if (maximum < up) {
        maximum = up;
    }

    if (maximum == 0) {
        return 0.0f;
    }

    forward = command->forwardmove;
    right = command->rightmove;
    up = command->upmove;
    lengthSquared = forward * forward + right * right + up * up;
    ps = pm->ps;
    length = (float)CoduoLibm_SqrtGlibc((double)lengthSquared);
#if EMULATE_X87
    scale = x87f_store_f32(x87f_div(
        x87f_mul(x87f_load_i32(ps->speed), x87f_load_i32(maximum)),
        x87f_mul(x87f_load_f32(length), x87f_load_f32(127.0f))));
#else
    scale = (float)(
        ((long double)ps->speed * (long double)maximum) /
        ((long double)length * 127.0L));
#endif

    if ((ps->playerStateFlags & PMF_WALKING) != 0 ||
        ps->leanFraction != 0.0f) {
#if EMULATE_X87
        scale = x87f_store_f32(x87f_mul(
            x87f_load_f32(scale), x87f_load_f32(ps->walkSpeedScale)));
#else
        scale = (float)((long double)scale *
                        (long double)ps->walkSpeedScale);
#endif
    } else if ((ps->playerStateFlags & PMF_SPRINTING) != 0) {
#if EMULATE_X87
        scale = x87f_store_f32(x87f_mul(
            x87f_load_f32(scale), x87f_load_f32(ps->sprintSpeedScale)));
#else
        scale = (float)((long double)scale *
                        (long double)ps->sprintSpeedScale);
#endif
    } else {
#if EMULATE_X87
        scale = x87f_store_f32(x87f_mul(
            x87f_load_f32(scale), x87f_load_f32(ps->runSpeedScale)));
#else
        scale = (float)((long double)scale *
                        (long double)ps->runSpeedScale);
#endif
    }

    if (ps->pmType == PM_TYPE_NOCLIP) {
#if EMULATE_X87
        scale = x87f_store_f32(x87f_mul(
            x87f_load_f32(scale), x87f_load_f32(3.0f)));
#else
        scale = (float)((long double)scale * 3.0L);
#endif
    }
    if (ps->pmType == PM_TYPE_UFO) {
#if EMULATE_X87
        scale = x87f_store_f32(x87f_mul(
            x87f_load_f32(scale), x87f_load_f32(6.0f)));
#else
        scale = (float)((long double)scale * 6.0L);
#endif
    }
    return scale;
}
#endif
