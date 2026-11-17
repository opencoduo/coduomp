// Source: uo_cgame_mp_x86.dll 0x3003f170..0x3003f180

#include "client/cgame/client_recovered.h"

/* Resolve a named tag on an entity's DObj to a bone index; negative when the
 * tag is absent. The original wrapper pushes tagName (EAX), objId (ECX) and
 * syscall 0xe3 (CG_RESOLVE_TAG). */
int32_t trap_FX_GetBoneIndex(int32_t objId, const char *tagName)
{
    return (int32_t)cgame_syscall(CG_RESOLVE_TAG, objId, (intptr_t)tagName);
}
