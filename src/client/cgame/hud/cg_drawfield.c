// Source: uo_cgame_mp_x86.dll 0x30017bc0..0x30017db3
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30017bc0_30017db3.mcode

#include "../client_recovered.h"
#include "../globals.h"

#include <stdint.h>

/*
 * CG_DrawField (0x30017bc0) — the DRAW counterpart of the width-measurement
 * helper CG_DrawFieldWidth (0x30017aa0, its immediate neighbor). Given a fixed
 * digit-count field `width`, an integer `value`, a screen position and a per-glyph
 * cell size, it clamps the value to what fits in `width` characters, formats it as
 * signed decimal ("%i"), and blits each character left-to-right as one bitmap glyph
 * from cg_numberShaders[] via trap_R_DrawStretchPic (cgame trap 0x49 ==
 * CG_R_DRAWSTRETCHPIC). It returns the field's starting X pen position (after any
 * right/center-justify adjustment), matching the pen-advance return of the sibling.
 *
 * Behavior proven from the machine code:
 *   - width (ECX -> EBX): if width < 1, return 0 immediately (0x30017bdb JGE past the
 *     early XOR EAX,EAX / RET); if width > 5 it is clamped to 5 (0x30017bf1 CMP 5 /
 *     MOV 5). Width 5 skips the value clamp (the switch only handles widths 1..4).
 *   - value clamp switch on (width-1) via the jump table at 0x30017db4 (LEA ECX,[EBX-1];
 *     CMP ECX,3; JA default), the SAME per-digit-magnitude clamps as
 *     CG_DrawFieldWidth / CG_HudEmitDigits:
 *         width 1 -> [0, 9]      (max 9; negatives forced to 0 via SETL/DEC/AND)
 *         width 2 -> [-9, 99]    (0x63 / 0xfffffff7)
 *         width 3 -> [-99, 999]  (0x3e7 / 0xffffff9d)
 *         width 4 -> [-999, 9999](0x270f / 0xfffffc19)
 *         width 5 -> no clamp
 *   - Com_sprintf(text, 16, "%i", value) formats the clamped value (0x3004e820; dest in
 *     EDI, size 16 in ESI, "%i" and value pushed then caller-cleaned via ADD ESP,8).
 *   - strlen(text) computed inline (0x30017c84..0x30017c8e), capped at `width`
 *     (glyphs = min(strlen, width)).
 *   - Right/center-justify (0x30017c9e): when the `justify` flag arg is 0, the starting
 *     X pen is shifted left by (2 + glyphs*charWidth); a non-zero flag leaves it as-is
 *     (left-justify). The adjusted pen is what the function returns.
 *   - Per-character loop (0x30017cd0..0x30017d9a): for each formatted character, index
 *     cg_numberShaders[] by digit value (MOVSX AL; SUB '0') or by 10 for '-'
 *     (CG_NUMBER_SHADER_MINUS), and — only when the draw-enable flag arg is non-zero
 *     (0x30017cef TEST/JZ) — issue trap_R_DrawStretchPic with the glyph's device-pixel
 *     rect: x = pen*screenXScale, y = y*screenYScale, w = charWidth*screenXScale,
 *     h = charHeight*screenYScale, full 0..1 texture coords. The pen always advances by
 *     charWidth (0x30017d8e ADD pen,charWidth), whether or not the glyph was drawn, and
 *     the loop stops on the NUL terminator or once the glyph budget (min(strlen,width))
 *     is exhausted.
 *
 * Register/stack ABI (proven from the body; no direct callers are visible because it is
 * dispatched through a HUD-element method table, like the width sibling):
 *   ECX   = width       (thiscall-style register argument, clamped 1..5)
 *   arg0  = x           ([ESP+0x44] on entry) — starting X pen; RETURNED (adjusted)
 *   arg1  = y           ([ESP+...]) — field Y
 *   arg2  = value       — the integer to draw
 *   arg3  = charWidth   ([ESP+0x4c] -> EBP) — per-glyph horizontal advance AND draw width
 *   arg4  = charHeight  — per-glyph draw height
 *   arg5  = drawGlyphs  — draw-enable flag: 0 lays out the pen without emitting any pic
 *   arg6  = justify     — 0 => right/center adjust (pen -= 2 + glyphs*charWidth); != 0 => left
 * Returns the (justify-adjusted) starting X pen in EAX. All coordinates are virtual
 * 640x480 units scaled to device pixels by cgs_screenXScale/cgs_screenYScale, exactly
 * as CG_HudEmitDigits and the other 2D draw paths do.
 *
 * Both exits carry the MSVC /GS canary boilerplate (MOV EAX,[__security_cookie] at
 * 0x30017bc3, __security_check_cookie 0x30061639 at both exits) — a compiler artifact
 * with no source-level meaning.
 *
 * Name adjudication: the .mcode header's mechanical guess "PM_CmdScale" (a pure win-size
 * 0x1f3 == matched-size 0x1f4 guess) is REJECTED — the real PM_CmdScale is 0x30008690
 * (reconstructed already), a pmove command-scaling routine with no drawing. This body
 * formats an integer and issues per-glyph 2D HUD stretch-pics against cg_numberShaders[]
 * with screen-scale multiplies and the 0x49 draw trap. The same-module Mac symbol
 * bank identifies this exact bitmap numeric-field behavior as CG_DrawField. That
 * name also avoids colliding with the distinct CG_DrawHudElemString at 0x30029f70.
 */

