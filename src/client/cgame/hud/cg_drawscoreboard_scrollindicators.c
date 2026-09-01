// Source: uo_cgame_mp_x86.dll 0x300378b0..0x30037b45
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_300378b0_30037b45.mcode
//
// CG_DrawScoreboard_ScrollIndicators — draw the multiplayer scoreboard's vertical
// scrollbar down the right edge when the collected row list overflows the visible
// window. Drawn by CG_DrawScoreboardBody (0x30037b50) only when it decided a
// scrollbar column is needed. Four elements:
//   1. a dimmed "black" track bar (trap_R_DrawStretchPic) spanning the window,
//   2. a dimmer "white" proportional thumb (trap_R_DrawStretchPic) whose position
//      tracks cg_scoreboardScrollPos and whose height tracks the visible fraction,
//   3. an up-arrow + up-key glyph pair (CG_DrawPic) at the top, only when scrolled
//      down from the top (cg_scoreboardScrollPos > 0),
//   4. a down-arrow + down-key glyph pair (CG_DrawPic) at the bottom, only when more
//      rows remain below (visibleLineCount < lineCount).
//
// NAME: the .mcode size-match guess "Item_TextField_Paint" is REJECTED (win size
// 0x295 ~ some PPC 0x294 with zero behavioral basis; see memory
// size-match-name-is-noise). This draws the scoreboard scroll indicators — it
// registers "black"/"white" and the hudScoreboardScroll_* arrow/key shaders and
// stretch-draws them; it is not a UI text-field widget. The behavioral name
// CG_DrawScoreboard_ScrollIndicators is the role already used by the caller's
// reconstruction and the client_recovered.h forward decl; exact CoD symbol unproven.
//
// ABI (proven from this function's own bytes and the sole caller at 0x30037d62):
//   - color  : ESI register arg, pointer to the {r,g,b,alpha} 2D draw color vec4.
//              r/g/b (color[0..2]) are copied verbatim into a local color; alpha
//              (color[3], read as [ESI+0xc]) is scaled per element. ESI is never
//              saved/restored => incoming register arg.
//   - topY   : first cdecl stack arg (the caller's ECX). Header-baseline Y where the
//              track top and the up glyphs sit.
//   - lineCount        : second cdecl stack arg (the caller's EDI). Total collected
//              line count; the caller cleans these two stack args (ADD ESP,8).
//   - visibleLineCount : EBX register arg (the caller's running line cursor). Never
//              saved/restored => incoming register arg. Drives thumb height and the
//              down-glyph visibility.
//   No return; bare RET after the frame teardown (ADD ESP,0x1c).
//
// COORDINATES: every draw is in virtual-640x480 space rescaled to the physical
// framebuffer by cgs_screenXScale (x/width) and cgs_screenYScale (y/height), the
// standard CG_AdjustFrom640 rescale that trap_R_DrawStretchPic expects. CG_DrawPic
// (0x3001caa0) performs that same rescale internally, so the arrow/key glyphs pass
// their virtual coordinates directly. Every coordinate is single-precision x87 math
// forwarded to the variadic trap as a raw 32-bit word via CG_FloatBits.
//
// FLOAT constants (dumped exact via objdump -s -j .rdata):
//   0x3007bce0 = 0x3f800000 = 1.0f   0x3007bce4 = 0x40000000 = 2.0f
//   0x3007bce8 = 0x3f000000 = 0.5f   0x3007be58 = 0x3e800000 = 0.25f
//   0x3007bdc0 = 0x43d80000 = 432.0f (visible window height)
//   0x3007be08 = 0x41000000 = 8.0f   (track width)
//   0x3007bddc = 0x40c00000 = 6.0f   (thumb width)
//   0x3007c00c = 0x43fc8000 = 505.0f (track x)
//   0x3007c008 = 0x43fd0000 = 506.0f (thumb x)
//   0x3007bf10 = 0x41900000 = 18.0f  (up-key x offset from track)
//   immediates: 0x41800000 = 16.0f (glyph w/h), 0x44024000 = 521.0f (glyph x),
//               0x43cf8000 = 415.0f (down-arrow y), 0x43c68000 = 397.0f (down-key y).

