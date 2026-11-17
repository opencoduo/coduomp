// Source: uo_cgame_mp_x86.dll 0x3003f330..0x3003f354

#include "client/cgame/client_recovered.h"

/* Set the shell-shock mouse speed limits. The original wrapper forwards
 * maxPitchSpeed (stack) and maxYawSpeed (stack) with syscall 0xf6
 * (CG_SET_SHELLSHOCK_MOUSE_LIMITS). */
void trap_SetShellshockMouseLimits(float maxPitchSpeed, float maxYawSpeed)
{
    cgame_syscall(CG_SET_SHELLSHOCK_MOUSE_LIMITS, CG_FloatBits(maxPitchSpeed), CG_FloatBits(maxYawSpeed));
}
