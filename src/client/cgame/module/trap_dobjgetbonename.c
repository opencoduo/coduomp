// Source: uo_cgame_mp_x86.dll 0x3003ed30..0x3003ed40

#include "client/cgame/client_recovered.h"

/* Return the name of a DObj bone. The original wrapper forwards obj (ECX) and
 * boneIndex (EAX) with syscall 0xb3 (CG_DOBJ_GET_BONE_NAME). */
const char *trap_DObjGetBoneName(struct DObj_s *obj, int32_t boneIndex)
{
    return (const char *)(uintptr_t)cgame_syscall(CG_DOBJ_GET_BONE_NAME, (intptr_t)obj, boneIndex);
}
