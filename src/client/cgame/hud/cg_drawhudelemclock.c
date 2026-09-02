// Source: uo_cgame_mp_x86.dll 0x3002a000..0x3002a1c4
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3002a000_3002a1c4.mcode

#include "../client_recovered.h"
#include "../globals.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define CG_CLOCK_NEEDLE_SUFFIX "Needle"

enum {
    CG_CLOCK_NEEDLE_SUFFIX_LENGTH = sizeof(CG_CLOCK_NEEDLE_SUFFIX) - 1,
    CG_CLOCK_BASE_NAME_CAPACITY = MAX_QPATH - CG_CLOCK_NEEDLE_SUFFIX_LENGTH
};

_Static_assert(MAX_QPATH > CG_CLOCK_NEEDLE_SUFFIX_LENGTH, "clock shader path cannot hold its Needle suffix");

/*
 * CG_DrawHudElemClock (0x3002a000) — render a CLOCK / analog-timer HUD element:
 * a static clock-face shader plus a rotating "Needle" hand whose angle encodes
 * the element's remaining/elapsed time. This is the type-{8,9} branch of
 * CG_DrawSingleHudElem's draw dispatcher (0x3002a310); its sibling the type-3 branch
 * is CG_DrawHudElemShader (0x3002a1d0), which this function mirrors closely.
 *
 * NAMING: the .mcode size-match guess `BG_GetMaxPickupableAmmo` (win 0x1c4 ==
 * some game_mp_uo symbol) is REJECTED — this touches no ammo/inventory state.
 * The behavior is proven: it looks up a shader config-string by
 * elem->materialIndex, registers that name as the clock face AND the same name
 * with the literal suffix "Needle" appended as the hand, converts the element's
 * timer value into a rotation angle, then draws the face with CG_DrawPic and the
 * hand with CG_DrawRotatedPic. The name is anchored by the same-module PPC
 * cgame_mp.dll bank (CG_DrawHudElemClock) and matches the existing provisional
 * decl in client_recovered.h. The "Needle" data string at 0x300777d0
 * ("Needle\0", via objdump -s -j .rdata) is the decisive tell.
 *
 * ABI (__usercall, proven at the sole call site 0x3002a3a7 in CG_DrawSingleHudElem):
 *   ECX = cgAlignedDrawItem *item   (saved to ESI: 3002a01a MOV ESI,ECX)
 *   EDX = hudElem_t *elem           (saved to EBX: 3002a012 MOV EBX,EDX)
 * Plain RET, caller-cleaned. The prologue snapshots the /GS __security_cookie
 * (0x30081650) into the frame top and the epilogue verifies it via
 * __security_check_cookie (0x30061639) — a compiler artifact, plain C here.
 *
 * The interpolation helpers are __usercall(EAX=node, ESI=owner). Here node=elem
 * and owner=item, exactly as in CG_DrawHudElemShader:
 * CG_HudElemShaderWidth lerps the elem width track (+0x30/+0x3c) and its
 * sibling CG_HudElemShaderHeight lerps the height track (+0x34/+0x40); a zero
 * endpoint falls back to item->fontHeight (+0x28). See
 * src/client/cgame/hud/cg_hudelemshaderwidth.c and
 * src/client/cgame/hud/cg_hudelemshaderheight.c.
 */

/* The two size interpolators are __usercall(EAX=node, ESI=owner). At this call
 * site those registers carry a hud element and draw item. Their shared
 * declarations use the provisional offset-view tags in client_recovered.h;
 * casts at the calls document this caller's concrete views. */

/* Provenance for the item fields read here (char* members => guard to 32-bit ABI). */
_Static_assert(offsetof(cgAlignedDrawItem, x) == 0x00, "item.x @ +0x00");
_Static_assert(offsetof(cgAlignedDrawItem, y) == 0x04, "item.y @ +0x04");
_Static_assert(offsetof(cgAlignedDrawItem, height) == 0x0c, "item.height @ +0x0c");
#if defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 4
_Static_assert(offsetof(cgAlignedDrawItem, fontHeight) == 0x28, "item.fontHeight @ +0x28");
_Static_assert(offsetof(cgAlignedDrawItem, color) == 0x30, "item.color @ +0x30");
#endif

