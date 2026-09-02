// Source: uo_cgame_mp_x86.dll 0x30042fc0..0x30043094
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30042fc0_30043094.mcode
//
// CG_ResetWeaponAnimTrees(playerState_t *ps)
// ----------------------------------------------
// Walk every registered weapon (index 1..bg_numWeapons) and, for each weapon
// whose cgame record carries a live DObj, reset that weapon's XAnim tree: resolve
// the runtime tree, clear its goal weights, seed the root weight/rate, and call
// CG_StartWeaponAnim. The activated node is chosen from the player's ammo state:
// WEAPON_XANIM_IDLE when the clip has rounds, WEAPON_XANIM_EMPTY_IDLE otherwise.
//
// NAMING: the .mcode header guess "Script_ExecOnCvarIntValue" is REJECTED. It is a
// size-only match (win size 0xd4 == matched size 0xd4). This body accesses no cvar
// and no script VM: it reads bg_numWeapons (0x30134cd4), the cg_weaponInfos value
// array (0x30413744 == &cg_weaponInfos[1], stride 0x1c4), the bg_weaponInfos pointer
// array (0x30134cd8), and the caller's playerState clips[] — i.e. a cgame-side
// weapon XAnim-tree reset, not a cvar-gated script action. The macOS/PPC function
// with the exact CG_ResetWeaponAnimTrees symbol has
// the same weapon loop, DObjGetTree, ClearTreeGoalWeights, root goal-weight seed,
// clip lookup, and CG_StartWeaponAnim calls.
//
// ARGUMENT (one cdecl stack arg; plain RET, so the caller cleans it):
//   ps = [ESP+0x18] at 0x30042fc9 (loaded before the EBP/ESI/EBX/EDI pushes, so it
//        is the sole incoming stack argument). Used only as ps->clips[clipIndex]
//        at 0x3004305a (MOV EAX,[EBP + ECX*4 + 0x334]); +0x334 is playerState_t.clips
//        (proven by PM_ReloadClip, 0x30012290). EBP therefore holds a
//        playerState_t *.
//
// INSTRUCTION TRACE (every behavior-affecting instruction):
//   30042fc0  MOV EAX,[0x30134cd4]      EAX = bg_numWeapons
//   30042fc9  MOV EBP,[ESP+0x18]        EBP = ps (arg0)
//   30042fce  MOV ESI,1                 ESI = weaponIndex = 1
//   30042fd3  CMP EAX,ESI ; JL 0x3004308e   bg_numWeapons < 1 -> return
//   30042fdd  MOV EDI,0x30413744        EDI = &cg_weaponInfos[1]
//   -- loop head (0x30042fe2) --
//   30042fe2  MOV EAX,[EDI]             EAX = cg_weaponInfos[i].overlayDObj (record +0x0)
//   30042fe4  TEST EAX,EAX ; JZ 0x30043078   no overlay DObj -> skip this weapon
//   30042fec  PUSH EAX ; PUSH 0xb5 ; CALL [0x30085e9c]   EBX = trap(0xb5, overlayDObj)
//   30042ff8  MOV EBX,EAX              EBX = runtime XAnim tree (returned)
//   30042ffa  local0 = 0
//   30043002  PUSH local0 ; PUSH 0 ; PUSH EBX ; PUSH 0x8a ; CALL   trap(0x8a, EBX, 0, 0)
//   30043015  MOV ECX,[EDI+0x4]        ECX = cg_weaponInfos[i].animRates[ROOT]
//   -- build trap(0x8f) args (last push = first arg) --
//   30043018  PUSH 0                   flagD = 0
//   3004301a  PUSH 1                   flagC = 1
//   3004301e  PUSH ECX                 rate = animRates[ROOT]
//   30043027  PUSH 0                   blendTime = 0.0f
//   3004303c  PUSH 0x3f800000          weight = 1.0f
//   3004303d  PUSH 0                   animIndex = ROOT
//   3004303f  PUSH EBX                 runtime tree returned by trap 0xb5
//   30043040  PUSH 0x8f ; CALL         trap(0x8f, EBX, ROOT, 1.0f, 0.0f, rate, 1, 0)
//   3004304b  MOV EDX,[0x30134cd8]     EDX = bg_weaponInfos
//   30043051  MOV EAX,[EDX+ESI*4]      EAX = bg_weaponInfos[i]  (weaponInfo_t*)
//   30043054  MOV ECX,[EAX+0x1f0]      ECX = weap->clipIndex
//   3004305a  MOV EAX,[EBP+ECX*4+0x334] EAX = ps->clips[clipIndex]
//   30043061  ADD ESP,0x38             clean the 0x8a+0x8f stack args
//   30043064  TEST EAX,EAX             clip empty?
//   30043066  MOV EAX,ESI              EAX = weaponIndex (CG_StartWeaponAnim arg)
//   30043068  JNZ 0x3004306e ; PUSH 2 / else PUSH 1   activeAnimIndex = clip? IDLE : EMPTY_IDLE
//   30043070  CALL 0x30042ac0          CG_StartWeaponAnim(weaponIndex=ESI,
//                                       animTree=EBX, activeAnimIndex)
//   30043075  ADD ESP,4                clean the activeAnimIndex arg
//   -- loop tail (0x30043078) --
//   30043078  MOV EAX,[0x30134cd4]     reload bg_numWeapons
//   3004307d  INC ESI                  ++weaponIndex
//   3004307e  ADD EDI,0x1c4            advance to next cg_weaponInfos record
//   30043084  CMP ESI,EAX ; JLE 0x30042fe2   continue while weaponIndex <= count
//   30043090  ADD ESP,0x10 ; RET       plain RET (cdecl; caller cleans arg0)
//
// The loop bound is re-read each iteration (0x30042fc0 and 0x30043078 both load
// 0x30134cd4). All compares are signed (JL / JLE). Weight and blendTime travel as
// raw 32-bit float patterns: 1.0f = 0x3f800000 and 0.0f = 0x00000000. There is no
// x87 here; the values move through integer registers and stack slots.

