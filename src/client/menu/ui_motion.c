#include "ui_runtime.h"
#include "ui_parse.h"
#include "compat/coduo_fp_conversion.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

/*
 * The authoritative Windows cgame/UI bodies have identical instructions after
 * rebasing calls and the 1.0f constant:
 *
 *                                  cgame       UI
 * Menu_TransitionItemByName        0x30051d90  0x400138e0
 * Script_Transition                0x30051f40  0x40013a90
 * Menu_OrbitItemByName             0x30052010  0x40013b60
 * Script_Orbit                     0x30052100  0x40013c50
 *
 * The shared signatures use the canonical menu-first ui_shared ordering.  The
 * PE optimizer passes the name in EAX (and the orbit menu in EDI), which had
 * been mistaken for source parameter order in cgame's local reconstruction.
 */

static int32_t ui_motion_delta_to_i32(float destination, float source)
{
    double difference;

    /* NOT_FROM_ORIGINAL_SOURCE: source-level spelling of the repeated in-line
     * FLD/FSUB/_ftol2 operation. Windows runs it under x87 PC=53, so a binary64
     * carrier preserves the retained result on SSE/NEON and 64-bit hosts. */
    difference = (double)destination - (double)source;
    return coduo_fp_to_i32_extended((long double)difference);
}

static int32_t ui_motion_abs_i32(int32_t value)
{
    /* NOT_FROM_ORIGINAL_SOURCE: portable spelling of the original CDQ/XOR/SUB
     * absolute-value sequence, including its INT32_MIN result. */
    if (value == INT32_MIN) {
        return INT32_MIN;
    }
    return value < 0 ? -value : value;
}

void Menu_TransitionItemByName(menuDef_t *menu, const char *name, rectDef_t rectFrom, rectDef_t rectTo, int32_t time, float amount)
{
    const int32_t count = Menu_ItemsMatchingGroup(menu, name);
    int32_t index;

    for (index = 0; index < count; ++index) {
        itemDef_t *item = Menu_GetMatchingItemByNumber(menu, name, index);
        windowDef_t *window;
        menuDef_t *parent;
        double velocityScale;
        double parentX;
        double parentY;
        float savedParentX;
        float savedParentY;
        double itemX;
        float itemY;
        int32_t delta;

        if (item == NULL) {
            continue;
        }

        /* The original recomputes this retained x87 value for each non-NULL
         * match and keeps it live through the four component stores. */
        velocityScale = 1.0 / (double)amount;
        window = &item->window;
        window->flags |= WINDOW_INTRANSITION | WINDOW_VISIBLE;
        window->offsetTime = time;
        memcpy(&window->rectClient, &rectFrom, sizeof(rectFrom));
        memcpy(&window->rectEffects, &rectTo, sizeof(rectTo));

        /* Conversion and store are interleaved by lane in both binaries. */
        delta = ui_motion_delta_to_i32(rectTo.x, rectFrom.x);
        window->rectEffects2.x = (float)((double)ui_motion_abs_i32(delta) * velocityScale);
        delta = ui_motion_delta_to_i32(rectTo.y, rectFrom.y);
        window->rectEffects2.y = (float)((double)ui_motion_abs_i32(delta) * velocityScale);
        delta = ui_motion_delta_to_i32(rectTo.w, rectFrom.w);
        window->rectEffects2.w = (float)((double)ui_motion_abs_i32(delta) * velocityScale);
        delta = ui_motion_delta_to_i32(rectTo.h, rectFrom.h);
        window->rectEffects2.h = (float)((double)ui_motion_abs_i32(delta) * velocityScale);

        parent = item->parent;
        if (parent == NULL) {
            continue;
        }

        parentX = (double)parent->window.rect.x;
        parentY = (double)parent->window.rect.y;
        savedParentX = (float)parentX;
        savedParentY = (float)parentY;
        if (parent->window.border != 0) {
            parentX += (double)parent->window.borderSize;
            parentY += (double)parent->window.borderSize;
            savedParentX = (float)parentX;
            savedParentY = (float)parentY;
        }
        if (window->border != 0) {
            itemX = (double)savedParentX + (double)window->borderSize;
            itemY = (float)((double)savedParentY + (double)window->borderSize);
        } else {
            itemX = parentX;
            itemY = savedParentY;
        }

        window->rect.w = window->rectClient.w;
        window->rect.h = window->rectClient.h;
        window->rect.x = (float)(itemX + (double)window->rectClient.x);
        window->rect.y = (float)((double)itemY + (double)window->rectClient.y);
        item->textRect.w = 0.0f;
        item->textRect.h = 0.0f;
    }
}

