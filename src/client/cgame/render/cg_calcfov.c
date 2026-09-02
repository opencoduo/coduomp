// Source: uo_cgame_mp_x86.dll 0x3003ffc0..0x300402a7
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3003ffc0_300402a7.mcode
//
// CG_CalcFov (0x3003ffc0) — compute the current view field of view (degrees) and,
// as a side effect, maintain the FOV/zoom transition trackers and the cg_fovFade
// animator. The value is returned on the x87 stack (a float return).
//
// Naming: the .mcode carries the size-guessed name BG_ParseWeaponInfoSpecificFieldType.
// That is REJECTED here — this function parses nothing and touches no weapon-info
// field-parse table. It is pure x87 FOV/zoom math over the cgame view globals
// (cg_fov_vmCvar.value, cg_predictedPlayerState.pmType, cg.predictedPlayerState.{adsFraction, currentWeapon,
// vehicleType, vehiclePosition, entityStateFlags}, cg_currentWeaponInfo). The FOV
// clamp signature (base FOV forced to 90 in game mode 5, clamped to [80,160]) plus
// the float FOV return matches the cgame_mp.dll PPC symbol CG_CalcFov; adopted as a
// provisional (behavior-based, NOT size-based) name.
//
// Callers observed: 0x300402b0 and 0x30042085, both consuming the returned float
// immediately as a view FOV (screen-projection / trig). Neither passes a real
// argument (the `push esi` at the first call site is a caller-saved register).
//
// x87 note: every FCOMP/FUCOMP branch below was decoded from the FNSTSW AX + TEST
// AH pattern in the .mcode:
//   TEST AH,0x05 / JP   : C0|C2, parity  -> "value < mem" (low clamp)
//   TEST AH,0x41 / JNZ  : C3|C0           -> "value >= mem" / equality bail
//   TEST AH,0x44 / JP   : C3|C2, parity   -> ordered-equal test (adsFraction vs const)
//
// The two stack locals reserved by `sub esp,8` are:
//   fov       (E-8) : the working FOV / return value
//   adsFrac   (E-4) : a snapshot of cg.predictedPlayerState.adsFraction
//
// This function reads cg_frametime (0x304831ac): the current frame's elapsed time
// in ms; here it is the staleness-window width for the per-ADS-slot FOV update
// (resolved from the cgame time-cluster consumers; see globals.h).
//
// Unresolved symbols left address-shaped (referenced but not proven from this one
// function's evidence, so not renamed here):
//   cg_timeoutEndTime (0x30447fd0): the "timeout HUD end time (ms)"; a
//     prior worker proved the timeout role but kept the address suffix pending full
//     adjudication. Used here only as a `== 0` gate ("no HUD timeout active").

#include "client/cgame/client_recovered.h"
#include "client/cgame/globals.h"

#include <math.h>

/* File-local FOV constants (degrees), all proven as float immediates in the .mcode. */
#define FOV_DEFAULT_GAMEMODE5 90.0f /* 0x42b40000, forced FOV in cg_predictedPlayerState.pmType == 5 */
#define FOV_CLAMP_MIN 80.0f /* 0x42a00000 (0x3007bfb0), low clamp */
#define FOV_CLAMP_MAX 160.0f /* 0x43200000 (0x3007be90), high clamp */
#define FOV_VEHICLE_ADS 70.0f /* 0x428c0000 (0x3007bf84), vehicle-ADS lerp target */
#define FOV_ZOOM_SCOPE 55.0f /* 0x425c0000 (0x3007c004), returned for the zoom/scope pair */

/* cg.predictedPlayerState.vehiclePosition values the FOV path distinguishes (1/2/3).
 * Exact source enum name unresolved; named by proven role in this function. */
enum {
    VEHICLE_POSITION_DRIVER = 1, /* the position that runs the ADS-blend FOV lerp */
    VEHICLE_POSITION_GUNNER = 2, /* suppresses the ADS-blend lerp */
    VEHICLE_POSITION_PASSENGER = 3 /* diverts to the non-vehicle FOV path */
};

/* cg.predictedPlayerState.vehicleType value gated on (== 1) before the position check. */
enum {
    VEHICLE_TYPE_ONE = 1
};

/* FOV-fade timing constants proven as decimal immediates in the .mcode. */
enum {
    FOV_FADE_BACKDATE_MS = 10,  /* startTime = cg.time - 10 for the "just changed" fade */
    FOV_FADE_QUICK_MS = 1,   /* 1 ms duration for the snap-in fade */
    FOV_FADE_STALE_MS = 50,  /* transition is "stale" once older than cg.time - 50 (0x32) */
    FOV_FADE_SETTLE_MS = 700  /* 0x2bc, settle-out fade duration */
};

