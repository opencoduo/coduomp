// Source: uo_cgame_mp_x86.dll 0x3001fec0..0x3001ffcd
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3001fec0_3001ffcd.mcode

#include "../client_recovered.h"
#include "../globals.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

/*
 * CG_DObjGetSpecialTagWorldMatrix (provisional-by-role) — resolve a named DObj
 * bone/tag on `self`, advance that bone's DObj trace-part state, then compose the
 * fixed placement `cg_specialTagPlacement` into the bone's engine matrix to produce
 * a world-space tag matrix in `out`. Returns qtrue on success, qfalse when the
 * entity has no such tag.
 *
 * The assigned .mcode "# name PM_BeginReloadLoop" is a pure corpus/size guess
 * (win 0x10d == matched 0x10d) and is REJECTED: this function does no pmove or
 * weapon-reload work. It is the third sibling of the DObj bone-world-matrix family
 * alongside CG_DObjGetWorldTagMatrix (0x3001fdf0) and the lighter 0x3001fda0.
 * All three resolve a bone index via trap 0xb2, index the per-bone matrix table via
 * trap 0xa0 (base + handle<<6), and (for the composing variants) compose a local
 * placement matrix into that bone matrix via CG_ComposeBoneMatrix. This variant
 * differs in two proven ways:
 *   - its local placement is the FIXED global cg_specialTagPlacement (an
 *     orientation_t at 0x3048b0e4), not a per-entity angles-derived matrix;
 *   - between resolving the bone and reading the matrix table it inlines the four
 *     DObj bone-calculation traps (CreateSkelForBone 0xab, GetHierarchyBits 0xad,
 *     CalcAnim 0x9a, CalcSkel 0xae) rather than calling CG_DObjCalcBone, and it
 *     issues no controller call. It also omits the
 *     NULL bone-matrix guard the 0x3001fdf0 sibling has.
 *
 * ABI (register-argument, proven from the machine code and the sibling producers):
 *   - `self` (the DObj context object) arrives in EDI; it is the first argument to
 *     every cgame trap here (0xb2/0xab/0xad/0x9a/0xae/0xa0).
 *   - `tagName` arrives in EAX; it is the second argument to the bone-index trap
 *     0xb2 (trap(0xb2, self, tagName)).
 *   - `out` (a full 4x4 float world matrix) is the sole incoming stack argument
 *     ([base+0x4]); it is the composer's EDX destination at the end.
 * Returns qboolean in EAX (1 success / 0 no-tag). The prologue PUSH ESI/EAX/EDI is
 * a mixed save+arg frame: ESI is a saved register (POP ESI in the epilogue), while
 * the pushed EAX and EDI double as the first two stack args of the initial 0xb2
 * trap. Callee cleans its own stack (SUB/ADD ESP,0x40, plus the RET). Because the
 * convention is register-based it is modeled with ordered parameters and no
 * calling-convention attribute (syntax-only build does not require one).
 *
 * Per-instruction proof of every behavior-affecting statement:
 *   3001fec6 PUSH 0xb2 (over pushed EDI,EAX) / CALL [cgame_syscall]
 *                                         ESI = trap(0xb2, self=EDI, tagName=EAX)
 *   3001fed6 TEST ESI,ESI ; JGE           ESI < 0 (JGE not taken) -> return 0
 *   3001fee3 PUSH ESI/EDI/0xab / CALL      trap(0xab, self, boneHandle) begin
 *   3001fef1 TEST EAX,EAX ; JNZ 0x3001ff2c nonzero begin -> skip fetch/prep/flush
 *   3001fefc PUSH &part/ESI/EDI/0xad       trap(0xad, self, boneHandle, &part) fetch
 *   3001ff0d PUSH &part/EDI/0x9a           trap(0x9a, self, &part) prep
 *   3001ff1e PUSH &part/EDI/0xae           trap(0xae, self, &part) flush
 *   3001ff2f PUSH 0/EDI/0xa0 / CALL        ECX = trap(0xa0, self, 0) matrix table
 *   3001ff3a..ffb1 build local matrix from cg_specialTagPlacement (12 dwords):
 *                  local.axis[0..2] <- cg_specialTagPlacement.axis (0x3048b0f0..)
 *                  local.origin    <- cg_specialTagPlacement.origin (0x3048b0e4..)
 *   3001ffa6 SHL ESI,6 ; 3001ffb8 ADD ECX,ESI  ECX = table + (boneHandle<<6)
 *   3001ffad MOV EDX,[ESP+0x54] (=[base+4])     EDX = out (incoming stack arg)
 *   3001ffba LEA EAX,[ESP+0x14]                 EAX = &local
 *   3001ffbe CALL 0x3004ab30                    CG_ComposeBoneMatrix(bone,&local,out)
 *   3001ffc3 MOV EAX,1 ; ... ; RET              return qtrue
 *
 * The 16-byte `partBits` local (base-0x40) is the four-word DObj hierarchy bitset:
 * 0xad fills it, 0x9a consumes it for animation, and 0xae consumes it for the
 * final skeleton. The local matrix (base-0x30) is adjacent and non-overlapping.
 */
