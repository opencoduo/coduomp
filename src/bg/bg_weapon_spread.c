#include "bg_weapon.h"

#include "bg_pmove.h"
#include "compat/coduo_int32_bits.h"
#include "compat/coduo_x87emu.h"

#include <stdint.h>

#if defined(WINDOWS_BEHAVIOR) && defined(LINUX_BEHAVIOR)
#error "Select only one of WINDOWS_BEHAVIOR or LINUX_BEHAVIOR"
#elif !defined(WINDOWS_BEHAVIOR) && !defined(LINUX_BEHAVIOR)
#error "Select WINDOWS_BEHAVIOR or LINUX_BEHAVIOR"
#endif

enum {
    BG_VIEWHEIGHT_LERP_LONG_MSEC = 400,
    BG_VIEWHEIGHT_LERP_SHORT_MSEC = 200
};

/*
 * The Windows cgame and game bodies are instruction-identical apart from
 * relocated globals and constants:
 *
 *   uo_cgame_mp_x86.dll 0x30011950
 *   uo_game_mp_x86.dll  0x20011890
 *
 * Both retain the active transition fraction and final interpolation in x87
 * ST0 under the process PC=53 policy.  The caller may therefore consume more
 * than binary32 precision even though each settled-stance return is a loaded
 * float.  The long-double carrier preserves that original live-ST0 contract.
 *
 * Linux game RVA 0x00032902 makes the same stance decisions but stores the
 * fraction and every interpolated result as binary32 before returning.  Its
 * public body consequently retains the original float return boundary.
 */

