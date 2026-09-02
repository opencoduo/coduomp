// Source: uo_cgame_mp_x86.dll 0x30042b50..0x30042c3f
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30042b50_30042c3f.mcode
//
// CG_PlayADSAnim (0x30042b50)
// ---------------------------------------------------------------------------
// Toggle the complementary ADS_UP/ADS_DOWN XAnim nodes and set their times from
// the ADS (aim-down-sight) zoom fraction. Called once per frame while a weapon's
// ADS animation is transitioning between its two states.
//
// This is NOT `vectopitch`. The mechanical `# name vectopitch` is a size-only
// guess (win size 0xef matched a game_mp_uo symbol of the same size) and is
// REJECTED: the body contains no vector-angle math (no FSQRT, no atan2 helper,
// no BAMS/180 constants). It is a fixed pair of the same trap-0x8f XAnim-node
// update that CG_StartWeaponAnim (0x30042ac0, the immediately
// preceding function) emits in a 21-slot loop, followed by a pair of
// trap_XAnimSetTime calls. The single FLD/FSUB is `1.0f - adsFraction`.
//
// ABI: register args, no stack args; plain RET (SUB ESP,0xc at entry and the
// two trailing ADD ESP cleanups net to zero — the callee balances its own
// scratch). Register args proven from the one caller at 0x30042dcf:
//   EAX  = activeAnimIndex: ADS_UP (22) selects ADS_UP, else ADS_DOWN (23).
//          The caller computes EAX = (cond ? 22 : 23) at 0x30042dc3/0x30042dca.
//   ESI  = animTree, forwarded verbatim into every trap call.
//
// Behavior (all four calls go through cgame_syscall @ 0x30085e9c):
//
//   Trap 0x8f (CG_XANIM_SET_GOAL_WEIGHT), 7 forwarded args
//   after the trap id, matching trap_XAnimSetGoalWeight's shape
//   (animTree, animIndex, weight, blendTime, rate, notifyName, restart):
//     if (activeAnimIndex == WEAPON_XANIM_ADS_UP):
//       (0x8f, esi, 22, 1.0f, 0.5f, 0, 1, 0)   // ADS_UP active
//       (0x8f, esi, 23, 0.0f, 0.5f, 0, 0, 0)   // ADS_DOWN reset
//     else:
//       (0x8f, esi, 22, 0.0f, 0.5f, 0, 0, 0)   // ADS_UP reset
//       (0x8f, esi, 23, 1.0f, 0.5f, 0, 1, 0)   // ADS_DOWN active
//   Weight is 1.0f on the active branch and 0.0f on the reset branch;
//   blendTime is always 0.5f and rate is always 0.0f.
//
//   Trap 0x94 (CG_XANIM_SET_TIME), 3 forwarded args (animTree, animIndex,
//   timeBits), independent of activeAnimIndex:
//     (0x94, esi, 22, adsFraction)
//     (0x94, esi, 23, 1.0f - adsFraction)
//   adsFraction is cg.predictedPlayerState.adsFraction, read as a float from
//   0x304832a4 (cg_predictedPlayerState.adsFraction). The complement 1.0f-adsFraction
//   is FLD [0x3007bce0]=1.0f / FSUB [0x304832a4]. The two node times therefore
//   sum to 1.0 across the ADS zoom transition.
//
// NAME: the macOS/PPC CG_PlayADSAnim body has the same two goal-weight calls in
// each selector arm, followed by XAnimSetTime(ADS_UP, adsFraction) and
// XAnimSetTime(ADS_DOWN, 1.0f-adsFraction).

#include <stdint.h>

#include "client/cgame/globals.h"          /* cgame_syscall, cg_predictedPlayerState.adsFraction */
#include "client/cgame/client_recovered.h" /* CG_XANIM_SET_GOAL_WEIGHT, CG_XANIM_SET_TIME */

/* Blend time pushed on every trap-0x8f call (ECX = 0x3f000000). */
#define CG_ADS_BLEND_TIME 0.5f

void CG_PlayADSAnim(int32_t activeAnimIndex, intptr_t animTree)
{
    /* --- Trap 0x8f: activate one ADS node, reset the other. --- */
    if (activeAnimIndex == WEAPON_XANIM_ADS_UP) {
        cgame_syscall(CG_XANIM_SET_GOAL_WEIGHT, animTree, WEAPON_XANIM_ADS_UP, CG_FloatBits(1.0f), CG_FloatBits(CG_ADS_BLEND_TIME), 0, 1,
                      0);
        cgame_syscall(CG_XANIM_SET_GOAL_WEIGHT, animTree, WEAPON_XANIM_ADS_DOWN, CG_FloatBits(0.0f), CG_FloatBits(CG_ADS_BLEND_TIME), 0, 0,
                      0);
    } else {
        cgame_syscall(CG_XANIM_SET_GOAL_WEIGHT, animTree, WEAPON_XANIM_ADS_UP, CG_FloatBits(0.0f), CG_FloatBits(CG_ADS_BLEND_TIME), 0, 0,
                      0);
        cgame_syscall(CG_XANIM_SET_GOAL_WEIGHT, animTree, WEAPON_XANIM_ADS_DOWN, CG_FloatBits(1.0f), CG_FloatBits(CG_ADS_BLEND_TIME), 0, 1,
                      0);
    }

    /* --- Trap 0x94: set complementary XAnim times from the ADS zoom fraction.
     * The reads happen in this order: adsFraction is loaded (MOV EAX,[..]) and
     * the first call issued, then FLD 1.0f / FSUB [..] forms the complement for
     * the second. Both reads are of the same live global; order preserved. --- */
    float adsFraction = cg_predictedPlayerState.adsFraction;
    cgame_syscall(CG_XANIM_SET_TIME, animTree, WEAPON_XANIM_ADS_UP, CG_FloatBits(adsFraction));

    /* 1.0f from FLD float ptr [0x3007bce0] (bit pattern 0x3f800000). */
    float complement = (float)(1.0L - (long double)cg_predictedPlayerState.adsFraction);
    cgame_syscall(CG_XANIM_SET_TIME, animTree, WEAPON_XANIM_ADS_DOWN, CG_FloatBits(complement));
}
