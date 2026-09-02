// Source: uo_cgame_mp_x86.dll 0x30036d60..0x30036e46
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30036d60_30036e46.mcode

#include "client/cgame/client_recovered.h"
#include "client/cgame/globals.h"

#include <stdint.h>

/*
 * CG_DrawScoreboard_ListColumnHeaders (0x30036d60)
 *
 * Draws the scoreboard column-header row: for each of the five entries in the
 * static column table cg_scoreboardColumns[5] (.rdata @ 0x30071a64), if the
 * column has a header label it localizes the label via CG_SafeTranslateString_Internal and
 * emits a 2D text draw (cgame trap 54) at the column's running X cursor. Columns
 * whose header reference is the empty string (the two leftmost name columns)
 * draw nothing but still advance the X cursor. Returns the baseline Y for the row
 * below the headers (startY + 14.0f) on the x87 stack.
 *
 * NAME: the .mcode size-matched guess "G_CalcTagAxis" is REJECTED. G_CalcTagAxis
 * is a server bone/tag orientation-matrix routine; it does no drawing, no string
 * localization, and no scoreboard table walk. This function walks a scoreboard
 * column table, calls CG_SafeTranslateString_Internal("cgame", ref) on each localized header
 * key ("CGAME_SB_SCORE"/"CGAME_SB_DEATHS"/"CGAME_SB_PING"), and issues text
 * draws through cgame_syscall — pure HUD scoreboard code. The name matched only
 * by identical byte size (0xe6), which the contract forbids as evidence. The
 * behavioral name CG_DrawScoreboard_ListColumnHeaders comes from the same-module
 * (cgame_mp) PPC bank, matched by behavior (column-header table + localized
 * refs + trap draws), not by RVA or size; exact CoD symbol otherwise unproven.
 *
 * ABI (proven from the call site at 0x30037bde):
 *   - startY  : float, cdecl stack arg0 (F+0x24). Caller pushes it via FSTP [ESP].
 *   - widthScale : float, cdecl stack arg1 (F+0x28) = board pixel width used to
 *                  turn each column's widthFraction into a pixel advance.
 *   - colorPtr : an incoming register argument in EBX (LEA EBX,[ESP+0x20] at the
 *                caller; a pointer to a vec4 color, forwarded verbatim to trap 54).
 *                EBX is never saved/restored here, confirming it is an incoming
 *                register arg, not a callee-save. Modeled as an opaque int32 word
 *                (the trap takes it as a raw dword, matching the CG_R_TEXT_PAINT family).
 *   - returns : float (startY + 14.0f), left on the x87 stack (FADD [0x3007c0a0]
 *               = 14.0f just before RET). ADD ESP,0x8 at the caller cleans the two
 *               stack args (caller-cleaned cdecl; plain RET here).
 *
 * X-CURSOR accumulation (F+0x8, a stack local): initialized to 129.0f
 * (0x43010000). Each iteration, after (optionally) drawing, advances by
 *   xCursor += widthScale * cg_scoreboardColumns[i].widthFraction
 * The tail reads the width via `FMUL [ESI+0x30071a54]` with ESI already
 * post-incremented, i.e. base-0x10 + (i+1)*0x10 == the CURRENT entry's
 * widthFraction at 0x30071a64 + i*0x10 — the same field the mode==2 body reads
 * via `FMUL [ESI+0x30071a64]`. Reproduced as the current entry's widthFraction.
 *
 * PER-COLUMN body:
 *   - ref = cg_scoreboardColumns[i].headerRef; if ref[0]==0 skip the draw.
 *   - str = CG_SafeTranslateString_Internal("cgame", ref)   (EAX="cgame", ECX=ref register ABI)
 *   - if mode == 2 (CG_SB_COLUMN_MODE_MEASURED):
 *       textWidth = cgame_syscall(CG_R_TEXT_WIDTH, str, 0, bits(0.3f), 0)  [int32]
 *       xInCell   = widthScale * widthFraction - (float)textWidth   [FMUL; FISUB]
 *     else (mode != 2): xInCell = 0.0f   (FLD [0x3007bcec] = 0.0f)
 *   - drawX = xInCell + xCursor
 *   - drawY = startY + 10.5f            (FLD arg0; FADD [0x3007c010]=10.5f)
 *   - cgame_syscall(CG_R_TEXT_PAINT, bits(drawX), bits(drawY), 0, bits(0.3f),
 *                   colorPtr, str, 0, 0, 3)
 *
 * FLOAT precision: all coordinate math is single-precision on the x87 stack
 * (FLD/FMUL/FADD/FSUB of float ptr, FISUB/FILD of int), stored via FSTP to
 * 4-byte slots and forwarded to the variadic traps as raw 32-bit words. Modeled
 * as `float` and forwarded through CG_FloatBits so the bit pattern is exact (no
 * double promotion), matching the CG_R_TEXT_PAINT emitter family. FISUB at 0x30036dbc
 * subtracts the INTEGER trap-52 return converted to float: st0 - (float)textWidth.
 *
 * The two float literals 0.3f appear as raw immediates 0x3e99999a stashed to
 * stack scratch then loaded (MOV [ESP+..],0x3e99999a) — the trap's variadic float
 * arguments. Constants: 129.0f (0x43010000), 10.5f (.rdata 0x3007c010),
 * 14.0f (.rdata 0x3007c0a0), 0.0f (.rdata 0x3007bcec).
 */

