// Source: uo_cgame_mp_x86.dll 0x3001cf90..0x3001cfeb
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3001cf90_3001cfeb.mcode

#include "../client_recovered.h"

void CG_Trap54DrawStyle4(const vec4_t color, int32_t x, float y,
                         const char *string)
{
    float yAdjusted = (float)(((long double)y + 16.0L) - 2.0L);
    cgame_syscall(CG_R_TEXT_PAINT, x, CG_FloatBits(yAdjusted),
                  4, CG_FloatBits(1.0f / 3.0f), (intptr_t)color,
                  (intptr_t)string,
                  CG_FloatBits(16.0f), 0, 3);
}
