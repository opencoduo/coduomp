// Source: uo_cgame_mp_x86.dll 0x3003e890..0x3003e8dc
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3003e890_3003e8dc.mcode

#include "client/cgame/client_recovered.h"
#include "client/cgame/globals.h"

/*
 * trap_XAnimSetGoalWeight — thin cdecl cgame trap wrapper for syscall id 0x8f (143).
 *
 * It forwards its seven stack arguments to cgame_syscall (the VM trap pointer at
 * .data 0x30085e9c, distinct from the KERNEL32/USER32/GDI32 IAT) and returns the
 * syscall's int32 result in EAX. This is the exact structural sibling of trap_XAnimSetCompleteGoalWeight
 * (0x3003e8e0, trap 0x90): same interleaved [ESP+0x14]-scratch push sequence, same
 * two 16-bit narrowings.
 *
 * Calling convention (all args cdecl on the stack; the wrapper issues a plain RET,
 * so the SEVEN incoming args are caller-cleaned; the ADD ESP,0x20 at 0x3003e8d8
 * only balances the 8 dwords this function pushed for the CALL). Incoming slots:
 *   [ESP+0x04]=arg0 [ESP+0x08]=arg1 [ESP+0x0c]=arg2 [ESP+0x10]=arg3
 *   [ESP+0x14]=arg4 [ESP+0x18]=arg5 [ESP+0x1c]=arg6
 * Confirmed at the call site 0x30033277: caller pushes 7 dwords then ADD ESP,0x1c
 * (7 dwords) after the call — caller-cleaned cdecl. There arg4 = 0x3f800000 (1.0f
 * float bits) and arg5 = arg6 = 0, matching the CG_XANIM_SET_GOAL_WEIGHT per-slot weapon-HUD /
 * overlay update semantics also emitted directly by CG_StartWeaponAnim
 * (0x30042ac0).
 *
 * The body interleaves reloads through the [ESP+0x14] scratch slot with the pushes,
 * but the dataflow reduces to pushing (in this order) arg6, (uint16_t)arg5, arg4,
 * arg3, arg2, (uint16_t)arg1, arg0, then the command 0x8f. Because the stack grows
 * downward, cgame_syscall receives them in ascending order as
 *   (0x8f, arg0, (uint16_t)arg1, arg2, arg3, arg4, (uint16_t)arg5, arg6).
 *
 * Two args are narrowed to their low 16 bits, zero-extended, before the push:
 *   0x3003e8a9  MOVZX ECX,word ptr [ESP+0x1c]  ; -> (uint16_t)arg5  (a5 slot after 1 push)
 *   0x3003e8c0  MOVZX EDX,word ptr [ESP+0x14]  ; -> (uint16_t)arg1  (a1 slot after 3 pushes)
 * The remaining five args (arg0, arg2, arg3, arg4, arg6) pass full 32 bits.
 *
 * Naming note: the mechanical .mcode header guessed `Q_strncpyz` purely by a size
 * match (win 0x4c == corpus 0x4c). That is rejected — Q_strncpyz is a bounded
 * string copy (dest/src pointers + length, strncpy + NUL terminate), whereas this
 * body touches no string, copies no bytes, and is a pure 7-arg forwarder to the
 * cgame VM syscall pointer. Size matching is disallowed and the body contradicts
 * the guess, so the function is named trap_XAnimSetGoalWeight. The recovered
 * engine dispatcher independently maps command 0x8f to XAnimSetGoalWeight.
 */
int32_t trap_XAnimSetGoalWeight(XAnimTree *tree, uint32_t anim, float weight,
                                float blendTime, float rate,
                                uint16_t notifyName, qboolean restart)
{
    return (int32_t)cgame_syscall(CG_XANIM_SET_GOAL_WEIGHT,
                         (intptr_t)tree,
                         (uint16_t)anim,
                         CG_FloatBits(weight),
                         CG_FloatBits(blendTime),
                         CG_FloatBits(rate),
                         notifyName,
                         restart);
}
