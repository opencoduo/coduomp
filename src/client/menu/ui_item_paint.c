// Sources: uo_cgame_mp_x86.dll 0x30058250..0x30058702 and
//          uo_ui_mp_x86.dll    0x40019dc0..0x4001a272.
// The complete normalized PE32 instruction streams are identical.

#include "ui_runtime.h"

#include "compat/coduo_int32_bits.h"
#include "compat/coduo_native_x87.h"
#include "ui_menu_globals.h"

#include <math.h>
#include <stdint.h>

extern displayContextDef_t *DC;

/*
 * Item_Paint — advance one menu item's orbit/transition state, apply its
 * visibility gates, paint the window, and dispatch its type-specific content.
 *
 * The .mcode header's Cmd_MatchTimeout_f name is a size-only guess and is
 * rejected. The callers pass menu->items[] entries, the body uses itemDef_t and
 * displayContextDef_t throughout, and the same-module PPC bank identifies this
 * central UI dispatcher as Item_Paint.
 */

#define ITEM_ORBIT_STEP_RADIANS 0.05235987901687622f
enum {
    ITEM_SHADER_NAME_CAPACITY = 1024
};

static const vec4_t itemPaintDebugColor = {0.0f, 1.0f, 0.0f, 1.0f};

void Item_Paint(itemDef_t *item)
{
    displayContextDef_t *display = DC;
    ui_setFont_t setFont = display->setFont;
    menuDef_t *parent = item->parent;
    float compatPaintOffset;

    /* 0x30058263..0x30058282: this callback is optional; the cgame display
     * context leaves it NULL, but the shared UI layer supports it. EAX holds
     * the one callback snapshot across the NULL test and call. */
    if (setFont != NULL) {
        setFont(item->font);
    }

    /* 0x30058285..0x3005834d: rotate the center of rectClient around the point
     * in rectEffects by a fixed three-degree step whenever the orbit timer fires. */
    if (((uint32_t)item->window.flags & WINDOW_ORBITING) != 0) {
        displayContextDef_t *display = DC;
        int32_t realTime = display->realTime;

        if (realTime > item->window.nextTime) {
            float halfWidth = item->window.rectClient.w * 0.5f;
            float halfHeight = item->window.rectClient.h * 0.5f;
            float x = item->window.rectClient.x + halfWidth - item->window.rectEffects.x;
            float y = item->window.rectClient.y + halfHeight - item->window.rectEffects.y;
            float cosine;
            float sine;

            item->window.nextTime = coduo_int32_from_bits((uint32_t)realTime + (uint32_t)item->window.offsetTime);
        /* 0x30058303: one FSINCOS, followed by the cosine store and then the
         * sine store. The adapter is explicit hardware x87 on Intel. */
            coduo_x87_sincosf(ITEM_ORBIT_STEP_RADIANS, &sine, &cosine);
            item->window.rectClient.x = (float)((long double)cosine * x - (long double)sine * y + item->window.rectEffects.x - halfWidth);
            item->window.rectClient.y = (float)((long double)cosine * y + (long double)sine * x + item->window.rectEffects.y - halfHeight);
            Item_UpdatePosition(item);
        }
    }

    /* 0x30058352..0x300584d1: advance each rect component toward its target by
     * the corresponding rectEffects2 step. The machine counts a component done
     * when it was already equal, or when a step overshoots and is clamped; a step
     * landing exactly on the target is counted on the next update.
     *
     * Each step uses FST (not FSTP): e.g. 0x3005839f writes the rounded sum to
     * rectClient.x but KEEPS the 80-bit result, and the following FCOMP against
     * rectEffects.x tests that UNROUNDED value (Class 8). Model every step with a
     * long double live value and an explicit (float) store of the rounded copy. */
    if (((uint32_t)item->window.flags & WINDOW_INTRANSITION) != 0) {
        displayContextDef_t *display = DC;
        int32_t realTime = display->realTime;

        if (realTime > item->window.nextTime) {
            int32_t finished = 0;

            item->window.nextTime = coduo_int32_from_bits((uint32_t)realTime + (uint32_t)item->window.offsetTime);

            if (item->window.rectClient.x == item->window.rectEffects.x) {
                finished++;
            } else if (item->window.rectClient.x < item->window.rectEffects.x) {
                long double v = (long double)item->window.rectClient.x + item->window.rectEffects2.x;
                item->window.rectClient.x = (float)v;
                if (v > item->window.rectEffects.x) {
                    item->window.rectClient.x = item->window.rectEffects.x;
                    finished++;
                }
            } else {
                long double v = (long double)item->window.rectClient.x - item->window.rectEffects2.x;
                item->window.rectClient.x = (float)v;
                if (v < item->window.rectEffects.x) {
                    item->window.rectClient.x = item->window.rectEffects.x;
                    finished++;
                }
            }

            if (item->window.rectClient.y == item->window.rectEffects.y) {
                finished++;
            } else if (item->window.rectClient.y < item->window.rectEffects.y) {
                long double v = (long double)item->window.rectClient.y + item->window.rectEffects2.y;
                item->window.rectClient.y = (float)v;
                if (v > item->window.rectEffects.y) {
                    item->window.rectClient.y = item->window.rectEffects.y;
                    finished++;
                }
            } else {
                long double v = (long double)item->window.rectClient.y - item->window.rectEffects2.y;
                item->window.rectClient.y = (float)v;
                if (v < item->window.rectEffects.y) {
                    item->window.rectClient.y = item->window.rectEffects.y;
                    finished++;
                }
            }

            if (item->window.rectClient.w == item->window.rectEffects.w) {
                finished++;
            } else if (item->window.rectClient.w < item->window.rectEffects.w) {
                long double v = (long double)item->window.rectClient.w + item->window.rectEffects2.w;
                item->window.rectClient.w = (float)v;
                if (v > item->window.rectEffects.w) {
                    item->window.rectClient.w = item->window.rectEffects.w;
                    finished++;
                }
            } else {
                long double v = (long double)item->window.rectClient.w - item->window.rectEffects2.w;
                item->window.rectClient.w = (float)v;
                if (v < item->window.rectEffects.w) {
                    item->window.rectClient.w = item->window.rectEffects.w;
                    finished++;
                }
            }

            if (item->window.rectClient.h == item->window.rectEffects.h) {
                finished++;
            } else if (item->window.rectClient.h < item->window.rectEffects.h) {
                long double v = (long double)item->window.rectClient.h + item->window.rectEffects2.h;
                item->window.rectClient.h = (float)v;
                if (v > item->window.rectEffects.h) {
                    item->window.rectClient.h = item->window.rectEffects.h;
                    finished++;
                }
            } else {
                long double v = (long double)item->window.rectClient.h - item->window.rectEffects2.h;
                item->window.rectClient.h = (float)v;
                if (v < item->window.rectEffects.h) {
                    item->window.rectClient.h = item->window.rectEffects.h;
                    finished++;
                }
            }

            Item_UpdatePosition(item);
            if (finished == 4) {
                item->window.flags = (int32_t)((uint32_t)item->window.flags & ~WINDOW_INTRANSITION);
            }
        }
    }

    /* 0x300584d8..0x30058501: an owner-draw visibility predicate, when present,
     * directly controls WINDOW_VISIBLE. */
    {
        int32_t ownerDrawFlags = item->window.ownerDrawFlags;

        if (ownerDrawFlags != 0) {
            ui_ownerDrawVisible_t ownerDrawVisible;

            display = DC;
            ownerDrawVisible = display->ownerDrawVisible;
            if (ownerDrawVisible != NULL) {
                if (ownerDrawVisible(ownerDrawFlags) != 0) {
                    item->window.flags = (int32_t)((uint32_t)item->window.flags | WINDOW_VISIBLE);
                } else {
                    item->window.flags = (int32_t)((uint32_t)item->window.flags & ~WINDOW_VISIBLE);
                }
            }
        }
    }

    /* 0x30058504..0x30058525: show/hide cvar rules can suppress painting;
     * ordinary hidden items stop here as well. */
    if (((uint8_t)item->cvarFlags & (uint8_t)ITEM_CVAR_SHOW_MASK) != 0 && Item_EnableShowViaCvar(item, ITEM_CVAR_SHOW) == qfalse) {
        return;
    }
    if (((uint32_t)item->window.flags & WINDOW_VISIBLE) == 0) {
        return;
    }

    /* 0x3005852b..0x30058568: style 6 treats the item's cvar value as a shader
     * name and refreshes the window background handle each paint. */
    if (item->window.style == WINDOW_STYLE_SHADER_NO_TINT) {
        char shaderName[ITEM_SHADER_NAME_CAPACITY];
        const char *cvar = item->cvar;
        int32_t loadMode;

        display = DC;
        display->getCVarString(cvar, shaderName, ITEM_SHADER_NAME_CAPACITY);
        loadMode = item->loadMode;
        display = DC;
        item->window.background = display->registerShaderNoMip(shaderName, loadMode);
    }

    /* COMPATIBILITY_PATCH (NOT_FROM_ORIGINAL_SOURCE): the isolated adapter is
     * inert for ordinary/open menus. During Menu_PaintAll's passive HUD pass it
     * translates this complete item only for the paint phase, after the stock
     * animation and visibility state above has settled. */
    compatPaintOffset = client_ui_compat_begin_item_paint(item);

    Window_Paint(&item->window, parent->fadeAmount, parent->fadeInAmount, parent->fadeClamp, (float)parent->fadeCycle);

    if (debugMode != 0) {
        rectDef_t *rect = Item_CorrectedTextRect(item);

        DC->drawRect(rect->x, rect->y, rect->w, rect->h, 1.0f, itemPaintDebugColor);
    }

    /* Style 6 is window-only; it deliberately bypasses type-specific content. */
    if (item->window.style == WINDOW_STYLE_SHADER_NO_TINT) {
        client_ui_compat_end_item_paint(item, compatPaintOffset);
        return;
    }

    /* 0x300585f3..0x300586e7: exact 16-entry item-type jump table. Types 2, 3,
     * and 5 have no specialized painter and share the default return. */
    switch (item->type) {
    case ITEM_TYPE_TEXT:
    case ITEM_TYPE_BUTTON:
        Item_Text_Paint(item);
        break;

    case ITEM_TYPE_EDITFIELD:
    case ITEM_TYPE_NUMERICFIELD:
    case ITEM_TYPE_UPREDITFIELD:
        Item_TextField_Paint(item);
        break;

    case ITEM_TYPE_LISTBOX:
        Item_ListBox_Paint(item);
        break;

    case ITEM_TYPE_MODEL:
    case ITEM_TYPE_MENUMODEL:
        Item_Model_Paint(item);
        break;

    case ITEM_TYPE_OWNERDRAW:
        Item_OwnerDraw_Paint(item);
        break;

    case ITEM_TYPE_SLIDER:
        Item_Slider_Paint(item);
        break;

    case ITEM_TYPE_YESNO:
        Item_YesNo_Paint(item);
        break;

    case ITEM_TYPE_MULTI:
        Item_Multi_Paint(item);
        break;

    case ITEM_TYPE_BIND:
        Item_Bind_Paint(item);
        break;

    default:
        break;
    }

    client_ui_compat_end_item_paint(item, compatPaintOffset);
}
