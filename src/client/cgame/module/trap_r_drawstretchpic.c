#include "../client_recovered.h"

// Source: uo_cgame_mp_x86.dll 0x3003e0f0..0x3003e169
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3003e0f0_3003e169.mcode
//
// trap_R_DrawStretchPic: the cgame trap-73 wrapper for the engine's 2D
// stretch-pic draw. It shuffles its nine 32-bit stack arguments and forwards them
// unchanged to cgame_syscall with command id CG_R_DRAWSTRETCHPIC (73). Arguments
// are opaque 32-bit words (x, y, w, h, s1, t1, s2, t2 carried as float bit
// patterns, then the shader handle); the wrapper never interprets them, so
// bit-exact forwarding is preserved. The trap-id-to-service binding is proven by
// the CG_DrawPic call site (0x3001caa0), which scales the four coordinates by
// cgs.screenXScale/screenYScale and passes texcoords (0,0,1,1) plus a shader
// handle — the R_DrawStretchPic signature.

int32_t trap_R_DrawStretchPic(int32_t x, int32_t y, int32_t w, int32_t h, int32_t s1, int32_t t1, int32_t s2, int32_t t2, int32_t hShader)
{
    return coduo_int32_from_bits((uint32_t)cgame_syscall(CG_R_DRAWSTRETCHPIC, x, y, w, h, s1, t1, s2, t2, hShader));
}
