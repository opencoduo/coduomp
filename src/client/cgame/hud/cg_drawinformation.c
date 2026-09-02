// Source: uo_cgame_mp_x86.dll 0x3002a530..0x3002a9d8
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3002a530_3002a9d8.mcode

#include "../client_recovered.h"
#include "../globals.h"

#include <stddef.h>
#include <stdint.h>

/*
 * CG_DrawInformation — draw the connection/loading information screen.
 *
 * The size-only PM_LadderMove guess is rejected: this function manages the
 * cl_serverload* cvars, draws the levelshot and loading text/progress, and forces
 * an UpdateScreen trap. The behavior matches the same-module PPC symbol
 * CG_DrawInformation. `force` is the sole cdecl stack argument.
 */

enum {
    LOADING_MAP_BUFFER_SIZE = 1024,
    LOADING_HUNK_BUFFER_SIZE = 64,
    LOADING_TEXT_DRAW_MODE = 3,
    LEVELSHOT_SHADER_SORT = 2
};

static const float LOADING_TEXT_SCALE = 0.5f;
static const float LOADING_TEXT_CENTER_X = 640.0f;
static const float LOADING_GAMETYPE_Y = 55.0f;
static const float LOADING_MAP_Y = 85.0f;
static const float LOADING_WAITING_Y = 435.0f;