/* "%i" (0x300769e0 in .rdata): signed-decimal format for the numeric field. */
static const char CG_HUDELEM_NUM_FORMAT[] = "%i";

/* MOV ESI,0x10 -> Com_sprintf size argument (the value-text buffer size). */
enum { CG_HUDELEM_NUM_BUFSIZE = 16 };

/* Per-digit-count magnitude clamps (jump table at 0x30017db4). */
enum {
    CG_HUDELEM_W1_MAX =    9,  /* 0x9 */
    CG_HUDELEM_W2_MAX =   99,  /* 0x63 */
    CG_HUDELEM_W2_MIN =   -9,  /* 0xfffffff7 */
    CG_HUDELEM_W3_MAX =  999,  /* 0x3e7 */
    CG_HUDELEM_W3_MIN =  -99,  /* 0xffffff9d */
    CG_HUDELEM_W4_MAX = 9999,  /* 0x270f */
    CG_HUDELEM_W4_MIN = -999,  /* 0xfffffc19 */
};

/* Field width bounds (CMP EBX,1 early-out / CMP EBX,5 clamp). */
enum {
    CG_HUDELEM_MIN_WIDTH = 1,
    CG_HUDELEM_MAX_WIDTH = 5,
};

/* The minus sign occupies the last cg_numberShaders[] slot (MOV EAX,0xa on '-'). */
enum { CG_NUMBER_SHADER_MINUS = 10 };

/* Right/center-justify pen bias: MOV EAX,0xfffffffe (-2) minus glyphs*charWidth. */
enum { CG_HUDELEM_JUSTIFY_BIAS = -2 };

