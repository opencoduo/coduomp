// Source: uo_cgame_mp_x86.dll 0x3003ece0..0x3003ecf1

#include "client/cgame/client_recovered.h"

/* Fill basePose from one of a DObj's models. The original wrapper forwards obj
 * (EDX), modelIndex (ECX) and basePose (EAX) with syscall 0xb0
 * (CG_XMODEL_GET_BASE_POSE). */
void trap_XModelGetBasePose(struct DObj_s *obj, int32_t modelIndex, DObjSkelMat *basePose)
{
    cgame_syscall(CG_XMODEL_GET_BASE_POSE, (intptr_t)obj, modelIndex, (intptr_t)basePose);
}
