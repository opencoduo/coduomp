// Source: uo_cgame_mp_x86.dll 0x3003d7a0..0x3003d7ab

#include "client/cgame/client_recovered.h"

/* Return the number of inline (brush) models. The original wrapper pushes only
 * syscall 0x20 (CG_CM_NUM_INLINE_MODELS). */
int32_t trap_CM_NumInlineModels(void)
{
    return (int32_t)cgame_syscall(CG_CM_NUM_INLINE_MODELS);
}
