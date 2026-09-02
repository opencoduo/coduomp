// Source: uo_cgame_mp_x86.dll 0x3001fdf0..0x3001feba
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3001fdf0_3001feba.mcode

#include "../client_recovered.h"
#include "../globals.h"

#include <stddef.h>
#include <stdint.h>

/*
 * CG_DObjGetWorldTagMatrix — build the world-space
 * bone/tag matrix for a client entity's DObj skeleton and write it into `out`.
 *
 * The assigned .mcode "# name script_method_player_playlocalsound" is a pure
 * corpus/size guess (win 0xca vs matched 0xcb) and is REJECTED: this function
 * does no sound work at all. It resolves a DObj bone handle, advances the entity's
 * DObj trace-part state, fetches the engine bone-matrix table, builds a local
 * orientation matrix from the entity's angles+origin via AngleVectors, and
 * composes the two into a world matrix. It is the composing sibling of the lighter
 * 0x3001fda0, which shares the exact prologue and DObj-handle logic but returns
 * the raw bone-matrix pointer instead of a composed world matrix.
 *
 * ABI (register-argument, proven from the machine code and the 0x3001fda0
 * sibling): `self` (the DObj context object) arrives in ECX (MOV ESI,ECX at
 * 0x3001fdf8, reused for both cgame syscalls). `tagName` (the named bone/tag to
 * resolve, e.g. "tag_flash") arrives in EAX: the prologue `PUSH EAX` at 0x3001fdf7
 * is NOT saved-register scratch — it is the second argument to trap(0xb2, self,
 * tagName) (PUSH EAX / PUSH ESI / PUSH 0xb2 / CALL / ADD ESP,0xc). `entity` is the
 * first STACK arg ([ESP+0x50] after the prologue) — a centity_t* whose origin
 * (+0x208) and axis angles (+0x214) feed the local matrix. `out` is the second
 * stack arg ([ESP+0x54]) — the float[16] world matrix the composer writes. Returns
 * qboolean in EAX: 1 on success, 0 when the entity has no DObj (negative handle) or
 * a NULL bone-matrix pointer. Callee cleans its own stack (SUB/ADD ESP,0x3c).
 * Because the convention is register-based it is modeled with ordered parameters
 * and no calling-convention attribute (syntax-only build does not require one).
 * (tagName was omitted by the earlier reconstruction, which mislabeled the EAX
 * push as scratch; the two-arg trap(0xb2) shape and the "tag_flash"/weaponName
 * strings loaded into EAX at every call site prove it is the tag-name argument.)
 *
 * Per-instruction proof of every behavior-affecting statement:
 *   3001fdfa PUSH EAX(tagName) / PUSH ESI / PUSH 0xb2 / CALL [cgame_syscall]
 *                                                       EDI = trap(0xb2,self,tagName)
 *   3001fe0b TEST EDI,EDI ; JL 0x3001fe33                 handle < 0 -> return 0
 *   3001fe0f MOV EBX,[ESP+0x50]                            EBX = entity (arg0)
 *   3001fe13 PUSH EBX / CALL 0x30022080                   CG_DObjCalcBone
 *                                                          (ESI=self, EDI=handle,
 *                                                           stack arg = entity)
 *   3001fe19 PUSH 0 / PUSH ESI / PUSH 0xa0 / CALL [syscall] EBP = trap(0xa0,self,0)
 *   3001fe29 SHL EDI,0x6 ; ADD EBP,EDI                     EBP = table + (handle<<6)
 *   3001fe31 JNZ 0x3001fe3d                                EBP == 0 -> return 0
 *                (JNZ tests EBP after ADD; the fall-through at 0x3001fe33 returns 0)
 *   3001fe3d LEA EDX,[EBX+0x214]                           angles = &entity.axisAngles
 *   3001fe43 LEA EBX,[ESP+0x34] (up) / EDI,[ESP+0x10] (right) / ESI,[ESP+0x1c] (fwd)
 *   3001fe4f CALL 0x3004a200                               AngleVectors(angles,
 *                                                           fwd,right,up)
 *   3001fe54 FLD 0.0f ; FSUB [ESP+0x10] ; FSTP [ESP+0x28]  local.row1[0] = -right[0]
 *   3001fe5e MOV EAX,[ESP+0x50] ; load [EAX+0x208/20c/210] = entity.origin
 *   3001fe74 (store negated right) ; ...
 *   3001fe78 FLD 0.0f ; FSUB [ESP+0x14] ; FSTP [ESP+0x2c]  local.row1[1] = -right[1]
 *   3001fe7e MOV [ESP+0x40]=origin[0] / [ESP+0x44]=origin[1] / [ESP+0x48]=origin[2]
 *   3001fe8a MOV EDX,[ESP+0x54]                            EDX = out (arg1)
 *   3001fe96 MOV EAX,ESI (=&local matrix) ; MOV ECX,EBP (=bone matrix)
 *   3001fe98 FLD 0.0f ; FSUB [ESP+0x18] ; FSTP [ESP+0x30]  local.row1[2] = -right[2]
 *   3001fea8 CALL 0x3004ab30                               CG_ComposeBoneMatrix(
 *                                                           bone, &local, out)
 *   3001feb0 MOV EAX,1 ; RET                                return qtrue
 *
 * The stack local at [ESP+0x1c..+0x4b] is one contiguous matrix43_t built as:
 *   row0 (+0x1c) = forward   (AngleVectors forward output)
 *   row1 (+0x28) = -right    (0.0f - AngleVectors right, component-wise)
 *   row2 (+0x34) = up        (AngleVectors up output)
 *   row3 (+0x40) = origin    (entity.lerpOrigin at +0x208, a plain dword copy)
 * The 0.0f subtrahend is the shared .rdata constant at 0x3007bcec; the plain
 * dword copies of the origin carry the float bit patterns through unchanged.
 * The Mac CG_DObjGetWorldTagMatrix follows the corresponding local-tag lookup,
 * AnglesToAxis, and matrix-composition path, resolving the source name.
 */
