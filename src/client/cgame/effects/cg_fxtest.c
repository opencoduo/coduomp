#include "../client_recovered.h"
#include "../globals.h"
#include "../platform/crt_boundary.h"

#include <stdint.h>

// Source: uo_cgame_mp_x86.dll 0x3003f430..0x3003f505
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3003f430_3003f505.mcode
// The Mac CG_FxTest has the same two Com_Printf and two CG_Argv calls plus the
// Q_strncpyz filename copy, resolving the command handler's source name.

void CG_FxTest(void)
{
    qhandle_t effect;

    if (cgame_syscall(CG_ARGC) < 2) {
        /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
        Com_Printf("Must supply filename from base path.  Optional restart time.\n");
    }

    cgame_syscall(CG_ARGV, 1, (intptr_t)g_textScratchBuffer, (int32_t)sizeof(g_textScratchBuffer));
    Q_strncpyz(cg_periodicEffectName, g_textScratchBuffer, 63);
    cg_periodicEffectName[63] = '\0';

    effect = (qhandle_t)cgame_syscall(CG_FX_REGISTER_EFFECT, (intptr_t)cg_periodicEffectName);
    Com_Printf("Spawning Fx %s with ID: %d\n", cg_periodicEffectName, effect);
    cgame_syscall(CG_PLAY_EFFECT_ORIGIN, (int32_t)effect, (intptr_t)cg_periodicEffectOrigin);
    cg_periodicEffectLastTime = (int32_t)cg_time;

    if (cgame_syscall(CG_ARGC) == 3) {
        cgame_syscall(CG_ARGV, 2, (intptr_t)g_textScratchBuffer, (int32_t)sizeof(g_textScratchBuffer));
        /* 0x3003f4e1..0x3003f4f4: atof's live x87 result is multiplied by the
         * binary64 1000.0 operand and passed directly to MSVC _ftol2. */
        cg_periodicEffectInterval = coduo_fp_to_i32_extended((long double)atof(g_textScratchBuffer) * 1000.0L);
    } else {
        cg_periodicEffectInterval = 0;
    }
}