qboolean CG_DObjGetSpecialTagWorldMatrix(struct DObj_s *self, const char *tagName, DObjSkelMat *out)
{
    /* 3001fec6: ESI = trap(0xb2, self, tagName) -> the DObj bone/tag handle. */
    int32_t boneHandle = coduo_int32_from_bits((uint32_t)cgame_syscall(CG_DOBJ_GET_BONE_INDEX, (intptr_t)self, (intptr_t)tagName));

    /* 3001fed6: JGE continues on non-negative; a negative handle means no such
     * tag on this entity's DObj -> leave `out` untouched and return 0. */
    if (boneHandle < 0) {
        return qfalse;
    }

    /* 3001fee1-0x3001ff2c: calculate this bone hierarchy without running entity
     * controllers. CreateSkelForBone returns nonzero when it is already current;
     * otherwise GetHierarchyBits fills the four-word bitset, then CalcAnim and
     * CalcSkel consume it. */
    uint32_t partBits[4];
    if (cgame_syscall(CG_DOBJ_CREATE_SKEL_FOR_BONE, (intptr_t)self, boneHandle) == 0) {
        cgame_syscall(CG_DOBJ_GET_HIERARCHY_BITS, (intptr_t)self, boneHandle, (intptr_t)partBits);
        cgame_syscall(CG_DOBJ_CALC_ANIM, (intptr_t)self, (intptr_t)partBits);
        cgame_syscall(CG_DOBJ_CALC_SKEL, (intptr_t)self, (intptr_t)partBits);
    }

    /* 3001ff2f: ECX = trap(0xa0, self, 0) -> base of the per-bone matrix table;
     * 3001ffa6/ffb8: index it by boneHandle*0x40 (SHL 6) to select this bone.
     * Unlike the 0x3001fdf0 sibling, this variant does not NULL-check the result. */
    DObjSkelMat *boneMatrixTable = (DObjSkelMat *)(intptr_t)cgame_syscall(CG_DOBJ_GET_BONE_MATRICES, (intptr_t)self, 0);
    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if (boneMatrixTable == NULL) {
        return qfalse;
    }

    uint32_t boneOffset = (uint32_t)boneHandle << 6;
    DObjSkelMat *boneMatrix = (DObjSkelMat *)((uintptr_t)(void *)boneMatrixTable + (uintptr_t)boneOffset);

    /* 3001ff3a-0x3001ffb1: build the local placement matrix from the fixed global
     * cg_specialTagPlacement (an orientation_t). The 3x3 axis becomes the first
     * three matrix rows and the origin becomes the fourth (origin-after-axis in
     * source, axis-before-origin in the matrix rows). Plain dword copies carry the
     * float bit patterns through unchanged. */
    matrix43_t local;
    memcpy(&local.axis[0], &cg_specialTagPlacement.axis[0], sizeof(local.axis[0]));
    memcpy(&local.axis[1], &cg_specialTagPlacement.axis[1], sizeof(local.axis[1]));
    memcpy(&local.axis[2], &cg_specialTagPlacement.axis[2], sizeof(local.axis[2]));
    memcpy(&local.origin, &cg_specialTagPlacement.origin, sizeof(local.origin));

    /* 3001ffbe: compose the fixed local placement into the bone matrix -> `out`. */
    CG_ComposeBoneMatrix(boneMatrix, &local, out);

    /* 3001ffc3: MOV EAX,1 ; RET. */
    return qtrue;
}
