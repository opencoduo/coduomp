#include <stdlib.h>

#include "ui_functions.h"
#include "ui_globals.h"
#include "compat/crt/qsort_compat.h"

enum {
    UI_NO_CINEMATIC = -1
};

// NOT_FROM_ORIGINAL_SOURCE: C type bridge for the enum-typed syscall wrapper.
static void ui_compat_execute_text_callback(int32_t executionMode, const char *text)
{
    trap_Cmd_ExecuteText((cbufExec_t)executionMode, text);
}

// Source: uo_ui_mp_x86.dll 0x4000f900..0x4000fde6
// Evidence: cgame_mp/mcode/uo_ui_mp_x86/FUN_4000f900_4000fde6.mcode
// Exact same-module PPC symbol: _UI_Init.
void UI_Init(void)
{
    const char *menuFiles;
    int32_t index;

    UI_RegisterCvars();
    UI_InitMemory();
    trap_Cvar_Set("ui_menuFiles", "ui_mp/menus.txt");
    trap_GetGlconfig(&ui_displayContextStorage.context.glConfig);

    /* Machine code multiplies by the pre-rounded float reciprocals
     * 1/480 (0x40035a48) and 1/640 (0x40035a44), not a runtime divide;
     * the constant-folded 1.0f/N reproduces those exact float constants.
     * vidWidth/vidHeight enter via bare FILD (0x4000f934/0x4000f959,
     * reused for bias) with no intermediate float store, so there is
     * no (float) cast on them: an explicit cast would round the operand the
     * DLL keeps exact in 80-bit (Class 4). */
    ui_displayContextStorage.context.yscale = ui_displayContextStorage.context.glConfig.vidHeight * (1.0f / 480.0f);
    ui_displayContextStorage.context.xscale = ui_displayContextStorage.context.glConfig.vidWidth * (1.0f / 640.0f);
    if (ui_displayContextStorage.context.glConfig.vidWidth * 480 > ui_displayContextStorage.context.glConfig.vidHeight * 640) {
        ui_displayContextStorage.context.bias =
            (ui_displayContextStorage.context.glConfig.vidWidth - ui_displayContextStorage.context.glConfig.vidHeight * (4.0f / 3.0f)) *
            0.5f;
    } else {
        ui_displayContextStorage.context.bias = 0.0f;
    }

    ui_displayContextStorage.context.registerShaderNoMip = trap_R_RegisterShaderNoMip;
    ui_displayContextStorage.context.setColor = UI_SetColor;
    ui_displayContextStorage.context.drawHandlePic = UI_DrawHandlePic;
    ui_displayContextStorage.context.drawStretchPic = trap_R_DrawStretchPic;
    ui_displayContextStorage.context.drawText = trap_R_Text_Paint;
    ui_displayContextStorage.context.textWidth = trap_R_Text_Width;
    ui_displayContextStorage.context.textHeight = trap_R_Text_Height;
    ui_displayContextStorage.context.translateString = trap_SE_TranslateReference;
    ui_displayContextStorage.context.getLocalizedString = UI_SafeTranslateString;
    ui_displayContextStorage.context.localizeWithBinding = trap_SE_LocalizeMessage;
    ui_displayContextStorage.context.setFont = Text_SetActiveFont;
    ui_displayContextStorage.context.registerModel = trap_R_RegisterModel;
    ui_displayContextStorage.context.modelBounds = trap_R_ModelBounds;
    ui_displayContextStorage.context.fillRect = UI_FillRect;
    ui_displayContextStorage.context.drawRect = UI_DrawRectWithSize;
    ui_displayContextStorage.context.drawSides = UI_DrawSidesWithSize;
    ui_displayContextStorage.context.drawTopBottom = UI_DrawTopBottomWithSize;
    ui_displayContextStorage.context.clearScene = trap_R_ClearScene;
    ui_displayContextStorage.context.addRefEntityToScene = trap_R_AddRefEntity;
    ui_displayContextStorage.context.renderScene = trap_R_RenderScene;
    ui_displayContextStorage.context.registerFont = trap_R_RegisterFont;
    ui_displayContextStorage.context.ownerDrawItem = UI_OwnerDraw;
    ui_displayContextStorage.context.ownerDrawValue = UI_GetValue;
    ui_displayContextStorage.context.ownerDrawVisible = UI_OwnerDrawVisible;
    ui_displayContextStorage.context.runScript = UI_RunMenuScript;
    ui_displayContextStorage.context.getTeamColor = UI_GetTeamColor;
    ui_displayContextStorage.context.getCVarString = trap_Cvar_VariableStringBuffer;
    ui_displayContextStorage.context.getCVarValue = ui_compat_display_cvar_value;
    ui_displayContextStorage.context.setCVar = trap_Cvar_Set;
    ui_displayContextStorage.context.getConfigString = UI_ConfigString;
    ui_displayContextStorage.context.drawTextWithCursor = trap_R_Text_PaintWithCursor;
    ui_displayContextStorage.context.setOverstrikeMode = trap_Key_SetOverstrikeMode;
    ui_displayContextStorage.context.getOverstrikeMode = trap_Key_GetOverstrikeMode;
    ui_displayContextStorage.context.startLocalSound = trap_MSS_PlayLocalSoundAlias;
    ui_displayContextStorage.context.ownerDrawHandleKey = UI_OwnerDrawHandleKey;
    ui_displayContextStorage.context.feederCount = UI_FeederCount;
    ui_displayContextStorage.context.feederItemText = UI_FeederItemText;
    ui_displayContextStorage.context.resolveTextToken = UI_FileText;
    ui_displayContextStorage.context.feederItemImage = UI_FeederItemImage;
    ui_displayContextStorage.context.feederSelection = UI_FeederSelection;
    ui_displayContextStorage.context.feederAddItem = UI_FeederAddItem;
    ui_displayContextStorage.context.getAutoUpdate = trap_GetAutoUpdate;
    ui_displayContextStorage.context.runningGame = trap_RunningGame;
    ui_displayContextStorage.context.setBinding = trap_Key_SetBinding;
    ui_displayContextStorage.context.getBindingBuf = trap_Key_GetBindingBuf;
    ui_displayContextStorage.context.keynumToStringBuf = trap_Key_KeynumToStringBuf;
    ui_displayContextStorage.context.executeText = ui_compat_execute_text_callback;
    ui_displayContextStorage.context.error = Com_Error;
    ui_displayContextStorage.context.print = Com_Printf;
    ui_displayContextStorage.context.pause = UI_Pause;
    ui_displayContextStorage.context.ownerDrawWidth = UI_OwnerDrawWidth;
    ui_displayContextStorage.context.registerAsset = trap_S_RegisterSound;
    ui_displayContextStorage.context.playCinematic = UI_PlayCinematic;
    ui_displayContextStorage.context.stopCinematic = UI_StopCinematic;
    ui_displayContextStorage.context.drawCinematic = UI_DrawCinematic;
    ui_displayContextStorage.context.runCinematicFrame = UI_RunCinematicFrame;

    Init_Display(&ui_displayContextStorage.context);
    String_Init();
    ui_displayContextStorage.context.whiteShader = trap_R_RegisterShaderNoMip("white", R_IMAGE_TRACK_UI);
    UI_AssetCache();
    (void)trap_Milliseconds();

    ui_gameTypeListReservedMarker = 0;
    ui_teamCount = 0;
    ui_teamListReservedMarker = 0;
    UI_GetGameTypesList();
    UI_LoadArenas();

    trap_Cvar_VariableStringBuffer("ui_menuFiles", ui_menuFilesBuffer, sizeof(ui_menuFilesBuffer));
    menuFiles = ui_menuFilesBuffer[0] != '\0' ? ui_menuFilesBuffer : "ui_mp/menus.txt";
    UI_LoadMenus(menuFiles, qtrue, R_IMAGE_TRACK_UI);
    UI_LoadMenus("ui_mp/ingame.txt", qfalse, R_IMAGE_TRACK_UI);
    for (index = 0; index < menuCount; ++index) {
        Menus_Close(&Menus[index]);
    }

    /* 0x4000fc8a..0x4000fd14 emits six direct registrations and stores; the
     * retail image has no intervening name-pointer table or indexing loop. */
    ui_serverHardwareShaders[0] = trap_R_RegisterShaderNoMip("ui_mp/assets/server_hardware_unknown", R_IMAGE_TRACK_UI);
    ui_serverHardwareShaders[1] = trap_R_RegisterShaderNoMip("ui_mp/assets/server_hardware_linux_dedicated", R_IMAGE_TRACK_UI);
    ui_serverHardwareShaders[2] = trap_R_RegisterShaderNoMip("ui_mp/assets/server_hardware_win_dedicated", R_IMAGE_TRACK_UI);
    ui_serverHardwareShaders[3] = trap_R_RegisterShaderNoMip("ui_mp/assets/server_hardware_mac_dedicated", R_IMAGE_TRACK_UI);
    ui_serverHardwareShaders[4] = trap_R_RegisterShaderNoMip("ui_mp/assets/server_hardware_win_listen", R_IMAGE_TRACK_UI);
    ui_serverHardwareShaders[5] = trap_R_RegisterShaderNoMip("ui_mp/assets/server_hardware_mac_listen", R_IMAGE_TRACK_UI);
    ui_punkbusterShader = trap_R_RegisterShaderNoMip("ui_mp/assets/punkbusterlogo", R_IMAGE_TRACK_UI);
    ui_compat_lan_load_cached_servers();

    if (ui_serverSortKey != LAN_SERVER_SORT_PING) {
        ui_serverSortKey = LAN_SERVER_SORT_PING;
        coduo_crt_qsort(ui_displayServers, (size_t)ui_displayServerCount, sizeof(ui_displayServers[0]), UI_ServersQsortCompare);
    }

    /* 0x4000fd63..0x4000fd82 tests only x87 C0: a negative or unordered
     * value selects "1"; zero and positive values select "0". */
    trap_Cvar_Set("ui_mousePitch", !(trap_Cvar_VariableValue("m_pitch") >= 0.0f) ? "1" : "0");
    ui_serverMapCinematic = UI_NO_CINEMATIC;
    ui_previewMovie = UI_NO_CINEMATIC;
    trap_Cvar_Register(NULL, "debug_protocol", "", 0);
    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if (ui_netGameType < 0 || ui_netGameType >= ui_gameTypeCount) {
        ui_netGameType = 0;
        trap_Cvar_Set("ui_netGameType", "0");
    }
    trap_Cvar_Set("ui_netGameTypeName", ui_gameTypes[ui_netGameType].gameType);
    trap_Cvar_Register(NULL, "ui_multiplayer", "1", CVAR_ROM);
}