void CG_DrawInformation(qboolean force)
{
    char mapBuffer[LOADING_MAP_BUFFER_SIZE];
    const char *mapName;
    const char *serverInfo;
    qhandle_t levelshotShader;
    vec4_t textColor;

    if (cg_snap != NULL && force == qfalse) {
        return;
    }
    if (cg_updateScreenActive != 0) {
        return;
    }

    cg_updateScreenActive = 1;

    if (force == qfalse) {
        if (cl_serverloadmap.string[0] != '\0') {
            trap_Cvar_Set("cl_serverloadmap", "");
        }
        if (cl_serverloadgametype.string[0] != '\0') {
            trap_Cvar_Set("cl_serverloadgametype", "");
        }
        if (cl_serverloadwaiting.integer != 0) {
            trap_Cvar_Set("cl_serverloadwaiting", "0");
        }
    } else if (cl_serverloadwaiting.integer == 0) {
        trap_Cvar_Set(ui_scriptMenuAllowResponseCvarName, "0");
        CG_CloseScriptMenu();
        CG_CloseScriptMenu();
        trap_Cvar_Set(ui_scriptMenuAllowResponseCvarName, "1");
        cgame_syscall(CG_MAP_RESTART_RESET_RENDERER);
        cgame_syscall(CG_MSS_STOP_SOUNDS, 0);
        trap_Cvar_Set("cl_serverloadwaiting", "1");
    }

    /* 0x3002a635..0x3002a63c resolves config string zero before the force
     * branch; the forced path does not consume it, but the target performs the
     * load unconditionally. */
    serverInfo = &cg_gameState.stringData[cg_gameState.stringOffsets[0]];

    if (force != qfalse) {
        trap_Cvar_VariableStringBuffer("cl_serverloadmap", mapBuffer,
                                       LOADING_MAP_BUFFER_SIZE);
        mapName = mapBuffer;
    } else {
        mapName = Info_ValueForKey(serverInfo, "mapname");
    }

    if (mapName != NULL && mapName[0] != '\0') {
        const char *levelshotName = va("levelshots/%s.tga", mapName);

        CG_DrawInformation(qfalse);
        levelshotShader = coduo_int32_from_bits((uint32_t)cgame_syscall(
            CG_R_REGISTERSHADER,
            (intptr_t)levelshotName,
            LEVELSHOT_SHADER_SORT));
    } else {
        levelshotShader = 0;
    }

    if (levelshotShader == 0) {
        CG_DrawInformation(qfalse);
        levelshotShader = coduo_int32_from_bits((uint32_t)cgame_syscall(
            CG_R_REGISTERSHADER,
            (intptr_t)"menu/art/unknownmap",
            LEVELSHOT_SHADER_SORT));
    }

    if (force != qfalse) {
        textColor[0] = 0.75f;
        textColor[1] = 0.75f;
        textColor[2] = 0.75f;
        textColor[3] = 1.0f;
        trap_R_SetColor(textColor);
    } else {
        trap_R_SetColor(NULL);
    }

    trap_R_DrawStretchPic(CG_FloatBits(cgs_screenXScale * 0.0f),
                          CG_FloatBits(cgs_screenYScale * 0.0f),
                          CG_FloatBits(cgs_screenXScale * 640.0f),
                          CG_FloatBits(cgs_screenYScale * 480.0f),
                          CG_FloatBits(0.0f),
                          CG_FloatBits(0.0f),
                          CG_FloatBits(1.0f),
                          CG_FloatBits(1.0f),
                          levelshotShader);

    if (force != qfalse) {
        const char *gametypeToken;
        const char *gametypeText;
        const char *mapText;
        const char *waitingText;
        const char *waitingDisplay;
        const char *dots;
        int32_t textWidth;
        float textX;
        int32_t dotPhase;

        gametypeToken = (const char *)(intptr_t)cgame_syscall(
            CG_UI_GET_GAMETYPE_DISPLAY_NAME,
            (intptr_t)cl_serverloadgametype.string);
        gametypeText = CG_SafeTranslateString_Internal("cgame", gametypeToken);
        textWidth = coduo_int32_from_bits((uint32_t)cgame_syscall(
            CG_R_TEXT_WIDTH,
            (intptr_t)gametypeText,
            0,
            CG_FloatBits(LOADING_TEXT_SCALE),
            0));
        /* 0x3002a7b1 FILD [textWidth]; 0x3002a7bb FSUBR 640.0f -- textWidth is
         * FILDed straight into the subtract with no float store, so it stays exact
         * in 80-bit; no (float) cast (cf. the waiting-text site below, which does
         * round). */
        textX = (float)(
            ((long double)LOADING_TEXT_CENTER_X - (long double)textWidth) *
            (long double)0.5f);
        trap_R_Text_Paint(CG_FloatBits(textX),
                  CG_FloatBits(LOADING_GAMETYPE_Y),
                  0,
                  CG_FloatBits(LOADING_TEXT_SCALE),
                  (intptr_t)textColor,
                  (intptr_t)gametypeText,
                  0,
                  0,
                  LOADING_TEXT_DRAW_MODE);

        mapText = (const char *)(intptr_t)cgame_syscall(
            CG_UI_GET_MAP_DISPLAY_NAME,
            (intptr_t)cl_serverloadmap.string);
        textWidth = coduo_int32_from_bits((uint32_t)cgame_syscall(
            CG_R_TEXT_WIDTH,
            (intptr_t)mapText,
            0,
            CG_FloatBits(LOADING_TEXT_SCALE),
            0));
        /* 0x3002a817 FILD [textWidth]; 0x3002a821 FSUBR 640.0f -- direct FILD, no
         * float store; textWidth stays exact in 80-bit. No (float) cast. */
        textX = (float)(
            ((long double)LOADING_TEXT_CENTER_X - (long double)textWidth) *
            (long double)0.5f);
        trap_R_Text_Paint(CG_FloatBits(textX),
                  CG_FloatBits(LOADING_MAP_Y),
                  0,
                  CG_FloatBits(LOADING_TEXT_SCALE),
                  (intptr_t)textColor,
                  (intptr_t)mapText,
                  0,
                  0,
                  LOADING_TEXT_DRAW_MODE);

        /* 0x3002a85a: imul 0x057619F1 (= ceil(2^36/750)); sar edx,4 -> divide by
         * 750, not 1000 (the /1000 magic would be 0x10624DD3). The "..." animation
         * advances every 750 ms. A prior pass used /1000. */
        {
            int32_t milliseconds = coduo_int32_from_bits(
                (uint32_t)cgame_syscall(CG_MILLISECONDS));
            dotPhase = (int32_t)((uint32_t)(milliseconds / 750) & 3u);
        }
        switch (dotPhase) {
        case 1:
            dots = ".";
            break;
        case 2:
            dots = "..";
            break;
        case 3:
            dots = "...";
            break;
        default:
            dots = "";
            break;
        }

        waitingText = CG_SafeTranslateString_Internal("cgame",
                                         "CGAME_WAITINGFORSERVERLOAD");
        textWidth = coduo_int32_from_bits((uint32_t)cgame_syscall(
            CG_R_TEXT_WIDTH,
            (intptr_t)waitingText,
            0,
            CG_FloatBits(LOADING_TEXT_SCALE),
            0));
        waitingDisplay = va("%s%s", waitingText, dots);
        /* 0x3002a8c3 FILD [textWidth]; 0x3002a8ce FSTP DWORD [same slot] -- unlike
         * the two sites above, this one ROUNDS textWidth to float and stores it back
         * before 0x3002a8e0 FSUB reloads it, so the (float) cast is faithful here.
         * Do NOT drop it to match the siblings. */
        {
            float widthRounded = (float)textWidth;
            textX = (float)(
                ((long double)LOADING_TEXT_CENTER_X -
                 (long double)widthRounded) * (long double)0.5f);
        }
        trap_R_Text_Paint(CG_FloatBits(textX),
                  CG_FloatBits(LOADING_WAITING_Y),
                  0,
                  CG_FloatBits(LOADING_TEXT_SCALE),
                  (intptr_t)textColor,
                  (intptr_t)waitingDisplay,
                  0,
                  0,
                  LOADING_TEXT_DRAW_MODE);
    } else {
        char hunkUsageString[LOADING_HUNK_BUFFER_SIZE];
        int32_t expectedHunkUsage;
        vec4_t progressColor = {0.8f, 0.8f, 0.8f, 0.8f};

        trap_Cvar_VariableStringBuffer("com_expectedhunkusage",
                                       hunkUsageString,
                                       LOADING_HUNK_BUFFER_SIZE);
        expectedHunkUsage = coduo_crt_atoi(hunkUsageString);
        (void)progressColor;

        if (expectedHunkUsage > 0) {
            /* 0x3002a968 FILD [hunkUsed]; 0x3002a96f FIDIV dword [expectedHunkUsage]
             * -- an INTEGER divide, so neither operand is rounded to float. The only
             * rounding is 0x3002a973 FST DWORD (an FST *keep*: it stores the rounded
             * copy while the 80-bit st(0) stays live), and 0x3002a977 FCOMP 1.0f
             * then compares the UNROUNDED value. Keeping the chain in a long double
             * preserves both the single rounding and the unrounded compare. */
            long double progressRaw =
                (long double)coduo_int32_from_bits(
                    (uint32_t)cgame_syscall(CG_HUNK_USED)) /
                expectedHunkUsage;
            float progress = (float)progressRaw; /* 0x3002a973 FST DWORD [0x8] */

            if (progressRaw > 1.0f) {
                progress = 1.0f;                 /* 0x3002a984 MOV [0x8],0x3f800000 */
            }
            CG_DrawFilledBarStyled(200.0f, 468.0f, 240.0f, 10.0f,
                                   progress);
        }
    }

    cgame_syscall(CG_UPDATE_SCREEN);
    cg_updateScreenActive = coduo_int32_from_bits(
        (uint32_t)cg_updateScreenActive - 1u);
}
