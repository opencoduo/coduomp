// Source: uo_cgame_mp_x86.dll 0x3003f2c0..0x3003f2cf

#include "client/cgame/client_recovered.h"

/* Update the effect system's camera from the view. The original wrapper
 * forwards refdef (EAX) with syscall 0xef (CG_FX_ADJUST_CAMERA). */
void trap_FX_AdjustCamera(refdef_t *refdef)
{
    cgame_syscall(CG_FX_ADJUST_CAMERA, (intptr_t)refdef);
}