#if defined(WINDOWS_BEHAVIOR)
long double BG_GetMinSpreadForWeapon(const playerState_t *ps, int32_t weapon,
                                     int32_t time, int32_t isAds)
{
    const weaponInfo_t *weaponInfo = bg_weaponInfos[weapon];
    const float standing = isAds != 0
        ? weaponInfo->adsSpread : weaponInfo->hipSpreadStandMin;
    const float crouched = isAds != 0
        ? weaponInfo->adsSpreadDucked : weaponInfo->hipSpreadDucked;
    const float prone = isAds != 0
        ? weaponInfo->adsSpreadProne : weaponInfo->hipSpreadProne;
    int32_t lerpStartTime = ps->viewHeightLerpTime;
    int32_t fromViewHeight;
    int32_t duration;
    int32_t elapsed;

    if (ps->viewHeightLerpTarget == ps->viewHeightCurrent ||
        lerpStartTime == 0) {
        if ((ps->playerStateFlags & PMF_PRONE) != 0) {
            return (long double)prone;
        }
        if ((ps->playerStateFlags & PMF_DUCKED) != 0) {
            return (long double)crouched;
        }
        return (long double)standing;
    }

    fromViewHeight = ps->viewHeightLerpTarget;
    if (fromViewHeight == ps->proneViewHeight) {
        duration = BG_VIEWHEIGHT_LERP_LONG_MSEC;
    } else if (fromViewHeight == ps->crouchViewHeight) {
        duration = ps->viewHeightLerpDown != 0
            ? BG_VIEWHEIGHT_LERP_SHORT_MSEC
            : BG_VIEWHEIGHT_LERP_LONG_MSEC;
    } else {
        duration = BG_VIEWHEIGHT_LERP_SHORT_MSEC;
    }

    elapsed = coduo_int32_from_bits(
        (uint32_t)time - (uint32_t)lerpStartTime);

#if EMULATE_X87
    x87f fraction = x87f_div(x87f_load_i32(elapsed),
                              x87f_load_i32(duration));
    const x87f zero = x87f_load_f32(0.0f);
    const x87f one = x87f_load_f32(1.0f);

    if (x87f_lt(fraction, zero)) {
        fraction = zero;
    } else if (x87f_lt(one, fraction)) {
        fraction = one;
    }

#define BG_WINDOWS_SPREAD_LERP(from, to)                                  \
    ((long double)x87f_store_f64(                                         \
        x87f_add(x87f_load_f32(from),                                     \
                 x87f_mul(fraction,                                       \
                          x87f_sub(x87f_load_f32(to),                     \
                                   x87f_load_f32(from))))))
#else
    long double fraction =
        (long double)elapsed / (long double)duration;

    if (fraction < 0.0L) {
        fraction = 0.0L;
    } else if (fraction > 1.0L) {
        fraction = 1.0L;
    }

#define BG_WINDOWS_SPREAD_LERP(from, to)                                  \
    ((long double)(from) + fraction *                                    \
        ((long double)(to) - (long double)(from)))
#endif

    if (fromViewHeight == ps->proneViewHeight) {
        return BG_WINDOWS_SPREAD_LERP(crouched, prone);
    }
    if (fromViewHeight == ps->standViewHeight) {
        return BG_WINDOWS_SPREAD_LERP(crouched, standing);
    }
    if (ps->viewHeightLerpDown != 0) {
        return BG_WINDOWS_SPREAD_LERP(standing, crouched);
    }
    return BG_WINDOWS_SPREAD_LERP(prone, crouched);

#undef BG_WINDOWS_SPREAD_LERP
}
#else
float BG_GetMinSpreadForWeapon(const playerState_t *ps, int32_t weapon,
                               int32_t time, int32_t isAds)
{
    const weaponInfo_t *weaponInfo = BG_GetInfoForWeapon(weapon);
    float fraction;
    int32_t elapsed;

    if ((long double)ps->viewHeightLerpTarget == ps->viewHeightCurrent ||
        ps->viewHeightLerpTime == 0) {
        if (isAds == 0) {
            if ((ps->playerStateFlags & PMF_PRONE) != 0) {
                return weaponInfo->hipSpreadProne;
            }
            if ((ps->playerStateFlags & PMF_DUCKED) != 0) {
                return weaponInfo->hipSpreadDucked;
            }
            return weaponInfo->hipSpreadStandMin;
        }

        if ((ps->playerStateFlags & PMF_PRONE) != 0) {
            return weaponInfo->adsSpreadProne;
        }
        if ((ps->playerStateFlags & PMF_DUCKED) != 0) {
            return weaponInfo->adsSpreadDucked;
        }
        return weaponInfo->adsSpread;
    }

    elapsed = coduo_int32_from_bits(
        (uint32_t)time - (uint32_t)ps->viewHeightLerpTime);
#if EMULATE_X87
    fraction = x87f_store_f32(x87f_div(
        x87f_load_i32(elapsed),
        x87f_load_i32(PM_GetViewHeightLerpTime(
            ps, ps->viewHeightLerpTarget,
            ps->viewHeightLerpDown))));
#else
    fraction = (float)(
        (long double)elapsed /
        (long double)PM_GetViewHeightLerpTime(
            ps, ps->viewHeightLerpTarget,
            ps->viewHeightLerpDown));
#endif
    if (fraction < 0.0f) {
        fraction = 0.0f;
    } else if (fraction > 1.0f) {
        fraction = 1.0f;
    }

#if EMULATE_X87
#define BG_LINUX_SPREAD_LERP(from, to)                                    \
    x87f_store_f32(x87f_add(                                             \
        x87f_mul(x87f_sub(x87f_load_f32(to), x87f_load_f32(from)),       \
                 x87f_load_f32(fraction)),                               \
        x87f_load_f32(from)))
#else
#define BG_LINUX_SPREAD_LERP(from, to)                                    \
    ((float)(((to) - (from)) * fraction + (from)))
#endif

    if (isAds == 0) {
        if (ps->viewHeightLerpTarget == ps->proneViewHeight) {
            return BG_LINUX_SPREAD_LERP(weaponInfo->hipSpreadDucked,
                                        weaponInfo->hipSpreadProne);
        }
        if (ps->viewHeightLerpTarget == ps->standViewHeight) {
            return BG_LINUX_SPREAD_LERP(weaponInfo->hipSpreadDucked,
                                        weaponInfo->hipSpreadStandMin);
        }
        if (ps->viewHeightLerpDown == 0) {
            return BG_LINUX_SPREAD_LERP(weaponInfo->hipSpreadProne,
                                        weaponInfo->hipSpreadDucked);
        }
        return BG_LINUX_SPREAD_LERP(weaponInfo->hipSpreadStandMin,
                                    weaponInfo->hipSpreadDucked);
    }

    if (ps->viewHeightLerpTarget == ps->proneViewHeight) {
        return BG_LINUX_SPREAD_LERP(weaponInfo->adsSpreadDucked,
                                    weaponInfo->adsSpreadProne);
    }
    if (ps->viewHeightLerpTarget == ps->standViewHeight) {
        return BG_LINUX_SPREAD_LERP(weaponInfo->adsSpreadDucked,
                                    weaponInfo->adsSpread);
    }
    if (ps->viewHeightLerpDown == 0) {
        return BG_LINUX_SPREAD_LERP(weaponInfo->adsSpreadProne,
                                    weaponInfo->adsSpreadDucked);
    }
    return BG_LINUX_SPREAD_LERP(weaponInfo->adsSpread,
                                weaponInfo->adsSpreadDucked);

#undef BG_LINUX_SPREAD_LERP
}
#endif
