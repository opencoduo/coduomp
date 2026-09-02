// Source: uo_cgame_mp_x86.dll 0x30015920..0x30015a08
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30015920_30015a08.mcode
//
// BG_CalculateWeaponAngles — compute the animated weapon/view ANGLE
// offset vector `angles` (pitch = angles[0], yaw = angles[1], roll = angles[2]). It zeroes all
// three components, folds in the ADS lean roll and the weapon's ADS pitch offset,
// accumulates five sibling angle-component contributions, and finally subtracts the
// caller-supplied reference view angles (pitch/yaw) with AngleSubtract.
//
// This is the top-level angle assembler in the BG_CalculateWeaponPosition
// family. It produces ANGLES and calls the five angle-component helpers,
// including BG_CalculateWeaponPosition_BobOffset. Name adopted by role from
// that same-module PPC family cluster
// (cgame_mp.dll BG_CalculateWeaponPosition_*), and from the call graph: the wrapper
// CG_CalculateWeaponPosition (0x30046570) calls this first (0x3004679b) and feeds
// `angles` into an angle->axis transform. The Mac
// BG_CalculateWeaponAngles performs the corresponding weapon-info,
// ADS, and idle-angle composition, resolving the family suffix.
//
// The .mcode header's size-only guess PM_SetReloadingState (win size 0xe8 matched
// size 0xe8) is REJECTED: this returns void, writes a 3-float angle vector, and has
// no reload/weapon-state side effects — it is not a reload-state setter.
//
// Non-default register ABI (proven at the caller 0x3004679b: LEA EAX,[ESP+0x14] as
// the output vector, LEA ESI,[ESP+0x20] as the state block): the output vector
// arrives in EAX and the state parameter block in ESI; no stack args; plain RET.
// Expressed here as (angles, state). EBP save/restore and the ADD ESP cleanup of the
// callee-arg scratch are i386 calling-convention detail, not source-level behavior.

#include "bg_weapon_position.h"
#include "bg_bob.h"
#include "bg_bob_binding.h"
#include "bg_vehicle.h"
#include "bg_weapon.h"
#include "compat/coduo_int32_bits.h"
#include "compat/coduo_x87emu.h"
#include "math/q_math.h"

#include <math.h>

