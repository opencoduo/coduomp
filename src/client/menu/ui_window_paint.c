#include "ui_runtime.h"
#include "ui_menu_globals.h"
#include "ui_parse.h"
#include "compat/coduo_fp_conversion.h"
#include "compat/coduo_int32_bits.h"

#include <stddef.h>
#include <stdint.h>

enum {
    UI_CINEMATIC_UNINITIALIZED = -1,
    UI_CINEMATIC_FAILED = -2
};

/*
 * The retained Windows cgame/UI bodies are instruction twins after rebasing
 * calls, globals, strings, and constants:
 *
 *                          cgame       UI
 * GradientBar_Paint        0x300509f0  0x40012510
 * Window_Paint             0x30050ad0  0x400125f0
 * Menu_Paint               0x30058bf0  0x4001a760
 *
 * The exact GradientBar_Paint name comes from both Mac client modules.  It
 * supersedes cgame's reconstruction-local CG_OwnerDraw_PaintAssetShader name;
 * the two PE32 helper bodies are byte-for-byte equivalent after relocating DC.
 */

void GradientBar_Paint(const rectDef_t *rect, const vec4_t color)
{
    displayContextDef_t *context = DC;

    context->setColor(color);
    context = DC;
    context->drawHandlePic(rect->x, rect->y, rect->w, rect->h, context->gradientBar);
    context = DC;
    context->setColor(NULL);
}

void Window_Paint(windowDef_t *window, float fadeAmount, float fadeInAmount, float fadeClamp, float fadeCycle)
{
    rectDef_t rect = window->rect;
    /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
    vec4_t teamColor = {0.0f, 0.0f, 0.0f, 0.0f};

    if (debugMode != 0) {
        teamColor[0] = 1.0f;
        teamColor[1] = 1.0f;
        teamColor[2] = 1.0f;
        teamColor[3] = 1.0f;
        DC->drawRect(window->rect.x, window->rect.y, window->rect.w, window->rect.h, 1.0f, teamColor);
    }

    if (window->style == WINDOW_STYLE_EMPTY && window->border == WINDOW_BORDER_NONE) {
        return;
    }
    if (window->border != WINDOW_BORDER_NONE) {
        rect.x += window->borderSize;
        rect.y += window->borderSize;
        rect.w -= window->borderSize + 1.0f;
        rect.h -= window->borderSize + 1.0f;
    }

    switch ((windowStyle_t)window->style) {
    case WINDOW_STYLE_EMPTY:
        break;

    case WINDOW_STYLE_FILLED:
        if (window->background != 0) {
            if (((uint32_t)window->flags & (WINDOW_FADINGOUT | WINDOW_FADINGIN)) != 0u && DC->realTime > window->nextTime) {
                int32_t cycleMs = coduo_fp_to_i32_extended(fadeCycle);
                int32_t realTime = DC->realTime;

                window->nextTime = coduo_int32_from_bits((uint32_t)realTime + (uint32_t)cycleMs);
                if (((uint32_t)window->flags & WINDOW_FADINGOUT) != 0u) {
                    /* Both originals store the rounded binary32 alpha but
                     * compare the retained PC=53 result. */
                    double newAlpha = (double)window->backColor[3] - fadeAmount;

                    window->backColor[3] = (float)newAlpha;
                    if (newAlpha <= 0.0) {
                        window->flags &= ~(int32_t)(WINDOW_FADINGOUT | WINDOW_VISIBLE);
                    }
                } else {
                    double newAlpha = (double)window->backColor[3] + fadeInAmount;

                    window->backColor[3] = (float)newAlpha;
                    if (newAlpha >= fadeClamp) {
                        window->backColor[3] = fadeClamp;
                        window->flags &= ~(int32_t)WINDOW_FADINGIN;
                    }
                }
            }
            DC->setColor(window->backColor);
            DC->drawHandlePic(rect.x, rect.y, rect.w, rect.h, window->background);
            DC->setColor(NULL);
        } else {
            DC->fillRect(rect.x, rect.y, rect.w, rect.h, window->backColor);
        }
        break;

    case WINDOW_STYLE_GRADIENT:
        GradientBar_Paint(&rect, window->backColor);
        break;

    case WINDOW_STYLE_TEAMCOLOR: {
        ui_getTeamColor_t getTeamColor = DC->getTeamColor;

        if (getTeamColor != NULL) {
            getTeamColor(teamColor);
            DC->fillRect(rect.x, rect.y, rect.w, rect.h, teamColor);
        }
        break;
    }

    case WINDOW_STYLE_CINEMATIC:
        if (window->cinematic == UI_CINEMATIC_UNINITIALIZED) {
            window->cinematic = DC->playCinematic(window->cinematicName, rect.x, rect.y, rect.w, rect.h);
            if (window->cinematic == UI_CINEMATIC_UNINITIALIZED) {
                window->cinematic = UI_CINEMATIC_FAILED;
            }
        }
        if (window->cinematic >= 0) {
            DC->runCinematicFrame(window->cinematic);
            DC->drawCinematic(window->cinematic, rect.x, rect.y, rect.w, rect.h);
        }
        break;

    case WINDOW_STYLE_SHADER_NO_TINT:
        if (window->background == 0) {
            break;
        }
        /* fall through */
    case WINDOW_STYLE_SHADER:
        if (((uint32_t)window->flags & WINDOW_FORECOLORSET) != 0u) {
            DC->setColor(window->foreColor);
        }
        DC->drawHandlePic(rect.x, rect.y, rect.w, rect.h, window->background);
        DC->setColor(NULL);
        break;

    default:
        break;
    }

    switch ((windowBorder_t)window->border) {
    case WINDOW_BORDER_FULL:
        if (window->style == WINDOW_STYLE_TEAMCOLOR) {
            teamColor[1] = 0.5f;
            if (teamColor[0] > 0.0f) {
                teamColor[0] = 1.0f;
                teamColor[2] = 0.5f;
            } else {
                teamColor[0] = 0.5f;
                teamColor[2] = 1.0f;
            }
            teamColor[3] = 1.0f;
            DC->drawRect(window->rect.x, window->rect.y, window->rect.w, window->rect.h, window->borderSize, teamColor);
        } else {
            DC->drawRect(window->rect.x, window->rect.y, window->rect.w, window->rect.h, window->borderSize, window->borderColor);
        }
        return;

    case WINDOW_BORDER_HORIZONTAL:
        DC->setColor(window->borderColor);
        DC->drawTopBottom(window->rect.x, window->rect.y, window->rect.w, window->rect.h, window->borderSize);
        DC->setColor(NULL);
        return;

    case WINDOW_BORDER_VERTICAL:
        DC->setColor(window->borderColor);
        DC->drawSides(window->rect.x, window->rect.y, window->rect.w, window->rect.h, window->borderSize);
        DC->setColor(NULL);
        return;

    case WINDOW_BORDER_KCGRADIENT:
        rect = window->rect;
        rect.h = window->borderSize;
        GradientBar_Paint(&rect, window->borderColor);
        rect.y = window->rect.y + window->rect.h - 1.0f;
        GradientBar_Paint(&rect, window->borderColor);
        return;

    case WINDOW_BORDER_NONE:
    default:
        return;
    }
}