#include <stdint.h>

#include "client/cgame/globals.h"          /* bg_numWeapons, bg_weaponInfos, cg_weaponInfos,
                               * cgWeaponInfo_t, cgame_syscall */
#include "client/cgame/client_recovered.h" /* weaponInfo_t, playerState_t,
                               * CG_DOBJ_GET_TREE, CG_XANIM_CLEAR_TREE_GOAL_WEIGHTS, CG_XANIM_SET_GOAL_WEIGHT,
                               * CG_StartWeaponAnim */

/* The overlay DObj and node-rate array are cgWeaponInfo_t::viewDObjSelf (+0x00)
 * and cgWeaponInfo_t::animRates (+0x04); see globals.h. */

void CG_ResetWeaponAnimTrees(playerState_t *ps)
{
    int32_t weaponIndex;

    /* bg_numWeapons is re-read each iteration (matches the two 0x30134cd4 loads);
     * the loop runs weaponIndex = 1..bg_numWeapons with a signed compare. */
    for (weaponIndex = 1; weaponIndex <= bg_numWeapons; ++weaponIndex) {
        cgWeaponInfo_t *record = &cg_weaponInfos[weaponIndex];

        struct DObj_s *overlayDObj = record->viewDObjSelf; /* MOV EAX,[EDI] on i386 */
        if (overlayDObj == 0)
            continue; /* no overlay DObj for this weapon */

        /* Resolve the weapon overlay DObj's runtime tree for the follow-on traps. */
        intptr_t animTree = cgame_syscall(CG_DOBJ_GET_TREE, (intptr_t)overlayDObj);
        /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
        if (animTree == 0) {
            continue;
        }

        /* Clear all goal weights below the root with zero blend time. */
        cgame_syscall(CG_XANIM_CLEAR_TREE_GOAL_WEIGHTS, animTree, WEAPON_XANIM_ROOT, 0);

        /* Seed the root with weight 1.0f, blend time 0.0f, its animation rate,
         * notifyName 1, and restart 0. */
        int32_t rootAnimRateBits = CG_FloatBits(record->animRates[WEAPON_XANIM_ROOT]);
        cgame_syscall(CG_XANIM_SET_GOAL_WEIGHT, animTree, WEAPON_XANIM_ROOT, CG_FloatBits(1.0f), CG_FloatBits(0.0f), rootAnimRateBits, 1,
                      0);

        /* Choose IDLE or EMPTY_IDLE from the player's clip state. */
        weaponInfo_t *weap = bg_weaponInfos[weaponIndex];
        int32_t clipIndex = weap->clipIndex;
        int32_t activeAnimIndex = (ps->clips[clipIndex] != 0) ? WEAPON_XANIM_IDLE : WEAPON_XANIM_EMPTY_IDLE;

        CG_StartWeaponAnim(weaponIndex, animTree, activeAnimIndex);
    }
}
