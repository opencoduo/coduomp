// Source: uo_cgame_mp_x86.dll 0x3003d7b0..0x3003d7bc

#include "client/cgame/client_recovered.h"

/* Return the clip handle of an inline (brush) model. The original wrapper
 * forwards index (EAX) with syscall 0x21 (CG_CM_INLINE_MODEL). */
int32_t trap_CM_InlineModel(int32_t index)
{
    return (int32_t)cgame_syscall(CG_CM_INLINE_MODEL, index);
}
