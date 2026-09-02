// Source: uo_cgame_mp_x86.dll 0x3001f470..0x3001f5c0
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3001f470_3001f5c0.mcode

#include "../client_recovered.h"
#include "../globals.h"

#include <string.h>

/*
 * CG_Portal (0x3001f470) — the CG_AddCEntity ET_PORTAL render handler.
 * It builds one oriented (RT_PORTALSURFACE) render entity from a centity whose facing
 * is encoded as a bytedirs[] direction index, computes a full orthonormal axis basis
 * around that direction, fills the two endpoints and a shader handle, and submits it
 * via trap_R_AddRefEntityToScene (cgame trap 0x3d).
 *
 * Name adjudication: the .mcode header's size-matched name "CG_DrawDisconnect" is
 * REJECTED. That was a pure size collision (win size 0x150 == matched 0x150), which
 * the contract forbids as evidence. This function draws no HUD/2D text and reads no
 * connection state; it zero-fills an on-stack refEntity_t, builds an axis basis, and
 * issues trap 0x3d (CG_R_ADD_REF_ENTITY_TO_SCENE) — the render-entity submit path,
 * not a HUD draw. Call-graph proof of the real identity: the sole caller is the
 * eType==6 arm of the CG_AddCEntity dispatcher (jump table at 0x30022228, entry
 * index 6 = 0x3002220b: `MOV ESI,EBX; CALL 0x3001f470`), exactly like CG_General
 * (0x3001e430) is the eType==0 arm. The exact CoD source symbol for the eType-6
 * handler is unproven, so the name is behavior-derived (builds an ORIENTED
 * refEntity); superseded if a stronger name is proven.
 *
 * Register-argument ABI (proven from the caller and the entry stream): the centity
 * arrives in ESI (`MOV ESI,EBX; CALL`), modeled as the leading pointer parameter.
 *
 * Behavior, every statement proven against the .mcode (buffer base for the refEntity
 * is the aligned stack slot pushed as the trap argument; call it B, addressed as
 * ESP+0x8 at entry). Two mid-body PUSHes (the trap args EDX and 0x3d) shift ESP by 8
 * before the negation/cross-product block, so those [ESP+X] slots resolve to B+(X-8);
 * this is accounted for below.
 *
 *   1. /GS frame: snapshot __security_cookie into the frame at entry
 *      (MOV EAX,[0x30081650]; MOV [ESP+0xa4],EAX) and verify it via
 *      __security_check_cookie (0x30061639) on exit. Not source-level; omitted.
 *   2. memset(&re, 0, 0x9c): REP STOSD zeroes 0x27 (39) dwords from B (= sizeof re).
 *   3. re.origin    = cent->lerpOrigin      (raw dword MOVs from cent+0x208/20c/210).
 *   4. re.oldorigin = cent->currentState.effectEndOrigin   (raw dword MOVs from cent+0x5c/60/64).
 *   5. axis[0] (forward): if the byte-direction index cent->currentState.eventParm is a valid
 *      bytedirs[] index (signed 0 <= idx < 162), axis[0] = bytedirs[idx]; else {0,0,0}.
 *      (CMP idx,0 / JL; CMP idx,0xa2 / JGE; else LEA idx*3; SHL 2 -> idx*12 byte
 *      displacement into the vec3 array bytedirs at 0x30085f20.)
 *   6. axis[1] (right) = -PerpendicularVector(axis[0]): PerpendicularVector (0x3004a3d0,
 *      EDI = &axis[0], EDX = &axis[1]) writes a unit perpendicular into axis[1], then
 *      the three FLD 0.0f / FSUB axis[1][k] / FSTP axis[1][k] instructions negate each
 *      component in place (0.0f is the shared .rdata constant at 0x3007bcec).
 *   7. axis[2] (up) = CrossProduct(axis[0], axis[1]): the three FLD/FMUL/FLD/FMUL/FSUBP
 *      triples compute the standard i,j,k determinant expansion of axis[0] x axis[1].
 *   8. re.reType       = RT_PORTALSURFACE (11)  (MOV [ESP+0x10],0xb after the pushes).
 *   9. re.customShader = cent->currentState.effectShaderHandle (cent+0x70 -> re+0x80).
 *  10. re+0x50 (an unused refEntity frame field) is re-cleared to 0 (already 0 from
 *      the memset; MOV [ESP+0x60],EBX after the pushes = B+0x58 = re+0x50). Redundant.
 *  11. trap_R_AddRefEntityToScene(&re): PUSH &re; PUSH 0x3d; CALL [cgame_syscall];
 *      ADD ESP,8.
 *
 * The register-arg ABI (cent in ESI) and the /GS cookie save/verify are i386
 * calling-convention details, recorded here and expressed as plain C per the contract.
 */

/* Valid range of the bytedirs[] direction index (signed compare: 0 <= idx < 162). */
enum {
    BYTEDIRS_INDEX_COUNT = NUMVERTEXNORMALS /* 162, 0xa2 */
};

