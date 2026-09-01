#include "../client_recovered.h"
#include "../globals.h"

#include <stddef.h>
#include <stdint.h>

// Source: uo_cgame_mp_x86.dll 0x30022040..0x3002207d
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30022040_3002207d.mcode
//
// CG_DObjCalcPose — calculate a renderer-requested DObj pose for a caller-owned
// four-word part bitset. Exact name from the Mac cgame symbol and from the
// CoDUOMP.exe renderer callback exported under that name.
//
// The assigned .mcode "# name trap_Cvar_VariableValue" is a pure size match
// (win 0x3d, matched 0x3d) and is REJECTED: there is no cvar work here at all -- no
// cvar-name/handle argument, no float/int VM return forwarded to a caller. This is
// the DObj CreateSkelForBones/CalcAnim/CalcSkel sequence plus a call into
// CG_DoControllers.
//
// ABI (proven from the machine code and the sole caller 0x3002b043):
//   EDI = DObj pointer. It is the FIRST data argument of every trap here
//         (PUSH ESI ; PUSH EDI ; PUSH id -> cgame_syscall(id, EDI, ESI)). The caller
//         loads it as EDI = [ebp+0x10]. CL_DObjCalcSkel in the executable proves
//         that vmMain arg1 is the renderer's native DObj pointer, not a 32-bit
//         integer handle; preserving it as a pointer is required on 64-bit hosts.
//   ESI = the caller's uint32_t partBits[4]. It is the SECOND data
//         argument of every trap and is also passed to the controller dispatcher
//         (0x30022065 MOV ECX,ESI). The caller loads it as ESI = [ebp+0x14].
//   [ESP+0xc] (post-call) = the owning entity pointer, the sole stack argument;
//         forwarded to the dispatcher as its `part` (EAX). The caller PUSHes exactly
//         one dword (0x3002b042 PUSH EAX, EAX = [ebp+0xc]) and cleans it itself
//         (0x3002b04c ADD ESP,4), so this function uses a plain RET (no callee
//         cleanup of the stack arg).
// The register-argument convention is not expressible as a plain cdecl prototype
// without inline asm, so the register inputs are modeled as explicit parameters and
// the caller-observed ABI is documented above (no calling-convention attribute --
// the syntax-only build needs none; see WORKFLOW RET-imm guidance). cgame_syscall is
// the engine VM trap entry (var-arg fn pointer at 0x30085e9c).
//
// Per-instruction proof of every behavior-affecting statement:
//   30022040 PUSH ESI / PUSH EDI / PUSH 0xac      CreateSkelForBones(obj, partBits)
//   30022047 CALL [0x30085e9c]                     EAX = begin result
//   3002204d ADD ESP,0xc                           clean the 3 begin args
//   30022050 TEST EAX,EAX ; 30022052 JNZ 0x3002207c   nonzero -> skip everything, RET
//   30022054 PUSH ESI / PUSH EDI / PUSH 0x9a      CalcAnim(obj, partBits)
//   3002205b CALL [0x30085e9c]                     (args left live, cleaned at end)
//   30022061 MOV EAX,[ESP+0x10]                    EAX = part (the sole stack arg;
//                                                   ESP is 0xc below entry here)
//   30022065 MOV ECX,ESI                           ECX = ESI = partBits
//   30022067 CALL 0x30021fe0                       CG_DoControllers(part, partBits)
//   3002206c PUSH ESI / PUSH EDI / PUSH 0xae      CalcSkel(obj, partBits)
//   30022073 CALL [0x30085e9c]
//   30022079 ADD ESP,0x18                          clean prep(0xc) + flush(0xc) args
//   3002207c RET
void CG_DObjCalcPose(centity_t *owner, struct DObj_s *obj,
                     uint32_t *partBits)
{
    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if (obj == NULL || partBits == NULL) {
        return;
    }

    // 30022042: an already-current skeleton returns nonzero.
    if (cgame_syscall(CG_DOBJ_CREATE_SKEL_FOR_BONES, (intptr_t)obj,
                      (intptr_t)partBits) != 0) {
        return;
    }

    // 30022056: calculate animation for the caller-selected bones.
    cgame_syscall(CG_DOBJ_CALC_ANIM, (intptr_t)obj, (intptr_t)partBits);

    // 30022067: entity controllers receive the same part-selection bitset.
    CG_DoControllers(owner, partBits);

    // 3002206e: produce final skeleton matrices.
    cgame_syscall(CG_DOBJ_CALC_SKEL, (intptr_t)obj, (intptr_t)partBits);
}
