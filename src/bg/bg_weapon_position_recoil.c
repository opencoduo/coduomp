// Source: uo_cgame_mp_x86.dll 0x30015760..0x30015914
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30015760_30015914.mcode
//
// BG_CalculateWeaponPosition_GunRecoil — the fifth (weapon-idle) angle
// contributor accumulated by BG_CalculateWeaponAngles (0x30015920),
// which calls it last in the chain (idle, idle-sway, sway, view-kick, WEAPON-IDLE)
// with `angles` and `state` both pushed on the stack (0x300159dc: PUSH EDI(angles);
// PUSH ESI(state); CALL 0x30015760). It advances a persistent two-axis damped sway
// spring on the state block over the frame's time budget and folds the resulting
// pitch/yaw offset (plus a static roll) into `angles`.
//
// The .mcode header name "ClientUserinfoChanged" is REJECTED. It is a size-only
// guess (win size 0x1b4 == a same-size function in game_mp.dll — the WRONG,
// server-side module) with zero behavioral basis. This is cgame code: it reads no
// userinfo string, touches no configstring/clientinfo table, and does no name/team
// parsing. The name adopted here is the one the already-reconstructed caller uses
// for this slot (client_recovered.h: BG_CalculateWeaponPosition_GunRecoil),
// proven by the call graph and by the fields it reads. Same-module PPC family is
// BG_CalculateWeaponPosition_*; exact family suffix provisional.
//
// Note on 0x30134cd8: it is bg_weaponInfos (the weaponInfo_t* array, indexed by
// ps->currentWeapon), identical to the sibling contributors' use of it — NOT a
// clientinfo block.
//
// Callee 0x30015610 is BG_CalculateWeaponPosition_GunRecoil_SingleAngle (reconstructed, FUN_30015610_3001575e.c):
// one bounded/damped 1-D spring integration step returning *pValue toward 0 via the
// companion *pRate, returning qtrue only when the spring has settled. Its non-default
// ABI (value ptr EDX, rate ptr ECX, six float params on the stack) is proven at both
// call sites here.
//
// Register/stack ABI: the two stack arguments as EMITTED are state at [esp+4] and angles
// at [esp+8] (the caller pushes angles then state, right-to-left). The signature below is
// kept as (angles, state) to match the shared header declaration and the caller's C call;
// the two contributors and their caller are treated as one abstract-ABI family.
//
// Float constants (exact .rdata addresses, dumped via objdump -s -j .rdata):
//   0.0f   : float  @0x3007bcec  (compare-against-zero for frameTime and the accum)
//   0.005  : double @0x3007c048  (7b14ae47 e17a743f -> 0.005) — the fixed substep
//   0.005f : imm    0x3ba3d70a   (the substep as a 32-bit float when a whole 5ms
//                                 substep is taken; MOV [ESP+0x14],0x3ba3d70a)

#include "bg_weapon_position.h"
#include "bg_bob.h"
#include "bg_bob_binding.h"
#include "bg_vehicle.h"
#include "bg_weapon.h"
#include "compat/coduo_int32_bits.h"
#include "compat/coduo_x87emu.h"
#include "math/q_math.h"

/* The spring simulation is advanced in fixed 0.005 s (5 ms) substeps. */
#define WEAPON_IDLE_SWAY_SUBSTEP 0.005

