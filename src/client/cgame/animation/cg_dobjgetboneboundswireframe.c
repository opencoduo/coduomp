// Source: uo_cgame_mp_x86.dll 0x30020020..0x30020189
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30020020_30020189.mcode

#include "../client_recovered.h"
#include "../globals.h"

#include <stddef.h>
#include <stdint.h>

/*
 * CG_DObjGetBoneBoundsWireframe (provisional-by-role) — build the world-space
 * wireframe box of one DObj bone's local bounding box and write its 24 line
 * endpoints (12 edges x 2 ends) into `out`.
 *
 * The assigned .mcode "# name CG_LoadMenus" is a pure corpus/size guess (win
 * 0x169 vs matched 0x168) and is REJECTED: this function loads no menus. It is a
 * DObj/entity render helper: it resolves a DObj bone handle for `self`, asks the
 * engine for the per-bone part table and the bone matrix table, then, for that one
 * bone, walks the 24-entry box-corner-selector table (cg_boxCornerSelectors, the
 * companion of cg_debugBoxEdges) building each of the 12 edges' two endpoints from
 * the bone's local {mins,maxs} record and transforming each by the bone world
 * matrix. The single caller (0x30020190) then feeds the resulting endpoints, plus
 * an AngleVectors-derived orientation from the owning entity, to the follow-on draw
 * path. "Wireframe/bounds" role is proven by the selector table being the exact
 * per-axis-mins/maxs corner set of the 12 box edges.
 *
 * ABI (register-argument, proven from the machine code and the single caller):
 * `self` (the DObj context object) arrives in ECX (MOV ESI,ECX at 0x30020027,
 * reused for all four cgame syscalls). `tagName` arrives in EAX: the entry
 * PUSH EAX is the third slot of trap(0xb2,self,tagName), not alloca scratch.
 * `out` is the one stack argument ([EBP+0x8], loaded into ECX at 0x3002008f) —
 * the float array the 24 transformed endpoints are written to (24 * vec3 = 72
 * floats). Returns qboolean in EAX: 0
 * when the bone handle is negative (no DObj), 1 on success. The prologue's extra
 * PUSH EAX is scratch for the alloca frame; callee restores ESP via LEA
 * ESP,[EBP-0xc] and pops its saved registers. Because the convention is
 * register-based it is modeled with ordered parameters and no calling-convention
 * attribute (the syntax-only build does not require one).
 *
 * Per-instruction proof of the behavior-affecting statements:
 *   30020026 PUSH EAX / PUSH ESI / PUSH 0xb2 / CALL       EBX =
 *                                                         trap(0xb2,self,tagName)
 *   3002003a TEST EBX,EBX ; JGE 0x30020048                 handle < 0 -> return 0
 *   30020048 PUSH ESI / PUSH 0xb1 / CALL [syscall]         EAX = trap(0xb1, self)
 *   30020054 SHL EAX,2 ; ADD EAX,3 ; AND EAX,~3            byteSize = count*4 -> up
 *   30020060 CALL 0x30060a30 (_chkstk/_alloca_probe)        EDI = alloca(byteSize)
 *   30020067 PUSH EDI / PUSH ESI / PUSH 0xb4 / CALL [syscall] trap(0xb4,self,table)
 *   30020074 PUSH 0 / PUSH ESI / PUSH 0xa0 / CALL [syscall] EAX = trap(0xa0,self,0)
 *   30020082 MOV ESI,[EDI+EBX*4]                            src = table[handle]
 *   30020085 MOV ECX,EBX ; SHL ECX,6 ; ADD EAX,ECX          m = matrices + handle*0x40
 *   3002008f MOV ECX,[EBP+8]                                ECX = out
 *   30020092 MOV EDX,0x30071918                             EDX = selector table
 *   30020097 MOV EBX,0xc                                    loop 12 iterations,
 *   300200a0..30020176 the 2x-unrolled body writes two endpoints per iteration
 *            (each endpoint: three selector dwords -> a box corner -> M*corner)
 *   3002017c MOV EAX,1 ; RET                                return qtrue
 *
 * Bone matrix layout (proven from the FMUL offsets): a 4x4 row-major float matrix
 * (stride 0x40 == handle<<6). For a corner p = (px,py,pz) the transform emits
 *   out.x = px*m[0][0] + pz*m[2][0] + py*m[1][0] + m[3][0]
 *   out.y = px*m[0][1] + pz*m[2][1] + py*m[1][1] + m[3][1]
 *   out.z = px*m[0][2] + pz*m[2][2] + py*m[1][2] + m[3][2]
 * i.e. out = px*row0 + pz*row2 + py*row1 + row3. Load order is FLD px, FLD py,
 * FLD pz (ST0=pz, ST1=py, ST2=px); the first FLD ST2 pushes px (paired with
 * row0 at [EAX]), after which the pushed copy shifts the stack so FLD ST1
 * pushes pz (paired with row2 at [EAX+0x20]) and FLD ST2 pushes py (paired
 * with row1 at [EAX+0x10]) -- verified against the FMUL EAX-offsets at
 * 0x300200c4 onward. The FADDP grouping is ((px*r0 + pz*r2) + py*r1) + r3,
 * which the left-to-right C term order below reproduces. Only columns 0..2 of
 * each row are read; column 3 is untouched.
 */

