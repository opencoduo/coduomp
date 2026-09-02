// Source: uo_cgame_mp_x86.dll 0x3002a1d0..0x3002a309
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3002a1d0_3002a309.mcode

#include "../client_recovered.h"
#include "../globals.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

/*
 * CG_DrawHudElemShader (0x3002a1d0) — render a SHADER-type HUD element (the type-3
 * branch of CG_DrawSingleHudElem's dispatcher at 0x3002a3c6).
 *
 * NAMING: the .mcode header carries the size-matched guess `Script_SetItemColor`
 * (win 0x139 == matched 0x138). That is REJECTED. This function parses no menu
 * script keyword and touches no itemDef_t/windowDef_t: it looks up a shader config
 * string by elem->materialIndex, registers it, interpolates the element's animated
 * size, brackets a shader draw with trap_R_SetColor, and issues CG_DrawPic /
 * CG_DrawStretchPic. The name is anchored by the same-module PPC cgame_mp.dll bank
 * (CG_DrawHudElemShader) and by CG_DrawSingleHudElem's shader-branch call site, which
 * hands it the draw item (ECX) and the hud element (EDI/this).
 *
 * ABI (__usercall, proven at the sole call site 0x3002a3c6 in CG_DrawSingleHudElem):
 *   ECX = cgAlignedDrawItem *item   (saved to ESI: 3002a1e7 MOV ESI,ECX)
 *   EDI = hudElem_t *elem           (the hud element; forwarded by the dispatcher)
 * Plain RET; caller-cleaned. The prologue snapshots the /GS __security_cookie into
 * the frame top and the epilogue verifies it via __security_check_cookie
 * (0x30061639) — a compiler artifact, expressed here as plain C.
 *
 * The interpolation helpers are __usercall(EAX=node, ESI=owner). Here node=elem
 * and owner=item: CG_HudElemShaderWidth lerps the elem->width track
 * (+0x30 goal / +0x3c prev / +0x44 start / +0x48 duration) and its sibling
 * CG_HudElemShaderHeight lerps the elem->height track (+0x34 / +0x40, same window);
 * a zero endpoint falls back to item->fontHeight (+0x28). See
 * src/client/cgame/hud/cg_hudelemshaderwidth.c and
 * src/client/cgame/hud/cg_hudelemshaderheight.c.
 */

/* The two size interpolators are __usercall(EAX=node, ESI=owner). At this call
 * site those registers carry a hud element and draw item. Their shared
 * declarations use the provisional offset-view tags in client_recovered.h;
 * casts at the calls document this caller's concrete views. */

/* trap_R_SetColor (cgame trap CG_R_SETCOLOR = 72 = 0x48) is declared in
 * client_recovered.h and consumed via the cgame_syscall pointer (0x30085e9c);
 * CG_DrawPic (0x3001caa0) and CG_DrawStretchPic (0x3001cb00) likewise. */

/* Provenance for the fields this function reads. item carries char* members past
 * +0x10, so guard those offset asserts to the 32-bit target ABI. */
_Static_assert(offsetof(cgAlignedDrawItem, x) == 0x00, "item.x @ +0x00");
_Static_assert(offsetof(cgAlignedDrawItem, y)      == 0x04, "item.y @ +0x04");
_Static_assert(offsetof(cgAlignedDrawItem, height) == 0x0c, "item.height @ +0x0c");
#if defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 4
_Static_assert(offsetof(cgAlignedDrawItem, fontHeight) == 0x28,
               "item.fontHeight @ +0x28");
_Static_assert(offsetof(cgAlignedDrawItem, color)  == 0x30, "item.color @ +0x30");
#endif