void BG_CalculateWeaponPosition_GunRecoil(pm_weapon_angle_state_t *state, vec3_t angles)
{
    playerState_t *ps = state->ps;               /* 0x30015764: EAX = [EDX] */

    /* 0x30015766..0x3001578b: vehicle-pose gate (same as the caller and the other
     * contributors). Off-vehicle players skip it; a mounted player only produces the
     * idle contribution for the gunner pose vehicleType==1 && vehiclePosition==3. */
    if (ps->entityStateFlags & EF_IN_VEHICLE) {
        if (ps->vehicleType != 1) {             /* CMP [EAX+0x618],1 / JNZ ret */
            return;
        }
        if (ps->vehiclePosition != 3) {         /* CMP [EAX+0x614],3 / JNZ ret */
            return;
        }
    }

    /* 0x30015791..0x3001579e: current weapon's weaponInfo_t. */
    int32_t currentWeapon = ps->currentWeapon;
    weaponInfo_t **weaponTable = bg_weaponInfos;
    weaponInfo_t *weaponInfo = weaponTable[currentWeapon]; /* [0x30134cd8 + [ps+0xd8]*4] */

    /* 0x300157a1..0x300157a9: only weapons that support aim-down-sight carry the idle
     * sway envelope; when adsEnabled == 0 there is nothing to add (`angles` untouched). */
    if (weaponInfo->adsEnabled == 0) {
        return;
    }

    /* 0x300157af..0x300157be: the frame's time budget (seconds) is state->frameTime.
     * ps->adsFraction (kept live on the x87 stack as ST1) is the hip<->ADS lerp
     * factor for the four spring coefficients below. */
    float adsFraction = ps->adsFraction;        /* FLD [EAX+0xe0] */
    float frameTime = state->frameTime;          /* [EDX+0x8] */

    /* 0x300157b8..0x30015818: blend each spring coefficient from its hip value toward
     * its ADS value by adsFraction:  c = (ads - hip) * adsFraction + hip.
     * (FLD ads; FSUB hip; FMUL ST1(adsFraction); FADD hip; FSTP slot.) */
#if EMULATE_X87
#define BG_RECOIL_LERP(hip, ads) \
    x87f_store_f32(x87f_add(x87f_mul(x87f_sub(x87f_load_f32(ads), x87f_load_f32(hip)), x87f_load_f32(adsFraction)), x87f_load_f32(hip)))
    float springAccel = BG_RECOIL_LERP(weaponInfo->recoilReturnHip, weaponInfo->recoilReturnAds);
    float velLimit = BG_RECOIL_LERP(weaponInfo->recoilVelocityHip, weaponInfo->recoilVelocityAds);
    float dampCoef = BG_RECOIL_LERP(weaponInfo->recoilDampingHip, weaponInfo->recoilDampingAds);
    float dampAccel = BG_RECOIL_LERP(weaponInfo->recoilFrictionHip, weaponInfo->recoilFrictionAds);
#undef BG_RECOIL_LERP
#else
    float springAccel = (weaponInfo->recoilReturnAds - weaponInfo->recoilReturnHip) * adsFraction + weaponInfo->recoilReturnHip;
    float velLimit = (weaponInfo->recoilVelocityAds - weaponInfo->recoilVelocityHip) * adsFraction + weaponInfo->recoilVelocityHip;
    float dampCoef = (weaponInfo->recoilDampingAds - weaponInfo->recoilDampingHip) * adsFraction + weaponInfo->recoilDampingHip;
    float dampAccel = (weaponInfo->recoilFrictionAds - weaponInfo->recoilFrictionHip) * adsFraction + weaponInfo->recoilFrictionHip;
#endif

    /* 0x3001581c: discard the retained adsFraction (FSTP ST0). */

    /* 0x3001581e..0x3001582d: nothing to integrate when the frame stepped no time
     * forward (frameTime <= 0.0f). Fall straight to the fold-in tail. */
    if (frameTime > 0.0f /* float @0x3007bcec */) {
        /* 0x30015840..0x300158e5: advance both axes' springs over `frameTime` in
         * fixed 0.005 s substeps (a standard accumulator), stopping early once both
         * springs have settled. `remaining` is the frame accumulator; on the final
         * partial substep it is clamped to what is left. */
        float remaining = frameTime;

        for (;;) {
            /* 0x30015844..0x3001587d: substep dt = min(remaining, 0.005). */
            float dt;
            if (remaining > WEAPON_IDLE_SWAY_SUBSTEP /* double @0x3007c048 */) {
                dt = 0.005f;                    /* imm 0x3ba3d70a */
#if EMULATE_X87
                remaining = x87f_store_f32(x87f_sub(x87f_load_f32(remaining), x87f_load_f64(WEAPON_IDLE_SWAY_SUBSTEP)));
#else
                remaining -= WEAPON_IDLE_SWAY_SUBSTEP;
#endif
            } else {
                dt = remaining;
                remaining = 0.0f;
            }

            /* 0x30015889..0x3001589b: axis 0 (pitch) spring step. posLimit is the
             * per-axis limit gunMaxPitch; the four blended coefficients are
             * shared by both axes. */
            qboolean settled0 = BG_CalculateWeaponPosition_GunRecoil_SingleAngle(
                &state->recoilPitch, &state->recoilPitchVelocity, dt, weaponInfo->gunMaxPitch, springAccel, velLimit, dampCoef, dampAccel);

            /* 0x300158a0..0x300158c2: axis 1 (yaw) spring step, posLimit
             * gunMaxYaw. */
            qboolean settled1 = BG_CalculateWeaponPosition_GunRecoil_SingleAngle(
                &state->recoilYaw, &state->recoilYawVelocity, dt, weaponInfo->gunMaxYaw, springAccel, velLimit, dampCoef, dampAccel);

            /* 0x300158ca..0x300158d4: stop as soon as BOTH springs report settled. */
            if (settled1 && settled0) {
                break;
            }
            /* 0x300158d6..0x300158e5: otherwise keep stepping until the budget is
             * exhausted (accum > 0.0f). */
            if (!(remaining > 0.0f /* float @0x3007bcec */)) {
                break;
            }
        }
    }

    /* 0x300158f2..0x3001590c: fold the settled idle-sway offset into the angle
     * vector — pitch/yaw from the two sprung values, roll from the static term. */
#if defined(WINDOWS_BEHAVIOR)
    angles[0] = state->recoilPitch + angles[0];
    angles[1] = state->recoilYaw + angles[1];
    angles[2] = state->recoilRoll + angles[2];
#elif EMULATE_X87
    angles[0] = x87f_store_f32(x87f_add(x87f_load_f32(angles[0]), x87f_load_f32(state->recoilPitch)));
    angles[1] = x87f_store_f32(x87f_add(x87f_load_f32(angles[1]), x87f_load_f32(state->recoilYaw)));
    angles[2] = x87f_store_f32(x87f_add(x87f_load_f32(angles[2]), x87f_load_f32(state->recoilRoll)));
#else
    angles[0] += state->recoilPitch;
    angles[1] += state->recoilYaw;
    angles[2] += state->recoilRoll;
#endif
}