qboolean CG_DObjGetBoneBoundsWireframe(struct DObj_s *self, const char *tagName, vec3_t out[24])
{
    /* 30020026..3002002f: EBX = trap(0xb2, self, tagName). The parent loads its
     * second stack argument into EAX immediately before this call. */
    int32_t boneHandle = coduo_int32_from_bits((uint32_t)cgame_syscall(CG_DOBJ_GET_BONE_INDEX, (intptr_t)self, (intptr_t)tagName));

    /* 3002003a: JGE keeps handle>=0; a negative handle means no DObj -> return 0. */
    if (boneHandle < 0) {
        return qfalse;
    }

    /* 30020048: EAX = trap(0xb1, self) -> number of DObj parts/bones. */
    int32_t partCount = coduo_int32_from_bits((uint32_t)cgame_syscall(CG_DOBJ_NUM_BONES, (intptr_t)self));
    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if ((uint32_t)boneHandle >= (uint32_t)partCount) {
        return qfalse;
    }

    /* 30020054-0x30020060: i386 allocates one four-byte pointer slot per part via
     * wrapping SHL/ADD/AND dword arithmetic. Native engines write native pointers
     * through trap 0xb4, so wider hosts must widen each slot while retaining the
     * same dword byte-count carrier. */
    uint32_t partTableBytes = ((uint32_t)partCount * (uint32_t)sizeof(XModelPartColl *) + 3u) & ~3u;
    XModelPartColl **partTable = (XModelPartColl **)__builtin_alloca(partTableBytes);

    /* 30020067: trap(0xb4, self, partTable) -> fill the table with a per-part
     * pointer (here, a pointer to each part's local {mins,maxs} bounds record). */
    cgame_syscall(CG_DOBJ_BUILD_PART_COLLISION_TABLE, (intptr_t)self, (intptr_t)partTable);

    /* 30020074: EAX = trap(0xa0, self, 0) -> base of the per-bone matrix table. */
    DObjSkelMat *matrixTable = (DObjSkelMat *)(intptr_t)cgame_syscall(CG_DOBJ_GET_BONE_MATRICES, (intptr_t)self, 0);

    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if (matrixTable == NULL || partTable[boneHandle] == NULL) {
        return qfalse;
    }

    /* 30020082/0x30020085: select this bone's source bounds and world matrix. */
    const XModelPartColl *src = partTable[boneHandle];
    uint32_t boneOffset = (uint32_t)boneHandle << 6;
    const DObjSkelMat *bone = (const DObjSkelMat *)((uintptr_t)(const void *)matrixTable + (uintptr_t)boneOffset);

    /* 300200a0-0x30020176: for each of the 24 box-edge endpoints, build the corner
     * by picking mins/maxs per axis from the selector table, then transform it by
     * the bone world matrix. (The machine code unrolls this by 2 and runs a
     * 12-iteration loop; the semantics are the 24 endpoints below.) */
    for (int32_t i = 0; i < 24; ++i) {
        uint32_t selX = cg_boxCornerSelectors[i][0];
        uint32_t selY = cg_boxCornerSelectors[i][1];
        uint32_t selZ = cg_boxCornerSelectors[i][2];

        const vec3_t *bounds = &src->mins;
        const long double px = bounds[selX][0];
        const long double py = bounds[selY][1];
        const long double pz = bounds[selZ][2];

        const long double outX = ((px * bone->axis[0][0] + pz * bone->axis[2][0]) + py * bone->axis[1][0]) + bone->origin[0];
        out[i][0] = (float)outX;

        const long double outY = ((px * bone->axis[0][1] + pz * bone->axis[2][1]) + py * bone->axis[1][1]) + bone->origin[1];
        out[i][1] = (float)outY;

        const long double outZ = ((px * bone->axis[0][2] + pz * bone->axis[2][2]) + py * bone->axis[1][2]) + bone->origin[2];
        out[i][2] = (float)outZ;
    }

    /* 3002017c: MOV EAX,1 ; RET. */
    return qtrue;
}
