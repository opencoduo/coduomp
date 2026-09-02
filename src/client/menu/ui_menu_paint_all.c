// Sources: uo_cgame_mp_x86.dll 0x3005adc0..0x3005b0f6 and
//          uo_ui_mp_x86.dll    0x4001cae0..0x4001ce16.
// The complete normalized PE32 instruction streams are identical.
//
// Menu_PaintAll (ui_shared.c) — paint every visible menu for one frame.
//
// The .mcode header's mechanical name `SP_func_door_rotating` is a size-only
// guess (win size 0x336 ~ matched 0x338) and is REJECTED: this function performs
// no entity spawning. Its behavior is the classic Quake3 ui_shared.c
// Menu_PaintAll, extended for UO with a two-pass paint:
//
//   1. run the per-frame capture callback (captureFunc(captureData)) if armed;
//   2. paint every registered menu in Menus[] (0..menuCount) that is VISIBLE and
//      NOT already on the open-menu stack (those are drawn in pass 3);
//   3. paint every menu on the open-menu stack menuStack[] (0..openMenuCount);
//   4. when debugMode is set, draw "fps: %f" at (5,25) via DC->drawText.
//
// Each menu is painted inline (not via Menu_Paint): fullscreen background through
// DC->drawHandlePic, window border through Window_Paint, then each item through
// Item_Paint, plus a magenta debug-outline rectangle through DC->drawRect when
// debugMode is set.
//
// Calling convention: standard __cdecl, no arguments (RET with no immediate;
// callee saves EBX/EBP/ESI/EDI). Names for the globals/callees/DC members it
// touches are resolved in globals.h and client_recovered.h.

#include "ui_runtime.h"

#include "compat/coduo_int32_bits.h"
#include "ui_menu_globals.h"

#include <stddef.h>
#include <stdint.h>

extern displayContextDef_t *DC;

/*
 * Paint one menu inline: its fullscreen background, its window border, all of its
 * items, and (in debugMode) a magenta debug outline. This body is emitted twice
 * in the machine code (once per loop, 0x3005ae1f.. and 0x3005af7f..) with
 * identical structure; expressed once here as a helper.
 *
 * The visibility gate (menu != NULL, flags & WINDOW_VISIBLE) is applied by each
 * caller loop before reaching this body, matching the machine code.
 */
/* NOT_FROM_ORIGINAL_SOURCE: source-factoring helper for the two inlined retail
 * paint bodies; the project-qualified name keeps it distinct from recovered
 * original functions. */
static void client_ui_compat_paint_menu(menuDef_t *menu,
                                        qboolean passiveHudPass)
{
    float passiveHudOffset = 0.0f;
    float openMenuPreviousXScale = 0.0f;

    if (client_ui_compat_should_skip_menu_paint(
            menu, passiveHudPass) != qfalse) {
        return;
    }

    /* menu->window.ownerDrawFlags (+0x40): if set, consult DC->ownerDrawVisible and
     * bail out entirely when the element reports not-visible. The DC slot is
     * NULL-checked first. (0x3005ae29 / 0x3005af89) */
    int32_t ownerDraw = menu->window.ownerDrawFlags;
    if (ownerDraw != 0) {
        if (DC->ownerDrawVisible != 0) {
            if (DC->ownerDrawVisible(ownerDraw) == 0) {
                return;
            }
        }
    }

    client_ui_compat_begin_menu_paint(
        menu, passiveHudPass, &passiveHudOffset,
        &openMenuPreviousXScale);

    /* Fullscreen background (0x3005ae49 / 0x3005afaa): when menu->fullScreen is
     * set, draw menu->window.background across the whole virtual 640x480 screen. */
    if (menu->fullScreen != 0) {
        DC->drawHandlePic(0.0f, 0.0f, 640.0f, 480.0f, menu->window.background);
    }

    /* Window border/decoration (0x3005ae6e / 0x3005afd5). The integer fadeCycle
     * at +0xcc is converted to float (FILD) and passed as the last arg; the
     * window pointer rides in EDI. */
    Window_Paint((windowDef_t *)menu,
                 menu->fadeAmount,
                 menu->fadeInAmount,
                 menu->fadeClamp,
                 (float)menu->fadeCycle);

    client_ui_compat_finish_menu_window_paint(
        menu, passiveHudPass, passiveHudOffset);

    /* Items (0x3005ae94 / 0x3005aff9): paint each of menu->items[0..itemCount).
     * itemCount is a signed dword; the loop runs only while (index < itemCount),
     * so a non-positive count paints nothing. */
    {
        int32_t itemCount = menu->itemCount;
        int32_t i;
        for (i = 0; i < itemCount;
             i = coduo_int32_from_bits((uint32_t)i + 1u)) {
            Item_Paint(menu->items[i]);
            /* itemCount is re-read from menu->itemCount each iteration in the
             * machine code (Item_Paint may change it). */
            itemCount = menu->itemCount;
        }
    }

    client_ui_compat_finish_menu_items(menu, passiveHudPass);

    /* Debug outline (0x3005aec5 / 0x3005b026): when debugMode is set, stroke a
     * magenta {1,0,1,1} rectangle at the menu's client rect with line width 1. */
    if (debugMode != 0) {
        vec4_t debugColor;
        float debugX = menu->window.rect.x;
        debugColor[0] = 1.0f;
        debugColor[1] = 0.0f;
        debugColor[2] = 1.0f;
        debugColor[3] = 1.0f;
        debugX += passiveHudOffset;
        DC->drawRect(debugX, menu->window.rect.y,
                       menu->window.rect.w, menu->window.rect.h,
                       1.0f, debugColor);
    }

    client_ui_compat_end_menu_paint(
        menu, passiveHudPass, openMenuPreviousXScale);
}