qboolean CG_DObjGetWorldTagMatrix(struct DObj_s *self, const char *tagName,
                                  centity_t *entity, DObjSkelMat *out)
{
    /* 3001fe00: EDI = trap(0xb2, self, tagName) -> the named bone/tag handle
     * (tagName arrives in EAX and is pushed as the trap's second argument). */
    int32_t boneHandle = coduo_int32_from_bits((uint32_t)cgame_syscall(
        CG_DOBJ_GET_BONE_INDEX, (intptr_t)self, (intptr_t)tagName));

    /* 3001fe0b: a negative handle means the entity has no DObj -> no matrix. */
    if (boneHandle < 0) {
        return qfalse;
    }

    /* 3001fe14: calculate this bone hierarchy and run the entity controllers.
     * The helper takes self in ESI, the bone index in EDI, and the owning entity
     * as its stack argument. */
    CG_DObjCalcBone(self, boneHandle, (centity_t *)entity);

    /* 3001fe21: EBP = trap(0xa0, self, 0) -> base of the entity's per-bone matrix
     * table; 3001fe29: index it by handle*0x40 (SHL 6) to select this bone. */
    DObjSkelMat *boneMatrixTable =
        (DObjSkelMat *)(intptr_t)cgame_syscall(
            CG_DOBJ_GET_BONE_MATRICES, (intptr_t)self, 0);

    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if (boneMatrixTable == NULL) {
        return qfalse;
    }

    /* SHL EDI,6 wraps in 32 bits. Add that byte offset at the explicit
     * engine-pointer boundary so a native 64-bit valid base is retained. */
    uint32_t boneOffset = (uint32_t)boneHandle << 6;
    DObjSkelMat *boneMatrix =
        (DObjSkelMat *)((uintptr_t)boneMatrixTable + (uintptr_t)boneOffset);

    /* 3001fe3d-0x3001fea4: build the local placement matrix from the entity's
     * axis angles (+0x214) and origin (+0x208). AngleVectors fills forward/right/up
     * temporaries; the matrix stores forward, the negated right vector, up, and the
     * origin as its four rows. */
    matrix43_t local;
    vec3_t forward;
    vec3_t right;
    vec3_t up;

    AngleVectors(entity->lerpAngles, forward, right, up);

    /* 3001fe54/78/98: FLD 0.0f / FSUB right[i] / FSTP -> negated right vector.
     * The 0.0f subtrahend is the shared .rdata constant at 0x3007bcec. */
    local.axis[0][0] = forward[0];
    local.axis[0][1] = forward[1];
    local.axis[0][2] = forward[2];

    local.axis[1][0] = 0.0f - right[0];
    local.axis[1][1] = 0.0f - right[1];
    local.axis[1][2] = 0.0f - right[2];

    local.axis[2][0] = up[0];
    local.axis[2][1] = up[1];
    local.axis[2][2] = up[2];

    /* 3001fe62-0x3001fe8e: origin copied as three plain dwords from +0x208. */
    local.origin[0] = entity->lerpOrigin[0];
    local.origin[1] = entity->lerpOrigin[1];
    local.origin[2] = entity->lerpOrigin[2];

    /* 3001fea8: compose the local matrix into the parent bone matrix -> `out`. */
    CG_ComposeBoneMatrix(boneMatrix, &local, out);

    /* 3001feb0: MOV EAX,1 ; RET. */
    return qtrue;
}
