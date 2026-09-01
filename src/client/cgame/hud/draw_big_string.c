// Source: uo_cgame_mp_x86.dll 0x3001cf10..0x3001cf8f
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3001cf10_3001cf8f.mcode

#include "client/cgame/client_recovered.h"
#include "client/cgame/globals.h"

#include <string.h>

/*
 * CG_DrawBigString (0x3001cf10)
 *
 * Emits a single fixed-shape 2D HUD draw through cgame trap 54 (CG_R_TEXT_PAINT).
 * This is a different emitter of trap 54 than the string-buffer family at
 * 0x30031940/0x30031a00/0x30031a90 documented on CG_R_TEXT_PAINT: instead of a
 * string buffer + three trailing zeros, this site passes a white color vec3
 * by pointer, a 1/3 scale, a 16.0f size, and a trailing mode word (3). The
 * engine service behind trap 54 is still unproven (no cgame syscall-id table
 * recovered), so the function is named by its proven role; the exact original
 * CoD symbol is unresolved.
 *
 * Name: the assigned-by-size guess "LerpAngle" is REJECTED. LerpAngle takes two
 * angles and a fraction and returns an interpolated float; it does not issue an
 * engine syscall. This function reads four caller args, does one float bias
 * (+16-2), and dispatches a 10-argument cdecl trap through *0x30085e9c
 * (cgame_syscall). The name matched only because both bodies happen to be
 * 0x7f bytes — a size collision, not evidence.
 *
 * Callers (all in the HUD region): 0x30018078, 0x30018316, 0x30019032. Each
 * pushes four dwords and cleans them with ADD ESP,0x10 (cdecl), confirming the
 * four-argument, caller-cleaned ABI. This callee runs `add esp,0x3c` to unwind
 * only its own frame + pushed trap args, then RET (no immediate).
 *
 * Behaviour, proven instruction-by-instruction from the .mcode:
 *   - yPos    = arg1 + 16.0f - 2.0f  (FLD [esp+1c]; FADD [16.0]; FSUB [2.0])
 *               i.e. arg1 + 14.0f, forwarded as a raw float dword (FSTP + PUSH).
 *   - color   = a local vec4 initialized to {1.0f, 1.0f, 1.0f, scale}, passed by
 *               address (LEA [esp+0x14]; PUSH). The three 1.0f words are written
 *               after the LEA (MOV [esp+2c/30/34],0x3f800000); the immediately
 *               following dword was filled from arg3 at 0x3001cf3c, so the
 *               renderer-visible alpha component is the caller's scale.
 *   - scale   = 0.33333f (0x3eaaaaab), stored to a stack local and PUSHed by
 *               value (adjacent to the color local but a separate argument).
 *   - trap    = cgame_syscall(54, arg0, yPosBits, 4, scaleBits, color,
 *                             arg2, 16.0fBits, 0, 3)
 *   - scale ([esp+0x24]) is read into EAX and stored immediately after the three
 *     white color components. RE_Text_PaintWithCursor reads that fourth float as
 *     alpha, so it is behaviorally live even though it is not a separate trap
 *     argument.
 *
 * Float dwords are forwarded to the variadic trap by raw bit pattern (the i386
 * code PUSHes the 4-byte float word, it is not promoted to double); CG_FloatBits
 * reproduces that exactly, matching the sibling emitters (CG_DrawFixedFadeElement).
 */

/* Vertical bias added to the incoming y coordinate: FADD 16.0f then FSUB 2.0f. */
enum { CG_TRAP54_Y_ADD = 16, CG_TRAP54_Y_SUB = 2 };

/* Fixed trap-54 draw parameters proven from the pushed immediates. */
#define CG_TRAP54_STYLE 4          /* PUSH 4 (int)                     */
#define CG_TRAP54_MODE  3          /* trailing PUSH 3 (int)            */
#define CG_TRAP54_SCALE (1.0f / 3.0f) /* 0x3eaaaaab, one-third scale   */
#define CG_TRAP54_SIZE  16.0f      /* 0x41800000, forwarded as a dword */

void CG_DrawBigString(float x, float y, const char *string, float scale)
{
    int32_t scaleBits = CG_FloatBits(scale);              /* 0x3001cf13 */
    long double yCarrier = (long double)y;                /* 0x3001cf17 */
    int32_t xBits = CG_FloatBits(x);                       /* 0x3001cf21 */
    vec4_t color;
    float yPos;

    yCarrier += (long double)(float)CG_TRAP54_Y_ADD;
    yCarrier -= (long double)(float)CG_TRAP54_Y_SUB;

    /* The incoming scale is copied into color[3] before the y FSTP; the three
     * white words are written last, after the outgoing syscall slots exist. */
    memcpy(&color[3], &scaleBits, sizeof(color[3]));
    yPos = (float)yCarrier;
    color[0] = 1.0f;
    color[1] = 1.0f;
    color[2] = 1.0f;

    cgame_syscall(CG_R_TEXT_PAINT,
                  xBits,
                  CG_FloatBits(yPos),
                  CG_TRAP54_STYLE,
                  CG_FloatBits(CG_TRAP54_SCALE),
                  (intptr_t)color,
                  (intptr_t)string,
                  CG_FloatBits(CG_TRAP54_SIZE),
                  0,
                  CG_TRAP54_MODE);
}
