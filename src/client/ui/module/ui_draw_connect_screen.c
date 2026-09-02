#include <math.h>
#include <string.h>

#include "ui_functions.h"
#include "client/common/client_branding.h"
#include "client/common/client_format_validation.h"

enum {
    UI_CONNECT_MESSAGE_LINE_SIZE = 64,
    UI_CONNECT_TEXT_SIZE = 256,
    UI_CONNECT_MESSAGE_WRAP_START = 40,
    UI_CONNECT_MESSAGE_WRAP_LIMIT = 58,
    UI_CONNECT_MESSAGE_START_Y = 265,
    UI_CONNECT_MESSAGE_LINE_HEIGHT = 22,
    UI_SERVER_NAME_COMPARE_LIMIT = 99999
};

/* NOT_FROM_ORIGINAL_SOURCE: safely applies the retail connection-text
 * translation contract (literal text plus zero or one %s conversion). */
static void ui_compat_format_connect_text(char *destination, size_t destinationSize, const char *format, const char *value)
{
    if (client_compat_validate_format_signature(format, "s") == qfalse) {
        Com_Printf("WARNING: rejected invalid connection-text format\n");
        Q_strncpyz(destination, format, (int32_t)destinationSize);
        return;
    }
    Com_sprintf(destination, destinationSize, format, value);
}

