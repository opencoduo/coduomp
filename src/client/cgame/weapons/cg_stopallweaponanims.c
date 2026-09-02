// Source: uo_cgame_mp_x86.dll 0x30042a30..0x30042abe
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30042a30_30042abe.mcode
//
// CG_StopAllWeaponAnims (0x30042a30)
// -------------------------------------
// The fixed-IDLE variant of CG_StartWeaponAnim (0x30042ac0, the immediately
// following function). It emits CG_XANIM_SET_GOAL_WEIGHT once for each ordinary
// weapon XAnim node, activating IDLE at weight 1.0f and resetting nodes EMPTY_IDLE
// through LMG_BREAKDOWN to weight 0.0f. Unlike the sibling,
// the active node is hardcoded to index 1 (CMP SI,0x1), so this function takes no
// activeAnimIndex argument.
//
// The record is cg_weaponInfos[weaponIndex] (0x30413580, stride 0x1c4 = 452 bytes;
// IMUL EAX,EAX,0x1c4 / ADD EAX,0x30413580 / MOV EDI,EAX). The walked field block is
// the 21 consecutive dwords at record offsets +0x8..+0x58. Both branches read the
// same element per iteration:
//   animIndex==1 branch: MOV EAX,[EDI+0x8]              -> record+0x8
//   else branch:         FLD [EDI + animIndex*4 + 0x4]  -> record + 0x4 + animIndex*4
// which for animIndex 1..21 is record+0x8..+0x58, i.e. animRates[1..21].
//
// ABI (register-only, no stack args; plain RET). Register args proven from the sole
// caller at 0x30039670 (0x300396aa..0x300396cd), which sets:
//   EAX = [ [0x30459160] + 0xe4 ]           -> weaponIndex (record selector)
//   EBX = [weaponInfoField + 0x30413580]    -> animTree (forwarded trap arg1)
// EBX is never written in this function; it is caller-supplied and forwarded into
// every trap call unchanged, exactly as in CG_StartWeaponAnim.
//
// For each animIndex in 1..21 the function issues XAnimSetGoalWeight(tree,
// animIndex, weight, blendTime, rate, notifyName, restart). IDLE uses weight 1.0f,
// notifyName 1, and restart 1; the remaining nodes use zero for those arguments.
//
// x87 detail: FLD [0x3007bcec] loads the base 0.0f (the .rdata dword at 0x3007bcec
// is exactly 0x00000000; the adjacent 0.5f at 0x3007bce8 is NOT used here). In the
// selected branch it is stored as blendTime; the 1.0f is the immediate 0x3f800000.
// In the reset branch the rate is FLD'd on top of the 0.0f, popped to a dword,
// leaving the 0.0f which is then written to both weight and blendTime.
//
// NAME: the macOS/PPC CG_StopAllWeaponAnims body performs the same 0x1c4 record
// indexing, 1..21 loop, animRates[] loads, hardcoded index-1 compare, and paired
// trap_XAnimSetGoalWeight calls. This supersedes the old role name.

#include <stdint.h>

#include "client/cgame/globals.h"          /* cg_weaponInfos, cgWeaponInfo_t, cgame_syscall */
#include "client/cgame/client_recovered.h" /* CG_XANIM_SET_GOAL_WEIGHT */

void CG_StopAllWeaponAnims(int32_t weaponIndex, intptr_t animTree)
{
    cgWeaponInfo_t *info = &cg_weaponInfos[weaponIndex];

    /* The 21 per-node dwords are cgWeaponInfo_t.animRates[1..21] at record
     * +0x8..+0x58 (LEA EDI,[EAX+0x8]). */
    int32_t animIndex;
    for (animIndex = WEAPON_XANIM_IDLE;
         animIndex < WEAPON_XANIM_ADS_UP;
         ++animIndex) {
        int32_t animRateBits = CG_FloatBits(info->animRates[animIndex]);

        if (animIndex == WEAPON_XANIM_IDLE) {
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
