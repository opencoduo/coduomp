// Source: uo_cgame_mp_x86.dll 0x30017aa0..0x30017ba6
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30017aa0_30017ba6.mcode

#include "../client_recovered.h"
#include "../globals.h"

#include <stdint.h>

/*
 * CG_DrawFieldWidth (0x30017aa0) — compute the pixel width of a fixed-width
 * numeric HUD field. Given a field digit-count `width` and an integer `value`, it
 * clamps `value` to the largest magnitude that fits in `width` characters, formats
 * it as signed decimal ("%i"), counts the resulting characters (never more than
 * `width`), and returns count * charWidth — the total advance the field occupies.
 *
 * Behavior proven from the machine code:
 *   - width (ECX -> EBX): if width < 1, return 0 immediately (0x30017ab9 JGE past the
 *     early-out); if width > 5, it is clamped to 5 (0x30017acc CMP 5 / MOV 5). A width
 *     of 5 skips clamping entirely (the switch below only handles 1..4).
 *   - value clamp switch on (width-1) via jump table at 0x30017ba8 (LEA ECX,[EBX-1];
 *     CMP ECX,3; JA default):
 *         width 1 -> value clamped to [0, 9]     (max 9; negatives forced to 0 via
 *                                                  SETL/DEC/AND at 0x30017aef)
 *         width 2 -> value clamped to [-9, 99]    (0x63 / 0xfffffff7)
 *         width 3 -> value clamped to [-99, 999]  (0x3e7 / 0xffffff9d)
 *         width 4 -> value clamped to [-999, 9999](0x270f / 0xfffffc19)
 *         width 5 -> no clamp
 *     These are the same per-digit-magnitude clamps CG_HudEmitDigits (0x30031300) uses
 *     for its 3-glyph field.
 *   - Com_sprintf(buffer, 16, "%i", value) formats the clamped value (0x3004e820; dest
 *     in EDI, size 16 in ESI, "%i" and value pushed and caller-cleaned via ADD ESP,8).
 *   - strlen(buffer) is computed inline (0x30017b63..0x30017b6e) then capped at `width`
 *     (CMP EAX,EBX / MOV EAX,EBX): count = min(strlen, width).
 *   - width accumulation (0x30017b76..0x30017b93): while there is another non-NUL
 *     character AND count != 0, add charWidth to the accumulator; return the sum.
 *     charWidth is reloaded from its stack slot each iteration but is loop-invariant,
 *     so the result is (number of formatted chars, capped at width) * charWidth.
 *
 * Register/stack ABI (proven from the body; no direct callers are visible because it is
 * dispatched through a HUD-element method table):
 *   ECX      = width      (first arg, thiscall-style register argument)
 *   arg0     = value      ([ESP+0x20] on entry after the two callee-saved pushes)
 *   arg1     = charWidth  ([ESP+0x24]); the per-character horizontal advance
 * Returns the accumulated field width in EAX. Both exits carry the MSVC /GS canary
 * boilerplate (MOV EAX,[__security_cookie] at entry, __security_check_cookie 0x30061639
 * at exit) — a compiler artifact with no source-level meaning.
 *
 * Name adjudication: the .mcode header's mechanical guess `script_method_player_setfatigue`
 * (a pure win-size 0x106 == matched-size 0x106 guess, mapping to the server bank's
 * void PlayerCmd_SetFatigue(uint32_t)) is REJECTED — this routine reads no player/entity
 * state, sets no fatigue, takes three arguments not one, and returns an int. It is a
 * numeric-field width measurement helper. The same-module Mac symbol
 * CG_HudElemStringWidth instead maps to the later 0x30029730 string measurer, so
 * this routine keeps the conservative role name CG_DrawFieldWidth. Its exact
 * original symbol remains unresolved.
 */

/* "%i" (0x300769e0 in .rdata): signed-decimal format for the numeric field. */
static const char CG_HUDELEM_NUM_FORMAT[] = "%i";

/* MOV ESI,0x10 -> Com_sprintf size argument (the value-text buffer size). */
enum { CG_HUDELEM_NUM_BUFSIZE = 16 };

/* Per-digit-count magnitude clamps (jump table at 0x30017ba8). */
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

int CG_DrawFieldWidth(int width, int value, int charWidth)
{
    char buffer[CG_HUDELEM_NUM_BUFSIZE];
    int count;
    int total;
    int i;

    /* CMP EBX,1 / JGE: a sub-1 width has no field to measure. */
    if (width < CG_HUDELEM_MIN_WIDTH) {
        return 0;
    }

    /* CMP EBX,5 / JLE / MOV EBX,5: never measure more than five characters. */
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

    /* Com_sprintf(buffer, 16, "%i", value): dest in EDI, size 16 in ESI. */
    Com_sprintf(buffer, CG_HUDELEM_NUM_BUFSIZE, CG_HUDELEM_NUM_FORMAT, value);

    /* Inline strlen, capped at `width` (CMP EAX,EBX / MOV EAX,EBX). */
    count = 0;
    while (buffer[count] != '\0') {
        count++;
    }
    if (count > width) {
        count = width;
    }

    /* Accumulate charWidth for each of the first `count` non-NUL characters
     * (loop 0x30017b76..0x30017b93: TEST DL / TEST EAX terminate). */
    total = 0;
    for (i = 0; buffer[i] != '\0' && count != 0; i++) {
        total = coduo_int32_from_bits((uint32_t)total + (uint32_t)charWidth);
        count--;
    }

    return total;
}
