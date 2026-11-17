// Source: uo_cgame_mp_x86.dll 0x3003e370..0x3003e37c

#include "client/cgame/client_recovered.h"

/* Set whether user commands are generated in shell shock. The original wrapper
 * forwards blur (EAX) with syscall 0x57 (CG_SET_SHELLSHOCK_SCREEN_BLUR). */
void trap_SetShellshockScreenBlur(int32_t blur)
{
    cgame_syscall(CG_SET_SHELLSHOCK_SCREEN_BLUR, blur);
}
