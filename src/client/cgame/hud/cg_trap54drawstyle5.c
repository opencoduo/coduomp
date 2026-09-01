// Source: uo_cgame_mp_x86.dll 0x3001d070..0x3001d0cb
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3001d070_3001d0cb.mcode

#include "../client_recovered.h"

void CG_Trap54DrawStyle5(const vec4_t color, int32_t x, float y,
                         const char *string)
{
    float yAdjusted = (float)(((long double)y + 16.0L) - 2.0L);
    cgame_syscall(CG_R_TEXT_PAINT, x, CG_FloatBits(yAdjusted),
                  5, CG_FloatBits(1.0f / 3.0f), (intptr_t)color,
                  (intptr_t)string,
                  CG_FloatBits(8.0f), 0, 0);
}