void BG_CalculateWeaponAngles(pm_weapon_angle_state_t *state,
                              vec3_t angles)
{
    /* 0x30015921: ECX = state->ps (state arrives in ESI, ps is at state+0x00). */
    playerState_t *ps = state->ps;

    /* 0x30015926..0x30015946: vehicle-pose gate. If the player is riding in a
     * vehicle (entityStateFlags bit 0x100000 set), the angle offset is only produced
     * for the specific gunner pose vehicleType==1 && vehiclePosition==3; any other
     * vehicle pose returns immediately, leaving `angles` untouched. A non-vehicle player
     * (bit clear) skips the check entirely (JZ over it). */
    if (ps->entityStateFlags & EF_IN_VEHICLE) {
        if (ps->vehicleType != 1) {         /* CMP [ps+0x618],1 / JNZ 0x30015a05 */
            return;
        }
        if (ps->vehiclePosition != 3) {     /* CMP [ps+0x614],3 / JNZ 0x30015a05 */
            return;
        }
    }

    /* 0x3001594c..0x3001595a: zero all three angle components. (XOR EDX,EDX then
     * store EDX to angles[2], angles[1], angles[0]; EDX stays 0 and is reused below as the
     * adsEnabled compare operand.) The 0.0f (.rdata 0x3007bcec) is loaded onto the
     * x87 stack here as ST1 for the leanFraction compare that follows. */
    angles[0] = 0.0f;
    angles[1] = 0.0f;
    angles[2] = 0.0f;

    /* 0x3001595c..0x30015991: ADS lean roll -> angles[2].
     * FLD leanFraction / FUCOMPP against 0.0f / FNSTSW / TEST AH,0x44 / JNP: proceeds
     * only when leanFraction != 0.0f (equal takes the JNP and skips). Then:
     *   |lean| via AND 0x7fffffff on the stored float bits;
     *   angles[2] = leanFraction * (2.0f - |lean|) * -2.0f
     * (FLD 2.0f @0x3007bce4, FSUB |lean|, FMUL ST1(=lean), FMUL -2.0f @0x3007c150).
     * 0.0f/2.0f/-2.0f are exactly representable, written as natural float literals. */
    if (ps->leanFraction != 0.0f) {
#if defined(WINDOWS_BEHAVIOR) && EMULATE_X87
        const float absLean = fabsf(ps->leanFraction);
        angles[2] = x87f_store_f32(x87f_mul(
            x87f_mul(x87f_load_f32(ps->leanFraction),
                     x87f_sub(x87f_load_f32(2.0f),
                              x87f_load_f32(absLean))),
            x87f_load_f32(-2.0f)));
#elif defined(WINDOWS_BEHAVIOR)
        const float absLean = fabsf(ps->leanFraction);
        angles[2] = (float)(
            ((long double)ps->leanFraction *
             (2.0L - (long double)absLean)) * -2.0L);
#elif EMULATE_X87
        angles[2] = x87f_store_f32(x87f_sub(
            x87f_load_f32(angles[2]),
            x87f_mul(
                x87f_mul(
                    x87f_sub(x87f_load_f32(2.0f),
                             x87f_abs(x87f_load_f32(ps->leanFraction))),
                    x87f_load_f32(ps->leanFraction)),
                x87f_load_f32(2.0f))));
#else
        angles[2] -= GetLeanFraction(ps->leanFraction) * 2.0f;
#endif
    }

    /* 0x30015993..0x300159b9: ADS pitch offset -> angles[0].
     * weaponInfo = bg_weaponInfos[ps->currentWeapon]. When the weapon supports ADS
     * (adsEnabled != 0; CMP [wi+0x328],EDX(=0) / JZ skips otherwise):
     *   angles[0] = weaponInfo->adsPitchOffset * ps->adsFraction
     * (FLD [wi+0x3c8] / FMUL [ps+0xe0] / FSTP [angles]). */
    weaponInfo_t *weaponInfo = bg_weaponInfos[ps->currentWeapon];
    if (weaponInfo->adsEnabled != 0) {
        angles[0] = weaponInfo->adsPitchOffset * ps->adsFraction;
    }

    /* 0x300159bb..0x300159de: accumulate the five sibling angle-component
     * contributions into `angles`. Call order and per-callee register/stack ABI are
     * preserved exactly as emitted (see the header declarations for each ABI). */
    BG_CalculateWeaponPosition_BasePosition_angles(state, angles); /* CALL 0x30014ea0 */
    BG_CalculateWeaponPosition_IdleAngles(state, angles);             /* CALL 0x300151d0 (angles EDI, state ECX) */
    BG_CalculateWeaponPosition_BobOffset(state, angles);                 /* CALL 0x300152f0 (angles EBX, state EAX) */
    BG_CalculateWeaponPosition_DamageKick(state, angles);    /* CALL 0x300154d0 (angles ECX, state EDX) */
    BG_CalculateWeaponPosition_GunRecoil(state, angles);  /* CALL 0x30015760 (angles & state on stack) */

    /* 0x300159e3..0x300159fe: fold the reference view angles back angles. Arguments are
     * pushed a then b (right-to-left cdecl), so a = angles[i], b = state->baseAngles[i]:
     *   angles[0] = AngleSubtract(angles[0], state->baseAngles[0]);   (state->+0x44)
     *   angles[1] = AngleSubtract(angles[1], state->baseAngles[1]);   (state->+0x48)
     * Only pitch and yaw are folded; the roll (angles[2]) computed above is kept as-is. */
    angles[0] = AngleSubtract(angles[0], state->baseAngles[0]);
    angles[1] = AngleSubtract(angles[1], state->baseAngles[1]);
}
