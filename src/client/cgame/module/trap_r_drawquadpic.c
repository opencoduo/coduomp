#include "../client_recovered.h"

// Source: uo_cgame_mp_x86.dll 0x3003e200..0x3003e286
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3003e200_3003e286.mcode
//
// trap_R_DrawQuadPic: variadic cgame system-call wrapper. It forwards ten stack
// arguments (nine float bit patterns and one shader handle) with command id 75.
// The recovered engine dispatcher maps command 75 to its rotated-stretch-pic
// renderer service, and the same-module PPC wrapper proves this source identity.

int32_t trap_R_DrawQuadPic(int32_t arg0,
                              int32_t arg1,
                              int32_t arg2,
                              int32_t arg3,
                              int32_t arg4,
                              int32_t arg5,
                              int32_t arg6,
                              int32_t arg7,
                              int32_t arg8,
                              int32_t arg9)
{
    return (int32_t)cgame_syscall(CG_R_DRAW_QUAD_PIC,
                                   arg0,
                                   arg1,
                                   arg2,
                                   arg3,
                                   arg4,
                                   arg5,
                                   arg6,
                                   arg7,
                                   arg8,
                                   arg9);
}
