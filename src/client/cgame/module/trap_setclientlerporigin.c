// Source: uo_cgame_mp_x86.dll 0x3003f0d0..0x3003f0f9

#include "client/cgame/client_recovered.h"

/* Set the client's interpolated view origin. The original wrapper forwards x
 * (stack), y (stack) and z (stack) with syscall 0xd4
 * (CG_SET_CLIENT_LERP_ORIGIN). */
void trap_SetClientLerpOrigin(float x, float y, float z)
{
    cgame_syscall(CG_SET_CLIENT_LERP_ORIGIN, CG_FloatBits(x), CG_FloatBits(y), CG_FloatBits(z));
}