// Source: uo_ui_mp_x86.dll 0x40010e30..0x400113ad
// Evidence: cgame_mp/mcode/uo_ui_mp_x86/FUN_40010e30_400113ad.mcode
// Exact same-module PPC symbol: UI_DrawConnectScreen.
void UI_DrawConnectScreen(qboolean overlay)
{
    static const vec4_t black = {0.0f, 0.0f, 0.0f, 1.0f};
    static const vec4_t white = {1.0f, 1.0f, 1.0f, 1.0f};
    static const vec4_t yellow = {1.0f, 1.0f, 0.0f, 1.0f};
    uiClientState_t state;
    menuDef_t *connectMenu = Menus_FindByName("Connect");
    char configInfo[MAX_STRING_CHARS];
    char downloadName[MAX_STRING_CHARS];
    char connectText[UI_CONNECT_TEXT_SIZE];
    char line[UI_CONNECT_MESSAGE_LINE_SIZE];
    const char *text = NULL;

    if (!overlay && connectMenu != NULL) {
        Menu_Paint(connectMenu, qtrue);
    } else {
        UI_FillRect(0.0f, 0.0f, 640.0f, 96.0f, black);
    }

    trap_GetClientState(&state);
    if (state.connState < CA_LOADING) {
        if (Q_stricmpn("localhost", state.servername, UI_SERVER_NAME_COMPARE_LIMIT) == 0) {
            text = va("%s - %s", UI_SafeTranslateString("GMI_EXE_CODUO_MULTIPLAYER"), "1.51");
            Text_PaintCenter(320.0f, 55.0f, text, 0.5f, white, 0);
        } else if (Q_stricmpn("Auto-Updater", state.servername, UI_SERVER_NAME_COMPARE_LIMIT) == 0) {
            trap_Cvar_VariableStringBuffer("cl_downloadName", downloadName, sizeof(downloadName));
            if (downloadName[0] != '\0') {
                trap_Cvar_VariableStringBuffer("cl_updateversion", downloadName, sizeof(downloadName));
                /* NOT_FROM_ORIGINAL_SOURCE: apply the one-string localization
                 * contract within the fixed connection-text destination. */
                ui_compat_format_connect_text(connectText, sizeof(connectText), UI_SafeTranslateString("EXE_DOWNLOADINGUPDATE"),
                                              downloadName);
            } else {
                ui_compat_format_connect_text(connectText, sizeof(connectText), UI_SafeTranslateString("EXE_CONNECTINGTO"),
                                              state.servername);
            }
            text = connectText;
            Text_PaintCenter(320.0f, 55.0f, text, 0.5f, white, 0);
        } else {
            ui_compat_format_connect_text(connectText, sizeof(connectText), UI_SafeTranslateString("EXE_CONNECTINGTO"), state.servername);
            text = connectText;
            Text_PaintCenter(320.0f, 55.0f, text, 0.5f, white, 0);
        }
    } else if (Q_stricmpn("Auto-Updater", state.servername, UI_SERVER_NAME_COMPARE_LIMIT) != 0) {
        configInfo[0] = '\0';
        if (trap_GetConfigString(0, configInfo, sizeof(configInfo))) {
            text = UI_SafeTranslateString(UI_GetGameTypeDisplayName(Info_ValueForKey(configInfo, "g_gametype")));
            Text_PaintCenter(320.0f, 55.0f, text, 0.5f, white, 0);
            text = va("%s", Info_ValueForKey(configInfo, "mapname"));
            Text_PaintCenter(320.0f, 85.0f, text, 0.5f, white, 0);
        }
    }

    text = Info_ValueForKey(state.updateInfoString, "motd");
    Text_PaintCenter(320.0f, 460.0f, text, 0.5f, white, 0);

    if (state.connState < CA_CONNECTED) {
        const char *message = UI_SafeTranslateString(state.messageString);
        int32_t messageLength = (int32_t)strlen(message);
        int32_t inputIndex;
        int32_t outputIndex = 0;
        int32_t y = UI_CONNECT_MESSAGE_START_Y;
        qboolean wrapAtSpace = qfalse;

        for (inputIndex = 0; inputIndex < messageLength; ++inputIndex, ++outputIndex) {
            int32_t width;

            line[outputIndex] = message[inputIndex];
            if (outputIndex > UI_CONNECT_MESSAGE_WRAP_START && inputIndex > 0) {
                wrapAtSpace = qtrue;
            }
            if (outputIndex < UI_CONNECT_MESSAGE_WRAP_LIMIT && inputIndex != messageLength - 1 &&
                (!wrapAtSpace || message[inputIndex] != ' ')) {
                continue;
            }

            line[outputIndex + 1] = '\0';
            width = trap_R_Text_Width(line, 0, 0.5f, 0);
            /* The DLL FILDs width/2 straight into the FSUBR from 320.0f
             * (0x400111f8/0x40011206); an explicit (float) cast on width/2
             * would round the integer before the subtract. */
            trap_R_Text_Paint(320.0f - (width / 2), (float)y, 0, 0.5f, yellow, line, 0, 0, 6);
            y += UI_CONNECT_MESSAGE_LINE_HEIGHT;
            outputIndex = -1;
            wrapAtSpace = qfalse;
        }
    }

    if (trap_Cvar_VariableValue("ui_dl_running") != 0.0f) {
        trap_Cvar_VariableStringBuffer("cl_downloadName", downloadName, sizeof(downloadName));
        if (downloadName[0] != '\0') {
            UI_DisplayDownloadInfo(0, downloadName, 320.0f, 55.0f, 0.3f);
        }
        return;
    }

    switch (state.connState) {
    case CA_CONNECTING: {
        const char *const format = UI_SafeTranslateString("EXE_AWAITINGCONNECTION");
        /* NOT_FROM_ORIGINAL_SOURCE: both connection-state branches supply one
         * promoted integer; accept only that conversion contract. */
        if (client_compat_validate_format_signature(format, "i") == qfalse) {
            Com_Printf("WARNING: rejected invalid connection-count format\n");
            text = format;
        } else {
            text = va(format, state.connectPacketCount);
        }
        break;
    }
    case CA_CHALLENGING: {
        const char *const format = UI_SafeTranslateString("EXE_AWAITINGCHALLENGE");
        if (client_compat_validate_format_signature(format, "i") == qfalse) {
            Com_Printf("WARNING: rejected invalid challenge-count format\n");
            text = format;
        } else {
            text = va(format, state.connectPacketCount);
        }
        break;
    }
    case CA_CONNECTED:
        trap_Cvar_VariableStringBuffer("cl_downloadName", downloadName, sizeof(downloadName));
        if (downloadName[0] != '\0') {
            UI_DisplayDownloadInfo(0, downloadName, 320.0f, 55.0f, 0.3f);
            return;
        }
        text = UI_SafeTranslateString("EXE_AWAITINGGAMESTATE");
        break;
    default:
        return;
    }

    if (text != NULL && Q_stricmpn("localhost", state.servername, UI_SERVER_NAME_COMPARE_LIMIT) != 0) {
        Text_PaintCenter(320.0f, 85.0f, text, 0.5f, white, 0);
    }
}
