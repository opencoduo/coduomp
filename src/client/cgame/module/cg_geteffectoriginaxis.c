// Source: uo_cgame_mp_x86.dll 0x3002ae70..0x3002aed8
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3002ae70_3002aed8.mcode

#include "client/cgame/client_recovered.h"
#include "client/cgame/globals.h"

/*
 * CG_GetEffectOriginAxis — cgame script/VM builtin (dispatched from the builtin
 * table at 0x3002af.., sole caller 0x3002b085) that returns the placement of one
 * entity from cg_entities: it copies the stored world origin to
 * *outOrigin and derives an orientation axis from the slot's stored Euler angles
 * into outAxis. Name is provisional-by-role; the .mcode's size-matched
 * "Menu_SetupKeywordHash" guess is rejected (this does effect-pool math and vector
 * orientation, not menu keyword hashing).
 *
 * Machine-code facts proven for every statement below:
 *   3002ae70  IMUL EAX,EAX,0x288        EAX = effectIndex * sizeof(centity_t)
 *   3002ae7b  MOV ESI,EDX               ESI = outAxis (arg3, EDX)
 *   3002ae7d  MOV EDX,[EAX+0x3048c8e8]  cg_entities[i]+0x208
 *   3002ae83  MOV [ECX],EDX             outOrigin[0] = entity lerpOrigin[0]
 *   3002ae85  MOV EDX,[EAX+0x3048c8ec] ; MOV [ECX+0x4],EDX   outOrigin[1] = .origin[1]
 *   3002ae8e  MOV EDX,[EAX+0x3048c8f0] ; MOV [ECX+0x8],EDX   outOrigin[2] = .origin[2]
 *   3002ae98  LEA EBX,[ESI+0x18]        EBX = &outAxis[2] (up output, +0x18 = 2*vec3)
 *   3002ae9b  LEA EDX,[EAX+0x3048c8f4]  EDX = entity lerpAngles (+0x214)
 *   3002aea1  LEA EDI,[ESP+0xc]         EDI = &tmpRight (3-float stack temp)
 *   3002aea5  CALL 0x3004a200           AngleVectors(angles=EDX, forward=ESI=&outAxis[0],
 *                                                    right=EDI=&tmpRight, up=EBX=&outAxis[2])
 * Then the "left" row outAxis[1] = 0.0f - tmpRight (FLD 0.0 const @0x3007bcec / FSUB /
 * FSTP), giving the id-Tech AnglesToAxis layout (axis[0]=forward, axis[1]=-right,
 * axis[2]=up):
 *   3002aeab  FLD [0x3007bcec]=0.0f ; FSUB [ESP+0x8]=tmpRight[0] ; FSTP [ESI+0x0c]=outAxis[1][0]
 *   3002aeb8  FLD 0.0f ; FSUB [ESP+0x0c]=tmpRight[1] ; FSTP [ESI+0x10]=outAxis[1][1]
 *   3002aec5  FLD 0.0f ; FSUB [ESP+0x10]=tmpRight[2] ; FSTP [ESI+0x14]=outAxis[1][2]
 * (After POP EDI at 3002aeaa, the [ESP+0xc] temp is at [ESP+0x8..0x10]; those are the
 * three tmpRight components AngleVectors wrote via EDI.)
 *   3002aed2  POP ESI/EBX ; ADD ESP,0xc ; RET   returns void (caller reads no EAX result)
 *
 * outAxis[0] (forward) and outAxis[2] (up) are written directly by AngleVectors; only
 * outAxis[1] is post-negated here, which is exactly why the "right" output is routed to
 * a local temp rather than into outAxis[1].
 *
 * Register-argument ABI (custom regparm): effectIndex in EAX, outOrigin in ECX,
 * outAxis in EDX. Modeled as ordered parameters; no calling-convention attribute is
 * added because the syntax-only build does not require one.
 */
void CG_GetEffectOriginAxis(int32_t effectIndex, vec3_t outOrigin, axis_t outAxis)
{
    vec3_t right;

    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if ((uint32_t)effectIndex >= (uint32_t)MAX_GENTITIES) {
        outOrigin[0] = 0.0f;
        outOrigin[1] = 0.0f;
        outOrigin[2] = 0.0f;
        outAxis[0][0] = 1.0f;
        outAxis[0][1] = 0.0f;
        outAxis[0][2] = 0.0f;
        outAxis[1][0] = 0.0f;
        outAxis[1][1] = 1.0f;
        outAxis[1][2] = 0.0f;
        outAxis[2][0] = 0.0f;
        outAxis[2][1] = 0.0f;
        outAxis[2][2] = 1.0f;
        return;
    }

    centity_t *entity = &cg_entities[effectIndex];

    outOrigin[0] = entity->lerpOrigin[0];
    outOrigin[1] = entity->lerpOrigin[1];
    outOrigin[2] = entity->lerpOrigin[2];

    /* AngleVectors writes forward -> outAxis[0], up -> outAxis[2]; right goes to a
     * temp so it can be negated into the middle ("left") basis row. */
    AngleVectors(entity->lerpAngles, outAxis[0], right, outAxis[2]);

    outAxis[1][0] = 0.0f - right[0];
    outAxis[1][1] = 0.0f - right[1];
    outAxis[1][2] = 0.0f - right[2];
}
