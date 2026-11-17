// Source: uo_cgame_mp_x86.dll 0x3003eb30..0x3003eb41

#include "client/cgame/client_recovered.h"

/* DObjGetMatrixArray model index: the DObj's first (root) model. */
#define CG_DOBJ_ROOT_MODEL_INDEX 0

/* Return the bone matrices of a DObj's root model. The original wrapper
 * forwards obj (EAX) and a literal 0 with syscall 0xa0
 * (CG_DOBJ_GET_BONE_MATRICES). */
DObjSkelMat *trap_DObjGetBoneMatrices(struct DObj_s *obj)
{
    return (DObjSkelMat *)(uintptr_t)cgame_syscall(CG_DOBJ_GET_BONE_MATRICES, (intptr_t)obj, CG_DOBJ_ROOT_MODEL_INDEX);
}
