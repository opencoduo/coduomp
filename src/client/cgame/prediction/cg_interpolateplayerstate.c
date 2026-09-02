// Source: uo_cgame_mp_x86.dll 0x300354b0..0x30035676
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_300354b0_30035676.mcode

#include "../client_recovered.h"
#include "../globals.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

/*
 * CG_InterpolatePlayerState(qboolean grabAngles)
 *
 * The .mcode size-guess "G_UpdateClientInfo" is REJECTED: that is a game_mp.dll
 * (server) symbol matched only by byte size (win 0x1c6 vs 0x1c8). This function is
 * cgame code — it reads cg_snap/cg_nextSnap (0x30459160/0x30459164), the cgame
 * predicted-player globals, and the cgame usercmd traps — and its behaviour is the
 * stock Quake3/CoD CG_InterpolatePlayerState.
 *
 * It produces cg.predictedPlayerState for the current render frame:
 *
 *   1. Copies the incoming snapshot's playerState wholesale into
 *      cg.predictedPlayerState (MOVSD.REP of 0x1141 == 4417 dwords == 0x4504 bytes,
 *      the embedded playerState at cg_nextSnap + 0x0c). This is the classic
 *      `cg.predictedPlayerState = cg.nextSnap->ps;` copy.
 *
 *   2. Caches the current predicted weapon's weaponInfo_t record:
 *      cg_currentWeaponInfo = bg_weaponInfos[cg.predictedPlayerState.currentWeapon]
 *      (bg_weaponInfos base 0x30134cd8, index 0x3048329c, cache 0x30487980).
 *
 *   3. When `grabAngles` is set, fetches the most recent buffered usercmd via
 *      trap_GetCurrentCmdNumber (CG_GET_CURRENT_CMD_NUMBER 0x53) / trap_GetUserCmd
 *      (CG_GET_USER_CMD 0x54) into a local usercmd_t, then runs PM_UpdateViewAngles
 *      on cg.predictedPlayerState with that command, threading the trace/mark
 *      callback CG_TraceCapsule (0x30035390) as the `ctx` argument. This lets the
 *      view angles track the very latest input instead of the (older) snapshot's.
 *
 *   4. When the next snapshot is strictly ahead of the current one in server time,
 *      lerps the smoothly-interpolated view state between cg_snap and cg_nextSnap
 *      by the fraction
 *          t = (cg.time - cg_snap->serverTime)
 *              / (cg_nextSnap->serverTime - cg_snap->serverTime)
 *      writing bobCycle (wrap + truncating Q_rint), origin, velocity, viewangles
 *      (per-component short-way LerpAngle, only when NOT grabbing angles from the
 *      command), viewHeightCurrent, adsFraction, leanFraction, and the +0x628 ps scalar
 *      into the corresponding predicted/view globals.
 *
 * ABI: one stack argument `grabAngles` (read at [ESP+0x40] after SUB ESP,0x2c and
 * four register pushes == the first argument). Returns nothing; EBX/EBP/ESI/EDI are
 * callee-saved.
 *
 * Callees:
 *   - cgame_syscall (0x30085e9c): traps 0x53 (CG_GET_CURRENT_CMD_NUMBER) and 0x54
 *     (CG_GET_USER_CMD).
 *   - PM_UpdateViewAngles (0x3000c8e0): ps in EAX, cmd + ctx on the stack.
 *   - LerpAngle (0x3004bd00): shared short-way angle interpolation, (from,to,frac).
 *   - Q_rint (0x3006be3c == _ftol2): TRUNCATES toward zero (the FMUL result is
 *     converted with a plain (int) cast, reproduced faithfully below).
 */

