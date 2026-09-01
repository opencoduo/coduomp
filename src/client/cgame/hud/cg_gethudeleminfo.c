#include "../client_recovered.h"
#include <string.h>

// Source: uo_cgame_mp_x86.dll 0x30029c00..0x30029efa
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30029c00_30029efa.mcode
//
// Build the 0x40-byte aligned HUD draw descriptor: select/format its strings,
// evaluate its animated value tracks and coordinates, and resolve packed RGBA
// fading. The register/stack ABI is described by the shared declaration.
// The Mac cgame symbol CG_GetHudElemInfo shares the four timer/string-formatting
// callees and performs the same descriptor-building role, resolving the name.

/* The evaluator declarations are centralized in client_recovered.h. The
 * coordinate evaluators receive this exact cgAlignedDrawItem; their original
 * owner+0x08/+0x0c reads are item->width/item->height. */

enum {
    HUD_FONT_SMALL = 0,
    HUD_FONT_BIG = 1,
    HUD_FONT_CONSOLE = 2
};

void CG_GetHudElemInfo(cgAlignedDrawItem *item, hudElem_t *elem,
                             char *scratch, int32_t scratchLen)
{
    float fontScale;
    int32_t font = elem->font;

    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if ((uint32_t)font > (uint32_t)HUD_FONT_CONSOLE) {
        font = HUD_FONT_SMALL;
    }

    switch (font) {
    case HUD_FONT_SMALL:
        item->font = 0;
        fontScale = elem->fontScale * floatOneQuarter;
        item->fontScale = fontScale;
        item->fontHeight = (float)coduo_int32_from_bits((uint32_t)cgame_syscall(
            CG_R_TEXT_HEIGHT, 0, item->fontScaleBits));
        item->fontWidthBits = 0;
        break;
    case HUD_FONT_BIG:
        item->font = 4;
        fontScale = elem->fontScale * floatOneThird;
        item->fontScale = fontScale;
        item->fontHeight = 16.0f;
        fontScale = 16.0f;
        item->fontWidth = fontScale;
        break;
    case HUD_FONT_CONSOLE:
        item->font = 5;
        fontScale = elem->fontScale * floatOneThird;
        item->fontScale = fontScale;
        item->fontHeight = 16.0f;
        fontScale = 8.0f;
        item->fontWidth = fontScale;
        break;
    }

    item->label = (char *)CG_SafeTranslateHudElemString(elem->label);

    switch (elem->type) {
    case HE_TYPE_TEXT:
        item->text = elem->text != 0
                   ? (char *)CG_SafeTranslateHudElemString(elem->text)
                   : (char *)g_str_empty;
        break;
    case HE_TYPE_VALUE:
        item->text = (char *)va("%g", (double)elem->value);
        break;
    case HE_TYPE_TIMER:
    case HE_TYPE_TIMER_UP:
        item->text = (char *)CG_HudElemTimerString(elem);
        break;
    case HE_TYPE_TENTHS_TIMER:
    case HE_TYPE_TENTHS_TIMER_UP:
        item->text = (char *)CG_HudElemTenthsTimerString(elem);
        break;
    default:
        item->text = (char *)g_str_empty;
        break;
    }

    if (item->label[0] != '\0' && item->text[0] != '\0') {
        CG_ConsolidateHudElemText(item, scratchLen, scratch);
    }

    if (item->label[0] != '\0') {
        item->labelWidth = CG_HudElemStringWidth(item->label, item);
    } else {
        item->labelWidthBits = 0;
    }
    if (item->text[0] != '\0') {
        item->textWidth = CG_HudElemStringWidth(item->text, item);
    } else {
        item->textWidthBits = 0;
    }

    switch (elem->type) {
    case HE_TYPE_TEXT:
    case HE_TYPE_VALUE:
    case HE_TYPE_TIMER:
    case HE_TYPE_TIMER_UP:
    case HE_TYPE_TENTHS_TIMER:
    case HE_TYPE_TENTHS_TIMER_UP:
        item->width = item->textWidth + item->labelWidth;
        break;
    case HE_TYPE_SHADER:
    case HE_TYPE_CLOCK:
    case HE_TYPE_CLOCK_UP:
        item->width = CG_HudElemShaderWidth(elem, item) + item->labelWidth;
        break;
    default:
        item->width = floatZero;
        break;
    }

    item->height = CG_HudElemHeight(elem, item);
    item->x = CG_HudElemX(elem, item);
    item->y = CG_HudElemY(elem, item);

    {
        const uint8_t *to = (const uint8_t *)&elem->color;
        const uint8_t *from = (const uint8_t *)&elem->fromColor;
        int32_t elapsed = coduo_int32_from_bits(
            (uint32_t)cg_time - (uint32_t)elem->fadeStartTime);

        /* Float faithfulness (0x30029dfe..0x30029ef3): each component performs
         * exactly ONE rounding -- the FSTP DWORD into item->color[i]. In the fade
         * path `fraction` is built by FILD+FIDIV at 0x30029e02/0x30029e0a and then
         * kept in st1 across ALL FOUR components (FMUL ST1 at e1d/e42/e67/e8c),
         * never stored; the FSTP ST0 at 0x30029e9d finally discards it (a pop, not
         * a rounding). So it is held as `long double` here.
         *
         * The endpoint arithmetic is INTEGER, not float: to[i]-from[i] is a SUB
         * on ECX/EDX (0x30029e0e) and from[i] re-enters via FIADD (0x30029e1f),
         * so neither endpoint is ever converted to float -- (float) casts on them
         * would be roundings the DLL does not perform. Likewise the no-fade path
         * feeds a bare FILD straight into the FMUL (0x30029eaa/0x30029eae). */
        if (elem->fadeTime > 0 && elapsed < elem->fadeTime) {
            long double fraction = (long double)elapsed / elem->fadeTime;
            for (int i = 0; i < 4; ++i) {
                item->color[i] = ((long double)(to[i] - from[i]) * fraction +
                    from[i]) * colorByteToUnitScale;
            }
        } else {
            for (int i = 0; i < 4; ++i) {
                item->color[i] = (long double)to[i] * colorByteToUnitScale;
            }
        }
    }
}
