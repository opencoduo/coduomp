#include "q_math.h"

#include "compat/coduo_x87emu.h"

/*
 * The executable, cgame, Windows game, Linux engine, and Linux game bodies
 * store the subtraction to binary32 before calling AngleNormalize180.  The
 * supporting Mac cgame/game traceback symbols retain the canonical
 * AngleDelta name.
 *
 *   CoDUOMP.exe                 0x00433c70
 *   uo_cgame_mp_x86.dll        0x3004bfc0
 *   uo_game_mp_x86.dll         0x20018e50
 *   coduo_lnxded               0x08069efc
 *   game.mp.uo.i386.so         RVA 0x0003dc61
 *
 * The Windows UI body at 0x40003f90 is intentionally not here: it retains
 * the subtraction in ST0 and inlines the BAMS conversion, so its complete
 * implementation remains UI-local.
 */
float AngleDelta(float first, float second)
{
#if EMULATE_X87
    const float difference = x87f_store_f32(x87f_sub(x87f_load_f32(first), x87f_load_f32(second)));
#else
    const float difference = (float)((long double)first - (long double)second);
#endif
    return AngleNormalize180(difference);
}