float CG_CalcFov(void)
{
    /* EDX in the machine code: held live across the whole body. */
    const int32_t now = coduo_int32_from_bits(cg_time);
    /* EBP: local player's predicted entityStateFlags. */
    const uint32_t flags = cg_predictedPlayerState.entityStateFlags;

    float fov;
    /* adsFrac snapshot (local E-4), read as a float. */
    const float adsFrac = cg_predictedPlayerState.adsFraction;

    if (cg_predictedPlayerState.pmType == PM_TYPE_INTERMISSION) {
        /* 0x3003ffe2: forced 90 degrees, then straight to the tail transition logic. */
        fov = FOV_DEFAULT_GAMEMODE5;
    } else {
        /* 0x3003ffef: base FOV from the cg_fov cvar mirror, clamped to [80,160]. */
        fov = cg_fov_vmCvar.value;
        if (fov < FOV_CLAMP_MIN) {       /* 0x3003fffd: FCOMP 80.0, TEST 0x05/JP */
            fov = FOV_CLAMP_MIN;
        }
        if (fov > FOV_CLAMP_MAX) {       /* 0x30040018: FCOMP 160.0, TEST 0x41/JNZ */
            fov = FOV_CLAMP_MAX;
        }

        /* 0x3004002d: without the vehicle/turret flag, take the non-vehicle path. */
        if (!(flags & EF_IN_VEHICLE)) {
            goto non_vehicle;
        }

        {
            /* Vehicle/turret view FOV path (0x3004003a). */
            const int32_t vehiclePosition = cg_predictedPlayerState.vehiclePosition; /* ESI */

            if (cg_predictedPlayerState.vehicleType == VEHICLE_TYPE_ONE && vehiclePosition == VEHICLE_POSITION_PASSENGER) {
                /* 0x3004004c: passenger of a type-1 vehicle -> non-vehicle path. */
                goto non_vehicle;
            }

            /* 0x30040052: state slot index = (adsFrac != 0.0f). */
            const int32_t adsSlot = (adsFrac != 0.0f) ? 1 : 0; /* EDI */

            /* 0x3004006f: current predicted weapon's ADS-enable flag. */
            weaponInfo_t *weapon = bg_weaponInfos[cg_predictedPlayerState.currentWeapon];
            const int32_t adsEnabled = weapon->adsEnabled; /* +0x328 */

            /* 0x30040083..0x300400ac: only the driver of an ADS-capable vehicle runs
             * the lerp toward FOV_VEHICLE_ADS; gunner (==2) or a non-ADS weapon skips
             * the lerp, and any position other than driver diverts to the commit path. */
            if (adsEnabled != 0 && vehiclePosition == VEHICLE_POSITION_DRIVER) {
                /* 0x30040091: fov = fov - (fov - 70.0f) * adsFrac. */
                fov = fov - (fov - FOV_VEHICLE_ADS) * adsFrac;
            } else if (vehiclePosition != VEHICLE_POSITION_DRIVER) {
                goto commit_position; /* 0x30040096/0x300400ac -> 0x300400f6 */
            }

            /* 0x300400ae (COMMIT for the driver, adsSlot-indexed).
             * If this slot's last update is inside the current
             * [now - cg_frametime, now] window, fall through to
             * the shared position-commit path; otherwise start a fade and finish. */
            {
                const int32_t last = cg_fovAdsUpdateTime[adsSlot];
                /* 0x300400b5: JG do-fade; 0x300400c3: JGE (in-window) commit_position. */
                if (last <= now) {
                    int32_t windowStart = coduo_int32_from_bits((uint32_t)now - (uint32_t)cg_frametime);
                    if (last >= windowStart) {
                        goto commit_position;
                    }
                }
                /* 0x300400c5: only when no HUD timeout is active. */
                if (cg_timeoutEndTime == 0) {
                    CG_StartFovFade(coduo_int32_from_bits((uint32_t)now - (uint32_t)FOV_FADE_BACKDATE_MS), FOV_FADE_QUICK_MS, 255);
                }
                cg_fovAdsUpdateTime[adsSlot] = now; /* 0x300400e3 */
                cg_fovTransitionTime = now;         /* 0x300400ea */
                goto tail;                          /* 0x300400f1 */
            }

        commit_position:
            /* 0x300400f6: commit the current vehiclePosition; fade on a change.
             * The commit-path CG_StartFovFade call at 0x3004010b is NOT guarded
             * by cg_timeoutEndTime -- 0x30447fd0 is never loaded between 0x300400f6
             * and 0x30040119; only the driver do-fade block at 0x300400c5 checks
             * it. A prior pass added a spurious `if (cg_timeoutEndTime == 0)` here,
             * suppressing the FOV fade on a vehicle-position change whenever a HUD
             * timeout was active. */
            if (cg_fovLastVehiclePosition != vehiclePosition) {
                CG_StartFovFade(coduo_int32_from_bits((uint32_t)now - (uint32_t)FOV_FADE_BACKDATE_MS), FOV_FADE_QUICK_MS, 255);
                cg_fovTransitionTime = now; /* 0x30040113 */
            }
            cg_fovAdsUpdateTime[adsSlot] = now;               /* 0x30040119 */
            cg_fovLastVehiclePosition = vehiclePosition;      /* 0x30040120 */
            goto tail;
        }
    }
    /* gameMode==5 reaches here (fov = 90) and jumps straight to the tail (0x300401f8). */
    goto tail;

non_vehicle:
    /* 0x3004012c: non-vehicle FOV, blended by the ADS zoom fraction. */
    {
        weaponInfo_t *weapon = bg_weaponInfos[cg_predictedPlayerState.currentWeapon];
        if (weapon->adsEnabled == 0) {   /* 0x30040140 */
            goto tail;
        }
        if (adsFrac == 1.0f) {           /* 0x30040148: fully zoomed */
            /* 0x30040161: FOV := current weapon's fully-zoomed ADS FOV. */
            fov = cg_currentWeaponInfo->adsZoomFov; /* +0x274 */
            goto tail;
        }
        if (adsFrac == 0.0f) {           /* 0x30040170: not zooming -> leave FOV as-is */
            goto tail;
        }

        /* 0x30040183: partial zoom. Map adsFrac through the in/out zoom fraction to a
         * blend parameter t in (0,1], then lerp FOV toward adsZoomFov by t. */
        weaponInfo_t *cur = cg_currentWeaponInfo;
        /* t is carried in st0/st1 across both `t <= 0` FCOMs, the FDIV, and the fov
         * lerp FMUL (0x3004019e..0x300401ed) with no float store -- long double. */
        long double t;
        if (cg_adsZoomingIn != 0) {
            /* 0x30040198: t0 = adsFrac - (1.0f - adsZoomInFrac). */
            t = (long double)adsFrac - ((long double)1.0f - cur->adsZoomInFrac);
            if (isnan(t) || t <= 0.0f) { /* FCOM/TEST 0x41 also bails unordered */
                goto tail;
            }
            t = t / cur->adsZoomInFrac;  /* 0x300401af */
        } else {
            /* 0x300401b7: t0 = adsFrac - (1.0f - adsZoomOutFrac). */
            t = (long double)adsFrac - ((long double)1.0f - cur->adsZoomOutFrac);
            if (isnan(t) || t <= 0.0f) { /* 0x300401c1 */
                goto tail;
            }
            t = t / cur->adsZoomOutFrac; /* 0x300401ce */
        }

        if (isnan(t) || t <= 0.0f) {     /* 0x300401d4: final guard on t */
            goto tail;
        }
        /* 0x300401e1: fov = fov - (fov - adsZoomFov) * t. */
        fov = fov - (fov - cur->adsZoomFov) * t;
    }

tail:
    /* 0x300401f8: FOV-fade transition maintenance. */
    if (!(flags & EF_IN_VEHICLE)) {
        /* 0x30040200: leaving/absent the vehicle view. */
        if (cg_fovLastVehiclePosition > 0) {
            int32_t fadeStart = coduo_int32_from_bits((uint32_t)now - (uint32_t)FOV_FADE_BACKDATE_MS);
            cg_fovFade.startTime = fadeStart;
            int32_t fadeEnd = coduo_int32_from_bits((uint32_t)fadeStart + (uint32_t)FOV_FADE_QUICK_MS);
            /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
            qboolean expired = fadeEnd <= now;
            cg_fovFade.startValue = 1.0f;
            cg_fovFade.durationMs = FOV_FADE_QUICK_MS;
            if (expired) {
                cg_fovFade.currentValue = 1.0f;                /* 0x3044b6a0 */
            }
            cg_fovTransitionTime = now;                        /* 0x30040234 */
            cg_fovLastVehiclePosition = 0;                     /* 0x3004023a */
        }
    }

    /* 0x30040244: settle-out an old transition. */
    int32_t transitionTime = cg_fovTransitionTime;
    if (transitionTime > 0) {
        int32_t staleThreshold = coduo_int32_from_bits((uint32_t)now - (uint32_t)FOV_FADE_STALE_MS);
        if (transitionTime < staleThreshold) {
            int32_t fadeStart = coduo_int32_from_bits((uint32_t)now - 1u);
            cg_fovFade.startTime = fadeStart;
            int32_t fadeEnd = coduo_int32_from_bits((uint32_t)fadeStart + (uint32_t)FOV_FADE_SETTLE_MS);
            /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
            qboolean expired = fadeEnd <= now;
            cg_fovFade.startValue = 0.0f;
            cg_fovFade.durationMs = FOV_FADE_SETTLE_MS;
            if (expired) {
                cg_fovFade.currentValue = 0.0f;                /* 0x30040279 */
            }
            cg_fovTransitionTime = -1;                         /* 0x30040283 */
        }
    }

    /* 0x3004028d: the scope/zoom flag pair overrides the FOV with a fixed value. */
    if (flags & EF_ZOOM_FOV_MASK) {
        return FOV_ZOOM_SCOPE; /* 0x30040296: 55.0f */
    }
    return fov;                /* 0x300402a0 */
}
