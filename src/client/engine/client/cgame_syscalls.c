#include "cgame.h"

#include "cinematic.h"
#include "console.h"
#include "debug_lines.h"
#include "../animation/dobj.h"
#include "animation/xanim_asset_load.h"
#include "animation/xanim_eval.h"
#include "../animation/xanim_pool.h"
#include "qcommon/precompiler.h"
#include "../effects/fx_api.h"
#include "../effects/fx_runtime.h"
#include "../localization/string_ed_api.h"
#include "../math/vector_math.h"
#include "qcommon/hunk.h"
#include "../physics/cm_trace.h"
#include "qcommon/q_string.h"
#include "compat/coduo_native_x87.h"
#include "../renderer/renderer_api.h"
#include "../scripting/script_runtime.h"
#include "../sound/miles_boundary.h"
#include "sound/alias/sound_alias.h"
#include "widescreen_2d_compat.h"

#include <float.h>
#include <math.h>
#include <stddef.h>
#include <string.h>

enum {
    CG_RENDERER_LOAD_MODE = 5,
    CG_WEAPON_MEMORY_OWNER = 2,
    CG_UI_GET_ACTIVE_MENU = 6,
    CG_UI_SET_ACTIVE_MENU = 7,
    CG_UI_GET_MAP_DISPLAY_NAME_COMMAND = 9,
    CG_UI_GET_GAMETYPE_DISPLAY_NAME_COMMAND = 10,
    CG_UI_REGISTER_MENU = 15,
    CG_UI_ACTIVE_MENU_NONE = 0,
    CG_UI_ACTIVE_MENU_QUICK_MESSAGE = 8,
    CG_UI_ACTIVE_MENU_AUTO_UPDATE = 9,
    CG_UI_ACTIVE_MENU_SCRIPT_POPUP = 10,
    CG_UI_ACTIVE_MENU_SCRIPT_POPUP_NO_MOUSE = 11,
    CG_UI_ACTIVE_MENU_QUICK_MAP = 12,
    CG_UI_ACTIVE_MENU_PURCHASE = 13,
    CG_UI_GET_MENU_SCREEN = 8,
    CG_UI_KEY_EVENT = 3,
    CG_UI_ESCAPE_KEY = 27,
    CG_XANIM_ALLOCATION_ALIGNMENT = 32
};

static const float cg_xanimMilliseconds = 1000.0f;

#define CG_ARG(index) (arguments[(index)])
#define CG_PTR(type, index) ((type *)(uintptr_t)CG_ARG(index))
#define CG_CONST_PTR(type, index) \
    ((const type *)(uintptr_t)CG_ARG(index))
#define CG_STRING(index) ((const char *)(uintptr_t)CG_ARG(index))
#define CG_INT(index) ((int32_t)CG_ARG(index))
#define CG_SIZE(index) ((size_t)CG_ARG(index))

/* NOT_FROM_ORIGINAL_SOURCE: explicit source expression of the VM ABI's
 * four-byte float-bit transport. memcpy preserves the bit pattern without
 * depending on a compiler's inactive-union-member extension. */
_Static_assert(sizeof(float) == 4 && FLT_RADIX == 2 &&
                   FLT_MANT_DIG == 24 && FLT_MAX_EXP == 128,
               "cgame syscall float transport requires IEEE binary32");
