#include "../client_recovered.h"
#include "../globals.h"

#include <stddef.h>
#include <string.h>

// Source: uo_cgame_mp_x86.dll 0x3001c5d0..0x3001c855
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3001c5d0_3001c855.mcode
//
// CG_FilledBar — draw a 2D "filled bar" (a progress/health/ammo bar) in the virtual
// 640x480 UI space by issuing one or more CG_FillRect draws (0x3001c4e0). The bar
// occupies (x, y, width, height); `frac` (0..1) sets how much of it is filled. Flags
// (EBX) choose orientation, an optional background fill, the fill anchor, the color
// source, and whether the global HUD alpha fade (cg_hudAlpha_vmCvar.value) is applied.
//
// Register-argument ABI (proven from these bytes and both call sites 0x3001c860 /
// 0x300303a0): flags in EBX, fillColor in ECX, color3 in EDX, and
// (x, y, width, height, borderColor, frac) pushed on the stack; the caller cleans
// the 6 stack dwords (ADD ESP,0x18). Modeled here as an ordinary C signature; the
// EBX/ECX/EDX register passing and callee ESI save/restore are i386 ABI details.
//
// The .mcode pre-hint "CG_CalcMuzzlePoint" (a size-match guess, win 0x285/matched
// 0x284) is REJECTED: this function computes no muzzle point. It draws a 2D bar via
// the CG_FillRect (0x3001c4e0 = trap_R_SetColor + trap_R_DrawStretchPic + reset)
// helper, and the two globals it/its helper touch are already resolved
// (cg_hudAlpha_vmCvar.value at 0x304583c8; cgs_media_whiteShader used only inside
// CG_FillRect). The name CG_FilledBar comes from the same-module PPC bank and the
// proven behavior.
//
// Machine-code facts anchored below:
//  - 0x3001c5d3..0x3001c614: copy *borderColor (the stack color[4]) into local
//    `color`; the filled span is drawn in this color unless BLEND_COLOR3 is set.
//  - 0x3001c5f4..0x3001c635: init local `fill` = {1,1,1,0.25} (0x3f800000 x3,
//    0x3e800000 = 0.25f), overridden from *fillColor when FILLCOLOR set and
//    fillColor != NULL. This is the background-fill color.
//  - 0x3001c639..0x3001c666: unless NO_ALPHA_FADE, multiply the alpha (index 3) of
//    color, color3 (if non-NULL), and fill by cg_hudAlpha_vmCvar.value (0x304583c8, FLD).
//  - 0x3001c66a..0x3001c6c5: if BLEND_COLOR3, blend[i] = (1-frac)*color[i] +
//    frac*color3[i] for i=0..3 (x87 order verified).
//  - 0x3001c6c9..0x3001c746: if FILLCOLOR, draw the background fill over
//    (x,y,width,height) with `fill`, then (unless NO_INSET) inset the rect for the
//    filled span: INSET_VERT -> y+=3, height-=6 (0x3007be5c=3.0f, 0x3007bddc=6.0f);
//    else x+=1, y+=1, width-=2, height-=2 (0x3007bce0=1.0f, 0x3007bce4=2.0f).
//  - 0x3001c74a..0x3001c854: draw the filled span. VERTICAL varies height, else
//    width; ANCHOR_END offsets origin by (1-frac)*length, ANCHOR_CENTER by
//    (1-frac)*length*0.5 (0x3007bce8=0.5f); the span color is `blend` when
//    BLEND_COLOR3, else `color`.