void CG_InterpolatePlayerState(qboolean grabAngles)
{
    snapshot_t *snap = cg_snap;      /* EBX */
    snapshot_t *next = cg_nextSnap;  /* EBP */

    playerState_t *out = &cg_predictedPlayerState;

    /* 1. cg.predictedPlayerState = cg.nextSnap->ps; (0x1141 dwords from next+0x0c). */
    memcpy(out, &next->ps, sizeof(*out));

    /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered client-module boundary input and state before use. */
    cg_currentWeaponInfo = bg_weaponInfos[cg_predictedPlayerState.currentWeapon];

    /* 3. Optionally fold the latest usercmd's angles into the predicted state. */
    if (grabAngles) {
        usercmd_t cmd;
        int32_t cmdNumber = coduo_int32_from_bits((uint32_t)cgame_syscall(CG_GET_CURRENT_CMD_NUMBER));
        cgame_syscall(CG_GET_USER_CMD, cmdNumber, &cmd);
        /* ctx = CG_TraceCapsule (0x30035390), the capsule-trace callback. */
        PM_UpdateViewAngles(out, &cmd, CG_TraceCapsule);
    }

    /* 4. Interpolate only when the next snapshot is ahead in server time. */
    if (next->serverTime <= snap->serverTime) {
        return;
    }

    /* The machine forms both operands as 32-bit integer differences and FILDs them
     * (signed): numerator (cg.time - snap.serverTime), denominator (next - snap). */
    int32_t sinceSnap = coduo_int32_from_bits(cg_time - (uint32_t)snap->serverTime);
    int32_t snapSpan = coduo_int32_from_bits((uint32_t)next->serverTime - (uint32_t)snap->serverTime);
    float t = (float)((long double)sinceSnap / (long double)snapSpan);

    /* bobCycle: wrap the next value up by 256 when it is below the current one,
     * then snap + (int)((next - snap) * t). Q_rint (_ftol2) TRUNCATES toward zero. */
    {
        int32_t nextBob = next->ps.bobCycle;
        if (nextBob < snap->ps.bobCycle) {
            nextBob = coduo_int32_from_bits((uint32_t)nextBob + 256u);
        }
        int32_t bobDelta = coduo_int32_from_bits((uint32_t)nextBob - (uint32_t)snap->ps.bobCycle);
        int32_t bobStep = coduo_fp_to_i32_extended((long double)bobDelta * (long double)t);
        out->bobCycle = coduo_int32_from_bits((uint32_t)snap->ps.bobCycle + (uint32_t)bobStep);
    }

    /* Interpolate ps.aimSpreadScale into its predicted-player-state field. */
    cg_predictedPlayerState.aimSpreadScale =
        (float)(((long double)next->ps.aimSpreadScale - (long double)snap->ps.aimSpreadScale) * (long double)t +
                (long double)snap->ps.aimSpreadScale);

    /* origin[3], velocity[3], and (only when not grabbing angles) viewangles[3]
     * are interpolated together in one 3-iteration loop.
     *   origin   -> cg_predictedPlayerState.psOrigin (0x304831d8 == predictedPlayerState.origin)
     *   viewangle-> interpolated-view-angle globals (0x304832ac/b0/b4, LerpAngle)
     *   velocity -> predictedPlayerState.velocity (0x304831e4) */
    for (int32_t i = 0; i < 3; i++) {
        cg_predictedPlayerState.psOrigin[i] =
            (float)(((long double)next->ps.psOrigin[i] - (long double)snap->ps.psOrigin[i]) * (long double)t +
                    (long double)snap->ps.psOrigin[i]);

        /* 0x300355d4..0x300355ff: the view-angle call is interleaved between
         * this component's origin and velocity stores. */
        if (!grabAngles) {
            cg_predictedPlayerState.viewAngles[i] = LerpAngle(snap->ps.viewAngles[i], next->ps.viewAngles[i], t);
        }

        out->velocity[i] = (float)(((long double)next->ps.velocity[i] - (long double)snap->ps.velocity[i]) * (long double)t +
                                   (long double)snap->ps.velocity[i]);
    }

    /* viewHeightCurrent -> cg_predictedPlayerState.viewHeightCurrent (0x304832bc). */
    cg_predictedPlayerState.viewHeightCurrent =
        (float)(((long double)next->ps.viewHeightCurrent - (long double)snap->ps.viewHeightCurrent) * (long double)t +
                (long double)snap->ps.viewHeightCurrent);

    /* leanFraction -> shared interpolated global at 0x30483208. */
    cg_predictedPlayerState.leanFraction =
        (float)(((long double)next->ps.leanFraction - (long double)snap->ps.leanFraction) * (long double)t +
                (long double)snap->ps.leanFraction);

    /* adsFraction -> cg_predictedPlayerState.adsFraction (0x304832a4). */
    cg_predictedPlayerState.adsFraction = (float)(((long double)next->ps.adsFraction - (long double)snap->ps.adsFraction) * (long double)t +
                                                  (long double)snap->ps.adsFraction);
}
