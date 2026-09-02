// Source: uo_cgame_mp_x86.dll 0x3001fd50..0x3001fd9c
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3001fd50_3001fd9c.mcode

#include "../client_recovered.h"

/* The Mac CG_DObjSetControlTagAngles is the same thin bone-index,
 * control-rot/trans-index, and local-tag setter. */

/* Register ABI: self=EDI, tagName=EAX, angles=EBX; partBits is the sole stack
 * argument at entry +4. */
qboolean CG_DObjSetControlTagAngles(struct DObj_s *self, const char *tagName, const uint32_t partBits[4], const vec3_t angles)
{
    int32_t boneIndex = coduo_int32_from_bits((uint32_t)cgame_syscall(CG_DOBJ_GET_BONE_INDEX, (intptr_t)self, (intptr_t)tagName));
    if (boneIndex < 0 || cgame_syscall(CG_DOBJ_SET_CONTROL_ROT_TRANS_INDEX, (intptr_t)self, (intptr_t)partBits, boneIndex) == 0) {
        return qfalse;
    }
    CG_DObjSetLocalTagInternal(self, boneIndex, angles, vec3_origin);
    return qtrue;
}
