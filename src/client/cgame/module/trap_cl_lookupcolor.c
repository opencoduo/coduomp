// Source: uo_cgame_mp_x86.dll 0x3003e410..0x3003e420

#include "client/cgame/client_recovered.h"

/* Look up the RGBA color for a color-code character. The original wrapper
 * forwards colorCode (low byte of ECX) and color (EAX) with syscall 0x5f
 * (CG_CL_LOOKUP_COLOR). */
void trap_CL_LookupColor(uint8_t colorCode, vec4_t color)
{
    cgame_syscall(CG_CL_LOOKUP_COLOR, colorCode, (intptr_t)color);
}
