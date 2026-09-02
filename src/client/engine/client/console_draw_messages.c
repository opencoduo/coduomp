#include "console.h"

#include "../math/vector_math.h"
#include "../renderer/renderer_api.h"

#include "widescreen_2d_compat.h"

enum {
    CON_DRAW_MESSAGE_DEFAULT_MODE = 0,
    CON_CHAT_PROMPT_X = 8,
    CON_CHAT_PROMPT_Y_OFFSET = 16,
    CON_CHAT_PROMPT_TEXT_STYLE = 3,
    CON_HUD_TEXT_BASELINE_OFFSET = 12,
    CON_HUD_TEXT_STYLE = 3,
    CON_HUD_CENTERED_FONT = 4,
    CON_HUD_CENTERED_LINE_HEIGHT = 16,
    CON_HUD_LINE_HEIGHT = 12
};

typedef enum con_draw_message_mode_e {
    CON_DRAW_MESSAGE_TOP_DOWN = 0,
    CON_DRAW_MESSAGE_REVERSED = 1,
    CON_DRAW_MESSAGE_BOTTOM_UP = 2,
    CON_DRAW_MESSAGE_BOTTOM_UP_CENTERED = 3
} con_draw_message_mode_t;

/* Source: CoDUOMP.exe 0x0040a780..0x0040a822.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0040a780_0040a823.mcode.
 * Name: exact same-module Mac symbol Con_DrawStringOnHUD. The original carries
 * encodedText and encodedCount in EBX/EDI; the maintained signature makes all
 * inputs explicit. */
void Con_DrawStringOnHUD(const uint16_t *encodedText, int32_t encodedCount, int32_t x, int32_t y, float alpha, qboolean centered)
{
    vec4_t color = {1.0f, 1.0f, 1.0f, alpha};
    int32_t fontHandle = 0;
    float scale = 0.25f;

    if (centered != qfalse) {
        fontHandle = CON_HUD_CENTERED_FONT;
        scale = 0.3333333432674408f;
        x -= rendererExports.TextConsoleWidth(encodedText, fontHandle, scale, 0.0f, encodedCount) / 2;
    }

    rendererExports.TextConsolePaint((float)x, (float)(y + CON_HUD_TEXT_BASELINE_OFFSET), fontHandle, scale, color, encodedText, 0.0f,
                                     encodedCount, CON_HUD_TEXT_STYLE);
}

/* Source: CoDUOMP.exe 0x0040a830..0x0040aaae.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0040a830_0040aaaf.mcode.
 * Name: exact same-module Mac symbol Con_DrawMessageWindowBottomUp. The Win32
 * function receives window in ESI; the maintained signature is portable. */
void Con_DrawMessageWindowBottomUp(console_message_window_t *window, int32_t x, int32_t y, float alpha, qboolean centered)
{
    const int32_t lineHeight = centered != qfalse ? CON_HUD_CENTERED_LINE_HEIGHT : CON_HUD_LINE_HEIGHT;
    const int32_t firstLine = window->activeLineIndex;
    const int32_t pastLastLine = window->activeLineIndex + window->lineCapacity;

    for (int32_t line = firstLine; line < pastLastLine; ++line) {
        const int32_t slot = line % window->lineCapacity;
        const int32_t startTime = window->lineStartTimes[slot];

        if (startTime == 0)
            continue;
        if (startTime > (int32_t)Sys_Milliseconds()) {
            window->lineStartTimes[slot] = 0;
            continue;
        }

        const int32_t elapsed = (int32_t)(Sys_Milliseconds() - (uint32_t)startTime);
        if (elapsed < window->scrollTime) {
            const float displacement = (1.0f - (float)elapsed / (float)window->scrollTime) * (float)lineHeight;
            y += FastRound(displacement);
        }
    }

    for (int32_t line = pastLastLine - 1; line >= firstLine; --line) {
        const int32_t slot = line % window->lineCapacity;
        const int32_t startTime = window->lineStartTimes[slot];
        float lineAlpha;

        if (startTime == 0)
            continue;
        if ((int32_t)(Sys_Milliseconds() - (uint32_t)window->lineEndTimes[slot]) >= 0) {
            window->lineStartTimes[slot] = 0;
            continue;
        }

        const int32_t elapsed = (int32_t)(Sys_Milliseconds() - (uint32_t)startTime);
        if (elapsed < window->fadeInTime) {
            lineAlpha = (float)elapsed * alpha / (float)window->fadeInTime;
        } else {
            const int32_t remaining = (int32_t)((uint32_t)window->lineEndTimes[slot] - Sys_Milliseconds());
            if (remaining < window->fadeOutTime) {
                lineAlpha = (float)remaining * alpha / (float)window->fadeOutTime;
            } else {
                lineAlpha = alpha;
            }
        }

        const int32_t consoleLine = window->lineIndices[slot];
        const uint16_t *encodedText = &con.text[(consoleLine % con.totalLines) * con.lineWidth];
        y -= lineHeight;
        Con_DrawStringOnHUD(encodedText, con.lineWidth, x, y, lineAlpha, centered);
    }
}