void CG_FilledBar(int flags,
                  const float *fillColor,
                  float *color3,
                  float x,
                  float y,
                  float width,
                  float height,
                  const float *borderColor,
                  float frac)
{
    /* Local color[4] = a copy of the stack borderColor, made first thing
     * (0x3001c5d3..0x3001c614). This is the base color for the filled span. */
    vec4_t color;
    uint32_t color2Bits;
    memcpy(&color[0], &borderColor[0], sizeof(color[0]));
    memcpy(&color[1], &borderColor[1], sizeof(color[1]));
    memcpy(&color2Bits, &borderColor[2], sizeof(color2Bits));
    memcpy(&color[3], &borderColor[3], sizeof(color[3]));

    /* Local fill[4] = background-fill color, default {1,1,1,0.25}
     * (0x3001c5f4..0x3001c60c). Overridden by *fillColor only when the FILLCOLOR
     * flag is set AND fillColor is non-NULL (0x3001c5ef AND 0x10; 0x3001c61a
     * TEST ECX,ECX). */
    vec4_t fill = { 1.0f, 1.0f, 1.0f, 0.25f };
    /* 0x3001c614 delays the already-loaded blue word until after the default
     * fill object has been initialized. */
    memcpy(&color[2], &color2Bits, sizeof(color[2]));
    if ((flags & CG_FILLEDBAR_FILLCOLOR) != 0 && fillColor != NULL) {
        memcpy(&fill[0], &fillColor[0], sizeof(fill[0]));
        memcpy(&fill[1], &fillColor[1], sizeof(fill[1]));
        memcpy(&fill[2], &fillColor[2], sizeof(fill[2]));
        memcpy(&fill[3], &fillColor[3], sizeof(fill[3]));
    }

    /* Alpha fade (0x3001c639 TEST BL,0x8; skipped when NO_ALPHA_FADE set): scale
     * the alpha channel of color, color3 (if present), and fill by the global HUD
     * fade factor cg_hudAlpha_vmCvar.value (0x304583c8). color3's alpha is modified in
     * place through the caller's pointer, exactly as the machine code does. */
    if ((flags & CG_FILLEDBAR_NO_ALPHA_FADE) == 0) {
        qboolean color3Present = color3 != NULL;           /* 0x3001c63e TEST EDX */
        color[3] = (float)((long double)cg_hudAlpha_vmCvar.value *
                           (long double)color[3]);            /* 0x3001c640 */
        if (color3Present) {
            color3[3] = (float)((long double)cg_hudAlpha_vmCvar.value *
                                (long double)color3[3]); /* 0x3001c650: *(EDX+0xc) */
        }
        fill[3] = (float)((long double)cg_hudAlpha_vmCvar.value *
                          (long double)fill[3]);               /* 0x3001c65c */
    }

    /* Optional frac-blend of the span color between the base color and color3
     * (0x3001c66a AND ESI,0x100). blend[i] = frac*color3[i] + (1-frac)*color[i].
     * Computed for all four components; used as the span color below.
     *
     * Float faithfulness: (1.0f - frac) is computed ONCE at 0x3001c674/0x3001c67a
     * and kept in an x87 register across all four components (re-copied via
     * FLD ST1 at c684/c697/c6aa, FXCH at c6bd for the last use); it is never
     * stored, so it is NOT rounded to float -- a `float inv` local would add a
     * rounding the DLL does not perform. The only roundings in this block are the
     * four FSTP DWORDs at 0x3001c68c/c69f/c6b2/c6c5 (one per blend[i]).
     * FADDP computes st1+st0 = (frac*color3[i]) + ((1-frac)*color[i]); the
     * frac*color3[i] term is the left addend, so that order is preserved here. */
    vec4_t blend;
    if ((flags & CG_FILLEDBAR_BLEND_COLOR3) != 0) {
        long double inverse = 1.0L - (long double)frac;
        blend[0] = (float)((long double)frac * (long double)color3[0] +
                           inverse * (long double)color[0]); /* 0x3001c67e..0x3001c68c */
        blend[1] = (float)((long double)frac * (long double)color3[1] +
                           inverse * (long double)color[1]); /* 0x3001c690..0x3001c69f */
        blend[2] = (float)((long double)frac * (long double)color3[2] +
                           inverse * (long double)color[2]); /* 0x3001c6a3..0x3001c6b2 */
        blend[3] = (float)((long double)frac * (long double)color3[3] +
                           inverse * (long double)color[3]); /* 0x3001c6b6..0x3001c6c5 */
    }

    /* Optional background fill (0x3001c6c9 TEST EAX,EAX, EAX = flags & 0x10). When
     * drawn, fill the whole rect first, then inset the rect for the filled span. */
    if ((flags & CG_FILLEDBAR_FILLCOLOR) != 0) {
        CG_FillRect(x, y, width, height, fill);        /* 0x3001c6cd..0x3001c6eb */

        if ((flags & CG_FILLEDBAR_NO_INSET) == 0) {    /* 0x3001c6ee TEST BL,0x40 */
            if ((flags & CG_FILLEDBAR_INSET_VERT) != 0) { /* 0x3001c6f3 TEST BL,0x20 */
                y = (float)((long double)y + 3.0f);     /* 0x3001c6f8 FADD 0x3007be5c */
                height = (float)((long double)height - 6.0f); /* 0x3001c706 FSUB 0x3007bddc */
            } else {
                x = (float)((long double)x + 1.0f);     /* 0x3001c712 FADD 0x3007bce0 */
                y = (float)((long double)y + 1.0f);     /* 0x3001c720 FADD 0x3007bce0 */
                width = (float)((long double)width - 2.0f); /* 0x3001c72e FSUB 0x3007bce4 */
                height = (float)((long double)height - 2.0f); /* 0x3001c73c FSUB 0x3007bce4 */
            }
        }
    }

    /* Color used to draw the filled span: the frac-blend when BLEND_COLOR3, else the
     * base color. Both tail paths pass either &blend or &color to CG_FillRect. */
    const float *spanColor =
        ((flags & CG_FILLEDBAR_BLEND_COLOR3) != 0) ? blend : color;

    if ((flags & CG_FILLEDBAR_VERTICAL) != 0) {
        /* Vertical bar: filled length runs along HEIGHT (0x3001c74a JZ not taken).
         * ANCHOR_END/ANCHOR_CENTER shift y so the span is bottom/center anchored. */
        if ((flags & CG_FILLEDBAR_ANCHOR_END) != 0) {          /* 0x3001c74f */
            y = (float)((long double)y +
                        (1.0L - (long double)frac) *
                        (long double)height);                    /* 0x3001c754..0x3001c781 */
        } else if ((flags & CG_FILLEDBAR_ANCHOR_CENTER) != 0) { /* 0x3001c764 */
            y = (float)((long double)y +
                        (1.0L - (long double)frac) *
                        (long double)height * 0.5L);             /* 0x3001c769..0x3001c781 */
        }
        float spanHeight = (float)((long double)height *
                                   (long double)frac);
        CG_FillRect(x, y, width, spanHeight, spanColor);        /* 0x3001c785..0x3001c83e */
    } else {
        /* Horizontal bar: filled length runs along WIDTH (0x3001c7c9). */
        if ((flags & CG_FILLEDBAR_ANCHOR_END) != 0) {          /* 0x3001c7c9 */
            x = (float)((long double)x +
                        (1.0L - (long double)frac) *
                        (long double)width);                     /* 0x3001c7ce..0x3001c7fb */
        } else if ((flags & CG_FILLEDBAR_ANCHOR_CENTER) != 0) { /* 0x3001c7de */
            x = (float)((long double)x +
                        (1.0L - (long double)frac) *
                        (long double)width * 0.5L);              /* 0x3001c7e3..0x3001c7fb */
        }
        float spanWidth = (float)((long double)width *
                                  (long double)frac);
        CG_FillRect(x, y, spanWidth, height, spanColor);        /* 0x3001c7ff..0x3001c854 */
    }
}
