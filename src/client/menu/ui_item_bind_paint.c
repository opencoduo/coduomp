#include "ui_runtime.h"

#include "compat/coduo_native_x87.h"
#include "ui_menu_globals.h"

#include <math.h>    /* sinf */
#include <stddef.h>  /* NULL */

extern displayContextDef_t *DC;
void Com_Printf(const char *format, ...);

// Sources: uo_cgame_mp_x86.dll 0x30056e10..0x3005700b and
//          uo_ui_mp_x86.dll    0x40018970..0x40018b6b.
// Their normalized instruction streams are identical. The binding-text
// adapter preserves UI's optional non-original console row without forking
// this original painter.
//
// Naming: the .mcode "CG_DrawTurretCrossHair" tag is a pure win-size==0x1fb guess
// and is REJECTED — this body has nothing to do with a turret crosshair. It is the
// Q3 ui_shared.c menu-item painter Item_Bind_Paint (0x30056e10): it validates the
// item's committed type against the exact bind-compatible set, paints the item's
// label via Item_Text_Paint, resolves the bound key name via BindingFromName, and
// draws that key text via DC->drawText. Callees / data prove the identity:
//   0x3002b420 = Com_Printf  (the "^1Menu Error: Expecting type: ITEM_TYPE_..."
//                diagnostic string @0x3007b7b8, listing exactly the accepted types)
//   0x30050110 = LerpColor    (focused-item color pulse)
//   0x30055fc0 = Item_Text_Paint (draws the item's label)
//   0x300568a0 = BindingFromName(item->cvar, 0), returning the value text.
//
// Register ABI: the item pointer arrives in EDI (caller-observed; matches the whole
// Item_*_Paint family, e.g. the sibling Item_Slider_Paint 0x30056c80). Expressed
// here as a normal C signature.
//
// Behavior decoded from the body:
//  1. 0x30056e10: switch on item->typeValidated (+0xcc). Accept only TEXT(0),
//     EDITFIELD(4), NUMERICFIELD(9), SLIDER(0xa), YESNO(0xb), BIND(0xd),
//     UPREDITFIELD(0xf). Anything else -> Com_Printf(error) and fall through.
//     (This is Item_ValidateTypeData's accepted set, echoed in the error text.)
//  2. 0x30056e46: maxChars = item->typeData ? ((editFieldDef_t*)typeData)->maxChars
//     (+0x18) : 0. Used later as the drawText glyph limit.
//  3. 0x30056e53: if item->cvar != NULL, touch it via DC->getCVarValue(cvar)
//     (return discarded with FSTP ST0) so the linked cvar is live before drawing.
//  4. Choose the value-text color:
//     - window.flags & WINDOW_HASFOCUS (0x2) set:
//         * if this item IS the one being rebound (item == g_bindItem, 0x30134d38):
//             from = {0.8, 0, 0, 0.8}  (the "press a key" red highlight)
//         * else:
//             from = 0.8 * parent->focusColor per component (0.8f @ 0x3007bdf0)
//         then t = (sin(realTime/75) + 1.0) * 0.5 (1.0 @ 0x3007bce0, 0.5 @ 0x3007bce8;
//         realTime/75 is the signed magic-number division 0x1b4e81b5 + SAR 3 of
//         DC->realTime, +0xec) and LerpColor(color, to=parent->focusColor,
//         from, t) writes the clamped pulse into color.
//     - else (unfocused): color = item->window.foreColor (+0x74).
//  5. If item->text != NULL: Item_Text_Paint(item) draws the label, then draw the
//     value text at x = textRect.x + textRect.w + 8.0 (8.0f @ 0x3007be08); the value
//     string is BindingFromName(item->cvar, 0).
//     If item->text == NULL: draw the literal "FIXME" (0x3007b540) at x = textRect.x
//     (no label, no +width offset). Either way:
//       DC->drawText(x, textRect.y, adjust=item->font, scale=item->textscale,
//                      color, text, style=0, limit=maxChars, font=item->textStyle).
//
// The "FIXME" placeholder in the no-text branch is a genuine artifact of the
// ORIGINAL ui_shared.c source (a bind item with no label had no value text wired
// up), preserved here as it appears in the machine code.