void CG_DrawHudElemClock(cgAlignedDrawItem *item, hudElem_t *elem)
{
    /*
     * 3002a00e..a026: copy the shader config-string for elem->materialIndex into
     * a 0x3a-byte accepted base-name domain via CG_GetShaderConfigString (index
     * in EAX). An empty/oversized name returns qfalse and the whole draw is
     * skipped. At the stock MAX_QPATH this retains that exact input limit while
     * making the destination large enough for the later suffix; an intentional
     * global MAX_QPATH increase expands both capacities together.
     */
    char shaderName[MAX_QPATH];
    if (!CG_GetShaderConfigString(elem->materialIndex, shaderName, CG_CLOCK_BASE_NAME_CAPACITY)) {
        return;
    }

    /*
     * 3002a02e..a044: register the clock-FACE shader. The machine inlines
     * CG_RegisterMaterial's body — CG_DrawInformation(0) loading pump
     * (3002a030), then cgame_syscall(CG_R_REGISTERSHADER, name, 5) (3002a03e).
     * Expressed as the equivalent CG_RegisterMaterial call with
     * R_IMAGE_TRACK_HUD (the machine's literal 5), matching
     * CG_DrawHudElemShader. Handle kept in EBP across the body.
     */
    qhandle_t hFace = CG_RegisterMaterial(shaderName, R_IMAGE_TRACK_HUD);

    /*
     * 3002a044..a074: append the literal "Needle" to the config-string name and
     * register that as the clock HAND shader (same inlined register-material
     * sequence). The suffix bytes are copied from the .rdata string at 0x300777d0
     * ("Needle\0", dumped via objdump -s -j .rdata) onto the NUL terminator found
     * by the a050 strlen loop — i.e. strcat(shaderName, "Needle").
     */
    /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered client-module boundary input and state before use. */
    strcat(shaderName, CG_CLOCK_NEEDLE_SUFFIX);
    qhandle_t hNeedle = CG_RegisterMaterial(shaderName, R_IMAGE_TRACK_HUD);

    /*
     * 3002a092..a10e: convert the element's timer value into a needle rotation
     * angle. CG_GetHudElemTime(elem) (ECX=elem) yields the timer/clock value
     * in ms (>= 0), promoted to float.
     *
     * If elem->rotationPeriodMs (+0x60) is nonzero, the value maps onto that
     * period: bams = (timer * 360.0 / period) * 182.04445f. Otherwise it maps onto
     * a fixed 60000 ms (60 s) revolution: bams = timer * (65536/60000 = 1.0922667f).
     * Either bams is truncated to 16 bits (mod 65536, one full circle) by _ftol +
     * AND 0xffff, then scaled back to degrees by * (360/65536 = 0.0054931641f).
     * The two-step float math and exact constants (0x3007bd54=360.0f,
     * 0x3007bd60=0x43360b61=182.04445f, 0x3007c01c=0x3f8bcf65=1.0922667f,
     * 0x3007bd5c=0x3bb40000=0.0054931641f) are preserved to reproduce the DLL's
     * rounding.
     */
    float timerValue = (float)CG_GetHudElemTime(elem);
    int32_t periodMs = elem->rotationPeriodMs;
    uint32_t bams;
    if (periodMs != 0) {
        float periodRounded = (float)periodMs; /* FILD then FSTP m32 at 0x3002a0b8 */
        long double bamsRaw = ((long double)timerValue * (long double)360.0f) / (long double)periodRounded * (long double)182.04445f;
        bams = (uint32_t)coduo_fp_to_i32_extended(bamsRaw) & 0xffffu;
    } else {
        long double bamsRaw = (long double)timerValue * (long double)1.0922667f;
        bams = (uint32_t)coduo_fp_to_i32_extended(bamsRaw) & 0xffffu;
    }
    float bamsRounded = (float)(int32_t)bams; /* FILD then FSTP m32 */
    float angle = (float)((long double)bamsRounded * (long double)0.0054931641f); /* bams -> degrees */

    /*
     * 3002a114..a12a: interpolate the element's animated size.
     *   sizeW = CG_HudElemShaderWidth(elem, item)  (elem width track +0x30)
     *   sizeH = CG_HudElemShaderHeight(elem, item)         (elem height track +0x34)
     */
    float sizeW = CG_HudElemShaderWidth(elem, item);      /* [S+0x1c] */
    float sizeH = CG_HudElemShaderHeight(elem, item);   /* [S+0x14] */

    /*
     * 3002a12e..a170: vertical placement of the element's top edge, selected by
     * elem->alignY, subtracting sizeH from the rectangle (same as the shader path).
     */
    float yTop;
    switch (elem->alignY) {
    case HUDELEM_ALIGN_START:  /* 0: a16e — yTop = item->y */
        yTop = item->y;
        break;
    case HUDELEM_ALIGN_CENTER: /* 1: a154 — (item->height - sizeH)*0.5 + item->y */
        yTop = (float)(((long double)item->height - (long double)sizeH) * (long double)0.5f + (long double)item->y);
        break;
    case HUDELEM_ALIGN_END:    /* 2: a140 — item->height + item->y - sizeH */
        yTop = (float)((long double)item->height + (long double)item->y - (long double)sizeH);
        break;
    default:                   /* >=3: a13c — yTop = 0 */
        yTop = 0.0f;
        break;
    }

    /*
     * 3002a171..a1c0: bracket the two draws with the element's RGBA color.
     *   trap_R_SetColor(item->color)              (a171: LEA [ESI+0x30])
     *   CG_DrawPic(item->x, yTop, sizeW, sizeH, hFace) — clock face
     *   CG_DrawRotatedPic(item->x, yTop, sizeW, sizeH, angle, hNeedle) — hand
     * item->x is forwarded as its raw float bit pattern for the x
     * coordinate of both draws. 0x3002a1a6 then resets the color before the epilogue.
     */
    trap_R_SetColor(item->color);

    float x = item->x;
    CG_DrawPic(x, yTop, sizeW, sizeH, hFace);
    CG_DrawRotatedPic(x, yTop, sizeW, sizeH, angle, hNeedle);
    /* 0x3002a1a6 PUSH 0; PUSH 0x48 (CG_R_SETCOLOR); CALL syscall =
     * trap_R_SetColor(NULL): reset the element color so it does not leak into
     * subsequent HUD draws. A prior pass omitted this (its comment wrongly claimed
     * there was no reset here). */
    trap_R_SetColor(NULL);
}