/* Source: CoDUOMP.exe 0x0040aab0..0x0040adaf.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0040aab0_0040adb0.mcode.
 * Name: exact same-module Mac symbol Con_DrawMessageWindow. The original uses
 * EAX/EDX/ECX for drawMode/window/y; the maintained signature makes the
 * boundary explicit. */
void Con_DrawMessageWindow(console_message_window_t *window, int32_t x, int32_t y, float alpha, int32_t drawMode)
{
    if (cl.snap.ps.pmType != PM_TYPE_INTERMISSION && (cls.keyCatchers & (KEYCATCH_UI | KEYCATCH_CGAME)) != 0) {
        return;
    }

    if (drawMode == CON_DRAW_MESSAGE_BOTTOM_UP || drawMode == CON_DRAW_MESSAGE_BOTTOM_UP_CENTERED) {
        Con_DrawMessageWindowBottomUp(window, x, y, alpha, drawMode == CON_DRAW_MESSAGE_BOTTOM_UP_CENTERED);
        return;
    }
    if (drawMode != CON_DRAW_MESSAGE_TOP_DOWN && drawMode != CON_DRAW_MESSAGE_REVERSED) {
        return;
    }

    const qboolean reversed = drawMode == CON_DRAW_MESSAGE_REVERSED ? qtrue : qfalse;
    const int32_t firstLine = window->activeLineIndex;
    const int32_t pastLastLine = window->activeLineIndex + window->lineCapacity;
    int32_t drawY = reversed != qfalse ? y - CON_HUD_LINE_HEIGHT : y;
    int32_t groupedLineCountdown = 0;
    int32_t groupedLineBaseY = drawY;

    for (int32_t line = firstLine; line < pastLastLine; ++line) {
        const int32_t slot = line % window->lineCapacity;
        const int32_t startTime = window->lineStartTimes[slot];

        if (startTime == 0)
            continue;
        if (startTime > (int32_t)Sys_Milliseconds()) {
            window->lineStartTimes[slot] = 0;
            continue;
        }

        if ((int32_t)(Sys_Milliseconds() - (uint32_t)window->lineEndTimes[slot]) > 0) {
            const int32_t remaining = (int32_t)((uint32_t)window->lineEndTimes[slot] + (uint32_t)window->scrollTime - Sys_Milliseconds());
            if (remaining <= 0)
                continue;

            const float displacement = (float)remaining / (float)window->scrollTime * (float)CON_HUD_LINE_HEIGHT;
            if (reversed != qfalse)
                drawY -= FastRound(displacement);
            else
                drawY += FastRound(displacement);
            continue;
        }

        float lineAlpha = alpha;
        const int32_t elapsed = (int32_t)(Sys_Milliseconds() - (uint32_t)startTime);
        if (elapsed < window->fadeInTime) {
            lineAlpha = (float)elapsed / (float)window->fadeInTime * alpha;
        } else {
            const int32_t remaining = (int32_t)((uint32_t)window->lineEndTimes[slot] - Sys_Milliseconds());
            if (remaining < window->fadeOutTime) {
                lineAlpha = (float)remaining / (float)window->fadeOutTime * alpha;
            }
        }

        const int32_t consoleLine = window->lineIndices[slot];
        const uint16_t *encodedText = &con.text[(consoleLine % con.totalLines) * con.lineWidth];

        if (reversed != qfalse) {
            if (groupedLineCountdown != 0) {
                drawY += 2 * CON_HUD_LINE_HEIGHT;
            } else {
                int32_t matchingLineCount = 0;
                for (int32_t nextLine = line + 1; nextLine < pastLastLine; ++nextLine) {
                    const int32_t nextSlot = nextLine % window->lineCapacity;
                    if (window->lineStartTimes[nextSlot] != startTime)
                        break;
                    ++matchingLineCount;
                }

                if (matchingLineCount != 0) {
                    drawY -= matchingLineCount * CON_HUD_LINE_HEIGHT;
                    groupedLineBaseY = drawY;
                    groupedLineCountdown = matchingLineCount + 1;
                }
            }
        }

        Con_DrawStringOnHUD(encodedText, con.lineWidth, x, drawY, lineAlpha, qfalse);

        if (reversed != qfalse) {
            if (groupedLineCountdown != 0) {
                --groupedLineCountdown;
                if (groupedLineCountdown == 0)
                    drawY = groupedLineBaseY;
            }
            drawY -= CON_HUD_LINE_HEIGHT;
        } else {
            drawY += CON_HUD_LINE_HEIGHT;
        }
    }
}

