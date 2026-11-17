// Source: uo_cgame_mp_x86.dll 0x3003f360..0x3003f36f

#include "client/cgame/client_recovered.h"

/* Set the client view angles from a vehicle view. The original wrapper
 * forwards angles (EAX) with syscall 0xf7 (CG_VEH_VIEW_ANGLE_DELTA). */
void trap_VehViewAngleDelta(const vec3_t angles)
{
    cgame_syscall(CG_VEH_VIEW_ANGLE_DELTA, (intptr_t)angles);
}
