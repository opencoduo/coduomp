// Source: uo_cgame_mp_x86.dll 0x300195e0..0x30019b9f
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_300195e0_30019b9f.mcode
//
// CG_DrawWeapReticle - draw the current weapon's ADS overlay and its selected
// overlay-reticle style. The overlay fraction becomes the white draw color's
// alpha and the function returns its complement for the ordinary crosshair.
// Name is corroborated by the same-module PPC symbol bank. StuckInClient is a
// rejected server-side size match.

#include "client/cgame/client_recovered.h"

enum weaponOverlayReticle_e {
    WEAPON_OVERLAY_RETICLE_NONE = 0,
    WEAPON_OVERLAY_RETICLE_CROSSHAIR = 1,
    WEAPON_OVERLAY_RETICLE_FG42 = 2,
    WEAPON_OVERLAY_RETICLE_SPRINGFIELD = 3,
    WEAPON_OVERLAY_RETICLE_GEWEHR43 = 4
};

/* NOT_FROM_ORIGINAL_SOURCE: this recovered function's rectangles are already
 * physical refdef pixels. Keep the stock optical composition proportional and
 * clip it to the centered 4:3 optical canvas; the native side extensions are
 * painted as a true letterbox instead of stretching the mask texture. */
#define DRAW_RETICLE_PIC(x_, y_, w_, h_, s1_, t1_, s2_, t2_, shader_) \
    cgame_compat_draw_letterboxed_optical_pic( \
        (x_), (y_), (w_), (h_), (s1_), (t1_), (s2_), (t2_), \
        (int32_t)(shader_))