/* Source: CoDUOMP.exe 0x0040adc0..0x0040aedf.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0040adc0_0040aee0.mcode.
 * Name and signature: exact same-module Mac symbol Con_DrawSay. */
void Con_DrawSay(int32_t y)
{
    static const vec4_t color = {1.0f, 1.0f, 1.0f, 1.0f};
    const char *localizationKey;
    const char *prompt;
    int32_t promptWidth;
    int32_t promptX = CON_CHAT_PROMPT_X;

    if ((cls.keyCatchers & KEYCATCH_MESSAGE) == 0)
        return;

    /* COMPATIBILITY_PATCH (NOT_FROM_ORIGINAL_SOURCE): the chat prompt is the
     * left-edge sibling of the Con_DrawNotify feed directly above it; give
     * the complete prompt+field composition the same native left-edge
     * translation so the two columns stay aligned on widescreen. */
    promptX = coduomp_left_hud_virtual_x_compat(CON_CHAT_PROMPT_X);

    if (chat_team != qfalse)
        localizationKey = "EXE_SAYTEAM";
    else if (chat_squad != qfalse)
        localizationKey = "PATCH_1_5_SAYSQUAD";
    else
        localizationKey = "EXE_SAY";

    prompt = va("%s:", SEH_SafeTranslateString(localizationKey));
    rendererExports.TextPaint((float)promptX, (float)(y + CON_CHAT_PROMPT_Y_OFFSET), 0, 0.3333333432674408f, color, prompt, 0.0f, 0,
                              CON_CHAT_PROMPT_TEXT_STYLE);

    promptWidth = rendererExports.TextWidth(prompt, 0, 0.3333333432674408f, 0.0f, 0);
    Field_Draw(&chatField, promptWidth + promptX, y);
}

/* Source: CoDUOMP.exe 0x0040aee0..0x0040aefb.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0040aee0_0040aefc.mcode.
 * Name and signature: exact same-module Mac symbol Con_DrawNotify. */
void Con_DrawNotify(int32_t x, int32_t y, float alpha, int32_t drawMode)
{
    Con_DrawMessageWindow(&con_gameMessageWindow, x, y, alpha, drawMode);
}

/* Source: CoDUOMP.exe 0x0040af00..0x0040af1b.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0040af00_0040af1c.mcode.
 * Name and signature: exact same-module Mac symbol Con_DrawBoldMessages. */
void Con_DrawBoldMessages(int32_t x, int32_t y, float alpha, int32_t drawMode)
{
    Con_DrawMessageWindow(&con_boldGameMessageWindow, x, y, alpha, drawMode);
}

/* Source: CoDUOMP.exe 0x0040af20..0x0040af73.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0040af20_0040af74.mcode.
 * Name and signature: exact same-module Mac symbol Con_DrawMiniConsole. The
 * cvar setter can clamp the live cvar immediately, so the capacity is sampled
 * after the setter just as in the executable. */
void Con_DrawMiniConsole(int32_t x, int32_t y, float alpha)
{
    /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered engine boundary input and state before use. */
    if (con_miniconLines->integer < CON_MINICON_MIN_LINES) {
        Cvar_Set("con_miniconlines", "0");
    } else if (con_miniconLines->integer > CON_MINICON_MAX_LINES) {
        Cvar_Set("con_miniconlines", va("%d", CON_MINICON_MAX_LINES));
    }

    con_miniConsoleWindow.lineCapacity = con_miniconLines->integer;
    Con_DrawMessageWindow(&con_miniConsoleWindow, x, y, alpha, CON_DRAW_MESSAGE_DEFAULT_MODE);
}

/* Source: CoDUOMP.exe 0x0040af80..0x0040af9b.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0040af80_0040af9c.mcode.
 * Name and signature: exact same-module Mac symbol Con_DrawSubtitles. */
void Con_DrawSubtitles(int32_t x, int32_t y, float alpha, int32_t drawMode)
{
    Con_DrawMessageWindow(&con_subtitleWindow, x, y, alpha, drawMode);
}