void Item_Bind_Paint(itemDef_t *item)
{
    vec4_t      color;
    int32_t     maxPaintChars = 0;   /* drawText glyph limit (EBP in the body) */
    const char *bindingText;
    menuDef_t  *parent = item->parent; /* 0x30056e21, before diagnostic callbacks */

    /* 0x30056e1d..0x30056e44: accept only the bind-compatible item types; anything
     * else prints the ui_shared type-error diagnostic and still falls through to the
     * draw (the machine code does not early-return). */
    switch (item->typeValidated) {
        case ITEM_TYPE_TEXT:
        case ITEM_TYPE_EDITFIELD:
        case ITEM_TYPE_NUMERICFIELD:
        case ITEM_TYPE_SLIDER:
        case ITEM_TYPE_YESNO:
        case ITEM_TYPE_BIND:
        case ITEM_TYPE_UPREDITFIELD:
            /* 0x30056e46: invalid types branch around this read. */
            if (item->typeData != NULL) {
                maxPaintChars =
                    ((editFieldDef_t *)item->typeData)->maxPaintChars;
            }
            break;
        default:
            /* 0x30056ea5: Com_Printf("^1Menu Error: Expecting type: ITEM_TYPE_..."). */
            Com_Printf("^1Menu Error: Expecting type: ITEM_TYPE_EDITFIELD, "
                       "ITEM_TYPE_NUMERICFIELD, ITEM_TYPE_UPREDITFIELD, "
                       "ITEM_TYPE_YESNO, ITEM_TYPE_BIND, ITEM_TYPE_SLIDER, "
                       "or ITEM_TYPE_TEXT\n");
            break;
    }

    /* 0x30056e53: force the linked cvar to exist/refresh before drawing. */
    {
        const char *cvar = item->cvar;
        if (cvar != NULL)
            (void)DC->getCVarValue(cvar);
    }

    /* 0x30056e6f: retained across the color work and used by the no-text draw. */
    displayContextDef_t *display = DC;

    /* 0x30056e6b / 0x30056f3d: focused bind items pulse their value-text color;
     * unfocused ones use the item's static foreColor. */
    if (item->window.flags & WINDOW_HASFOCUS) {
        vec4_t     from;                    /* the "from" color for the lerp */
        int32_t    phase;
        float      t;

        if (g_bindItem == item) {
            /* 0x30056e83: this item is the one currently capturing a key press —
             * highlight in "press a key" red. 0.8f @ 0x3007bce0-family immediates
             * (0x3f4ccccd). */
            from[0] = 0.8f;
            from[1] = 0.0f;
            from[2] = 0.0f;
            from[3] = 0.8f;
        } else {
            /* 0x30056eb4: from = 0.8 * parent->focusColor per component. */
            from[0] = parent->focusColor[0] * 0.8f;
            from[1] = parent->focusColor[1] * 0.8f;
            from[2] = parent->focusColor[2] * 0.8f;
            from[3] = parent->focusColor[3] * 0.8f;
        }

        /* 0x30056ef4..0x30056f2a: phase = DC->realTime / 75 (signed magic div,
         * truncated to int), t = (sin(phase) + 1.0) * 0.5. */
        phase = display->realTime / 75;
        t = (float)((coduo_x87_sinl((long double)phase) + 1.0f) *
                    0.5f);

        /* 0x30056f33: LerpColor(out=color, to=parent->focusColor, from, t). */
        LerpColor(color, parent->focusColor, from, t);
    } else {
        color[0] = item->window.foreColor[0];
        color[1] = item->window.foreColor[1];
        color[2] = item->window.foreColor[2];
        color[3] = item->window.foreColor[3];
    }

    /* 0x30056f5b: a text-bearing bind item draws its label first, then the bound-key
     * text to the right of it; a label-less one draws only the placeholder text. */
    if (item->text != NULL) {
        Item_Text_Paint(item);                                  /* 0x30056f67 */
        bindingText = client_ui_compat_binding_from_name(item->cvar, qfalse);

        /* 0x30056f79: x = textRect.x + textRect.w + 8.0 (8.0f @ 0x3007be08). */
        float x = (float)((long double)item->textRect.w +
                          (long double)item->textRect.x + 8.0f);
        int32_t textStyle = item->textStyle;
        int32_t font = item->font;
        float y = item->textRect.y;
        float textScale = item->textscale;
        display = DC; /* 0x30056fb8: reload after both formatting callbacks. */
        display->drawText(x, y, font, textScale, color,
                          bindingText,
                          0, maxPaintChars, textStyle);
    } else {
        /* 0x30056fce: no label — draw the "FIXME" placeholder (0x3007b540) at the
         * text-rect origin. Preserved verbatim from the original source. */
        int32_t textStyle = item->textStyle;
        int32_t font = item->font;
        float y = item->textRect.y;
        float textScale = item->textscale;
        float x = item->textRect.x;
        display->drawText(x, y, font, textScale, color, "FIXME",
                          0, maxPaintChars, textStyle);
    }
}