void CG_DrawHudElemShader(cgAlignedDrawItem *item, hudElem_t *elem)
{
    /*
     * 3002a1dd..a1f3: copy the shader's config string for elem->materialIndex into
     * a 0x40-byte stack buffer. CG_GetShaderConfigString takes the index in EAX
     * (MOV EAX,[EDI+0x38]) and (dest, size) on the stack, returning qtrue on
     * success; an empty/oversized string returns qfalse and the whole draw is
     * skipped.
     */
    char shaderName[MAX_QPATH];
    if (!CG_GetShaderConfigString(elem->materialIndex, shaderName,
                                  (int)sizeof(shaderName))) {
        return;
    }

    /*
     * 3002a1fa..a208: register the named shader; the handle stays in EBX across the
     * body. R_IMAGE_TRACK_HUD is the machine's literal registration value 5.
     */
    qhandle_t hShader =
        CG_RegisterMaterial(shaderName, R_IMAGE_TRACK_HUD);

    /*
     * 3002a20b..a21d: interpolate the element's animated destination size.
     *   sizeW = CG_HudElemShaderWidth(elem, item)  (elem width track +0x30)
     *   sizeH = CG_HudElemShaderHeight(elem, item)         (elem height track +0x34)
     * Both stored as single-precision floats (FSTP).
     */
    float sizeW = CG_HudElemShaderWidth(elem, item);       /* [S+0xc] */
    float sizeH = CG_HudElemShaderHeight(elem, item);    /* [S+0x4] */
    float drawX = item->x;

    /* COMPATIBILITY_PATCH (NOT_FROM_ORIGINAL_SOURCE): make a complete
     * server/mod full-width shader (for example a killcam band) cover the
     * native drawable. Partial HUD shaders retain their stock dimensions. */
    cgame_compat_expand_native_server_hud_shader(
        item, elem, &drawX, &sizeW);

    /*
     * 3002a221..a266: vertical placement of the element's top edge (yTop),
     * selected by elem->alignY. The size subtracted from the rectangle is sizeH.
     */
    float yTop;
    switch (elem->alignY) {
    case HUDELEM_ALIGN_START:  /* 0: a25f — yTop = item->y */
        yTop = item->y;
        break;
    case HUDELEM_ALIGN_CENTER: /* 1: a249 — (item->height - sizeH)*0.5 + item->y */
        yTop = (float)(((long double)item->height - (long double)sizeH) *
                        (long double)0.5f + (long double)item->y);
        break;
    case HUDELEM_ALIGN_END:    /* 2: a239 — item->height + item->y - sizeH */
        yTop = (float)((long double)item->height + (long double)item->y -
                        (long double)sizeH);
        break;
    default:                   /* >=3: a22f — yTop = 0 */
        yTop = 0.0f;
        break;
    }

    /*
     * 3002a266..a272: set the 2D draw color to the element's RGBA at item+0x30
     * (item->color), then draw and reset.  trap_R_SetColor(item->color).
     */
    trap_R_SetColor(item->color);

    /*
     * 3002a272..a284: pick the draw path from the element's shader texcoords.
     * If both shaderRightTexcoord and shaderBottomTexcoord are exactly 1.0f
     * (compared as raw dword bits against 0x3f800000), draw the full image via
     * CG_DrawPic; otherwise draw a sub-rectangle via CG_DrawStretchPic.
     */
    if (elem->shaderRightTexcoord == 1.0f && elem->shaderBottomTexcoord == 1.0f) {
        /*
         * 3002a286..a29e: CG_DrawPic(x, y, w, h, hShader) with
         *   x = item->x, y = yTop, w = sizeW, h = sizeH.
         */
        CG_DrawPic(drawX, yTop, sizeW, sizeH, hShader);
    } else {
        /*
         * 3002a2a3..a2ea: CG_DrawStretchPic with explicit texcoords.
         *   x = item->x, y = item->y, w = sizeW, h = sizeH.
         * Texcoords depend on the sign of shaderRightTexcoord (FCOMP vs 0.0f at
         * 0x3007bcec, then TEST AH,0x5 / JP):
         *   right >= 0:  s1=0, t1=0, s2=right,  t2=bottom
         *   right <  0:  s1=-right, t1=0, s2=0,  t2=bottom   (mirrored S range)
         * shaderRightTexcoord = elem->+0x70, shaderBottomTexcoord = elem->+0x74.
         */
        float right  = elem->shaderRightTexcoord;
        float bottom = elem->shaderBottomTexcoord;
        float s1, t1, s2, t2;
        if (right < 0.0f) {
            s1 = -right; t1 = 0.0f; s2 = 0.0f;  t2 = bottom;
        } else {
            s1 = 0.0f;   t1 = 0.0f; s2 = right; t2 = bottom;
        }
        CG_DrawStretchPic(drawX, item->y, sizeW, sizeH,
                          s1, t1, s2, t2, hShader);
    }

    /*
     * 3002a2ed..a2f7: reset the 2D draw color to default. trap_R_SetColor(NULL).
     */
    trap_R_SetColor((const float *)0);
}
