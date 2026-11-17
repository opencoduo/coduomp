// Source: uo_cgame_mp_x86.dll 0x3003d4d0..0x3003d507

#include "client/cgame/client_recovered.h"

/* Print an obituary line with its kill icon. The original wrapper forwards
 * attackerName (stack), attackerColor (stack), targetName (stack), targetColor
 * (stack), killIcon (stack), iconWidth (stack), iconScale (stack) and
 * iconColor (EDX) with syscall 0x4 (CG_DEATH_MESSAGE). */
void trap_DeathMessage(const char *attackerName, const float *attackerColor, const char *targetName,
                       const float *targetColor, const char *killIcon, float iconWidth, float iconScale,
                       const float *iconColor)
{
    cgame_syscall(CG_DEATH_MESSAGE, (intptr_t)attackerName, (intptr_t)attackerColor, (intptr_t)targetName,
                  (intptr_t)targetColor, (intptr_t)killIcon, CG_FloatBits(iconWidth), CG_FloatBits(iconScale),
                  (intptr_t)iconColor);
}