/* Domain passed to CG_SafeTranslateString_Internal for every scoreboard header key
 * ("cgame", .rdata 0x30077b28; EAX at 0x30036d7f). */
#define CG_SB_LOCALIZATION_DOMAIN "cgame"

/* Initial X cursor for the header row (MOV [ESP+0x8],0x43010000). */
#define CG_SB_HEADER_X_START 129.0f

/* Vertical offset of the header baseline from startY (FADD [0x3007c010]). */
#define CG_SB_HEADER_Y_OFFSET 10.5f

/* Row-advance returned to the caller (FADD [0x3007c0a0] before RET). */
#define CG_SB_HEADER_ROW_ADVANCE 14.0f

/* Variadic float argument forwarded to both traps (0x3e99999a = 0.3f). */
#define CG_SB_HEADER_TRAP_FLOAT 0.3f

/* Fixed trailing mode word in the trap-54 draw vector (PUSH 0x3). */
enum {
    CG_SB_HEADER_TRAP54_MODE = 3
};

float CG_DrawScoreboard_ListColumnHeaders(float startY, float widthScale, const vec4_t color)
{
    /* Running left-edge cursor for the current column (stack local F+0x8). */
    float xCursor = CG_SB_HEADER_X_START;
    int32_t i;

    for (i = 0; i < CG_SCOREBOARD_COLUMN_COUNT; ++i) {
        const cgScoreboardColumn_t *col = CG_SCOREBOARD_COLUMN(i);

        /* 0x30036d70 MOV ECX,[ESI+headerRef]; 0x30036d76 CMP byte[ECX],0; JZ tail.
         * Empty header reference => draw nothing, only advance the cursor. */
        if (col->headerRef[0] != 0) {
            /* 0x30036d84 CALL CG_SafeTranslateString_Internal("cgame", ref)  (EAX=domain,
             * ECX=ref). Returns a pointer into the shared translate buffer. */
            const char *str = CG_SafeTranslateString_Internal(CG_SB_LOCALIZATION_DOMAIN, col->headerRef);

            /* In-cell horizontal offset before the running cursor. Neither branch
             * stores it: the measured branch leaves FMUL/FISUB's result in st(0)
             * (0x30036dbc, no FSTP) and the zero branch FLDs 0.0f (0x30036dc5), and
             * both fall into the shared FADD xCursor at 0x30036dee. The only
             * rounding is drawX's FSTP DWORD at 0x30036e00 -- hence long double. */
            long double xInCell;

            /* 0x30036d8b CMP [ESI+mode],2; JNZ -> the zero-offset branch. */
            if (col->mode == CG_SB_COLUMN_MODE_MEASURED) {
                /* 0x30036da8 trap 52: measure the localized string's pixel width.
                 * Args (push order): str, 0, bits(0.3f), 0. Returns int32 width. */
                int32_t textWidth = coduo_int32_from_bits(
                    (uint32_t)cgame_syscall(CG_R_TEXT_WIDTH, (intptr_t)str, 0, CG_FloatBits(CG_SB_HEADER_TRAP_FLOAT), 0));

                /* 0x30036dae FLD arg1(widthScale); 0x30036db2 FMUL widthFraction;
                 * 0x30036dbc FISUB dword (int)textWidth => right-aligned in-cell x.
                 * FISUB is an INTEGER subtract, so textWidth is NOT rounded to float
                 * first (no (float) cast; the int stays exact in 80-bit as x87 does). */
                xInCell = (long double)widthScale * (long double)col->widthFraction - (long double)textWidth;
            } else {
                /* 0x30036dc5 FLD [0x3007bcec] = 0.0f. */
                xInCell = 0.0f;
            }

            /* 0x30036dcb FLD arg0(startY); 0x30036dd1 FADD [0x3007c010]=10.5f. */
            float drawY = (float)((long double)startY + (long double)CG_SB_HEADER_Y_OFFSET);

            /* 0x30036dee FADD [ESP+0x8] => in-cell offset plus running cursor. */
            float drawX = xInCell + xCursor;

            /* 0x30036e0f trap 54: 10-slot draw vector.
             * (54, bits(drawX), bits(drawY), 0, bits(0.3f), colorPtr, str, 0, 0, 3) */
            cgame_syscall(CG_R_TEXT_PAINT, CG_FloatBits(drawX), CG_FloatBits(drawY), 0, CG_FloatBits(CG_SB_HEADER_TRAP_FLOAT),
                          (intptr_t)color, (intptr_t)str, 0, 0, CG_SB_HEADER_TRAP54_MODE);
        }

        /* 0x30036e18 FLD arg1(widthScale); 0x30036e22 FMUL widthFraction (current
         * entry, via post-incremented index / base-0x10 displacement);
         * 0x30036e28 FADD xCursor; FSTP xCursor. */
        xCursor = (float)((long double)widthScale * (long double)col->widthFraction + (long double)xCursor);
    }

    /* 0x30036e36 FLD arg0(startY); 0x30036e3b FADD [0x3007c0a0]=14.0f; RET (st0). */
    return startY + CG_SB_HEADER_ROW_ADVANCE;
}
