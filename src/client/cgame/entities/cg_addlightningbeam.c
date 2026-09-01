// Source: uo_cgame_mp_x86.dll 0x3001f3c0..0x3001f46c
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3001f3c0_3001f46c.mcode

#include "../client_recovered.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

/*
 * CG_AddLightningBeam (0x3001f3c0) — build one RT_BEAM render entity from a
 * source effect object's two endpoints and submit it to the current render scene.
 *
 * The .mcode header's size-matched name PerpendicularVector is REJECTED: this
 * function issues an engine syscall (trap 0x3d = CG_R_ADD_REF_ENTITY_TO_SCENE)
 * after zero-initializing a refEntity_t and populating render fields; it performs
 * no cross-product / basis-vector math. PerpendicularVector takes a vec3 and
 * returns a perpendicular vec3 and calls nothing — contradicted by the CALL to
 * cgame_syscall here. The behavior is exactly the trap-0x3d refEntity submit
 * pattern shared with CG_AddFadeRGB (0x3002abc0).
 *
 * Behavior (all offsets proven against the .mcode; buffer base B = the aligned
 * refEntity_t on the stack, addressed as ESP+8 / pushed as EDX):
 *   memset(&re, 0, 0x9c)                          // REP STOSD, 0x27 dwords from B
 *   re.reType   = RT_BEAM                    // MOV [B+0x00], 6
 *   re.renderfx = RF_NOSHADOW                // MOV [B+0x04], 0x40
 *   re.axis     = identity 3x3                    // [B+0x1c]=1.0, [B+0x2c]=1.0,
 *                                                 //   [B+0x3c]=1.0, rest 0
 *   re.origin    = src->start                     // src+0x18/0x1c/0x20 -> B+0x44
 *   re.oldorigin = src->end                       // src+0x5c/0x60/0x64 -> B+0x54
 *   trap_R_AddRefEntityToScene(&re)               // PUSH &re; PUSH 0x3d; CALL cgame_syscall
 *
 * Register-argument ABI (proven from the sole caller at 0x300234e6): the source
 * object arrives in EDX (`MOV EDX,ESI ; CALL 0x3001f3c0`), where ESI is the effect
 * entity the caller's per-type render dispatcher is processing. Modeled as the
 * leading pointer parameter `src`.
 *
 * Notes:
 *   - The identity matrix is written field-by-field: the diagonal elements
 *     [B+0x1c]/[B+0x2c]/[B+0x3c] get 1.0f (0x3f800000, held in ECX); all other axis
 *     slots and nonNormalizedAxes were already zero from the REP STOSD and are
 *     re-cleared with EAX=0. re.origin/re.oldorigin are dword-for-dword copies of
 *     the source's two vec3 fields (raw MOV, no FP), so they carry over verbatim.
 *   - The entry `MOV [B+0xa8], __security_cookie` / trailing reload and
 *     CALL __security_check_cookie (0x30061639) are the MSVC /GS stack-cookie
 *     guard, not source-level behavior; omitted from the C body.
 *   - `PUSH 0x3d ; CALL [cgame_syscall] ; ADD ESP,8` is the cdecl trap call with
 *     exactly one pointer argument, expressed via trap_R_AddRefEntityToScene.
 *
 * The sole caller passes its centity_t directly. The accessed +0x18/+0x5c
 * vectors are that record's already-proven currentState.pos.trBase and origin2
 * fields, not its distinct interpolated lerpOrigin at +0x208.
 */

void CG_AddLightningBeam(const centity_t *src)
{
    refEntity_t re;

    memset(&re, 0, sizeof(re));

    /* Dword-for-dword endpoint copies (raw MOV, no floating-point load). */
    memcpy(&re.origin[0], &src->currentState.origin[0], sizeof(re.origin[0]));
    memcpy(&re.origin[1], &src->currentState.origin[1], sizeof(re.origin[1]));
    memcpy(&re.origin[2], &src->currentState.origin[2], sizeof(re.origin[2]));
    memcpy(&re.oldorigin[0], &src->currentState.origin2[0], sizeof(re.oldorigin[0]));
    memcpy(&re.oldorigin[1], &src->currentState.origin2[1], sizeof(re.oldorigin[1]));
    memcpy(&re.oldorigin[2], &src->currentState.origin2[2], sizeof(re.oldorigin[2]));

    /* 0x3001f41e..0x3001f44a publishes the identity matrix, then the type and
     * render flags, after all endpoint copies and immediately before the call. */
    re.axis[0][0] = 1.0f;
    re.axis[0][1] = 0.0f;
    re.axis[0][2] = 0.0f;
    re.axis[1][0] = 0.0f;
    re.axis[1][1] = 1.0f;
    re.axis[1][2] = 0.0f;
    re.axis[2][0] = 0.0f;
    re.axis[2][1] = 0.0f;
    re.axis[2][2] = 1.0f;
    re.reType = RT_BEAM;
    re.renderfx = (int32_t)RF_NOSHADOW;

    trap_R_AddRefEntityToScene(&re);
}
