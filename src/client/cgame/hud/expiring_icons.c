// Source: uo_cgame_mp_x86.dll 0x3001b170..0x3001b2ab
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3001b170_3001b2ab.mcode

#include "../client_recovered.h"
#include "../globals.h"
#include "qcommon/statmon_types.h"

#include <stdint.h>

/*
 * CG_DrawExpiringIconGrid (0x3001b170)
 *
 * NAME ADJUDICATION: the mechanical `# name Com_Compress` in the .mcode is a pure
 * size-guess (win size 0x13b == corpus size 0x13b) and is REJECTED — the machine
 * code is not a string/whitespace compressor at all. It has no byte-copy loop and
 * touches no char buffer; it walks an engine-supplied array of 8-byte records and
 * draws each surviving one as a 2D stretch-pic. Named here by proven behavior.
 *
 * Behavior proven from the instruction stream:
 *   now = cgame_syscall(CG_MILLISECONDS);            // engine milliseconds (edi)
 *   cgame_syscall(CG_GET_EXPIRING_ICON_LIST, &list, &count); // engine fills an out array + count
 *   x = 2.0f; y = 200.0f;                      // virtual-640/480 grid cursor
 *   for (i = 0; i < count; ++i) {
 *       if (list[i].expireTime >= now)         // CMP [ecx+esi*8],edi ; JL skip-draw
 *           trap_R_DrawStretchPic(x*screenXScale, y*screenYScale,
 *                                 32.0f*screenXScale, 32.0f*screenYScale,
 *                                 0,0,1,1, list[i].hShader);
 *       x += 34.0f;                            // advance grid cursor every entry,
 *       if (x + 32.0f > 68.0f) { x = 2.0f; y += 34.0f; } //   drawn or not
 *   }
 *
 * The cursor advance (b261..b294) runs for EVERY element, including expired ones
 * whose draw was skipped (the JL at b1c3 jumps into the advance, not past it), so
 * an expired entry still consumes a grid slot. Two 32px icons per row on a 34px
 * pitch (x=2 then x=36; 36+32==68 is not > 68 so it stays, then 70 wraps), rows
 * stepping down in y by 34 starting at y=200. Coordinates are in the virtual
 * 640x480 space and scaled to the real backbuffer by cgs.screenXScale /
 * cgs.screenYScale exactly as CG_DrawPic does.
 *
 * The recovered engine dispatcher proves this is StatMon_GetStatsArray and that
 * the result rows are the engine's statmon_entry_t records.
 *
 * Callees / globals (objdump-resolved):
 *   cgame_syscall        <- [0x30085e9c]  (the cgame VM trap dispatcher pointer)
 *   trap_R_DrawStretchPic (trap id 0x49 == CG_R_DRAWSTRETCHPIC)
 *   cgs_screenXScale     <- float [0x30447aa4]
 *   cgs_screenYScale     <- float [0x30447aa8]
 *   .rdata floats: 32.0f [0x3007bdd0], 34.0f [0x3007bf20], 68.0f [0x3007bf1c]
 */

void CG_DrawExpiringIconGrid(void)
{
    /* now = (int32_t)cgame_syscall(CG_MILLISECONDS): engine milliseconds. Single dword pushed,
     * left on the stack and folded into the later combined ADD ESP,0x10 cleanup. */
    int32_t now = coduo_int32_from_bits((uint32_t)cgame_syscall(CG_MILLISECONDS));

    /* CG_GET_EXPIRING_ICON_LIST fills two adjacent stack locals: the list base pointer and the
     * element count. Both &locals are pushed (arg order &count,&list high-to-low),
     * id last; caller cleans them with the same ADD ESP,0x10. */
    const statmon_entry_t *list;
    int32_t count;
    cgame_syscall(CG_GET_EXPIRING_ICON_LIST, (intptr_t)&list, (intptr_t)&count);

    /* Grid cursor in virtual 640x480 space (MOV [esp+8]=2.0f, [esp+0xc]=200.0f). */
    float x = 2.0f;
    float y = 200.0f;

    /* TEST EAX,EAX ; JLE end: nothing to do when count <= 0. */
    for (int32_t i = 0; i < count; ++i) {
        /* CMP [ecx+esi*8],edi ; JL skip-draw: draw only while not yet expired. */
        /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
        if (list[i].expireTime >= now) {
            /* 0x3001b1c9..0x3001b248: height is begun first, with the shader
             * dword read while its scale is live; width follows, then screen Y,
             * then screen X. Each FSTP m32 boundary is explicit. */
            long double heightValue = (long double)cgs_screenYScale;
            int32_t shaderHandle = list[i].shaderHandle;
            heightValue *= 32.0L;
            float height = (float)heightValue;

            long double widthValue = (long double)cgs_screenXScale;
            widthValue *= 32.0L;
            float width = (float)widthValue;

            long double screenYValue = (long double)cgs_screenYScale;
            screenYValue *= (long double)y;
            float screenY = (float)screenYValue;

            long double screenXValue = (long double)cgs_screenXScale;
            screenXValue *= (long double)x;
            float screenX = (float)screenXValue;

            trap_R_DrawStretchPic(CG_FloatBits(screenX), CG_FloatBits(screenY), CG_FloatBits(width), CG_FloatBits(height),
                                  CG_FloatBits(0.0f), CG_FloatBits(0.0f), CG_FloatBits(1.0f), CG_FloatBits(1.0f), shaderHandle);
        }

        /* Advance the grid cursor for every element (drawn or skipped):
         *   x += 34.0f; if (x + 32.0f > 68.0f) { x = 2.0f; y += 34.0f; }
         * FCOMP (x+32) vs 68 then TEST AH,0x41 ; JNZ: the wrap (fall-through) runs
         * only when x+32 > 68 (C0 and C3 both clear).
         * FST-retain (0x3001b26b FST, not FSTP): the wrap test adds 32.0f to the
         * UNROUNDED x+34 still in st0, not to the stored float; xNew keeps that
         * 80-bit value for the compare. */
        long double xNew = (long double)x + 34.0L;
        x = (float)xNew;
        if (xNew + 32.0f > 68.0f) {
            x = 2.0f;
            y = (float)((long double)y + 34.0f);
        }
    }
}
