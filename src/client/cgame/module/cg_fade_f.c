// Source: uo_cgame_mp_x86.dll 0x300173c0..0x300174b0
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_300173c0_300174b0.mcode
//
// CG_Fade_f -- console-command setter for the timed screen/FOV fade animator.
// The same-module Mac symbol has the same command role. Windows requires six
// argv entries, parses the first three color components (their results are not
// retained by this build), uses argv[4] as alpha/255 and argv[5] as seconds.

#include "client/cgame/client_recovered.h"

enum { CG_MILLISECONDS_PER_SECOND = 1000 };
static const float CG_FADE_BYTE_SCALE = 1.0f / 255.0f;

void CG_Fade_f(void)
{
    int32_t alpha;
    int32_t seconds;

    if (cgame_syscall(CG_ARGC) < 6) {
        return;
    }

    for (int32_t i = 1; i <= 3; ++i) {
        trap_Argv(i, g_textScratchBuffer, MAX_STRING_CHARS);
        /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
        (void)coduo_crt_atoi(g_textScratchBuffer);
    }

    trap_Argv(4, g_textScratchBuffer, MAX_STRING_CHARS);
    alpha = coduo_crt_atoi(g_textScratchBuffer);
    trap_Argv(5, g_textScratchBuffer, MAX_STRING_CHARS);
    seconds = coduo_crt_atoi(g_textScratchBuffer);

    cg_fovFade.startValue = (float)(
        (long double)alpha * (long double)CG_FADE_BYTE_SCALE);
    cg_fovFade.durationMs =
        (int32_t)((uint32_t)seconds * (uint32_t)CG_MILLISECONDS_PER_SECOND);
    cg_fovFade.startTime = coduo_int32_from_bits(cg_time);
    int32_t endTime = coduo_int32_from_bits(
        (uint32_t)cg_fovFade.startTime + (uint32_t)cg_fovFade.durationMs);
    if (endTime <= cg_fovFade.startTime) {
        cg_fovFade.currentValue = cg_fovFade.startValue;
    }
}
