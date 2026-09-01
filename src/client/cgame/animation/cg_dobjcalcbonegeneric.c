#include "../client_recovered.h"
#include "../globals.h"

#include <stddef.h>
#include <stdint.h>

// Source: uo_cgame_mp_x86.dll 0x300220e0..0x30022163
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_300220e0_30022163.mcode
//
// CG_DObjCalcBoneGeneric — entity-index form of the one-bone DObj calculation.
// It acquires the DObj `self` handle itself via trap 0xa5 (with the
// i386 stack implicitly carrying the saved EDI/index as its argument)
// instead of receiving it, and the `part` it dispatches is an element of the
// per-index centity array at 0x3048c6e0 (centity_t, stride 0x288 = 648)
// whose leading fields are the entity state consumed by CG_DoControllers.
//
// ABI (proven from machine code): register-argument helper. EDI and EBX are
// consumed as incoming register values (neither is initialized in the body):
//   EDI = effect/client index, range-checked < 1024 (== MAX_GENTITIES)
//         before it selects the slot passed to the dispatcher (0x3002212c
//         CMP EDI,0x400; JGE skips-dispatch).
//   EBX = bone index forwarded to CreateSkelForBone/GetHierarchyBits.
// They are modeled here as explicit parameters `index` and `boneIndex`; the
// caller-observed register ABI is documented above (not expressible as plain cdecl
// without inline asm, so no attribute is added — see WORKFLOW RET-imm guidance).
//
// The assigned .mcode "# name G_FreeEntities" is a pure size match (win 0x83) and
// is REJECTED: there is no entity-array free loop here — this is DObj-trace cgame
// syscalls plus a call into CG_DoControllers, exactly the operation shape of
// CG_DObjCalcBone (0x30022080). The exact name comes from the Mac cgame symbol.
void CG_DObjCalcBoneGeneric(int32_t index, int32_t boneIndex)
{
    uint32_t partBits[4]; /* SUB ESP,0x10; exact DObj part-bitset width. */

    // 0x300220e4 PUSH EDI saves index immediately before 0x300220e5 pushes
    // syscall 0xa5. Thus the original variadic i386 syscall observes saved EDI
    // as argument 1. Make that implicit stack argument explicit for the native
    // ABI. 0x300220f0 MOV ESI,EAX: `self` for the remaining traps.
    struct DObj_s *self = (struct DObj_s *)(intptr_t)cgame_syscall(
        CG_DOBJ_GET_HANDLE, index);

    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    int32_t boneCount = coduo_int32_from_bits((uint32_t)cgame_syscall(
        CG_DOBJ_NUM_BONES, (intptr_t)self));
    if ((uint32_t)boneIndex >= (uint32_t)boneCount) {
        return;
    }

    // 0x300220f4: an already-current skeleton returns nonzero.
    if (cgame_syscall(CG_DOBJ_CREATE_SKEL_FOR_BONE,
                      (intptr_t)self, boneIndex) != 0) {
        return;
    }

    // 0x3002210d: expand the requested bone to its hierarchy bits.
    cgame_syscall(CG_DOBJ_GET_HIERARCHY_BITS, (intptr_t)self, boneIndex,
                  (intptr_t)partBits);

    // 0x3002211e: calculate animation for the selected hierarchy.
    cgame_syscall(CG_DOBJ_CALC_ANIM, (intptr_t)self, (intptr_t)partBits);

    // 0x3002212c CMP EDI,0x400; JGE 0x3002214a: only dispatch when the effect index
    // is in range. Signed compare (JGE), bound 1024 = MAX_GENTITIES.
    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if ((uint32_t)index < (uint32_t)MAX_GENTITIES) {
        // 0x30022134..0x3002213c: EAX = index*0x288 + 0x3048c6e0, i.e. the address
        // of effect slot [index]. The
        // slot base is the same array CG_StartFlameDamageEffect indexes (still a
        // mechanical symbol), so take its address and index by the effect index.
        uint32_t offsetBits =
            (uint32_t)index * (uint32_t)sizeof(cg_entities[0]);
        intptr_t displacement =
            (intptr_t)coduo_int32_from_bits(offsetBits);
        centity_t *part = (centity_t *)(
            (uintptr_t)(void *)cg_entities +
            (uintptr_t)displacement);

        // 0x30022145: dispatch with the selected hierarchy bitset.
        CG_DoControllers(part, partBits);
    }

    // 0x30022150: calculate the skeleton. Reached on both the dispatch and
    // the index-out-of-range paths (the JGE only skips the dispatch call).
    cgame_syscall(CG_DOBJ_CALC_SKEL, (intptr_t)self, (intptr_t)partBits);
}