#include "client/cgame/globals.h"
#include "client/cgame/client_recovered.h"

/* Visible scoreboard window height in virtual pixels (0x3007bdc0). */
#define CG_SB_SCROLL_WINDOW_HEIGHT 432.0f

/* Track (background bar) geometry. */
#define CG_SB_SCROLL_TRACK_X       505.0f  /* 0x3007c00c */
#define CG_SB_SCROLL_TRACK_WIDTH     8.0f  /* 0x3007be08 */
/* The track spans from topY to the window bottom, less a 1px top inset. */
#define CG_SB_SCROLL_TOP_INSET       1.0f  /* 0x3007bce0 */

/* Thumb (proportional handle) geometry. */
#define CG_SB_SCROLL_THUMB_X       506.0f  /* 0x3007c008 */
#define CG_SB_SCROLL_THUMB_WIDTH     6.0f  /* 0x3007bddc */
/* Thumb travel/height is measured in the track less this 2px end margin. */
#define CG_SB_SCROLL_THUMB_MARGIN    2.0f  /* 0x3007bce4 */

/* Alpha scale applied to the incoming color: dim for the track, dimmer for the
 * thumb, full for the arrow/key glyphs. */
#define CG_SB_SCROLL_TRACK_ALPHA   0.5f    /* 0x3007bce8 */
#define CG_SB_SCROLL_THUMB_ALPHA   0.25f   /* 0x3007be58 */

/* Arrow/key glyph geometry (fixed virtual pixels). */
#define CG_SB_SCROLL_GLYPH_X       521.0f  /* 0x44024000 */
#define CG_SB_SCROLL_GLYPH_SIZE     16.0f  /* 0x41800000 */
#define CG_SB_SCROLL_UPKEY_DY       18.0f  /* 0x3007bf10: up-key below up-arrow */
#define CG_SB_SCROLL_DOWNARROW_Y   415.0f  /* 0x43cf8000 */
#define CG_SB_SCROLL_DOWNKEY_Y     397.0f  /* 0x43c68000 */

/* sort/precache class passed to the register-shader trap for every element. */
enum { CG_SB_SCROLL_SHADER_SORT = 5 };

