// Source: uo_cgame_mp_x86.dll 0x3001fd00..0x3001fd4c
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3001fd00_3001fd4c.mcode

#include "../client_recovered.h"

/* Register ABI: self=EDI, tagName=EAX, angles=EBX; the partBits and origin
 * pointers are the two stack arguments at entry +4 and +8 respectively. */
qboolean CG_DObjSetLocalTag(struct DObj_s *self, const char *tagName,
                            const uint32_t partBits[4], const vec3_t angles,
                            const vec3_t origin)
{
    int32_t boneIndex = coduo_int32_from_bits((uint32_t)cgame_syscall(
        CG_DOBJ_GET_BONE_INDEX, (intptr_t)self, (intptr_t)tagName));
    if (boneIndex < 0 ||
        coduo_int32_from_bits((uint32_t)cgame_syscall(
            CG_DOBJ_SET_ROT_TRANS_INDEX,
            (intptr_t)self, (intptr_t)partBits, boneIndex)) == 0) {
        return qfalse;
    }
    CG_DObjSetLocalTagInternal(self, boneIndex, angles, origin);
    return qtrue;
}
