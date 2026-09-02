// Source: uo_cgame_mp_x86.dll 0x30036900..0x30036d5e
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30036900_30036d5e.mcode

#include "../client_recovered.h"
#include "../globals.h"

#include <stddef.h>
#include <stdint.h>

/*
 * CG_DrawObjectiveInfo — draw the objective-text band above the
 * scoreboard list and return the next Y coordinate.
 *
 * The size-only CG_AddViewWeapon guess is rejected: the function reads the
 * cg_objectiveText cvar, wraps and draws localized text through the scoreboard
 * trap-54 path, and draws a separator rule. Its sole caller is the scoreboard
 * body. Both arguments are cdecl stack slots: drawCtx then the starting Y; the
 * result is returned in ST0.
 * The Mac CG_DrawObjectiveInfo shares the picture, translation, and text-paint
 * calls and performs the same scoreboard objective draw, resolving the name.
 */

enum {
    SCOREBOARD_OBJECTIVE_MAX_WIDTH = 374,
    SCOREBOARD_OBJECTIVE_DRAW_MODE = 3,
    LOADING_HUNK_USAGE_BUFFER_SIZE = 64,
    SHADER_REGISTER_SORT_LEVELSHOT = 2,
    SHADER_REGISTER_SORT_UI = 5
};

static const float SCOREBOARD_OBJECTIVE_TOP_PAD = 4.0f;
static const float SCOREBOARD_OBJECTIVE_TEXT_X = 129.0f;
static const float SCOREBOARD_OBJECTIVE_BASELINE_OFFSET = 9.0f;
static const float SCOREBOARD_OBJECTIVE_TEXT_SCALE = 0.24f;
static const float SCOREBOARD_OBJECTIVE_LINE_ADVANCE = 12.0f;
static const float SCOREBOARD_OBJECTIVE_BOTTOM_PAD = 2.0f;
static const float SCOREBOARD_RULE_X = 125.0f;
static const float SCOREBOARD_RULE_WIDTH = 390.0f;
static const float SCOREBOARD_RULE_HEIGHT = 1.0f;
static const float SCOREBOARD_RULE_ALPHA_SCALE = 0.1f;

