#include "../client_recovered.h"
#include "../globals.h"

// Source: uo_cgame_mp_x86.dll 0x30047370..0x3004738d
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30047370_3004738d.mcode
//
// Role: predicate — "is weapon `weapon` currently held by the local player?".
// Reads the cg_predictedPlayerState.weaponBits (0x304836f8) 128-bit per-weapon ownership bitset and
// returns the value of the bit for the given weapon index. This is the shared
// point-query behind the weapon-select scanners in this cluster (0x30046bb0 /
// 0x300475f0 / 0x300478a0 / 0x30047960), all of which perform the identical
// `[reg*4 + 0x304836f8]` word/mask test inline; here it is factored into a tiny
// boolean predicate.
//
// Name adjudication: the .mcode header's `GScr_FreeScripts` is a pure win-size
// match (0x1d vs 0x1c) with ZERO behavioral basis and is REJECTED — this function
// frees nothing and touches no script state; it is a one-line bitset membership
// test over cg_predictedPlayerState.weaponBits. No exact CoD source symbol is proven for this exact
// predicate, so the name is behavioral (marked by the proven role: it queries the
// weapon-held bitset).
//
// ABI: the weapon index arrives in EDX (a compiler-chosen register argument in
// this binary; no ECX or stack input is read). Modeled as one int parameter; the
// EDX delivery is an i386 calling-convention detail. Body ends in a bare RET.
//
// Instruction trace:
//   30047370  MOV ECX,EDX                 ECX = weapon
//   30047372  AND ECX,0x1f                CL  = weapon & 31   (bit position)
//   30047375  MOV EAX,0x1
//   3004737a  SHL EAX,CL                  EAX = 1u << (weapon & 31)   (bit mask)
//   3004737c  SAR EDX,0x5                 EDX = weapon >> 5   (word index, signed)
//   3004737f  AND EAX,[EDX*4 + 0x304836f8] EAX = mask & cg_predictedPlayerState.weaponBits[word]
//   30047386  NEG EAX
//   30047388  SBB EAX,EAX
//   3004738a  NEG EAX                     EAX = (EAX != 0) ? 1 : 0   (booleanize)
//   3004738c  RET

qboolean CG_IsWeaponHeld(int weapon)
{
    uint32_t mask = 1u << ((uint32_t)weapon & 0x1f);
    int32_t word = coduo_int32_sar((uint32_t)weapon, 5);

    return (cg_predictedPlayerState.weaponBits[word] & mask) != 0 ? qtrue : qfalse;
}
