#include "../client_recovered.h"
#include "../globals.h"

#include <stddef.h>
#include <stdint.h>

// Source: uo_cgame_mp_x86.dll 0x30022080..0x300220de
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30022080_300220de.mcode
//
// CG_DObjCalcBone — calculate one requested DObj bone and its hierarchy. The
// exact Mac cgame symbol and the server G_DObjCalcBone operation sequence agree:
// CreateSkelForBone, GetHierarchyBits, CalcAnim, controllers, CalcSkel.
//
// ABI (proven from callers and machine code): this is a register-argument helper.
//   ESI = DObj pointer (incoming ECX in caller 0x3001fda0:
//         MOV ESI,ECX, then reused across all four syscalls here).
//   EDI = bone index (incoming; in caller 0x3001fda0 it is the
//         non-negative int returned by trap(0xb2, self): MOV EDI,EAX before the
//         call). Passed to CreateSkelForBone and GetHierarchyBits.
//   [ESP+4] on entry = a stack argument: the owning entity pointer, read at
//         0x300220b9 (MOV EAX,[ESP+0x30] resolves to the incoming slot) and passed
//         to the dispatcher as its `part` (EAX) argument.
// The 16-byte stack local is uint32_t partBits[4], the DObj hierarchy bitset
// written by GetHierarchyBits and consumed by CalcAnim/controllers/CalcSkel.
//
// The assigned .mcode "# name G_SetSoundBlend" is a pure size match (win 0x5e)
// and is REJECTED: there is no sound/blend work — this is DObj bone calculation.
//
// Because the caller convention is register-based (ESI/EDI incoming, one stack
// arg) and not expressible as a plain cdecl prototype without inline asm, the
// register inputs are modeled as explicit parameters and the caller-observed ABI
// is documented above. cgame_syscall is the engine VM trap entry (var-arg fn
// pointer at 0x30085e9c); the trailing local-buffer address is a real pointer the
// fetch traps write through.
void CG_DObjCalcBone(struct DObj_s *self, int32_t boneIndex, centity_t *part)
{
    uint32_t partBits[4]; /* SUB ESP,0x10; exact DObj part-bitset width. */

    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    int32_t boneCount = coduo_int32_from_bits((uint32_t)cgame_syscall(CG_DOBJ_NUM_BONES, (intptr_t)self));
    if ((uint32_t)boneIndex >= (uint32_t)boneCount) {
        return;
    }

    // 0x30022085: an already-current skeleton returns nonzero and needs no work.
    if (cgame_syscall(CG_DOBJ_CREATE_SKEL_FOR_BONE, (intptr_t)self, boneIndex) != 0) {
        return;
    }

    // 0x3002209d: expand the requested bone to its required hierarchy bits.
    cgame_syscall(CG_DOBJ_GET_HIERARCHY_BITS, (intptr_t)self, boneIndex, (intptr_t)partBits);

    // 0x300220ae: calculate animation for the selected hierarchy.
    cgame_syscall(CG_DOBJ_CALC_ANIM, (intptr_t)self, (intptr_t)partBits);

    // 0x300220c1: controllers may mark/write local tags within this same bitset.
    CG_DoControllers(part, partBits);

    // 0x300220cc: produce final skeleton matrices for the selected bits.
    cgame_syscall(CG_DOBJ_CALC_SKEL, (intptr_t)self, (intptr_t)partBits);
}
