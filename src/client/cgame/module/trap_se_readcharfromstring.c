// Source: uo_cgame_mp_x86.dll 0x3003df30..0x3003df3d

#include "client/cgame/client_recovered.h"

/* Read one character from a string and advance the cursor. The original
 * wrapper forwards text (ECX) and isTrailingPunctuation (EAX) with syscall
 * 0x3b (CG_SE_READ_CHAR_FROM_STRING). */
int32_t trap_SE_ReadCharFromString(char **text, qboolean *isTrailingPunctuation)
{
    return (int32_t)cgame_syscall(CG_SE_READ_CHAR_FROM_STRING, (intptr_t)text, (intptr_t)isTrailingPunctuation);
}
