// Source: uo_cgame_mp_x86.dll 0x3003e780..0x3003e7a5
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3003e780_3003e7a5.mcode

#include "client/cgame/client_recovered.h"
#include "client/cgame/globals.h"

/*
 * trap_XAnimClearTreeGoalWeightsStrict — cgame trap wrapper for syscall id 0x8b (139).
 *
 * It forwards three stack arguments to cgame_syscall (the VM trap pointer at
 * .data 0x30085e9c, distinct from the KERNEL32/USER32/GDI32 IAT at
 * 0x3006f000..0x3006f168) and leaves the syscall's int32 result in EAX.
 *
 * Calling convention (all args cdecl on the stack; the wrapper issues a plain RET
 * at 0x3003e7a4, so the THREE incoming args are caller-cleaned; the ADD ESP,0x10
 * at 0x3003e7a1 only balances the 4 dwords this function pushed for the CALL):
 *   [ESP+0x04]=arg0  [ESP+0x08]=arg1  [ESP+0x0c]=arg2
 *
 * Dataflow (offsets shown before the first push):
 *   0x3003e780  MOV   EAX,[ESP+0xc]           ; EAX = arg2
 *   0x3003e784  MOVZX EDX,word ptr [ESP+0x8]  ; EDX = (uint16_t)arg1
 *   0x3003e789  MOV   ECX,EAX                 ; ECX = arg2
 *   0x3003e78b  PUSH  ECX                     ; push arg2
 *   0x3003e78c  MOV   [ESP+0x10],EAX          ; dead store back into caller's arg2
 *                                             ;   slot (compiler register-shuffle
 *                                             ;   artifact; no source effect)
 *   0x3003e790  MOV   EAX,[ESP+0x8]           ; EAX = arg0 (slot after 1 push)
 *   0x3003e794  PUSH  EDX                     ; push (uint16_t)arg1
 *   0x3003e795  PUSH  EAX                     ; push arg0
 *   0x3003e796  PUSH  0x8b                     ; push command id 139
 *   0x3003e79b  CALL  [cgame_syscall]
 *   0x3003e7a1  ADD   ESP,0x10                ; balance the 4 pushes
 * Because the stack grows downward, cgame_syscall receives the args in ascending
 * order as (0x8b, arg0, (uint16_t)arg1, arg2). arg1 is narrowed to its low 16 bits
 * (zero-extended, MOVZX word); arg0 and arg2 pass full 32 bits.
 *
 * The original engine dispatcher maps command 139 to
 * XAnimClearTreeGoalWeightsStrict, and the same-module PPC binary supplies the
 * exact wrapper name trap_XAnimClearTreeGoalWeightsStrict. This wrapper sits in
 * the same XAnim/DObj trap-wrapper cluster as the adjacent goal-weight wrappers.
 *
 * Naming note: the mechanical .mcode header guessed `AngleDelta` purely by a size
 * match (win 0x25 == corpus 0x25). That is rejected — size matching is disallowed
 * and the body is a pure trap forwarder to the cgame VM syscall pointer, not an
 * angle-difference computation.
 */
void trap_XAnimClearTreeGoalWeightsStrict(XAnimTree *tree, uint32_t animIndex,
                                          float blendTime)
{
    cgame_syscall(CG_XANIM_CLEAR_TREE_GOAL_WEIGHTS_STRICT,
                  (intptr_t)tree, (uint16_t)animIndex,
                  CG_FloatBits(blendTime));
}
