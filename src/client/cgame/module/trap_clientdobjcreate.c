// Source: uo_cgame_mp_x86.dll 0x3003ebf0..0x3003ec09

#include "client/cgame/client_recovered.h"

/* Create the client DObj for a handle from a model set and anim tree. The
 * original wrapper forwards models (stack), numModels (low word of EDX), tree
 * (ECX) and handle (EAX) with syscall 0xa7 (CG_CLIENT_DOBJ_CREATE). */
void trap_ClientDObjCreate(const DObjModel *models, uint16_t numModels, XAnimTree *tree, int32_t handle)
{
    cgame_syscall(CG_CLIENT_DOBJ_CREATE, (intptr_t)models, numModels, (intptr_t)tree, handle);
}
