// Source: uo_cgame_mp_x86.dll 0x30034880..0x300349f3
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30034880_300349f3.mcode
//
// CG_ResetPlayerEntity — (re)initialise a player DObj model's per-bone anim tags
// and its legs/torso/lean swing-angle state to the client's current view angles.
//
// The single cdecl stack argument (EBP = [ESP+0x14] after SUB ESP,8 / PUSH EBX /
// PUSH EBP) is the player model-info record, entityState_t. The per-client
// clientInfo_t is reached through renderEntity->clientNum (+0x94):
//   ESI = &bgs.clientinfo[animStateIndex]   (base 0x305e1f34, stride 0x4d0,
//                                                 IMUL ESI,ESI,0x4d0; ADD 0x305e1f34).
//
// Behaviour (0x30034889 MOV AL,[EBP+0x8]; TEST AL,0x1; JNZ 0x300349c7):
//   * When renderEntity->eFlags has EF_DEAD (bit 0x1) set, the whole
//     tag/reset block is skipped and control falls straight to the debug trailer.
//   * Otherwise it resets four XAnim nodes on the client's DObj instance
//     tree (clientInfo_t.animTree, +0x4c4, MOV EDI,[ESI+0x4c4]):
//       - trap 0x8a (CG_XANIM_CLEAR_TREE_GOAL_WEIGHTS) below the root node (bgs.rootAnimHandle low16);
//       - trap 0x90 (CG_XANIM_SET_COMPLETE_GOAL_WEIGHT) for the torso bone  (bgs.resolvedTorsoAnimHandle);
//       - trap 0x90 (CG_XANIM_SET_COMPLETE_GOAL_WEIGHT) for the legs bone   (bgs.resolvedLegsAnimHandle);
//       - trap 0x90 (CG_XANIM_SET_COMPLETE_GOAL_WEIGHT) for the turning bone(bgs.resolvedTurningAnimHandle).
//     then resets the legs/torso/lean swing-angle blocks: each 0x30-byte block is
//     zeroed (REP STOSD ECX=0xc) and the yaw/lean angles are seeded from the freshly
//     written view angles (legsYawAngle = viewYaw; torsoYawAngle = viewYaw;
//     leanAngle = viewPitch).
//   * Debug trailer (0x300349c7): if cg_debugposition_vmCvar.integer (0x3045266c) is nonzero,
//     print "%i ResetPlayerEntity yaw=%i\n" with renderEntity->number and the
//     just-set torsoYawAngle.
//
// The .mcode size-guess "CG_InterpolateEntityPosition" is REJECTED — no snapshot
// interpolation, no BG_EvaluateTrajectory, no cg_frameInterpolation. Named from the
// proven debug string "ResetPlayerEntity" plus the reset behaviour.
//
// Frame/ABI notes: XOR EBX,EBX at entry makes EBX the constant 0 used for every
// zero-argument push and every zero store; not modeled as a named local. The
// interleaved [ESP+N] scratch slots (0x3f800000 written then re-loaded before a push)
// are the compiler materialising the 1.0f trap arguments; modeled as 1.0f literals.

#include <string.h>
#include "client/cgame/client_recovered.h"
#include "client/cgame/globals.h"

/* trap-arg 1.0f as its i386 bit pattern (the code writes 0x3f800000 to a stack slot
 * and pushes it; cgame_syscall takes int32 varargs, so the float is passed as bits,
 * matching the sibling BG_SetNewAnimation 0x30003a90 trap-144 call sites). */
#define CG_TRAP_ARG_ONE_F ((int32_t)0x3F800000)   /* 1.0f */

void CG_ResetPlayerEntity(entityState_t *renderEntity)
{
    /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered client-module boundary input and state before use. */
    const int32_t clientNum =
        coduo_int32_from_bits(renderEntity->clientNumBits);
    if ((uint32_t)clientNum >= (uint32_t)MAX_CLIENTS) {
        Com_Error(ERR_DROP,
                  "\x15" "CG_ResetPlayerEntity: invalid client number %i",
                  clientNum);
        return;
    }
    clientInfo_t *state = &bgs.clientinfo[clientNum];

    /* 0x300348a1 TEST AL,0x1 — skip the tag/reset block for a non-drawn model. */
    if ((renderEntity->eFlags & EF_DEAD) == 0) {
        XAnimTree *animTree = state->animTree;   /* [ESI+0x4c4] on i386 */

        /* trap(0x8a, handle, rootAnim16, 0) — clear the root subtree's goal weights. */
        cgame_syscall(CG_XANIM_CLEAR_TREE_GOAL_WEIGHTS, (intptr_t)animTree,
                      (int32_t)bgs.rootAnimHandle.animIndex, 0);

        /* trap(0x90, handle, torsoBone16, 0, 0, 1.0f, 0, 0). */
        cgame_syscall(CG_XANIM_SET_COMPLETE_GOAL_WEIGHT, (intptr_t)animTree,
                      (int32_t)bgs.resolvedTorsoAnimHandle.animIndex,
                      0, 0, CG_TRAP_ARG_ONE_F, 0, 0);

        /* trap(0x90, handle, legsBone16, 1.0f, 0, 1.0f, 0, 0). */
        cgame_syscall(CG_XANIM_SET_COMPLETE_GOAL_WEIGHT, (intptr_t)animTree,
                      (int32_t)bgs.resolvedLegsAnimHandle.animIndex,
                      CG_TRAP_ARG_ONE_F, 0, CG_TRAP_ARG_ONE_F, 0, 0);

        /* trap(0x90, handle, turningBone16, 0, 0, 1.0f, 0, 0). */
        cgame_syscall(CG_XANIM_SET_COMPLETE_GOAL_WEIGHT, (intptr_t)animTree,
                      (int32_t)bgs.resolvedTurningAnimHandle.animIndex,
                      0, 0, CG_TRAP_ARG_ONE_F, 0, 0);

        /* 0x30034967 reset the legs swing block (0x380..0x3b0, one 0x30-byte block via
         * REP STOSD ECX=0xc), then seed legsYawAngle from the current view yaw. */
        memset(&state->legsYawAngle, 0, 0x30);
        state->legsYawAngle = state->viewYaw;   /* [ESI+0x380] = [ESI+0x3ec] */

        /* 0x30034980 reset the torso/lean swing block (0x3b0..0x3e0), then seed the
         * yaw/lean angles: torsoYawAngle from view yaw, leanAngle from view pitch. */
        memset(&state->torsoYawAngle, 0, 0x30);
        state->torsoYawAngle = state->viewYaw;  /* [ESI+0x3b0] = [ESI+0x3ec] */
        state->leanAngle     = state->viewPitch;/* [ESI+0x3b8] = [ESI+0x3e8] */
        /* torsoYawActive/leanActive land in the just-memset block (explicit 0 stores at
         * 0x300349b4/0x300349c0 re-write what REP STOSD already cleared). */
    }

    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if (cg_debugposition_vmCvar.integer != 0) {
        Com_PrintMessage("%i ResetPlayerEntity yaw=%f\n",
                         (int32_t)renderEntity->numberBits,
                         (double)state->torsoYawAngle);
    }
}