void CG_Portal(centity_t *cent /* ESI */)
{
    refEntity_t re;
    uint32_t originXBits;
    uint32_t originYBits;
    uint32_t originZBits;
    uint32_t oldOriginXBits;
    uint32_t oldOriginYBits;
    uint32_t oldOriginZBits;

    /* 0x3001f481: the z origin dword is fetched before the stack entity is
     * cleared and remains live in EDX until the store at 0x3001f4c1. */
    memcpy(&originZBits, &cent->lerpOrigin[2], sizeof(originZBits));

    /* 0x3001f476..0x3001f49b: aligned frame + REP STOSD zero of 0x9c bytes. */
    memset(&re, 0, sizeof(re));

    /* 0x3001f49d..0x3001f4d0: reproduce the interleaved raw-dword load/store
     * graph. No component travels through an FP register, so NaN payloads and
     * signed-zero bits copy verbatim. */
    memcpy(&originXBits, &cent->lerpOrigin[0], sizeof(originXBits));
    memcpy(&originYBits, &cent->lerpOrigin[1], sizeof(originYBits));
    memcpy(&re.origin[0], &originXBits, sizeof(originXBits));
    memcpy(&oldOriginXBits, &cent->currentState.effectEndOrigin[0], sizeof(oldOriginXBits));
    memcpy(&re.oldorigin[0], &oldOriginXBits, sizeof(oldOriginXBits));

    /* 0x3001f4b4 reads eventParm before publishing origin.y and the remaining
     * endpoint components. The original compare interprets the dword as signed. */
    int32_t dirIndex = coduo_int32_from_bits(cent->currentState.eventParm);
    memcpy(&re.origin[1], &originYBits, sizeof(originYBits));
    memcpy(&oldOriginYBits, &cent->currentState.effectEndOrigin[1], sizeof(oldOriginYBits));
    memcpy(&re.origin[2], &originZBits, sizeof(originZBits));
    memcpy(&oldOriginZBits, &cent->currentState.effectEndOrigin[2], sizeof(oldOriginZBits));
    memcpy(&re.oldorigin[1], &oldOriginYBits, sizeof(oldOriginYBits));
    memcpy(&re.oldorigin[2], &oldOriginZBits, sizeof(oldOriginZBits));

    /* 0x3001f4b4..0x3001f50f: axis[0] (forward) = the byte-encoded direction.
     * The index is compared as a signed int against 0 and 162. */
    {
        if (dirIndex < 0 || dirIndex >= BYTEDIRS_INDEX_COUNT) {
            re.axis[0][0] = 0.0f;
            re.axis[0][1] = 0.0f;
            re.axis[0][2] = 0.0f;
        } else {
            re.axis[0][0] = bytedirs[dirIndex][0];
            re.axis[0][1] = bytedirs[dirIndex][1];
            re.axis[0][2] = bytedirs[dirIndex][2];
        }
    }

    /* 0x3001f517: axis[1] = PerpendicularVector(axis[0]). */
    PerpendicularVector(re.axis[1], re.axis[0]);

    /* 0x3001f51c..0x3001f55f: negate axis[1] in place (right = -perpendicular).
     * These are x87 `0.0f - component` operations with a float store after each
     * result. The first result remains live while the shader handle is fetched;
     * the second loads zero before reType is published, then subtracts afterward. */
    long double negRight0 = 0.0L - (long double)re.axis[1][0];
    int32_t shaderHandle = cent->currentState.effectShaderHandle;
    re.axis[1][0] = (float)negRight0;

    long double negRight1 = 0.0L;
    re.reType = RT_PORTALSURFACE;
    negRight1 -= (long double)re.axis[1][1];
    re.frame = 0; /* 0x3001f546: redundant re-clear after the initial memset. */
    re.customShader = shaderHandle;
    re.axis[1][1] = (float)negRight1;

    re.axis[1][2] = (float)(0.0L - (long double)re.axis[1][2]);

    /* 0x3001f563..0x3001f5a1: axis[2] = CrossProduct(axis[0], axis[1]) (up), the
     * standard determinant expansion. Each FLD/FMUL pair and FSUBP remains in
     * x87 extended precision until that component's float FSTP. */
    re.axis[2][0] =
        (float)((long double)re.axis[0][1] * (long double)re.axis[1][2] - (long double)re.axis[0][2] * (long double)re.axis[1][1]);
    re.axis[2][1] =
        (float)((long double)re.axis[0][2] * (long double)re.axis[1][0] - (long double)re.axis[0][0] * (long double)re.axis[1][2]);
    re.axis[2][2] =
        (float)((long double)re.axis[0][0] * (long double)re.axis[1][1] - (long double)re.axis[0][1] * (long double)re.axis[1][0]);

    /* 0x3001f5a5: trap_R_AddRefEntityToScene(&re). */
    trap_R_AddRefEntityToScene(&re);

    /* 0x3001f5ab..0x3001f5bf: /GS cookie verify + epilogue (omitted). */
}