/* Return qtrue if `menu` is currently present on the open-menu stack. The machine
 * code (0x3005adf6..0x3005ae0f) scans menuStack[openMenuCount-1 .. 0]
 * downward and stops at the first match. */
/* NOT_FROM_ORIGINAL_SOURCE: source-factoring helper for the inline reverse
 * scan at 0x3005adf6..0x3005ae0f. */
static qboolean client_ui_compat_menu_is_on_open_stack(const menuDef_t *menu)
{
    uint32_t indexBits = (uint32_t)openMenuCount - 1u;
    while (coduo_int32_from_bits(indexBits) >= 0) {
        if (menuStack[indexBits] == menu) {
            return qtrue;
        }
        indexBits -= 1u;
    }
    return qfalse;
}

void Menu_PaintAll(void)
{
    int32_t i;

    /* Per-frame capture callback (0x3005adc0): run the armed auto-repeat handler
     * (e.g. a held scroll arrow) with its opaque data. */
    if (captureFunc != 0) {
        captureFunc(captureData);
    }

    /* Pass 1 (0x3005adeb..0x3005af2a): paint every registered menu that is
     * visible and not on the open stack. menuCount is a signed dword; JLE at the
     * top skips the whole loop when it is <= 0. */
    i = 0;
    if (menuCount > 0) {
        do {
            menuDef_t *menu = &Menus[i];

            /* Skip menus already on the open-menu stack: those are painted in
             * pass 2 (avoids double-painting). */
            if (!client_ui_compat_menu_is_on_open_stack(menu) &&
                menu != 0 &&
                (menu->window.flags & (int32_t)WINDOW_VISIBLE) != 0) {
                client_ui_compat_paint_menu(menu, qtrue);
            }

            /* 0x3005af13..0x3005af2a reloads menuCount before the dword INC. */
            int32_t currentCount = menuCount;
            i = coduo_int32_from_bits((uint32_t)i + 1u);
            if (i >= currentCount) {
                break;
            }
        } while (qtrue);
    }

    /* Pass 2 (0x3005af30..0x3005b07c): paint every menu on the open-menu stack.
     * The machine code first scans menuStack[openMenuCount-1 .. 0] for the
     * highest index whose entry has a nonzero menu->fullScreen field
     * (+0xbc, read as [ESI+0xbc]); it starts the paint
     * loop at that index and runs upward to openMenuCount. This reproduces the
     * "start below the topmost fullscreen menu" behavior. */
    {
        int32_t count = openMenuCount;
        int32_t start = 0;

        /* Find the highest-index stack entry whose fullScreen field is set; the
         * paint loop begins there (0x3005af36..0x3005af56). */
        uint32_t indexBits = (uint32_t)count - 1u;
        while (coduo_int32_from_bits(indexBits) >= 0) {
            menuDef_t *menu = menuStack[indexBits];
            if (menu->fullScreen != 0) {
                start = coduo_int32_from_bits(indexBits);
                break;
            }
            indexBits -= 1u;
        }

        /* Paint from `start` upward (0x3005af58..0x3005b07c). The loop's
         * terminal compare reloads openMenuCount after every paint body, so
         * callbacks may grow or shrink the remaining traversal. */
        i = start;
        if (i < count) {
            do {
                menuDef_t *menu = menuStack[i];

                /* Gate: entry non-NULL and WINDOW_VISIBLE set. */
                if (menu != 0 &&
                    (menu->window.flags & (int32_t)WINDOW_VISIBLE) != 0) {
                    client_ui_compat_paint_menu(menu, qfalse);
                }

                /* 0x3005b074 reloads the global before the dword INC EBX. */
                int32_t currentCount = openMenuCount;
                i = coduo_int32_from_bits((uint32_t)i + 1u);
                if (i >= currentCount) {
                    break;
                }
            } while (qtrue);
        }
    }

    /* Debug FPS readout (0x3005b082..0x3005b0eb): when debugMode is set, draw the
     * current FPS in white at screen (5, 25). The float DC->fps is promoted to
     * double for va()'s "%f", then DC->drawText renders the formatted string. */
    if (debugMode != 0) {
        displayContextDef_t *display = DC;
        float fps = display->fps;
        vec4_t textColor;
        const char *text;

        textColor[0] = 1.0f;
        textColor[1] = 1.0f;
        textColor[2] = 1.0f;
        textColor[3] = 1.0f;

        /* 0x3005b0e9: nine __cdecl args pushed (the three trailing style/limit/font
         * slots are all 0 here — pushed at 0x3005b096..0x3005b09a). */
        text = va("fps: %f", (double)fps);
        display->drawText(5.0f, 25.0f, 0, 0.5f, textColor, text, 0, 0, 0);
    }
}