int CG_DrawField(int width /*ECX*/,
                 int x /*arg0*/, int y /*arg1*/, int value /*arg2*/,
                 int charWidth /*arg3*/, int charHeight /*arg4*/,
                 int drawGlyphs /*arg5*/, int justify /*arg6*/)
{
    /* CMP EBX,1 / JGE: a sub-1 width has no field to draw. */
    if (width < CG_HUDELEM_MIN_WIDTH) {
        return 0;
    }

    /* CMP EBX,5 / JLE / MOV EBX,5: never draw more than five characters. */
    if (width > CG_HUDELEM_MAX_WIDTH) {
        width = CG_HUDELEM_MAX_WIDTH;
    }

    /* Clamp the value to what fits in `width` digits. Switch on (width-1);
     * width 5 (default) is left unclamped. */
    switch (width - 1) {
    case 0: /* width 1: [0, 9] */
        if (value > CG_HUDELEM_W1_MAX) {
            value = CG_HUDELEM_W1_MAX;
        }
        /* SETL CL / DEC ECX / AND EAX,ECX: negatives become 0, non-negatives kept. */
        if (value < 0) {
            value = 0;
        }
        break;
    case 1: /* width 2: [-9, 99] */
        if (value > CG_HUDELEM_W2_MAX) {
            value = CG_HUDELEM_W2_MAX;
        } else if (value < CG_HUDELEM_W2_MIN) {
            value = CG_HUDELEM_W2_MIN;
        }
        break;
    case 2: /* width 3: [-99, 999] */
        if (value > CG_HUDELEM_W3_MAX) {
            value = CG_HUDELEM_W3_MAX;
        } else if (value < CG_HUDELEM_W3_MIN) {
            value = CG_HUDELEM_W3_MIN;
        }
        break;
    case 3: /* width 4: [-999, 9999] */
        if (value > CG_HUDELEM_W4_MAX) {
            value = CG_HUDELEM_W4_MAX;
        } else if (value < CG_HUDELEM_W4_MIN) {
            value = CG_HUDELEM_W4_MIN;
        }
        break;
    default: /* width 5: no clamp */
        break;
    }

    /* Format the clamped value (0x30017c7a: Com_sprintf(text, 16, "%i", value)). */
    char text[CG_HUDELEM_NUM_BUFSIZE];
    Com_sprintf(text, CG_HUDELEM_NUM_BUFSIZE, CG_HUDELEM_NUM_FORMAT, value);

    /* strlen(text), then cap the glyph count at `width` (0x30017c84..0x30017c96). */
    int glyphs = 0;
    {
        const char *p = text;
        while (*p != '\0') {
            ++p;
        }
        glyphs = (int)(p - text);
    }
    if (glyphs > width) {
        glyphs = width;
    }

    /* Right/center-justify: when `justify` is 0, bias the starting pen left by
     * (2 + glyphs*charWidth) (0x30017c9e..0x30017cb2). A non-zero flag = left-justify. */
    int pen = x;
    if (justify == 0) {
        uint32_t glyphSpan = (uint32_t)glyphs * (uint32_t)charWidth;
        int32_t adjustment = coduo_int32_from_bits(
            (uint32_t)CG_HUDELEM_JUSTIFY_BIAS - glyphSpan);
        pen = coduo_int32_from_bits((uint32_t)pen + (uint32_t)adjustment);
    }

    /* The return value is this justify-adjusted starting pen (EBX = [ESP+0x48] latched
     * at 0x30017cbc, returned via MOV EAX,EBX at 0x30017da7); the per-glyph advance
     * below runs on a separate cursor and does not change it. */
    int startPen = pen;

    /* Draw each formatted character as its own stretched glyph, advancing the cursor
     * by charWidth per character (loop 0x30017cd0). The loop only runs while the string
     * is non-empty and the glyph budget remains; an empty string draws nothing
     * (0x30017cc4 TEST text[0]; JZ end). */
    const char *cp = text;
    if (*cp != '\0') {
        while (glyphs != 0) {
            int glyphIndex;
            int8_t c = (int8_t)*cp;
            if (c == '-') {
                glyphIndex = CG_NUMBER_SHADER_MINUS;
            } else {
                /* MOVSX EAX,AL; SUB EAX,'0' — signed digit-to-index. */
                glyphIndex = (int)c - '0';
            }

            /* drawGlyphs != 0 (0x30017cef TEST/JZ) gates the actual pic emission; when
             * 0 the pen still advances but no glyph is drawn (layout-only pass). */
            if (drawGlyphs != 0) {
                /* Convert the integer virtual-screen rect to device pixels; the texture
                 * spans the whole glyph (s1=0, t1=0, s2=1, t2=1). Coordinate/scale
                 * order proven at 0x30017cf7..0x30017d76. Each int is FILDed straight
                 * into its FMUL by the screen scale (no intervening float store: e.g.
                 * 0x30017cf7 FILD; 0x30017d0f FMUL), so it stays exact in 80-bit -- no
                 * (float) casts, which would round the integers first. */
                float px = (float)((long double)pen *
                                   (long double)cgs_screenXScale);
                float py = (float)((long double)y *
                                   (long double)cgs_screenYScale);
                float pw = (float)((long double)charWidth *
                                   (long double)cgs_screenXScale);
                float ph = (float)((long double)charHeight *
                                   (long double)cgs_screenYScale);

                cgame_syscall(CG_R_DRAWSTRETCHPIC,
                              CG_FloatBits(px), CG_FloatBits(py),
                              CG_FloatBits(pw), CG_FloatBits(ph),
                              CG_FloatBits(0.0f), CG_FloatBits(0.0f),
                              CG_FloatBits(1.0f), CG_FloatBits(1.0f),
                              cg_numberShaders[glyphIndex]);
            }

            /* Advance the pen and step to the next character (0x30017d8e..0x30017d9a);
             * the loop stops on NUL or when the glyph budget is exhausted. */
            pen = coduo_int32_from_bits((uint32_t)pen + (uint32_t)charWidth);
            ++cp;
            --glyphs;
            if (*cp == '\0') {
                break;
            }
        }
    }

    return startPen;
}
