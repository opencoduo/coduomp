// Source: uo_cgame_mp_x86.dll 0x30042ac0..0x30042b4e
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30042ac0_30042b4e.mcode
//
// CG_StartWeaponAnim (0x30042ac0)
// ------------------------------------
// Emit CG_XANIM_SET_GOAL_WEIGHT once for each of 21 weapon XAnim nodes,
// activating `activeAnimIndex` and resetting all the others.
//
// The record is cg_weaponInfos[weaponIndex] (0x30413580, stride 0x1c4 = 452 bytes;
// IMUL EAX,EAX,0x1c4 / ADD EAX,0x30413580). The walked field block is the 21
// consecutive dwords at record offsets +0x8..+0x58 (LEA EDI,[EAX+0x8]; the loop
// advances EDI by 4 and decrements a 0x15=21 counter). Each dword is the node's
// animation-rate float, forwarded to the engine unchanged.
//
// ABI (register + one cdecl stack arg, plain RET so the caller cleans the stack
// arg; register args proven from the call sites at 0x30042e33/0x30042e8d, which set
// EAX=[edi+0xd8] (weapon index), EBX=esi (runtime XAnim tree), and PUSH the
// active XAnim index 0x13/0x2):
//   EAX  = weaponIndex   (record selector; IMUL by 0x1c4)
//   EBX  = animTree      (forwarded straight into every trap call, arg1)
//   [sp] = activeAnimIndex (compared against the 1-based index each iteration)
//
// For each animIndex in 1..21 the function issues XAnimSetGoalWeight(tree,
// animIndex, weight, blendTime, rate, notifyName, restart). For the active node:
//   weight=1.0f, blendTime=0.0f, notifyName=1, restart=1
// and for every other node:
//   weight=0.0f, blendTime=0.0f, notifyName=0, restart=0.
// The 0.0f base is FLD [0x3007bcec] (bit pattern 0x00000000); the 1.0f is the
// immediate 0x3f800000 written on the selected branch. blendTime is always 0.0f.
//
// NAME: the macOS/PPC CG_StartWeaponAnim body performs the same 0x1c4 record
// indexing, 1..21 loop, active-index compare, animRates[] loads, and paired
// trap_XAnimSetGoalWeight calls. This supersedes the old role name.

#include <stdint.h>

#include "client/cgame/globals.h"          /* cg_weaponInfos, cgWeaponInfo_t, cgame_syscall */
#include "client/cgame/client_recovered.h" /* CG_XANIM_SET_GOAL_WEIGHT */

void CG_StartWeaponAnim(int32_t weaponIndex, intptr_t animTree,
                        int32_t activeAnimIndex)
{
    cgWeaponInfo_t *info = &cg_weaponInfos[weaponIndex];

    /* The 21 per-node dwords are cgWeaponInfo_t.animRates[1..21] at record
     * +0x8..+0x58 (LEA EDI,[EAX+0x8]). */
    int32_t animIndex;
    for (animIndex = WEAPON_XANIM_IDLE;
         animIndex < WEAPON_XANIM_ADS_UP;
         ++animIndex) {
        int32_t animRateBits = CG_FloatBits(info->animRates[animIndex]);

        if (animIndex == activeAnimIndex) {
            cgame_syscall(CG_XANIM_SET_GOAL_WEIGHT, animTree, animIndex,
                          CG_FloatBits(1.0f), CG_FloatBits(0.0f),
                          animRateBits, 1, 1);
        } else {
            cgame_syscall(CG_XANIM_SET_GOAL_WEIGHT, animTree, animIndex,
                          CG_FloatBits(0.0f), CG_FloatBits(0.0f),
                          animRateBits, 0, 0);
        }
    }
}