void Menu_Paint(menuDef_t *menu, qboolean forcePaint)
{
    displayContextDef_t *display;
    int32_t index;

    if (menu == NULL) {
        return;
    }
    if (((uint32_t)menu->window.flags & WINDOW_VISIBLE) == 0u && !forcePaint) {
        return;
    }
    if (menu->window.ownerDrawFlags != 0) {
        int32_t ownerDrawFlags = menu->window.ownerDrawFlags;
        ui_ownerDrawVisible_t ownerDrawVisible;

        display = DC;
        ownerDrawVisible = display->ownerDrawVisible;
        if (ownerDrawVisible != NULL && !ownerDrawVisible(ownerDrawFlags)) {
            return;
        }
    }

    if (forcePaint) {
        menu->window.flags |= (int32_t)WINDOW_MOUSE_INTERACTIVE;
        Menus_AddToStack(menu);
    }
    if (menu->fullScreen != 0) {
        qhandle_t background = menu->window.background;

        display = DC;
        display->drawHandlePic(0.0f, 0.0f, 640.0f, 480.0f, background);
    }

    Window_Paint(&menu->window, menu->fadeAmount, menu->fadeInAmount, menu->fadeClamp, (float)menu->fadeCycle);
    for (index = 0; index < menu->itemCount; ++index) {
        Item_Paint(menu->items[index]);
    }

    if (debugMode != 0) {
        vec4_t color;

        color[0] = 1.0f;
        color[1] = 0.0f;
        color[2] = 1.0f;
        color[3] = 1.0f;
        display = DC;
        display->drawRect(menu->window.rect.x, menu->window.rect.y, menu->window.rect.w, menu->window.rect.h, 1.0f, color);
    }
}