void CG_DrawScoreboard_ScrollIndicators(const vec_t *color, float topY,
                                        int lineCount, int visibleLineCount)
{
    /* 0x300378b3..0x300378cb: copy r/g/b out of the incoming color; the alpha slot
     * is filled per element below. This mirrors the machine's stack-local color. */
    vec4_t drawColor;
    drawColor[0] = color[0];
    drawColor[1] = color[1];
    drawColor[2] = color[2];

    /* ---- 1. black track bar --------------------------------------------------
     * 0x300378cf..0x300378fd: pump the loading HUD, register "black", set the 2D
     * color to {r,g,b, alpha*0.5}. */
    CG_DrawInformation(0);
    qhandle_t blackShader = coduo_int32_from_bits(
        (uint32_t)cgame_syscall(CG_R_REGISTERSHADER,
                                (intptr_t)cg_blackMaterialName,
                                CG_SB_SCROLL_SHADER_SORT));
    drawColor[3] = (float)((long double)color[3] *
                           (long double)CG_SB_SCROLL_TRACK_ALPHA);
    trap_R_SetColor(drawColor);

    /* 0x300378ff..0x3003795d: draw the track. The track height (in virtual pixels)
     * is kept on the x87 stack across the draw call and reused below as the thumb
     * travel range, so it is computed once here.
     *   trackHeight = 432 - topY - 1     (window bottom, less the 1px top inset)
     *   x = 505, y = topY, w = 8, h = trackHeight   (each rescaled by screen scale)
     * texcoords (0,0)-(1,1). */
    /* 0x300378ff FLD 432.0f; FSUB topY; 0x30037912 FSUB 1.0f -- the result is NEVER
     * stored to a float slot: it stays live in st(0) across the draw call below and
     * is re-consumed unrounded by thumbHeight at 0x3003797a. Hence long double. */
    long double trackHeight =
        (long double)CG_SB_SCROLL_WINDOW_HEIGHT - (long double)topY -
        (long double)CG_SB_SCROLL_TOP_INSET;
    trap_R_DrawStretchPic(
        CG_FloatBits((float)((long double)cgs_screenXScale *
                             (long double)CG_SB_SCROLL_TRACK_X)),
        CG_FloatBits((float)((long double)cgs_screenYScale *
                             (long double)topY)),
        CG_FloatBits((float)((long double)cgs_screenXScale *
                             (long double)CG_SB_SCROLL_TRACK_WIDTH)),
        CG_FloatBits((float)((long double)cgs_screenYScale * trackHeight)),
        CG_FloatBits(0.0f), CG_FloatBits(0.0f),
        CG_FloatBits(1.0f), CG_FloatBits(1.0f),
        (int32_t)blackShader);

    /* ---- thumb geometry (computed before the white draw) ---------------------
     * 0x30037962..0x300379be. Base position and full-travel extent:
     *   thumbY     = topY + 1                (thumb top, at the track's top inset)
     *   thumbHeight = trackHeight - 2        (full track less the 2px end margin)
     * When scrolled down from the top, offset the thumb by the scroll fraction:
     *   if (scrollPos != 0 && lineCount != 0)
     *       thumbY += (scrollPos / lineCount) * thumbHeight
     * Then shrink the thumb to the visible fraction (only when there is a window to
     * show and more than one total line):
     *   if ((visibleLineCount - scrollPos) > 1 && lineCount > 1)
     *       thumbHeight = ((visibleLineCount - scrollPos) / lineCount) * thumbHeight
     * All ratios are integer/integer promoted to float (FILD/FIDIV) then scaled. */
    float thumbY = (float)((long double)topY +
                           (long double)CG_SB_SCROLL_TOP_INSET);
    float thumbHeight = (float)(trackHeight -
                                (long double)CG_SB_SCROLL_THUMB_MARGIN);

    if (cg_scoreboardScrollPos != 0 && lineCount != 0) {
        /* 0x3003798a FILD scrollPos; 0x30037990 FIDIV dword lineCount -- both operands
         * are integers fed straight into the FP divide, kept exact in 80-bit (no float
         * store). (long double)/int keeps that structure; (float) casts would round
         * each int first. Value-identical for the reachable line counts, but faithful. */
        thumbY = (float)(((long double)cg_scoreboardScrollPos /
                          (long double)lineCount) *
                         (long double)thumbHeight + (long double)thumbY);
    }

    int visibleRows = coduo_int32_from_bits((uint32_t)visibleLineCount -
                                       (uint32_t)cg_scoreboardScrollPos);
    if (visibleRows > 1 && lineCount > 1) {
        /* 0x300379b2 FILD visibleRows; 0x300379b6 FIDIV dword lineCount -- same exact
         * integer FP-divide as above; no (float) rounding of the operands. */
        thumbHeight = (float)(((long double)visibleRows /
                               (long double)lineCount) *
                              (long double)thumbHeight);
    }

    /* ---- 2. white thumb ------------------------------------------------------
     * 0x300379c2..0x30037a44: pump, register "white", set color {r,g,b, alpha*0.25},
     * draw the thumb at x=506, y=thumbY, w=6, h=thumbHeight (texcoords (0,0)-(1,1)). */
    CG_DrawInformation(0);
    qhandle_t whiteShader = coduo_int32_from_bits(
        (uint32_t)cgame_syscall(CG_R_REGISTERSHADER,
                                (intptr_t)cg_whiteMaterialName,
                                CG_SB_SCROLL_SHADER_SORT));
    drawColor[3] = (float)((long double)color[3] *
                           (long double)CG_SB_SCROLL_THUMB_ALPHA);
    trap_R_SetColor(drawColor);

    trap_R_DrawStretchPic(
        CG_FloatBits((float)((long double)cgs_screenXScale *
                             (long double)CG_SB_SCROLL_THUMB_X)),
        CG_FloatBits((float)((long double)cgs_screenYScale *
                             (long double)thumbY)),
        CG_FloatBits((float)((long double)cgs_screenXScale *
                             (long double)CG_SB_SCROLL_THUMB_WIDTH)),
        CG_FloatBits((float)((long double)cgs_screenYScale *
                             (long double)thumbHeight)),
        CG_FloatBits(0.0f), CG_FloatBits(0.0f),
        CG_FloatBits(1.0f), CG_FloatBits(1.0f),
        (int32_t)whiteShader);

    /* 0x30037a49..0x30037a5b: restore the draw color to the full incoming alpha for
     * the glyphs (no scale). */
    drawColor[3] = color[3];
    trap_R_SetColor(drawColor);

    /* ---- 3. up-arrow + up-key (only when scrolled down from the top) ----------
     * 0x30037a5d..0x30037ad5. Both glyphs are 16x16 at x=521. The up-arrow sits on
     * the header baseline (y=topY); the up-key sits 18px below it (y=topY+18). */
    if (cg_scoreboardScrollPos > 0) {
        CG_DrawInformation(0);
        qhandle_t upArrow = coduo_int32_from_bits(
            (uint32_t)cgame_syscall(CG_R_REGISTERSHADER,
                (intptr_t)cg_scoreboardScrollUpArrowMaterialName,
                CG_SB_SCROLL_SHADER_SORT));
        CG_DrawPic(CG_SB_SCROLL_GLYPH_X, topY,
                   CG_SB_SCROLL_GLYPH_SIZE, CG_SB_SCROLL_GLYPH_SIZE, upArrow);

        CG_DrawInformation(0);
        qhandle_t upKey = coduo_int32_from_bits(
            (uint32_t)cgame_syscall(CG_R_REGISTERSHADER,
                (intptr_t)cg_scoreboardScrollUpKeyMaterialName,
                CG_SB_SCROLL_SHADER_SORT));
        CG_DrawPic(CG_SB_SCROLL_GLYPH_X,
                   (float)((long double)topY +
                           (long double)CG_SB_SCROLL_UPKEY_DY),
                   CG_SB_SCROLL_GLYPH_SIZE, CG_SB_SCROLL_GLYPH_SIZE, upKey);
    }

    /* ---- 4. down-arrow + down-key (only when rows remain below) ---------------
     * 0x30037ad9..0x30037b3e (guard CMP EBX,EBP; JGE skip). Both glyphs are 16x16 at
     * x=521, at fixed bottom-of-window rows: the down-key at y=397, the down-arrow
     * just below it at y=415. */
    if (visibleLineCount < lineCount) {
        CG_DrawInformation(0);
        qhandle_t downArrow = coduo_int32_from_bits(
            (uint32_t)cgame_syscall(CG_R_REGISTERSHADER,
                (intptr_t)cg_scoreboardScrollDownArrowMaterialName,
                CG_SB_SCROLL_SHADER_SORT));
        CG_DrawPic(CG_SB_SCROLL_GLYPH_X, CG_SB_SCROLL_DOWNARROW_Y,
                   CG_SB_SCROLL_GLYPH_SIZE, CG_SB_SCROLL_GLYPH_SIZE, downArrow);

        CG_DrawInformation(0);
        qhandle_t downKey = coduo_int32_from_bits(
            (uint32_t)cgame_syscall(CG_R_REGISTERSHADER,
                (intptr_t)cg_scoreboardScrollDownKeyMaterialName,
                CG_SB_SCROLL_SHADER_SORT));
        CG_DrawPic(CG_SB_SCROLL_GLYPH_X, CG_SB_SCROLL_DOWNKEY_Y,
                   CG_SB_SCROLL_GLYPH_SIZE, CG_SB_SCROLL_GLYPH_SIZE, downKey);
    }
}
