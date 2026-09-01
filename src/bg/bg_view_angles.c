// Source: uo_cgame_mp_x86.dll 0x30015c30..0x30015c4d
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30015c30_30015c4d.mcode
//
// BG_CalculateViewAngles — assemble the animated weapon/view POSITION offset.
//
// The mechanical .mcode name IsItemRegistered is a size-only guess (win size 0x1d
// matched a same-size corpus symbol) and is REJECTED: the body returns nothing,
// zeroes a 3-float offset vector, and forwards a bg_view_angle_state_t block to the two
// position helpers. That is the position half of the BG_CalculateViewAngles
// family (its angle counterpart is BG_CalculateWeaponAngles,
// 0x30015920); it is not an item-registration predicate.
//
// Register ABI (non-default, proven from the bytes):
//   EAX  -> ESI : the output offset vector `angles` (this / destination)
//   ECX  -> EBX : the bg_view_angle_state_t parameter block `state`
// No stack arguments; plain RET (0xc3). PUSH EBX / PUSH ESI at entry and the
// matching POPs at exit are callee-saved-register bookkeeping, not source behavior.
//
// Machine-code trace:
//   30015c30 push ebx ; 30015c31 push esi              (save callee-saved regs)
//   30015c32 mov esi,eax                               angles       = EAX
//   30015c34 xor eax,eax                               EAX       = 0
//   30015c36 mov ebx,ecx                               state      = ECX
//   30015c38 mov [esi+8],eax                           angles[2]    = 0   (Z)
//   30015c3b mov [esi+4],eax                           angles[1]    = 0   (Y)
//   30015c3e mov [esi],eax                             angles[0]    = 0   (X)
//   30015c40 call 0x30015a10   (angles in ESI, state in ECX)  -> DamageKick: writes angles[0],angles[2]
//   30015c45 call 0x30015b50   (angles in ESI, state in EBX)  -> BobOffset: writes angles[0],angles[1]
//   30015c4a pop esi ; 30015c4b pop ebx ; 30015c4c ret (0xc3)
//
// Note the store order is [+8], [+4], [+0]; all three are the same zero (EAX was
// XOR'd to 0), so the visible effect is angles[0]=angles[1]=angles[2]=0. DamageKick receives
// state in ECX (untouched between `mov ebx,ecx` and its call) and BobOffset receives
// state in EBX; both are the same pointer, and both accumulate into `angles` in ESI.

#include "bg_weapon_position.h"

// Source: uo_cgame_mp_x86.dll 0x30015c30..0x30015c4d
void BG_CalculateViewAngles(bg_view_angle_state_t *state, vec3_t angles)
{
    /* Reset the running position-offset accumulator (all three components 0). */
    angles[2] = 0.0f;
    angles[1] = 0.0f;
    angles[0] = 0.0f;

    /* Damage kick (writes angles[0] and angles[2]); state passed in ECX. */
    BG_CalculateView_DamageKick(state, angles);

    /* ADS view bob (writes angles[0] and angles[1]); state passed in EBX. */
    BG_CalculateView_Velocity(state, angles);
}