float CG_DrawObjectiveInfo(const cgScoreboardDrawCtx_t *drawCtx, float y)
{
    const char *localized;
    const char *lineStart;
    const char *cursor;
    const char *lastSpace;

    if (cg_objectiveText.string[0] == '\0') {
        return y;
    }

    y += SCOREBOARD_OBJECTIVE_TOP_PAD;
    localized = CG_TranslateMessage(cg_objectiveText.string, "scoreboard objective info");
    lineStart = localized;
    cursor = localized;
    lastSpace = NULL;

    if (lineStart != NULL) {
        for (;;) {
            int32_t length;
            int32_t textWidth;

            if (*lineStart == ' ') {
                lineStart++;
                cursor = lineStart;
                lastSpace = NULL;
                continue;
            }
            if (*lineStart == '\n') {
                lineStart++;
                cursor = lineStart;
                lastSpace = NULL;
                y += SCOREBOARD_OBJECTIVE_LINE_ADVANCE;
                continue;
            }
            if (*lineStart == '\\' && lineStart[1] == 'n') {
                lineStart += 2;
                cursor = lineStart;
                lastSpace = NULL;
                y += SCOREBOARD_OBJECTIVE_LINE_ADVANCE;
                continue;
            }

            cursor++;
            if (*cursor == '\0') {
                length = coduo_int32_from_bits((uint32_t)(uintptr_t)cursor - (uint32_t)(uintptr_t)lineStart);
                trap_R_Text_Paint(CG_FloatBits(SCOREBOARD_OBJECTIVE_TEXT_X), CG_FloatBits(y + SCOREBOARD_OBJECTIVE_BASELINE_OFFSET), 0,
                                  CG_FloatBits(SCOREBOARD_OBJECTIVE_TEXT_SCALE), (intptr_t)drawCtx, (intptr_t)lineStart, 0, length,
                                  SCOREBOARD_OBJECTIVE_DRAW_MODE);
                y += SCOREBOARD_OBJECTIVE_LINE_ADVANCE;
                break;
            }

            if (*cursor == '\n' || (*cursor == '\\' && cursor[1] == 'n')) {
                length = coduo_int32_from_bits((uint32_t)(uintptr_t)cursor - (uint32_t)(uintptr_t)lineStart);
                trap_R_Text_Paint(CG_FloatBits(SCOREBOARD_OBJECTIVE_TEXT_X), CG_FloatBits(y + SCOREBOARD_OBJECTIVE_BASELINE_OFFSET), 0,
                                  CG_FloatBits(SCOREBOARD_OBJECTIVE_TEXT_SCALE), (intptr_t)drawCtx, (intptr_t)lineStart, 0, length,
                                  SCOREBOARD_OBJECTIVE_DRAW_MODE);
                lineStart = cursor + ((*cursor == '\n') ? 1 : 2);
                cursor = lineStart;
                lastSpace = NULL;
                y += SCOREBOARD_OBJECTIVE_LINE_ADVANCE;
                continue;
            }

            length = coduo_int32_from_bits((uint32_t)(uintptr_t)cursor - (uint32_t)(uintptr_t)lineStart);
            textWidth = coduo_int32_from_bits(
                (uint32_t)cgame_syscall(CG_R_TEXT_WIDTH, (intptr_t)lineStart, 0, CG_FloatBits(SCOREBOARD_OBJECTIVE_TEXT_SCALE), length));

            if (textWidth > SCOREBOARD_OBJECTIVE_MAX_WIDTH) {
                /* The DLL computes the draw length as the raw 32-bit
                 * lastSpace-lineStart subtraction before it applies cursor-1 as
                 * the no-space continuation point. Preserve that original edge
                 * case rather than substituting lineEnd-lineStart. */
                /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
                length = (lastSpace != NULL) ? coduo_int32_from_bits((uint32_t)(uintptr_t)lastSpace - (uint32_t)(uintptr_t)lineStart) : -1;
                trap_R_Text_Paint(CG_FloatBits(SCOREBOARD_OBJECTIVE_TEXT_X), CG_FloatBits(y + SCOREBOARD_OBJECTIVE_BASELINE_OFFSET), 0,
                                  CG_FloatBits(SCOREBOARD_OBJECTIVE_TEXT_SCALE), (intptr_t)drawCtx, (intptr_t)lineStart, 0, length,
                                  SCOREBOARD_OBJECTIVE_DRAW_MODE);
                lineStart = (lastSpace != NULL) ? lastSpace + 1 : cursor - 1;
                cursor = lineStart;
                lastSpace = NULL;
                y += SCOREBOARD_OBJECTIVE_LINE_ADVANCE;
                continue;
            }

            if (*cursor == ' ') {
                lastSpace = cursor;
            }
        }
    }

    y += SCOREBOARD_OBJECTIVE_BOTTOM_PAD;

    if (cg_snap == NULL && cg_updateScreenActive == 0) {
        char hunkUsageString[LOADING_HUNK_USAGE_BUFFER_SIZE];
        const char *serverInfo;
        const char *mapName;
        qhandle_t levelshotShader;
        int32_t expectedHunkUsage;
        vec4_t progressColor = {0.8f, 0.8f, 0.8f, 0.8f};

        cg_updateScreenActive = 1;

        if ((uint8_t)cl_serverloadmap.string[0] != 0) {
            trap_Cvar_Set("cl_serverloadmap", "");
        }
        if ((uint8_t)cl_serverloadgametype.string[0] != 0) {
            trap_Cvar_Set("cl_serverloadgametype", "");
        }
        if (cl_serverloadwaiting.integer != 0) {
            trap_Cvar_Set("cl_serverloadwaiting", "0");
        }

        serverInfo = &cg_gameState.stringData[cg_gameState.stringOffsets[0]];
        mapName = Info_ValueForKey(serverInfo, "mapname");
        if (mapName != NULL && mapName[0] != '\0') {
            levelshotShader = trap_R_RegisterShaderNoMip(va("levelshots/%s.tga", mapName), SHADER_REGISTER_SORT_LEVELSHOT);
        } else {
            levelshotShader = 0;
        }
        if (levelshotShader == 0) {
            levelshotShader = trap_R_RegisterShaderNoMip("menu/art/unknownmap", SHADER_REGISTER_SORT_LEVELSHOT);
        }

        trap_R_SetColor(NULL);
        trap_R_DrawStretchPic(CG_FloatBits(cgs_screenXScale * 0.0f), CG_FloatBits(cgs_screenYScale * 0.0f),
                              CG_FloatBits(cgs_screenXScale * 640.0f), CG_FloatBits(cgs_screenYScale * 480.0f), CG_FloatBits(0.0f),
                              CG_FloatBits(0.0f), CG_FloatBits(1.0f), CG_FloatBits(1.0f), levelshotShader);

        trap_Cvar_VariableStringBuffer("com_expectedhunkusage", hunkUsageString, LOADING_HUNK_USAGE_BUFFER_SIZE);
        expectedHunkUsage = coduo_crt_atoi(hunkUsageString);
        (void)progressColor;

        if (expectedHunkUsage > 0) {
            /* 0x30036c70 FILD [hunkUsed]; 0x30036c77 FIDIV dword [expectedHunkUsage]
             * -- an INTEGER divide, so neither operand is rounded to float. The only
             * rounding is 0x30036c7b FST DWORD (an FST *keep*: stores the rounded copy
             * while the 80-bit st(0) stays live), and 0x30036c7f FCOMP 1.0f compares
             * the UNROUNDED value. Keeping the chain in a long double preserves both
             * the single rounding and the unrounded compare. (Same shape as
             * cg_drawinformation.c.) */
            int32_t hunkUsed = coduo_int32_from_bits((uint32_t)cgame_syscall(CG_HUNK_USED));
            long double progressRaw = (long double)hunkUsed / (long double)expectedHunkUsage;
            float progress = (float)progressRaw; /* 0x30036c7b FST DWORD [0x10] */

            if (progressRaw > 1.0f) {
                progress = 1.0f; /* 0x30036c8c MOV [0x10],0x3f800000 */
            }
            CG_DrawFilledBarStyled(200.0f, 468.0f, 240.0f, 10.0f, progress);
        }

        cgame_syscall(CG_UPDATE_SCREEN);
        cg_updateScreenActive = coduo_int32_from_bits((uint32_t)cg_updateScreenActive - 1u);
    }

    {
        qhandle_t whiteShader =
            coduo_int32_from_bits((uint32_t)cgame_syscall(CG_R_REGISTERSHADER, (intptr_t)"white", SHADER_REGISTER_SORT_UI));
        vec4_t ruleColor;

        ruleColor[0] = drawCtx->color[0];
        ruleColor[1] = drawCtx->color[1];
        ruleColor[2] = drawCtx->color[2];
        ruleColor[3] = drawCtx->color[3] * SCOREBOARD_RULE_ALPHA_SCALE;
        trap_R_SetColor(ruleColor);
        CG_DrawPic(SCOREBOARD_RULE_X, y, SCOREBOARD_RULE_WIDTH, SCOREBOARD_RULE_HEIGHT, whiteShader);
    }

    return y + SCOREBOARD_RULE_HEIGHT;
}