long double CG_DrawWeapReticle(void)
{
    weaponInfo_t *weapon;
    int32_t weaponIndex;
    vec4_t color = { 1.0f, 1.0f, 1.0f, 1.0f };
    float overlayFrac;
    float projectedX;
    float projectedY;
    float width;
    float height;
    float centerX;
    float centerY;
    float left;
    float top;

    if (!CG_CalcAdsOverlayFrac(&overlayFrac)) {
        return 1.0f;
    }

    weapon = cg_currentWeaponInfo;
    weaponIndex = weapon->weaponIndex;
    CG_ProjectDamageDirToScreen(&projectedX, &projectedY);
    /* 0x3001963a reads screenXScale, then 0x30019640 reloads the weapon
     * pointer after the projection call. This one reloaded pointer supplies
     * both overlay dimensions. */
    float widthScale = cgs_screenXScale;
    weapon = cg_currentWeaponInfo;
    width = (float)((long double)widthScale *
                    (long double)weapon->adsOverlayWidth);
    height = (float)((long double)cgs_screenYScale *
                     (long double)weapon->adsOverlayHeight);
    /* cg_refdef.x/y/width/height are FILD'd / FIADD'd (integer) with no FSTP DWORD
     * (0x30019670 FILD width; FMUL 0.5f; 0x3001967e FIADD x, and the y/height pair
     * at 0x30019692/0x300196a0), so the implicit int->float conversions stay exact;
     * no (float) casts (they would round under -std=c11). One store rounding each. */
    centerX = (float)(((long double)cgs_screenXScale *
                       (long double)projectedX) +
                      (long double)cg_refdef.width * 0.5f +
                      (long double)cg_refdef.x);
    centerY = (float)(((long double)cgs_screenYScale *
                       (long double)projectedY) +
                      (long double)cg_refdef.height * 0.5f +
                      (long double)cg_refdef.y);
    color[3] = overlayFrac;
    trap_R_SetColor(color);
    /* COMPATIBILITY_PATCH (NOT_FROM_ORIGINAL_SOURCE): the recovered optical
     * draws below remain intact and are clipped by the isolated adapter. Draw
     * the native-width side bars first and restore the stock white/alpha draw
     * color before entering those stock-derived branches. */
    cgame_compat_draw_optical_letterbox(overlayFrac);

    /* 0x300196b0 reloads the global after SetColor before testing +0x280. */
    weapon = cg_currentWeaponInfo;
    if (weapon->adsOverlayShader[0] != '\0') {
        qhandle_t shader = (qhandle_t)cg_weaponInfos[weaponIndex].adsOverlayShader;
        /* The mask strips anchor to the viewport ORIGIN (0,0) with the raw view
         * width/height -- NOT the cg_refdef.x/y offset. Proof in the block's bytes:
         * the left/top guards fcomp against 0.0f (0x30019727 `left > 0`, 0x300197b0
         * `top > 0`), the right/bottom guards and the strip extents fild
         * cg_refdef.width/height directly (0x30019766/0x30019779/0x30019737/0x300197e6),
         * and every strip pushes literal 0 for its x/y origin -- no fiadd of
         * cg_refdef.x (0x30487a78) or cg_refdef.y (0x30487a7c) occurs. A prior pass
         * used cg_refdef.x/y as the origin, which would offset the mask whenever the
         * viewport is inset (splitscreen/letterbox). (centerX/centerY above DO use
         * cg_refdef.x/y -- 0x3001967e/0x30019692 fiadd -- and are unchanged.) */
        left = (float)((long double)centerX - (long double)width * 0.5f);
        top = (float)((long double)centerY - (long double)height * 0.5f);

        DRAW_RETICLE_PIC(left, top, width, height, 0.0f, 0.0f, 1.0f, 1.0f,
                         shader);
        if (left > 0.0f) {
            /* 0x30019737: full-viewport-height strip -- y=viewTop (PUSH 0 @0x3001974e),
             * h=viewBottom-viewTop (FILD cg_refdef.height @0x30019737), NOT the scope
             * band top/height a prior pass used (which left the corners unmasked). */
            long double viewHeight = (long double)cg_refdef.height;
            DRAW_RETICLE_PIC(0.0f, 0.0f, left,
                             viewHeight,
                             0.0f, 0.0f, 0.0f, 1.0f, shader);
        }

        /* The right edge is formed only after the first draw and optional
         * left-strip draw. 0x30019766 then loads the current viewport width. */
        float right = (float)((long double)left + (long double)width);
        long double viewRight = (long double)cg_refdef.width;
        /* COMPATIBILITY_PATCH (NOT_FROM_ORIGINAL_SOURCE): the stock endpoint
         * above excludes refdef.x even though the scope center includes it.
         * Resolve that asymmetry at the optical-composition boundary. */
        viewRight = cgame_compat_optical_canvas_right(viewRight);
        if (right < viewRight) {
            /* 0x30019779: full-height strip (y=viewTop, h=viewBottom-viewTop) with
             * texcoords (s1,t1,s2,t2)=(0,0,0,1) -- 0x30019789/0x3001978d PUSH 0 for
             * s2/s1. A prior pass used the scope band top/height and s1=s2=1.0. */
            long double viewHeight = (long double)cg_refdef.height;
            DRAW_RETICLE_PIC(right, 0.0f, viewRight - (long double)right,
                             viewHeight,
                             0.0f, 0.0f, 0.0f, 1.0f, shader);
        }
        if (top > 0.0f) {
            DRAW_RETICLE_PIC(left, 0.0f, width, top,
                             0.0f, 0.0f, 1.0f, 0.0f, shader);
        }

        /* Likewise, the bottom edge and viewport-height load occur only after
         * the top-strip call site. */
        float bottom = (float)((long double)top + (long double)height);
        long double viewBottom = (long double)cg_refdef.height;
        if (bottom < viewBottom) {
            /* 0x300197f9: texcoords (s1,t1,s2,t2)=(0,0,1,0) -- 0x300197fe/0x30019805
             * PUSH 0 for t2/t1 (same edge coords as the top bar). A prior pass used
             * t1=t2=1.0. */
            DRAW_RETICLE_PIC(left, bottom, width, viewBottom - bottom,
                             0.0f, 0.0f, 1.0f, 0.0f, shader);
        }
    }

    /* 0x30019821 reloads the global after every possible renderer call above. */
    weapon = cg_currentWeaponInfo;
    switch (weapon->adsOverlayReticle) {
    case WEAPON_OVERLAY_RETICLE_NONE:
        break;

    case WEAPON_OVERLAY_RETICLE_CROSSHAIR: {
        /* 0x3001983e FILD reticleCenterSize; FLD ST0 -> size stays UNROUNDED in
         * ST0 (duplicated, never FSTP'd) and feeds both w=sx*size and h=sy*size,
         * so it is long double; a float `size` would round it before the two
         * multiplies. cg_refdef.x/y/width/height are FILD/FIADD'd exact (no FSTP),
         * so no (float) casts on them either. */
        long double size = weapon->reticleCenterSize;
        float w = (float)((long double)cgs_screenXScale * size);
        qhandle_t centerShader =
            cg_weaponInfos[weaponIndex].reticleCenterShader;
        float h = (float)((long double)cgs_screenYScale * size);
        /* 0x30019886..0x300198ac stages Y first, then 0x300198b0..0x300198cc
         * computes X. */
        float y = (float)(((long double)cgs_screenYScale *
                           (long double)projectedY) +
                          ((long double)cg_refdef.height - (long double)h) * 0.5f +
                          (long double)cg_refdef.y);
        float x = (float)(((long double)cgs_screenXScale *
                           (long double)projectedX) +
                          ((long double)cg_refdef.width - (long double)w) * 0.5f +
                          (long double)cg_refdef.x);
        DRAW_RETICLE_PIC(x, y, w, h, 0.0f, 0.0f, 1.0f, 1.0f,
                         centerShader);
        break;
    }

    case WEAPON_OVERLAY_RETICLE_FG42: {
        float verticalHeight;
        float verticalX;
        float horizontalWidth;
        float horizontalY;
        float leftX;
        float rightX;

        /* The three RGB stores overwrite the existing white color in reverse
         * address order; its alpha remains overlayFrac. */
        color[2] = 0.0f;
        color[1] = 0.0f;
        color[0] = 0.0f;
        trap_R_SetColor(color);
        verticalHeight = (float)((long double)height * 0.9f);
        verticalX = (float)((long double)centerX - 1.0L);
        DRAW_RETICLE_PIC(verticalX, centerY, 3.0f, verticalHeight,
                         0.0f, 0.0f, 1.0f, 1.0f,
                         cgs_media_hudSoftLineShader);
        horizontalWidth = (float)((long double)width * 0.75f);
        horizontalY = (float)((long double)centerY - 1.0L);
        leftX = (float)((long double)centerX - (long double)width * 0.9f);
        DRAW_RETICLE_PIC(leftX, horizontalY, horizontalWidth, 3.0f,
                         0.0f, 0.0f, 1.0f, 1.0f,
                         cgs_media_hudSoftLineHShader);
        rightX = (float)((long double)centerX + (long double)width * 0.15f);
        DRAW_RETICLE_PIC(rightX, horizontalY, horizontalWidth, 3.0f,
                         0.0f, 0.0f, 1.0f, 1.0f,
                         cgs_media_hudSoftLineHShader);
        break;
    }

    case WEAPON_OVERLAY_RETICLE_SPRINGFIELD: {
        float verticalHeight;
        float verticalY;
        float verticalX;
        float horizontalWidth;
        float horizontalY;
        float horizontalX;

        color[2] = 0.0f;
        color[1] = 0.0f;
        color[0] = 0.0f;
        trap_R_SetColor(color);
        verticalHeight = (float)((long double)height * 1.8f);
        verticalY = (float)((long double)centerY -
                            (long double)height * 0.9f);
        verticalX = (float)((long double)centerX - 1.0L);
        DRAW_RETICLE_PIC(verticalX, verticalY, 3.0f, verticalHeight,
                         0.0f, 0.0f, 1.0f, 1.0f,
                         cgs_media_hudSoftLineShader);
        horizontalWidth = (float)((long double)width * 1.8f);
        horizontalY = (float)((long double)centerY - 1.0L);
        horizontalX = (float)((long double)centerX -
                              (long double)width * 0.9f);
        DRAW_RETICLE_PIC(horizontalX, horizontalY, horizontalWidth, 3.0f,
                         0.0f, 0.0f, 1.0f, 1.0f,
                         cgs_media_hudSoftLineHShader);
        break;
    }

    case WEAPON_OVERLAY_RETICLE_GEWEHR43: {
        float verticalHeight;
        float verticalX;
        float horizontalWidth;
        float horizontalY;
        float leftX;
        float rightX;

        color[2] = 0.0f;
        color[1] = 0.0f;
        color[0] = 0.0f;
        trap_R_SetColor(color);
        verticalHeight = (float)((long double)height * 0.9f);
        verticalX = (float)((long double)centerX - 1.0L);
        DRAW_RETICLE_PIC(verticalX, centerY, 3.0f, verticalHeight,
                         0.0f, 0.0f, 1.0f, 1.0f,
                         cgs_media_hudSoftLineShader);
        horizontalWidth = (float)((long double)width * 0.75f);
        horizontalY = (float)((long double)centerY - 1.0L);
        leftX = (float)((long double)centerX - (long double)width * 0.9f);
        DRAW_RETICLE_PIC(leftX, horizontalY, horizontalWidth, 3.0f,
                         0.0f, 0.0f, 1.0f, 1.0f,
                         cgs_media_hudSoftLineHShader);
        rightX = (float)((long double)centerX + (long double)width * 0.15f);
        DRAW_RETICLE_PIC(rightX, horizontalY, horizontalWidth, 3.0f,
                         0.0f, 0.0f, 1.0f, 1.0f,
                         cgs_media_hudSoftLineHShader);
        break;
    }
    }

    trap_R_SetColor(NULL);
    return 1.0L - (long double)overlayFrac;
}

#undef DRAW_RETICLE_PIC