static float CL_CgameSyscallFloatArgument(intptr_t argument)
{
    const uint32_t bits = (uint32_t)argument;
    float value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

/* Source: CoDUOMP.exe 0x00401180..0x00401192.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00401180_00401193.mcode.
 * Name and argument: exact same-module Mac symbol CL_GetGameState. */
void CL_GetGameState(gameState_t *gameState)
{
    memcpy(gameState, &cl.gameState, sizeof(*gameState));
}

/* Source: CoDUOMP.exe 0x00401400..0x0040140f.
 * Name: exact same-module Mac symbol CL_SetUserCmdValue. */
void CL_SetUserCmdValue(int32_t value, float sensitivityScale)
{
    cl.inputState.userCmdValue = value;
    cl.inputState.userCmdSensitivityScale = sensitivityScale;
}

/* Source: CoDUOMP.exe 0x00401410..0x00401429.
 * Name: exact same-module Mac symbol CL_SetUserCmdAimValues. */
void CL_SetUserCmdAimValues(const vec3_t aimValues)
{
    uint32_t word;

    memcpy(&word, &aimValues[0], sizeof(word));
    memcpy(&cl.inputState.userCmdAimValues[0], &word, sizeof(word));
    memcpy(&word, &aimValues[1], sizeof(word));
    memcpy(&cl.inputState.userCmdAimValues[1], &word, sizeof(word));
    memcpy(&word, &aimValues[2], sizeof(word));
    memcpy(&cl.inputState.userCmdAimValues[2], &word, sizeof(word));
}

/* Source: CoDUOMP.exe 0x00401430..0x00401435.
 * Name: exact same-module Mac symbol CL_SetUserCmdInShellshock. */
void CL_SetUserCmdInShellshock(int32_t shellshockScreenBlur)
{
    cl.inputState.shellshockScreenBlur = shellshockScreenBlur;
}

/* Source: CoDUOMP.exe 0x00401440..0x00401445.
 * Name: exact same-module Mac symbol CL_SetUserCmdFlameDamage. */
void CL_SetUserCmdFlameDamage(int32_t flameDamage)
{
    cl.inputState.flameDamage = flameDamage;
}

/* Source: CoDUOMP.exe 0x00401450..0x0040146d.
 * Name: exact same-module Mac symbol CL_SetClientLerpOrigin. */
void CL_SetClientLerpOrigin(float x, float y, float z)
{
    cl.inputState.clientLerpOrigin[0] = x;
    cl.inputState.clientLerpOrigin[1] = y;
    cl.inputState.clientLerpOrigin[2] = z;
}

/* Source: CoDUOMP.exe 0x00401470..0x0040147b.
 * Name: exact same-module Mac symbol CL_AddCgameCommand. */
void CL_AddCgameCommand(const char *commandName)
{
    Cmd_AddCommand(commandName, NULL);
}

/* Source: CoDUOMP.exe executable gap 0x00401e30..0x00401e3e.
 * Name: exact same-module Mac symbol Hunk_AllocXAnimClientCreate. */
static void *Hunk_AllocXAnimClientCreate(size_t size)
{
    return Hunk_AllocAlignInternal(
        size, CG_XANIM_ALLOCATION_ALIGNMENT);
}

/* Source: CoDUOMP.exe executable gap 0x00401e40..0x00401e4e.
 * Name: exact same-module Mac symbol Hunk_AllocXAnimClientCreateTree. */
static void *Hunk_AllocXAnimClientCreateTree(size_t size)
{
    return Hunk_AllocAlignInternal(
        size, CG_XANIM_ALLOCATION_ALIGNMENT);
}

/* NOT_FROM_ORIGINAL_SOURCE: source-level form of the CL_CgameIsMenuOpen
 * helper inlined into the Win32 dispatcher at 0x00403a31..0x00403b41.
 * The helper remains a named function in the same-module Mac build. */
static qboolean CL_CgameIsMenuOpen(const char *menuName)
{
    int32_t menu;

    if (cls.state != CA_ACTIVE || clc.demoPlayback != qfalse ||
        VM_Call(coduo_uiVm, CG_UI_GET_ACTIVE_MENU,
                0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0) !=
            CG_UI_ACTIVE_MENU_NONE) {
        return qfalse;
    }

    if (Q_stricmp(menuName, "UIMENU_WM_QUICKMESSAGE") == 0) {
        menu = CG_UI_ACTIVE_MENU_QUICK_MESSAGE;
    } else if (Q_stricmp(menuName, "UIMENU_WM_PURCHASE") == 0) {
        menu = CG_UI_ACTIVE_MENU_PURCHASE;
    } else if (Q_stricmp(menuName, "UIMENU_QUICKMAP") == 0) {
        menu = CG_UI_ACTIVE_MENU_QUICK_MAP;
    } else if (Q_stricmp(menuName, "UIMENU_WM_AUTOUPDATE") == 0) {
        menu = CG_UI_ACTIVE_MENU_AUTO_UPDATE;
    } else if (Q_stricmpn(menuName, "UIMENU_SCRIPT_POPUP",
                          sizeof("UIMENU_SCRIPT_POPUP") - 1) == 0) {
        menu =
            Q_stricmp(menuName, "UIMENU_SCRIPT_POPUP_NO_MOUSE") == 0
                ? CG_UI_ACTIVE_MENU_SCRIPT_POPUP_NO_MOUSE
                : CG_UI_ACTIVE_MENU_SCRIPT_POPUP;
        return (qboolean)VM_Call(
            coduo_uiVm, CG_UI_SET_ACTIVE_MENU, menu,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    } else {
        return qtrue;
    }

    (void)VM_Call(coduo_uiVm, CG_UI_SET_ACTIVE_MENU, menu,
                  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    return qtrue;
}

/* NOT_FROM_ORIGINAL_SOURCE: source-level form of the CL_CgameClosePopup
 * helper inlined into the Win32 dispatcher at 0x00403b42..0x00403bab. */
static void CL_CgameClosePopup(const char *menuName)
{
    if (VM_Call(coduo_uiVm, CG_UI_GET_ACTIVE_MENU,
                0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0) !=
        CG_UI_ACTIVE_MENU_NONE) {
        return;
    }

    if ((Q_stricmp(menuName, "UIMENU_SCRIPT_POPUP_NO_MOUSE") == 0 &&
         VM_Call(coduo_uiVm, CG_UI_GET_MENU_SCREEN,
                 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0) ==
             CG_UI_ACTIVE_MENU_SCRIPT_POPUP) ||
        (Q_stricmp(menuName, "UIMENU_SCRIPT_POPUP") == 0 &&
         VM_Call(coduo_uiVm, CG_UI_GET_MENU_SCREEN,
                 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0) ==
             CG_UI_ACTIVE_MENU_SCRIPT_POPUP)) {
        (void)VM_Call(coduo_uiVm, CG_UI_KEY_EVENT,
                      CG_UI_ESCAPE_KEY, qtrue,
                      0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    }
}

/* NOT_FROM_ORIGINAL_SOURCE: source-level form of the CL_CgameResetUi helper
 * inlined into the Win32 dispatcher at 0x00403bad..0x00403c11. */
static void CL_CgameResetUi(void)
{
    if (VM_Call(coduo_uiVm, CG_UI_GET_ACTIVE_MENU,
                0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0) !=
        CG_UI_ACTIVE_MENU_NONE) {
        return;
    }

    for (int32_t press = 0; press < 3; ++press) {
        (void)VM_Call(coduo_uiVm, CG_UI_KEY_EVENT,
                      CG_UI_ESCAPE_KEY, qtrue,
                      0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    }
}

/* NOT_FROM_ORIGINAL_SOURCE: cgame 2D submitted outside CL_CGameRendering —
 * the loading pump draws its levelshot/progress bar and then forces a present
 * via CG_UPDATE_SCREEN — must receive the same centered-canvas presentation
 * as the cgame rendering scope, or widescreen loading frames alternate
 * between two different levelshot placements. Open a one-command scope only
 * when no outer scope is active. */
static qboolean CL_CgamePump2dScopeBegin(void)
{
    if (coduomp_cgame_rendering_compat_active != qfalse)
        return qfalse;
    coduomp_queue_cgame_2d_presentation(qtrue);
    coduomp_cgame_rendering_compat_active = qtrue;
    return qtrue;
}

static void CL_CgamePump2dScopeEnd(qboolean opened)
{
    if (opened == qfalse)
        return;
    coduomp_cgame_rendering_compat_active = qfalse;
    coduomp_queue_cgame_2d_presentation(qfalse);
}

/* Source: CoDUOMP.exe 0x00401f30..0x00404ae7, with its command jump table at
 * 0x00404ae8. Name and signature: exact same-module Mac symbol
 * CL_CgameSystemCalls. The cases stay in command-id order even though the
 * optimized Win32 body shares epilogues and folds several source helpers. */
intptr_t CL_CgameSystemCalls(intptr_t *arguments)
{
    const cgameSyscallId_t syscall = (cgameSyscallId_t)CG_ARG(0);

    switch (syscall) {
    case CG_PRINT:
        Com_Printf("%s", CG_STRING(1));
        return 0;
    case CG_ERROR:
        Com_Error(ERR_DROP, "\x15%s", CG_STRING(1));
        return 0;
    case CG_GAME_MESSAGE:
        CL_ConsolePrint(CG_STRING(1), CON_DEST_GAME_MESSAGE, 0,
                        CG_INT(2));
        return 0;
    case CG_BOLD_GAME_MESSAGE:
        CL_ConsolePrint(CG_STRING(1), CON_DEST_BOLD_GAME_MESSAGE, 0,
                        CG_INT(2));
        return 0;
    case CG_DEATH_MESSAGE:
        CL_DeathMessagePrint(
            CON_DEST_GAME_MESSAGE, CG_STRING(1), CG_CONST_PTR(float, 2),
            CG_STRING(3), CG_CONST_PTR(float, 4), CG_STRING(5),
            CL_CgameSyscallFloatArgument(CG_ARG(6)),
            CL_CgameSyscallFloatArgument(CG_ARG(7)),
            CG_CONST_PTR(float, 8), 0);
        return 0;
    case CG_SUBTITLE:
        CL_SubtitlePrint(CG_STRING(1), CG_INT(2), CG_INT(3));
        return 0;
    case CG_MILLISECONDS:
        return Sys_Milliseconds();
    case CG_CVAR_REGISTER:
        Cvar_Register(CG_PTR(vmCvar_t, 1), CG_STRING(2),
                      CG_STRING(3), (uint32_t)CG_ARG(4));
        return 0;
    case CG_CVAR_UPDATE:
        Cvar_Update(CG_PTR(vmCvar_t, 1));
        return 0;
    case CG_CVAR_SET:
        Cvar_Set(CG_STRING(1), CG_STRING(2));
        return 0;
    case CG_CVAR_SET_VALUE:
        Cvar_VMSet(CG_PTR(vmCvar_t, 1), CG_STRING(2));
        return 0;
    case CG_CVAR_VARIABLE_STRING_BUFFER:
        Cvar_VariableStringBuffer(CG_STRING(1), CG_PTR(char, 2),
                                  CG_INT(3));
        return 0;
    case CG_ARGC:
        return Cmd_Argc();
    case CG_ARGV:
        Cmd_ArgvBuffer(CG_INT(1), CG_PTR(char, 2), CG_INT(3));
        return 0;
    case CG_ARGS:
        Cmd_ArgsBuffer(CG_PTR(char, 1), CG_INT(2));
        return 0;
    case CG_FS_FOPEN_FILE:
        return FS_FOpenFileByMode(CG_STRING(1), CG_PTR(int32_t, 2),
                                  (fsMode_t)CG_INT(3));
    case CG_FS_READ:
        return FS_Read(CG_PTR(void, 1), CG_INT(2), CG_INT(3));
    case CG_FS_WRITE:
        return FS_Write(CG_CONST_PTR(void, 1), CG_INT(2), CG_INT(3));
    case CG_FS_FCLOSE_FILE:
        FS_FCloseFile(CG_INT(1));
        return 0;
    case CG_FS_GETFILELIST:
        return FS_GetFileList(CG_STRING(1), CG_STRING(2),
                              CG_PTR(char, 3), CG_INT(4));
    case CG_COM_SAVE_CVARS_TO_BUFFER:
        return Com_SaveCvarsToBuffer(
            (const char *const *)(uintptr_t)CG_ARG(1), CG_INT(2),
            CG_PTR(char, 3), CG_SIZE(4));
    case CG_COM_LOAD_CVARS_FROM_BUFFER:
        return Com_LoadCvarsFromBuffer(
            (const char *const *)(uintptr_t)CG_ARG(1), CG_INT(2),
            CG_PTR(char, 3), CG_STRING(4));
    case CG_SEND_CONSOLE_COMMAND:
        Cbuf_AddText(CG_STRING(1));
        return 0;
    case CG_ADD_COMMAND:
        CL_AddCgameCommand(CG_STRING(1));
        return 0;
    case CG_SEND_CLIENT_COMMAND:
        CL_AddReliableCommand(CG_STRING(1));
        return 0;
    case CG_UPDATE_SCREEN:
        SCR_UpdateScreen();
        return 0;
    case CG_DRAW_SLIDING_FADE_ELEMENT:
        /* NOT_FROM_ORIGINAL_SOURCE: only the left-side gameplay notify group
         * is edge-expanded; scoreboard and centered fade groups remain on the
         * stock 640x480 composition. */
        Con_DrawNotify(coduomp_left_hud_virtual_x_compat(CG_INT(1)),
                       CG_INT(2),
                       CL_CgameSyscallFloatArgument(CG_ARG(3)),
                       CG_INT(4));
        return 0;
    case CG_DRAW_SCOREBOARD_FADE_ELEMENT:
        Con_DrawBoldMessages(CG_INT(1), CG_INT(2),
                             CL_CgameSyscallFloatArgument(CG_ARG(3)),
                             CG_INT(4));
        return 0;
    case CG_DRAW_DEBUG_FADE_ELEMENT:
        Con_DrawMiniConsole(CG_INT(1), CG_INT(2),
                            CL_CgameSyscallFloatArgument(CG_ARG(3)));
        return 0;
    case CG_DRAW_FIXED_FADE_ELEMENT:
        Con_DrawSubtitles(CG_INT(1), CG_INT(2),
                          CL_CgameSyscallFloatArgument(CG_ARG(3)),
                          CG_INT(4));
        return 0;
    case CG_NOTIFY_PLAYER_SPAWNED:
        Con_DrawSay(CG_INT(1));
        return 0;
    case CG_CM_LOAD_MAP:
        CL_CM_LoadMap(CG_STRING(1));
        return 0;
    case CG_CM_NUM_INLINE_MODELS:
        return cm_numSubModels;
    case CG_CM_INLINE_MODEL:
        return CM_InlineModel(CG_INT(1));
    case CG_CM_TEMP_BOX_MODEL:
        return CM_TempBoxModel(
            CG_CONST_PTR(float, 1), CG_CONST_PTR(float, 2),
            CG_INT(3), qfalse);
    case CG_CM_TEMP_CAPSULE_MODEL:
        return CM_TempBoxModel(
            CG_CONST_PTR(float, 1), CG_CONST_PTR(float, 2),
            CG_INT(3), qtrue);
    case CG_CM_POINT_CONTENTS:
        return CM_PointContents(CG_CONST_PTR(float, 1), CG_INT(2));
    case CG_CM_TRANSFORMED_POINT_CONTENTS:
        return CM_TransformedPointContents(
            CG_CONST_PTR(float, 1), CG_INT(2),
            CG_CONST_PTR(float, 3), CG_CONST_PTR(float, 4));
    case CG_CM_BOX_TRACE:
        CM_BoxTrace(CG_PTR(trace_t, 1), CG_CONST_PTR(float, 2),
                    CG_CONST_PTR(float, 3), CG_CONST_PTR(float, 4),
                    CG_CONST_PTR(float, 5), CG_INT(6), CG_INT(7),
                    qfalse);
        return 0;
    case CG_CM_TRANSFORMED_BOX_TRACE:
        CM_TransformedBoxTraceExternal(
            CG_PTR(trace_t, 1), CG_CONST_PTR(float, 2),
            CG_CONST_PTR(float, 3), CG_CONST_PTR(float, 4),
            CG_CONST_PTR(float, 5), CG_INT(6), CG_INT(7),
            CG_CONST_PTR(float, 8), CG_CONST_PTR(float, 9), qfalse);
        return 0;
    case CG_CM_CAPSULE_TRACE:
        CM_BoxTrace(CG_PTR(trace_t, 1), CG_CONST_PTR(float, 2),
                    CG_CONST_PTR(float, 3), CG_CONST_PTR(float, 4),
                    CG_CONST_PTR(float, 5), CG_INT(6), CG_INT(7),
                    qtrue);
        return 0;
    case CG_CM_TRANSFORMED_CAPSULE_TRACE:
        CM_TransformedBoxTraceExternal(
            CG_PTR(trace_t, 1), CG_CONST_PTR(float, 2),
            CG_CONST_PTR(float, 3), CG_CONST_PTR(float, 4),
            CG_CONST_PTR(float, 5), CG_INT(6), CG_INT(7),
            CG_CONST_PTR(float, 8), CG_CONST_PTR(float, 9), qtrue);
        return 0;
    case CG_CM_MARKFRAGMENTS:
        return rendererExports.MarkFragments(
            CG_INT(1), CG_CONST_PTR(vec3_t, 2),
            CG_CONST_PTR(float, 3), CG_CONST_PTR(vec3_t, 4),
            CL_CgameSyscallFloatArgument(CG_ARG(5)),
            CG_INT(6), CG_PTR(polyVert_t, 7), CG_INT(8),
            CG_PTR(markFragment_t, 9), CG_INT(10));
    case CG_R_LOAD_WORLD_MAP:
        /* 0x00402e31..0x00402e3d passes a literal NULL checksum output after
         * arguments[1].  The cgame wrapper at 0x3003d910 likewise supplies
         * only the map-name payload; arguments[2] is not part of this trap. */
        rendererExports.LoadWorldMap(CG_STRING(1), NULL);
        return 0;
    case CG_R_REGISTER_MODEL:
        return rendererExports.RegisterModel(CG_STRING(1),
                                              CG_INT(2));
    case CG_R_FINISH_LOADING_MODELS:
        rendererExports.FinishLoadingModels();
        return 0;
    case CG_R_SET_IGNORE_PRECACHE_ERRORS:
        rendererExports.SetIgnorePrecacheErrors((qboolean)CG_ARG(1));
        return 0;
    case CG_REGISTER_MATERIAL:
        return rendererExports.RegisterShader(CG_STRING(1), CG_INT(2));
    case CG_R_GET_SHADER_FROM_MODEL:
        return rendererExports.GetShaderFromModel(CG_INT(1), CG_INT(2));
    case CG_DOBJ_WRAP_MODEL:
        return (intptr_t)rendererExports.GetXModelByHandle(CG_INT(1));
    case CG_R_REGISTER_FONT:
        rendererExports.RegisterFont(CG_STRING(1), CG_INT(2),
                                     CG_PTR(fontInfo_t, 3),
                                     CG_INT(4));
        return 0;
    case CG_R_TEXT_WIDTH:
        /* 0x00402f5b..0x00402f73 passes the cgame's four payload words as
         * (text, font, scale, 0.0f, limit); fixedAdvance is inserted here and
         * is not a fifth cgame argument. */
        return rendererExports.TextWidth(
            CG_STRING(1), CG_INT(2),
            CL_CgameSyscallFloatArgument(CG_ARG(3)),
            0.0f, CG_INT(4));
    case CG_R_TEXT_HEIGHT:
        return rendererExports.TextHeight(
            CG_INT(1), CL_CgameSyscallFloatArgument(CG_ARG(2)));
    case CG_R_TEXT_PAINT:
        rendererExports.TextPaint(
            CL_CgameSyscallFloatArgument(CG_ARG(1)),
            CL_CgameSyscallFloatArgument(CG_ARG(2)), CG_INT(3),
            CL_CgameSyscallFloatArgument(CG_ARG(4)),
            CG_CONST_PTR(float, 5), CG_STRING(6),
            CL_CgameSyscallFloatArgument(CG_ARG(7)), CG_INT(8),
            CG_INT(9));
        return 0;
    case CG_R_TEXT_PAINT_WITH_CURSOR:
        /* 0x00402fef..0x00403021 consumes ten cgame payload words and
         * explicitly PUSHes 0 for the renderer's fixedAdvance parameter
         * between cursorCharacter and limit. */
        rendererExports.TextPaintWithCursor(
            CL_CgameSyscallFloatArgument(CG_ARG(1)),
            CL_CgameSyscallFloatArgument(CG_ARG(2)), CG_INT(3),
            CL_CgameSyscallFloatArgument(CG_ARG(4)),
            CG_CONST_PTR(float, 5), CG_STRING(6), CG_INT(7),
            (uint8_t)CG_ARG(8),
            0.0f, CG_INT(9), CG_INT(10));
        return 0;
    case CG_SE_TRANSLATE_REFERENCE:
        return (intptr_t)SEH_StringEd_GetString(CG_STRING(1));
    case CG_SE_LOCALIZE_MESSAGE:
        return (intptr_t)SEH_LocalizeTextMessage(
            CG_STRING(1), CG_STRING(2), 0);
    case CG_SE_PRINT_STRLEN:
        return SEH_PrintStrlen(CG_STRING(1));
    case CG_SE_READ_CHAR_FROM_STRING:
        return SEH_ReadCharFromString(
            CG_PTR(const char *, 1), CG_PTR(qboolean, 2));
    case CG_R_CLEAR_SCENE:
        rendererExports.ClearScene();
        return 0;
    case CG_R_ADD_REF_ENTITY_TO_SCENE:
        rendererExports.AddRefEntityToScene(
            CG_CONST_PTR(refEntity_t, 1), NULL);
        return 0;
    case CG_R_GET_ENTITY_TOKEN:
        return rendererExports.GetEntityToken(CG_PTR(char, 1), CG_INT(2));
    case CG_HUNK_USED:
        return hunk_used;
    case CG_R_ADDPOLYTOSCENE:
        rendererExports.AddPolyToScene(
            CG_INT(1), CG_INT(2), CG_CONST_PTR(polyVert_t, 3));
        return 0;
    case CG_R_ADD_POLYS_TO_SCENE:
        rendererExports.AddPolysToScene(
            CG_INT(1), CG_INT(2), CG_CONST_PTR(polyVert_t, 3),
            CG_INT(4));
        return 0;
    case CG_R_ADD_LIGHT_TO_SCENE:
        rendererExports.AddLightToScene(
            CG_CONST_PTR(float, 1),
            CL_CgameSyscallFloatArgument(CG_ARG(2)),
            CL_CgameSyscallFloatArgument(CG_ARG(3)),
            CL_CgameSyscallFloatArgument(CG_ARG(4)),
            CL_CgameSyscallFloatArgument(CG_ARG(5)));
        return 0;
    case CG_R_ADD_CORONA_TO_SCENE:
        rendererExports.AddCoronaToScene(
            CG_CONST_PTR(float, 1),
            CL_CgameSyscallFloatArgument(CG_ARG(2)),
            CL_CgameSyscallFloatArgument(CG_ARG(3)),
            CL_CgameSyscallFloatArgument(CG_ARG(4)),
            CL_CgameSyscallFloatArgument(CG_ARG(5)), CG_INT(6), CG_INT(7));
        return 0;
    case CG_R_SET_FOG:
        rendererExports.SetFog(
            CG_INT(1), CG_INT(2), CG_INT(3),
            CL_CgameSyscallFloatArgument(CG_ARG(4)),
            CL_CgameSyscallFloatArgument(CG_ARG(5)),
            CL_CgameSyscallFloatArgument(CG_ARG(6)),
            CL_CgameSyscallFloatArgument(CG_ARG(7)));
        return 0;
    case CG_R_RENDER_SCENE:
        rendererExports.RenderScene(CG_CONST_PTR(refdef_t, 1));
        return 0;
    case CG_R_SAVE_SCREEN:
        rendererExports.SaveScreen();
        return 0;
    case CG_R_BLEND_SAVED_SCREEN:
        rendererExports.BlendSavedScreen(CG_INT(1));
        return 0;
    case CG_R_SETCOLOR:
        rendererExports.SetColor(CG_CONST_PTR(float, 1));
        return 0;
    case CG_R_DRAWSTRETCHPIC: {
        const qboolean pumpScope = CL_CgamePump2dScopeBegin();
        rendererExports.StretchPic(
            CL_CgameSyscallFloatArgument(CG_ARG(1)),
            CL_CgameSyscallFloatArgument(CG_ARG(2)),
            CL_CgameSyscallFloatArgument(CG_ARG(3)),
            CL_CgameSyscallFloatArgument(CG_ARG(4)),
            CL_CgameSyscallFloatArgument(CG_ARG(5)),
            CL_CgameSyscallFloatArgument(CG_ARG(6)),
            CL_CgameSyscallFloatArgument(CG_ARG(7)),
            CL_CgameSyscallFloatArgument(CG_ARG(8)), CG_INT(9));
        CL_CgamePump2dScopeEnd(pumpScope);
        return 0;
    }
    case CG_R_DRAW_STRETCH_PIC_ROTATE: {
        const qboolean pumpScope = CL_CgamePump2dScopeBegin();
        rendererExports.StretchPicGradient(
            CL_CgameSyscallFloatArgument(CG_ARG(1)),
            CL_CgameSyscallFloatArgument(CG_ARG(2)),
            CL_CgameSyscallFloatArgument(CG_ARG(3)),
            CL_CgameSyscallFloatArgument(CG_ARG(4)),
            CL_CgameSyscallFloatArgument(CG_ARG(5)),
            CL_CgameSyscallFloatArgument(CG_ARG(6)),
            CL_CgameSyscallFloatArgument(CG_ARG(7)),
            CL_CgameSyscallFloatArgument(CG_ARG(8)), CG_INT(9),
            CG_CONST_PTR(float, 10), CG_INT(11));
        CL_CgamePump2dScopeEnd(pumpScope);
        return 0;
    }
    case CG_R_DRAW_QUAD_PIC: {
        const qboolean pumpScope = CL_CgamePump2dScopeBegin();
        rendererExports.StretchPicRotate(
            CL_CgameSyscallFloatArgument(CG_ARG(1)),
            CL_CgameSyscallFloatArgument(CG_ARG(2)),
            CL_CgameSyscallFloatArgument(CG_ARG(3)),
            CL_CgameSyscallFloatArgument(CG_ARG(4)),
            CL_CgameSyscallFloatArgument(CG_ARG(5)),
            CL_CgameSyscallFloatArgument(CG_ARG(6)),
            CL_CgameSyscallFloatArgument(CG_ARG(7)),
            CL_CgameSyscallFloatArgument(CG_ARG(8)),
            CL_CgameSyscallFloatArgument(CG_ARG(9)), CG_INT(10));
        CL_CgamePump2dScopeEnd(pumpScope);
        return 0;
    }
    case CG_R_DRAW_ROTATED_QUAD: {
        const qboolean pumpScope = CL_CgamePump2dScopeBegin();
        rendererExports.DrawQuadPic(
            CG_CONST_PTR(vec2_t, 1), CG_CONST_PTR(vec2_t, 2),
            CG_INT(3));
        CL_CgamePump2dScopeEnd(pumpScope);
        return 0;
    }
    case CG_R_MODEL_BOUNDS:
        rendererExports.ModelBounds(CG_INT(1), CG_PTR(float, 2),
                                    CG_PTR(float, 3));
        return 0;
    case CG_GET_GLCONFIG:
        memcpy(CG_PTR(void, 1), &cls.rendererConfig,
               sizeof(cls.rendererConfig));
        return 0;
    case CG_GET_GAME_STATE:
        CL_GetGameState(CG_PTR(gameState_t, 1));
        return 0;
    case CG_GET_CURRENT_SNAPSHOT_NUMBER:
        CL_GetCurrentSnapshotNumber(CG_PTR(int32_t, 1),
                                    CG_PTR(int32_t, 2));
        return 0;
    case CG_GET_SNAPSHOT:
        return CL_GetSnapshot(CG_INT(1), CG_PTR(snapshot_t, 2));
    case CG_GET_SERVER_COMMAND:
        return CL_GetServerCommand(CG_INT(1));
    case CG_GET_CURRENT_CMD_NUMBER:
        return CL_GetCurrentCmdNumber();
    case CG_GET_USER_CMD:
        return CL_GetUserCmd(CG_INT(1), CG_PTR(usercmd_t, 2));
    case CG_SET_USER_CMD_VALUE:
        CL_SetUserCmdValue(
            CG_INT(1), CL_CgameSyscallFloatArgument(CG_ARG(2)));
        return 0;
    case CG_SET_USER_CMD_AIM_VALUES:
        CL_SetUserCmdAimValues(CG_CONST_PTR(float, 1));
        return 0;
    case CG_SET_SHELLSHOCK_SCREEN_BLUR:
        CL_SetUserCmdInShellshock(CG_INT(1));
        return 0;
    case CG_SET_USER_CMD_FLAME_DAMAGE:
        CL_SetUserCmdFlameDamage(CG_INT(1));
        return 0;
    case CG_R_REGISTERSHADER:
        return rendererExports.RegisterShaderNoMip(CG_STRING(1),
                                                    CG_INT(2));
    case CG_MEMORY_REMAINING:
        return Hunk_MemoryRemaining();
    case CG_KEY_IS_DOWN:
        return Key_IsDown(CG_INT(1));
    case CG_KEY_GET_CATCHER:
        return cls.keyCatchers;
    case CG_KEY_SET_CATCHER:
        Key_SetCatcher(CG_INT(1));
        return 0;
    case CG_KEY_GET_KEY:
        return Key_GetKey(CG_STRING(1));
    case CG_CL_LOOKUP_COLOR:
        CL_LookupColor((uint8_t)CG_ARG(1), CG_PTR(float, 2));
        return 0;
    case CG_PC_ADD_GLOBAL_DEFINE:
        return PC_AddGlobalDefine(CG_STRING(1));
    case CG_PC_LOAD_SOURCE:
        return PC_LoadSourceHandle(CG_STRING(1));
    case CG_PC_FREE_SOURCE:
        return PC_FreeSourceHandle(CG_INT(1));
    case CG_PC_READ_TOKEN:
        return PC_ReadTokenHandle(CG_INT(1), CG_PTR(pc_token_t, 2));
    case CG_PC_SOURCE_FILE_AND_LINE:
        return PC_SourceFileAndLine(
            CG_INT(1), CG_PTR(char, 2), CG_PTR(int32_t, 3));
    case CG_REAL_TIME:
        return (intptr_t)Com_RealTime(CG_PTR(qtime_t, 1));
    case CG_PM_NOTIFY_VELOCITY:
        Sys_SnapVector(CG_PTR(float, 1));
        return 0;
    case CG_REMOVE_COMMAND:
        Cmd_RemoveCommand(CG_STRING(1));
        return 0;
    case CG_CIN_PLAY_CINEMATIC:
        return CIN_PlayCinematic(
            CG_STRING(1), CG_INT(2), CG_INT(3), CG_INT(4),
            CG_INT(5), CG_INT(6));
    case CG_CIN_STOP_CINEMATIC:
        return CIN_StopCinematic(CG_INT(1));
    case CG_CIN_RUN_CINEMATIC:
        return CIN_RunCinematic(CG_INT(1));
    case CG_CIN_DRAW_CINEMATIC:
        CIN_DrawCinematic(CG_INT(1));
        return 0;
    case CG_CIN_SET_EXTENTS:
        CIN_SetExtents(CG_INT(1), CG_INT(2), CG_INT(3), CG_INT(4),
                       CG_INT(5));
        return 0;
    case CG_R_TRACK_STATISTICS:
        rendererExports.TrackStatistics(
            CG_PTR(renderer_frame_statistics_t, 1));
        return 0;
    case CG_GET_VIEW_INFO:
        return rendererExports.PickShader(
            CG_CONST_PTR(float, 1), CG_CONST_PTR(float, 2),
            CG_PTR(char, 3), CG_PTR(char, 4), CG_PTR(char, 5),
            CG_INT(6));
    case CG_MEMSET:
        return (intptr_t)memset(CG_PTR(void, 1), CG_INT(2), CG_SIZE(3));
    case CG_MEMCPY:
        return (intptr_t)memcpy(CG_PTR(void, 1), CG_CONST_PTR(void, 2),
                                CG_SIZE(3));
    case CG_STRNCPY:
        return (intptr_t)strncpy(CG_PTR(char, 1), CG_STRING(2),
                                 CG_SIZE(3));
    case CG_SIN:
        return FloatAsInt(sinf(
            CL_CgameSyscallFloatArgument(CG_ARG(1))));
    case CG_COS:
        return FloatAsInt(cosf(
            CL_CgameSyscallFloatArgument(CG_ARG(1))));
    case CG_ATAN2:
        return FloatAsInt(atan2f(
            CL_CgameSyscallFloatArgument(CG_ARG(1)),
            CL_CgameSyscallFloatArgument(CG_ARG(2))));
    case CG_SQRT:
        return FloatAsInt(sqrtf(
            CL_CgameSyscallFloatArgument(CG_ARG(1))));
    case CG_FLOOR:
        return FloatAsInt((float)floor(
            (double)CL_CgameSyscallFloatArgument(CG_ARG(1))));
    case CG_CEIL:
        return FloatAsInt((float)ceil(
            (double)CL_CgameSyscallFloatArgument(CG_ARG(1))));
    case CG_TEST_PRINT_INT:
        Com_Printf("%s%i", CG_STRING(1), CG_INT(2));
        return 0;
    case CG_TEST_PRINT_FLOAT:
        Com_Printf("%s%f", CG_STRING(1),
                   (double)CL_CgameSyscallFloatArgument(CG_ARG(2)));
        return 0;
    case CG_ACOS:
        return FloatAsInt(Q_acos(
            CL_CgameSyscallFloatArgument(CG_ARG(1))));
    case CG_R_REGISTERMENU:
        return VM_Call(
            coduo_uiVm, CG_UI_REGISTER_MENU, CG_ARG(1),
            CG_RENDERER_LOAD_MODE, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    case CG_UI_IS_MENU_OPEN:
        return CL_CgameIsMenuOpen(CG_STRING(1));
    case CG_UI_CLOSE_POPUP:
        CL_CgameClosePopup(CG_STRING(1));
        return 0;
    case CG_MAP_RESTART_RESET_RENDERER:
        CL_CgameResetUi();
        return 0;
    case CG_UI_GET_MAP_DISPLAY_NAME:
        return VM_Call(
            coduo_uiVm, CG_UI_GET_MAP_DISPLAY_NAME_COMMAND, CG_ARG(1),
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    case CG_UI_GET_GAMETYPE_DISPLAY_NAME:
        return VM_Call(
            coduo_uiVm, CG_UI_GET_GAMETYPE_DISPLAY_NAME_COMMAND,
            CG_ARG(1),
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    case CG_CL_GET_SERVER_IP_ADDRESS:
        return (intptr_t)CL_GetServerIPAddress();
    case CG_IS_IN_MATCH_TIMEOUT:
        return cls.frameTime == 0;
    case CG_XANIM_PRECACHE:
        XAnimLoadFile(CG_STRING(1), Hunk_AllocXAnimPrecache);
        return 0;
    case CG_XANIM_CREATE_ANIMS:
        return (intptr_t)XAnimAllocTree(
            CG_STRING(1), (uint32_t)CG_ARG(2),
            Hunk_AllocXAnimClientCreate);
    case CG_XANIM_CREATE:
        XAnimSetLeafNode(CG_PTR(XAnim, 1),
                         (uint16_t)CG_ARG(2), CG_STRING(3));
        return 0;
    case CG_XANIM_CREATE_TREE:
        return (intptr_t)XAnimAllocRuntimeTree(
            CG_PTR(XAnim, 1),
            Hunk_AllocXAnimClientCreateTree);
    case CG_XANIM_BLEND:
        XAnimSetParentNode(
            CG_PTR(XAnim, 1), (uint16_t)CG_ARG(2),
            CG_STRING(3), (uint16_t)CG_ARG(4),
            (uint16_t)CG_ARG(5), (uint16_t)CG_ARG(6));
        return 0;
    case CG_DOBJ_DESTROY:
        /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
        if (CG_PTR(DObj, 1) != NULL) {
            DObjFree(CG_PTR(DObj, 1), qtrue);
        }
        return 0;
    case CG_XANIM_CLEAR_GOAL_WEIGHT:
        XAnimClearGoalWeight(
            CG_PTR(XAnimTree, 1), CG_INT(2),
            CL_CgameSyscallFloatArgument(CG_ARG(3)));
        return 0;
    case CG_XANIM_CLEAR_TREE_GOAL_WEIGHTS:
        XAnimClearTreeGoalWeights(
            CG_PTR(XAnimTree, 1), CG_INT(2),
            CL_CgameSyscallFloatArgument(CG_ARG(3)));
        return 0;
    case CG_XANIM_CLEAR_TREE_GOAL_WEIGHTS_STRICT:
        XAnimClearTreeGoalWeightsStrict(
            CG_PTR(XAnimTree, 1), CG_INT(2),
            CL_CgameSyscallFloatArgument(CG_ARG(3)));
        return 0;
    case CG_XANIM_SET_COMPLETE_GOAL_WEIGHT_KNOB:
        XAnimSetCompleteGoalWeightKnob(
            CG_PTR(XAnimTree, 1), CG_INT(2),
            CL_CgameSyscallFloatArgument(CG_ARG(3)),
            CL_CgameSyscallFloatArgument(CG_ARG(4)),
            CL_CgameSyscallFloatArgument(CG_ARG(5)),
            (uint16_t)CG_ARG(6), 0, (qboolean)CG_ARG(7));
        return 0;
    case CG_XANIM_SET_COMPLETE_GOAL_WEIGHT_KNOB_ALL:
        return XAnimSetCompleteGoalWeightKnobAll(
            CG_PTR(XAnimTree, 1), CG_INT(2), CG_INT(3),
            CL_CgameSyscallFloatArgument(CG_ARG(4)),
            CL_CgameSyscallFloatArgument(CG_ARG(5)),
            CL_CgameSyscallFloatArgument(CG_ARG(6)),
            (uint16_t)CG_ARG(7), 0, (qboolean)CG_ARG(8));
    case CG_XANIM_CLEAR_CHILD_GOAL_WEIGHTS:
        XAnimClearChildGoalWeights(
            CG_PTR(XAnimTree, 1), CG_INT(2),
            CL_CgameSyscallFloatArgument(CG_ARG(3)));
        return 0;
    case CG_XANIM_SET_GOAL_WEIGHT:
        return XAnimSetGoalWeight(
            CG_PTR(XAnimTree, 1), CG_INT(2),
            CL_CgameSyscallFloatArgument(CG_ARG(3)),
            CL_CgameSyscallFloatArgument(CG_ARG(4)),
            CL_CgameSyscallFloatArgument(CG_ARG(5)),
            (uint16_t)CG_ARG(6), 0, (qboolean)CG_ARG(7));
    case CG_XANIM_SET_COMPLETE_GOAL_WEIGHT:
        XAnimSetCompleteGoalWeight(
            CG_PTR(XAnimTree, 1), CG_INT(2),
            CL_CgameSyscallFloatArgument(CG_ARG(3)),
            CL_CgameSyscallFloatArgument(CG_ARG(4)),
            CL_CgameSyscallFloatArgument(CG_ARG(5)),
            (uint16_t)CG_ARG(6), 0, (qboolean)CG_ARG(7));
        return 0;
    case CG_XANIM_SET_ANIM_RATE:
        XAnimSetAnimRate(
            CG_PTR(XAnimTree, 1), CG_INT(2),
            CL_CgameSyscallFloatArgument(CG_ARG(3)));
        return 0;
    case CG_XANIM_IS_LOOPED_BY_TREE_INDEX:
        return XAnimIsLooped(
            Scr_GetAnims((uint32_t)CG_ARG(1)), CG_INT(2));
    case CG_XANIM_IS_LOOPING:
        return XAnimIsLooped(CG_PTR(XAnim, 1), CG_INT(2));
    case CG_XANIM_SET_TIME:
        XAnimSetTime(
            CG_PTR(XAnimTree, 1), CG_INT(2),
            CL_CgameSyscallFloatArgument(CG_ARG(3)));
        return 0;
    case CG_XANIM_GET_TIME:
        return FloatAsInt(XAnimGetTime(
            CG_PTR(XAnimTree, 1), CG_INT(2)));
    case CG_XANIM_GET_WEIGHT:
        return FloatAsInt(XAnimGetWeight(
            CG_PTR(XAnimTree, 1), CG_INT(2)));
    case CG_DOBJ_INVALIDATE_SKELS:
        CL_DObjInvalidateSkels();
        return 0;
    case CG_DOBJ_ADVANCE_SERVER_TIME:
        /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
        if (CG_PTR(DObj, 1) != NULL) {
            DObjUpdateClientInfo(
                CG_PTR(DObj, 1),
                CL_CgameSyscallFloatArgument(CG_ARG(2)));
        }
        return 0;
    case CG_DOBJ_GET_CLIENT_NOTIFY_LIST:
        return DObjGetClientNotifyList(
            CG_PTR(xanim_deferred_notify_t *, 1));
    case CG_DOBJ_CALC_ANIM:
        CL_DObjCalcAnim(CG_PTR(DObj, 1),
                        CG_CONST_PTR(uint32_t, 2));
        return 0;
    case CG_DOBJ_DISPLAY_ANIM:
        /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
        if (CG_PTR(DObj, 1) != NULL) {
            DObjDisplayAnim(CG_PTR(DObj, 1));
        }
        return 0;
    case CG_XANIM_CALC_DELTA:
        XAnimCalcDelta(
            CG_PTR(XAnimTree, 1), CG_INT(2),
            CG_PTR(float, 3), CG_PTR(float, 4), CG_INT(5));
        return 0;
    case CG_XANIM_CALC_ABS_DELTA:
        XAnimCalcAbsDelta(
            CG_PTR(XAnimTree, 1), CG_INT(2),
            CG_PTR(float, 3), CG_PTR(float, 4));
        return 0;
    case CG_XANIM_GET_REL_DELTA:
        XAnimGetRelDelta(
            Scr_GetAnims((uint32_t)CG_ARG(1)), CG_INT(2),
            CG_PTR(float, 3), CG_PTR(float, 4),
            CL_CgameSyscallFloatArgument(CG_ARG(5)),
            CL_CgameSyscallFloatArgument(CG_ARG(6)));
        return 0;
    case CG_XANIM_GET_ABS_DELTA:
        XAnimGetAbsDelta(
            Scr_GetAnims((uint32_t)CG_ARG(1)), CG_INT(2),
            CG_PTR(float, 3), CG_PTR(float, 4),
            CL_CgameSyscallFloatArgument(CG_ARG(5)));
        return 0;
    case CG_DOBJ_GET_BONE_MATRICES:
        return (intptr_t)DObjGetMatrixArray(
            CG_PTR(DObj, 1), CG_INT(2));
    case CG_DOBJ_GET_ROT_TRANS_ARRAY:
        return (intptr_t)DObjGetRotTransArray(
            CG_PTR(DObj, 1));
    case CG_DOBJ_SET_ROT_TRANS_INDEX:
        return DObjSetRotTransIndex(
            CG_PTR(DObj, 1), CG_CONST_PTR(uint8_t, 2),
            CG_INT(3));
    case CG_DOBJ_SET_CONTROL_ROT_TRANS_INDEX:
        return DObjSetControlRotTransIndex(
            CG_PTR(DObj, 1), CG_CONST_PTR(uint8_t, 2),
            CG_INT(3));
    case CG_XANIM_GET_ANIM_NAME:
        return (intptr_t)XAnimGetAnimName(
            Scr_GetAnims((uint32_t)CG_ARG(1)),
            (uint16_t)CG_ARG(2));
    case CG_DOBJ_GET_HANDLE:
        return (intptr_t)Com_GetClientDObj(CG_INT(1));
    case CG_DOBJ_CREATE:
        /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
        if (CG_PTR(DObj, 4) != NULL &&
            ((uint16_t)CG_ARG(2) == 0 ||
             CG_CONST_PTR(DObjModel, 1) != NULL)) {
            DObjCreate(
                CG_CONST_PTR(DObjModel, 1), (uint16_t)CG_ARG(2),
                CG_PTR(XAnimTree, 3), CG_PTR(DObj, 4), 0);
        }
        return 0;
    case CG_CLIENT_DOBJ_CREATE:
        /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
        if ((uint16_t)CG_ARG(2) == 0 ||
            CG_CONST_PTR(DObjModel, 1) != NULL) {
            Com_ClientDObjCreate(
                CG_CONST_PTR(DObjModel, 1), (uint16_t)CG_ARG(2),
                CG_PTR(XAnimTree, 3), CG_INT(4), 0);
        }
        return 0;
    case CG_SAFE_CLIENT_DOBJ_FREE:
        Com_SafeClientDObjFree(CG_INT(1), (qboolean)CG_ARG(2));
        return 0;
    case CG_XANIM_GET_ANIMS:
        return (intptr_t)XAnimRuntimeTreeSourceTree(
            CG_PTR(XAnimTree, 1));
    case CG_DOBJ_GET_ALLOC_SKEL_SIZE:
        /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
        return CG_PTR(DObj, 1) != NULL
                   ? DObjGetAllocSkelSize(CG_PTR(DObj, 1))
                   : 0;
    case CG_DOBJ_CREATE_SKEL_FOR_BONE:
        return CL_DObjCreateSkelForBone(
            CG_PTR(DObj, 1), CG_INT(2));
    case CG_DOBJ_CREATE_SKEL_FOR_BONES:
        return CL_DObjCreateSkelForBones(
            CG_PTR(DObj, 1), CG_CONST_PTR(uint32_t, 2));
    case CG_DOBJ_GET_HIERARCHY_BITS:
        DObjGetHierarchyBits(
            CG_PTR(DObj, 1), CG_INT(2), CG_PTR(uint32_t, 3));
        return 0;
    case CG_DOBJ_CALC_SKEL:
        CL_DObjCalcSkel(CG_PTR(DObj, 1),
                        CG_CONST_PTR(uint32_t, 2));
        return 0;
    case CG_XMODEL_EXISTS:
        return XModelExists(CG_STRING(1));
    case CG_XMODEL_GET_BASE_POSE: {
        /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
        XModel *model = DObjGetModel(CG_PTR(DObj, 1), CG_INT(2));
        DObjSkelMat *basePose = CG_PTR(DObjSkelMat, 3);
        if (model == NULL || basePose == NULL) {
            return 0;
        }
        XModelGetBasePose(model, basePose);
        return 0;
    }
    case CG_DOBJ_NUM_BONES:
        return DObjNumBones(CG_PTR(DObj, 1));
    case CG_DOBJ_GET_BONE_INDEX:
        return DObjGetBoneIndex(CG_PTR(DObj, 1), CG_STRING(2));
    case CG_DOBJ_GET_BONE_NAME:
        return (intptr_t)DObjGetBoneName(
            CG_PTR(DObj, 1), CG_INT(2));
    case CG_DOBJ_BUILD_PART_COLLISION_TABLE:
        /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
        if (CG_PTR(DObj, 1) != NULL &&
            CG_PTR(XModelPartColl *, 2) != NULL) {
            DObjGetBoneInfo(
                CG_PTR(DObj, 1),
                CG_PTR(XModelPartColl *, 2));
        }
        return 0;
    case CG_DOBJ_GET_TREE:
        return (intptr_t)DObjGetTree(CG_PTR(DObj, 1));
    case CG_XANIM_IS_PRIMITIVE:
        return XAnimIsPrimitive(
            Scr_GetAnims((uint32_t)CG_ARG(1)), CG_INT(2));
    case CG_XANIM_GET_LENGTH:
#if !EMULATE_X87 && defined(CODUO_X87_TRUNCATE_I32)
        return CODUO_X87_SCALE_F32_TRUNCATE_I32(
            XAnimGetLength(CG_PTR(XAnim, 1), CG_INT(2)),
            cg_xanimMilliseconds);
#else
        return (int32_t)(cg_xanimMilliseconds * XAnimGetLength(
            CG_PTR(XAnim, 1), CG_INT(2)));
#endif
    case CG_XANIM_HAS_FINISHED:
        return XAnimHasFinished(
            CG_PTR(XAnimTree, 1), CG_INT(2));
    case CG_XANIM_GET_NUM_CHILDREN:
        return XAnimGetNumChildren(
            Scr_GetAnims((uint32_t)CG_ARG(1)), CG_INT(2));
    case CG_XANIM_GET_CHILD_AT:
        return XAnimGetChildAt(
            Scr_GetAnims((uint32_t)CG_ARG(1)),
            CG_INT(2), CG_INT(3));
    case CG_XANIM_GET_ANIM_TREE_SIZE:
        return XAnimGetAnimTreeSize(CG_PTR(XAnim, 1));
    case CG_XANIM_CLONE_ANIM_TREE:
        XAnimCloneAnimTree(
            CG_PTR(XAnimTree, 1),
            CG_PTR(XAnimTree, 2));
        return 0;
    case CG_DOBJ_DUMP_INFO:
        if (com_developer->integer != 0) {
            DObjDumpInfo(CG_PTR(DObj, 1));
        }
        return 0;
    case CG_STATMON_WARNING:
        StatMon_Warning(CG_INT(1), CG_INT(2), CG_STRING(3));
        return 0;
    case CG_GET_EXPIRING_ICON_LIST:
        StatMon_GetStatsArray(
            CG_PTR(statmon_entry_t *, 1), CG_PTR(int32_t, 2));
        return 0;
    case CG_Z_MALLOC_INTERNAL:
        return (intptr_t)Z_MallocInternal(CG_SIZE(1));
    case CG_Z_FREE_INTERNAL:
        Z_FreeInternal(CG_PTR(void, 1));
        return 0;
    case CG_COM_LOAD_SOUND_ALIASES:
        Com_LoadSoundAliases(CG_STRING(1), SND_ALIAS_BANK_CGAME);
        return 0;
    case CG_COM_SOUND_ALIAS_STRING:
        return (intptr_t)Com_SoundAliasString(
            CG_STRING(1), SND_ALIAS_BANK_CGAME);
    case CG_COM_PICK_SOUND_ALIAS:
        return (intptr_t)Com_PickSoundAlias(
            CG_STRING(1), SND_ALIAS_BANK_CGAME,
            CG_CONST_PTR(float, 2));
    case CG_COM_GET_SOUND_ALIAS:
        return (intptr_t)Com_GetSoundAlias(
            CG_INT(1), SND_ALIAS_BANK_CGAME);
    case CG_MSS_PLAY_SOUND_ALIAS:
        return MSS_PlaySoundAlias(
            CG_PTR(snd_alias_t, 1), CG_INT(2),
            CG_CONST_PTR(float, 3), CG_INT(4));
    case CG_MSS_PLAY_BLENDED_SOUND_ALIASES:
        (void)MSS_PlayBlendedSoundAliases(
            CG_PTR(snd_alias_t, 1), CG_PTR(snd_alias_t, 2),
            CL_CgameSyscallFloatArgument(CG_ARG(3)), CG_INT(4),
            CG_CONST_PTR(float, 5), CG_INT(6));
        return 0;
    case CG_SURFACE_TYPE_FROM_NAME:
        return CL_SurfaceTypeFromName(CG_STRING(1));
    case CG_SURFACE_TYPE_TO_NAME:
        return (intptr_t)CL_SurfaceTypeToName(CG_INT(1));
    case CG_ADD_DEBUG_LINE:
        CL_AddDebugLine(
            CG_CONST_PTR(float, 1), CG_CONST_PTR(float, 2),
            CG_CONST_PTR(float, 3), (qboolean)CG_ARG(4),
            CG_INT(5), qfalse);
        return 0;
    case CG_GET_WEAPON_INFO_MEMORY:
        return (intptr_t)Com_GetWeaponInfoMemory(
            CG_INT(1), CG_PTR(int32_t, 2), CG_WEAPON_MEMORY_OWNER);
    case CG_FREE_WEAPON_INFO_MEMORY:
        Com_FreeWeaponInfoMemory(
            CG_WEAPON_MEMORY_OWNER, (qboolean)CG_ARG(1));
        return 0;
    case CG_HUNK_ALLOC_ALIGN:
        return (intptr_t)Hunk_AllocAlignInternal(
            CG_SIZE(1), CG_XANIM_ALLOCATION_ALIGNMENT);
    case CG_HUNK_ALLOC_LOW_ALIGN:
        return (intptr_t)Hunk_AllocLowAlignInternal(
            CG_SIZE(1), CG_XANIM_ALLOCATION_ALIGNMENT);
    case CG_HUNK_ALLOC_ALIGN_EXPLICIT:
        return (intptr_t)Hunk_AllocAlignInternal(
            CG_SIZE(1), CG_INT(2));
    case CG_HUNK_ALLOC_LOW_ALIGN_EXPLICIT:
        return (intptr_t)Hunk_AllocLowAlignInternal(
            CG_SIZE(1), CG_INT(2));
    case CG_GET_NUM_SCRIPT_VARS:
    case CG_GET_NUM_SCRIPT_THREADS:
        return 0;
    case CG_GET_STRING_USAGE:
        return Scr_GetStringUsage();
    case CG_SET_CLIENT_LERP_ORIGIN:
        CL_SetClientLerpOrigin(
            CL_CgameSyscallFloatArgument(CG_ARG(1)),
            CL_CgameSyscallFloatArgument(CG_ARG(2)),
            CL_CgameSyscallFloatArgument(CG_ARG(3)));
        return 0;
    case CG_MSS_SET_LISTENER:
        MSS_SetListener(
            CG_INT(1), CG_CONST_PTR(float, 2),
            CG_CONST_PTR(vec3_t, 3));
        return 0;
    case CG_MSS_UPDATE_LOOPING_SOUNDS:
        MSS_UpdateLoopingSounds();
        return 0;
    case CG_MSS_STOP_SOUNDS:
        MSS_StopSounds((uint32_t)CG_ARG(1));
        return 0;
    case CG_MSS_PLAY_MUSIC_ALIAS:
        MSS_PlayMusicAlias(CG_PTR(snd_alias_t, 1));
        return 0;
    case CG_MSS_STOP_MUSIC:
        MSS_StopMusic(CG_INT(1));
        return 0;
    case CG_MSS_PLAY_AMBIENT_ALIAS:
        MSS_PlayAmbientAlias(CG_PTR(snd_alias_t, 1), CG_INT(2));
        return 0;
    case CG_MSS_FADE_ALL_SOUNDS:
        MSS_FadeAllSounds(
            CL_CgameSyscallFloatArgument(CG_ARG(1)), CG_INT(2));
        return 0;
    case CG_MSS_FADE_SELECT_SOUNDS:
        MSS_FadeSelectSounds(CG_CONST_PTR(float, 1), CG_INT(2));
        return 0;
    case CG_MSS_SET_ENVIRONMENT_EFFECTS:
        MSS_SetEnvironmentEffects(
            CG_STRING(1), CL_CgameSyscallFloatArgument(CG_ARG(2)),
            CG_INT(3));
        return 0;
    case CG_MSS_GET_SOUND_OVERLAY:
        return MSS_GetSoundOverlay(
            (mssSoundOverlayType_t)CG_ARG(1),
            CG_PTR(mss_sound_overlay_t, 2), CG_INT(3),
            CG_PTR(int32_t, 4));
    case CG_KEY_GET_BINDING_BUF:
        Key_GetBindingBuf(
            CG_INT(1), CG_PTR(char, 2), CG_INT(3));
        return 0;
    case CG_KEY_SET_BINDING:
        Key_SetBinding(CG_INT(1), CG_STRING(2));
        return 0;
    case CG_KEY_KEYNUM_TO_STRING_BUF:
        Key_KeynumToStringBuf(
            CG_INT(1), CG_PTR(char, 2), CG_INT(3));
        return 0;
    case CG_FX_REGISTER_EFFECT:
        return FX_RegisterEffect(CG_STRING(1));
    case CG_RESOLVE_TAG:
        return FX_GetBoneIndex(CG_INT(1), CG_STRING(2));
    case CG_FX_PLAY_SIMPLE_EFFECT:
        FX_PlaySimpleEffect(CG_STRING(1), CG_CONST_PTR(float, 2));
        return 0;
    case CG_FX_PLAY_EFFECT:
        FX_PlayEffect(
            CG_STRING(1), CG_CONST_PTR(float, 2),
            CG_CONST_PTR(float, 3));
        return 0;
    case CG_FX_PLAY_ENTITY_EFFECT:
        FX_PlayEntityEffect(
            CG_STRING(1), CG_CONST_PTR(float, 2),
            CG_CONST_PTR(vec3_t, 3),
            CG_CONST_PTR(sfx_bolt_info_t, 4));
        return 0;
    case CG_PLAY_EFFECT_ORIGIN:
        FX_PlaySimpleEffectID(CG_INT(1), CG_CONST_PTR(float, 2));
        return 0;
    case CG_PLAY_EFFECT_ORIENTED:
        FX_PlayEffectID(
            CG_INT(1), CG_CONST_PTR(float, 2),
            CG_CONST_PTR(float, 3));
        return 0;
    case CG_PLAY_EFFECT_ON_TAG:
        FX_PlayEntityEffectID(
            CG_INT(1), CG_CONST_PTR(float, 2),
            CG_CONST_PTR(vec3_t, 3),
            CG_CONST_PTR(sfx_bolt_info_t, 4));
        return 0;
    case CG_FX_ADD_SCHEDULED_EFFECTS:
        FX_AddScheduledEffects();
        return 0;
    case CG_FX_INIT_SYSTEM:
        return FX_InitSystem();
    case CG_FX_FREE_ACTIVE:
        return FX_FreeActive();
    case CG_FX_FREE_SYSTEM:
        return FX_FreeSystem();
    case CG_FX_ADJUST_TIME:
        FX_AdjustTime(CG_INT(1));
        return 0;
    case CG_FX_ADJUST_CAMERA:
        FX_AdjustCamera(
            CG_PTR(refdef_t, 1),
            rendererExports.GetFarPlaneDist());
        return 0;
    case CG_FX_REWIND_TIME:
        FX_RewindTime(CG_INT(1));
        return 0;
    case CG_FX_SET_WIND:
        FX_SetWind(
            CG_CONST_PTR(float, 1),
            CL_CgameSyscallFloatArgument(CG_ARG(2)));
        return 0;
    case CG_SET_SHELLSHOCK_MOUSE_LIMITS:
        /* The original dispatcher copies the two dwords unchanged and
         * CL_MouseMove consumes both with FLD. Preserve their VM float
         * payloads instead of numerically converting the integer bits. */
        cl.inputState.shellshockMouseMaxPitchSpeed =
            CL_CgameSyscallFloatArgument(CG_ARG(1));
        cl.inputState.shellshockMouseMaxYawSpeed =
            CL_CgameSyscallFloatArgument(CG_ARG(2));
        return 0;
    case CG_VEH_VIEW_ANGLE_DELTA: {
        uint32_t word;

        memcpy(&word, &CG_CONST_PTR(uint32_t, 1)[0], sizeof(word));
        memcpy(&cl.inputState.viewAngles[0], &word, sizeof(word));
        memcpy(&word, &CG_CONST_PTR(uint32_t, 1)[1], sizeof(word));
        memcpy(&cl.inputState.viewAngles[1], &word, sizeof(word));
        memcpy(&word, &CG_CONST_PTR(uint32_t, 1)[2], sizeof(word));
        memcpy(&cl.inputState.viewAngles[2], &word, sizeof(word));
        return 0;
    }
    case CG_SYNC_TIMES:
        if (cls.state == CA_ACTIVE) {
            CL_FirstSnapshot();
        }
        return 0;
    case CG_DATE_TIME_STAMP:
        return (intptr_t)Sys_DateTimeStamp();
    case CG_EXECUTE_COMMAND:
        Cmd_ExecuteString(CG_STRING(1));
        return 0;
    default:
        Com_Error(ERR_DROP,
                  "\x15" "Bad cgame system trap: %i", syscall);
        return -1;
    }
}

#undef CG_SIZE
#undef CG_INT
#undef CG_STRING
#undef CG_CONST_PTR
#undef CG_PTR
#undef CG_ARG