void Script_Transition(itemDef_t *item, char **arguments)
{
    const char *name;
    rectDef_t rectFrom;
    rectDef_t rectTo;
    int32_t time;
    float amount;

    if (!String_Parse(arguments, &name) || !Rect_Parse(arguments, &rectFrom) || !Rect_Parse(arguments, &rectTo) ||
        !Int_Parse(arguments, &time) || !Float_Parse(arguments, &amount)) {
        return;
    }
    Menu_TransitionItemByName(item->parent, name, rectFrom, rectTo, time, amount);
}

void Menu_OrbitItemByName(menuDef_t *menu, const char *name, float startX, float startY, float centerX, float centerY, int32_t time)
{
    const int32_t count = Menu_ItemsMatchingGroup(menu, name);
    int32_t index;

    for (index = 0; index < count; ++index) {
        itemDef_t *item = Menu_GetMatchingItemByNumber(menu, name, index);
        windowDef_t *window;
        menuDef_t *parent;
        double parentX;
        double parentY;
        float savedParentX;
        float savedParentY;
        double itemX;
        float itemY;

        if (item == NULL) {
            continue;
        }

        window = &item->window;
        window->flags |= WINDOW_ORBITING | WINDOW_VISIBLE;
        window->offsetTime = time;
        memcpy(&window->rectEffects.y, &centerY, sizeof(centerY));
        memcpy(&window->rectClient.y, &startY, sizeof(startY));

        /* These two stores occur after the parent read in the PE32 bodies. */
        parent = item->parent;
        memcpy(&window->rectEffects.x, &centerX, sizeof(centerX));
        memcpy(&window->rectClient.x, &startX, sizeof(startX));
        if (parent == NULL) {
            continue;
        }

        parentX = (double)parent->window.rect.x;
        parentY = (double)parent->window.rect.y;
        savedParentX = (float)parentX;
        savedParentY = (float)parentY;
        if (parent->window.border != 0) {
            parentX += (double)parent->window.borderSize;
            parentY += (double)parent->window.borderSize;
            savedParentX = (float)parentX;
            savedParentY = (float)parentY;
        }
        if (window->border != 0) {
            itemX = (double)savedParentX + (double)window->borderSize;
            itemY = (float)((double)savedParentY + (double)window->borderSize);
        } else {
            itemX = parentX;
            itemY = savedParentY;
        }

        window->rect.w = window->rectClient.w;
        window->rect.h = window->rectClient.h;
        window->rect.x = (float)(itemX + (double)startX);
        window->rect.y = (float)((double)itemY + (double)startY);
        item->textRect.w = 0.0f;
        item->textRect.h = 0.0f;
    }
}

void Script_Orbit(itemDef_t *item, char **arguments)
{
    const char *name;
    float startX;
    float startY;
    float centerX;
    float centerY;
    int32_t time;

    /* Direct stack tracing of both exact PE32 bodies proves this token order.
     * Cgame's former startY/startX local spelling was reversed. */
    if (!String_Parse(arguments, &name) || !Float_Parse(arguments, &startX) || !Float_Parse(arguments, &startY) ||
        !Float_Parse(arguments, &centerX) || !Float_Parse(arguments, &centerY) || !Int_Parse(arguments, &time)) {
        return;
    }
    Menu_OrbitItemByName(item->parent, name, startX, startY, centerX, centerY, time);
}
