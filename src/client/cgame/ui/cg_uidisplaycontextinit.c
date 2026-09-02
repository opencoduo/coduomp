// Source: uo_cgame_mp_x86.dll 0x3002da90..0x3002dce8
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3002da90_3002dce8.mcode
//
// CG_UIDisplayContextInit — install the original populated function-pointer and
// asset slots of the static ui_shared display context (displayContextDef_t at
// 0x30421f60), publish it as the global DC (0x30134d2c), then load the HUD menu
// list named by the cg_hudFiles cvar. The original seven empty callback slots
// receive bug-fix compatibility adapters identified below.
//
// Behavior proven from the machine code and the referenced .rdata strings:
//   1. 0x3002daaf..0x3002dc7b: store the fixed set of cgame service addresses into
//      the DC vtable slots (the block of `MOV [0x30421f60+off],<code addr>`
//      immediates). These are the ui_shared.c display->* entry points (registerShaderNoMip,
//      setColor, drawHandlePic, drawText, translateString, getLocalizedString,
//      fillRect, drawRect, ownerDrawVisible, runScript, getCVarString,
//      getCVarValue, setCVar, startLocalSound, ownerDrawHandleKey, feederCount,
//      feederItemText, feederItemImage, keynumToStringBuf, getBindingBuf, setBinding,
//      stopCinematic, ...) plus the remaining populated service slots. Every
//      original address is mapped to its recovered function; four text callbacks
//      use explicitly named native-ABI adapters around their recovered wrappers.
//   2. 0x3002dc85: publish the context: DC = &g_uiDCInstance (the store of
//      0x30421f60 into 0x30134d2c).
//   3. 0x3002dc8f: g_uiOwnerDrawCount = 0 (the store of 0 into 0x30134d40, which the
//      menu loader below also uses as menuCount).
//   4. 0x3002da9c..0x3002daa6 materialize the arguments and 0x3002dc99 calls
//      cgame_syscall(CG_CVAR_SET, "cg_hudFiles", "ui_mp/hud.txt"). The name string
//      is "cg_hudFiles" (0x30079064) and the value is "ui_mp/hud.txt" (0x300769a8).
//   5. 0x3002dc9f..0x3002dcbf: read the current cg_hudFiles value into a 0x400 stack
//      buffer via cgame_syscall(CG_CVAR_VARIABLE_STRING_BUFFER, "cg_hudFiles", buf,
//      0x400). If the value is empty (buf[0] == 0), fall back to the built-in default
//      "ui_mp/hud.txt" (0x300769a8); otherwise use the buffer contents.
//   6. 0x3002dcca..0x3002dccc: CG_LoadMenus(5, menuFile) — load the selected HUD menu
//      list. The literal 5 is pushed as the stack argument and the chosen filename is
//      passed in EDI (CG_LoadMenus's register argument).
//
// Naming: the .mcode carries the size-guessed broad-corpus name
// PM_Weapon_CheckForDeployBreakdown (win size 0x258 == matched 0x258). REJECTED: there is no
// aim-down-sight lerp math here — this is the ui_shared display-context installer.
// The real behavior (fill DC, set DC, load the HUD menu list) is the CoD/Q3
// front-end UI_Init/String_Init-style bootstrap; named CG_UIDisplayContextInit for
// its proven role. The mechanical export mislabeled the DC backing store and the
// ownerDraw/menu count with the same wrong `pm_updateaimdownsightlerp` owner; those
// are corrected in globals.{h,c}.
//
// /GS: the prologue snapshots __security_cookie (0x30081650) into the frame
// (0x3002da96/0x3002daa8) and the epilogue validates it via __security_check_cookie
// (0x30061639) — standard MSVC stack-guard framing around the 0x400 buffer.
//
// Register ABI: this is an ordinary cdecl void(void). EDI is a scratch register used
// only to hold the chosen menu-file pointer that is then passed to CG_LoadMenus in
// EDI (that callee takes its menuFile in EDI).

#include "client/cgame/client_recovered.h"
#include "client/cgame/globals.h"

// The default HUD menu-list file, used both as the cg_hudFiles default value and as
// the fallback when the cvar reads back empty. (0x300769a8 "ui_mp/hud.txt".)
static const char *const CG_HUD_MENU_FILE_DEFAULT = "ui_mp/hud.txt";

// The cvar naming the HUD menu-list file. (0x30079064 "cg_hudFiles".)
static const char *const CG_HUD_FILES_CVAR = "cg_hudFiles";

// The asset load mode passed to CG_LoadMenus (the literal 5 at 0x3002dcca).
enum {
    CG_HUD_LOAD_MODE = 5
};

enum {
    CG_UI_TEXT_FILE_BUFFER_SIZE = 4096
};

static qboolean cgame_compat_overstrikeMode;
static char cgame_compat_textFileBuffer[CG_UI_TEXT_FILE_BUFFER_SIZE];

/* NOT_FROM_ORIGINAL_SOURCE: cgame has no key-overstrike import, so retain the
 * shared edit-field mode locally instead of leaving the display slot NULL. */
static void cgame_compat_set_overstrike_mode(qboolean overstrike)
{
    cgame_compat_overstrikeMode = overstrike;
}

/* NOT_FROM_ORIGINAL_SOURCE: paired getter for the local edit-field mode. */
static qboolean cgame_compat_get_overstrike_mode(void)
{
    return cgame_compat_overstrikeMode;
}

/* NOT_FROM_ORIGINAL_SOURCE: cgame counterpart to UI_FileText using the
 * existing cgame filesystem boundary and the same fixed text capacity. */
static const char *cgame_compat_resolve_text_token(const char *filename)
{
    int32_t handle = 0;
    int32_t length = (int32_t)cgame_syscall(CG_FS_FOPEN_FILE, (intptr_t)filename, (intptr_t)&handle, FS_READ);

    if (handle == 0) {
        return NULL;
    }
    if (length < 0 || length >= CG_UI_TEXT_FILE_BUFFER_SIZE) {
        Com_Printf("^1text file has invalid length: %s is %i, max allowed is %i\n", filename, length, CG_UI_TEXT_FILE_BUFFER_SIZE - 1);
        cgame_syscall(CG_FS_FCLOSE_FILE, handle);
        return NULL;
    }
    cgame_syscall(CG_FS_READ, (intptr_t)cgame_compat_textFileBuffer, length, handle);
    cgame_compat_textFileBuffer[length] = '\0';
    cgame_syscall(CG_FS_FCLOSE_FILE, handle);
    return cgame_compat_textFileBuffer;
}

/* NOT_FROM_ORIGINAL_SOURCE: UI's matching callback is intentionally empty. */
static void cgame_compat_feeder_add_item(float feeder, const char *name, int32_t value)
{
    (void)feeder;
    (void)name;
    (void)value;
}

/* NOT_FROM_ORIGINAL_SOURCE: cgame exposes no automatic-update service. */
static void cgame_compat_get_auto_update(void)
{
}

/* NOT_FROM_ORIGINAL_SOURCE: cgame menu content is loaded only while a game is
 * running, which supplies the semantic result expected by this callback. */
static qboolean cgame_compat_running_game(void)
{
    return qtrue;
}

/* NOT_FROM_ORIGINAL_SOURCE: the cgame console-command trap appends text to the
 * engine command buffer, which is the only mode used by shared menu callers. */
static void cgame_compat_execute_text(int32_t executionMode, const char *text)
{
    (void)executionMode;
    cgame_syscall(CG_SEND_CONSOLE_COMMAND, (intptr_t)text);
}

void CG_UIDisplayContextInit(void)
{
    displayContextDef_t *display = &g_uiDCInstance;
    char cvarValue[MAX_STRING_CHARS]; // [ESP+0x14]: cg_hudFiles value buffer
    const char *menuFile;             // EDI: chosen menu-list filename

    // 0x3002daaf..0x3002dc7b: install every DC vtable slot from its fixed code
    // address. Each machine-code address has been resolved to the recovered
    // original function.
    display->registerShaderNoMip = trap_R_RegisterShaderNoMip; // +0x00
    display->setColor = trap_R_SetColor;            // +0x04
    display->drawHandlePic = CG_DrawPic;                 // +0x08
#if defined(_WIN32) && UINTPTR_MAX == UINT32_MAX
    /* The retail initializer stores these exact trap-wrapper addresses
     * (0x3003e0f0/0x3003de30/0x3003dde0/0x3003de10). Their recovered
     * declarations expose the original opaque 32-bit syscall words, whereas
     * the display table exposes the same words as semantic floats. Win32 i386
     * passes both forms in identical stack dwords, so the casts preserve the
     * proved function-pointer values and call frames. Register-based 64-bit
     * ABIs do not; those builds must deviate through the adapters below. */
    display->drawStretchPic = (ui_drawStretchPic_t)trap_R_DrawStretchPic; // +0x0c
    display->drawText = (ui_drawText_t)trap_R_Text_Paint;           // +0x10
    display->textWidth = (ui_textWidth_t)trap_R_Text_Width;          // +0x14
    display->textHeight = (ui_textHeight_t)trap_R_Text_Height;        // +0x18
#else
    display->drawStretchPic = OpenCoDUO_UI_DrawStretchPicAdapter; // +0x0c
    display->drawText = OpenCoDUO_UI_DrawTextAdapter;       // +0x10
    display->textWidth = OpenCoDUO_UI_TextWidthAdapter;      // +0x14
    display->textHeight = OpenCoDUO_UI_TextHeightAdapter;     // +0x18
#endif
    display->translateString = trap_SE_TranslateReference;     // +0x1c
    display->getLocalizedString = CG_SafeTranslateString;         // +0x20
    display->localizeWithBinding = CG_TranslateMessage;            // +0x24
    display->registerModel = CG_RegisterModel;               // +0x2c
    display->modelBounds = trap_R_ModelBounds;             // +0x30
    display->fillRect = CG_FillRect;                    // +0x34
    display->drawRect = CG_DrawRect;                    // +0x38
    display->drawSides = CG_DrawSides;                   // +0x3c
    display->drawTopBottom = CG_DrawTopBottom;               // +0x40
    display->clearScene = trap_R_ClearScene;              // +0x44
    display->addRefEntityToScene = trap_R_AddRefEntityToScene;     // +0x48
    display->renderScene = trap_R_RenderScene;             // +0x4c
    display->registerFont = trap_R_RegisterFont;            // +0x50
    display->ownerDrawItem = CG_OwnerDraw;                   // +0x54
    display->ownerDrawValue = CG_OwnerDrawValue;              // +0x58
    display->ownerDrawVisible = CG_OwnerDrawVisible;            // +0x5c
    display->runScript = CG_RunMenuScript;               // +0x60
    display->getTeamColor = CG_GetTeamColor;                // +0x64
    display->getCVarString = trap_Cvar_VariableStringBuffer; // +0x68
    display->getCVarValue = CG_Cvar_Get;                    // +0x6c
    display->setCVar = trap_Cvar_Set;                  // +0x70
    display->getConfigString = CG_ConfigString;                // +0x74
#if defined(_WIN32) && UINTPTR_MAX == UINT32_MAX
    display->drawTextWithCursor = (ui_drawTextWithCursor_t)trap_R_Text_PaintWithCursor; // +0x78
#else
    display->drawTextWithCursor = OpenCoDUO_UI_DrawTextWithCursorAdapter; // +0x78
#endif
    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    display->setOverstrikeMode = cgame_compat_set_overstrike_mode; // +0x7c
    display->getOverstrikeMode = cgame_compat_get_overstrike_mode; // +0x80
    display->startLocalSound = CG_PlayClientSoundAliasByName; // +0x84
    display->ownerDrawHandleKey = CG_OwnerDrawHandleKey; // +0x88
    display->feederCount = CG_FeederCount; // +0x8c
    display->feederItemText = CG_FeederItemText; // +0x90
    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    display->resolveTextToken = cgame_compat_resolve_text_token; // +0x94
    display->feederItemImage = CG_FeederItemImage; // +0x98
    display->feederSelection = CG_FeederSelection; // +0x9c
    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    display->feederAddItem = cgame_compat_feeder_add_item; // +0xa0
    display->getAutoUpdate = cgame_compat_get_auto_update; // +0xa4
    display->runningGame = cgame_compat_running_game; // +0xa8
    display->keynumToStringBuf = trap_Key_KeynumToStringBuf; // +0xac
    display->getBindingBuf = trap_Key_GetBindingBuf; // +0xb0
    display->setBinding = trap_Key_SetBinding; // +0xb4
    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    display->executeText = cgame_compat_execute_text; // +0xb8
    display->error = Com_Error; // +0xbc
    display->print = Com_Printf; // +0xc0
    display->ownerDrawWidth = CG_OwnerDrawWidth; // +0xc8
    display->registerAsset = trap_Com_SoundAliasString; // +0xcc
    display->playCinematic = CG_PlayCinematic; // +0xd0
    display->stopCinematic = CG_StopCinematic; // +0xd4
    display->drawCinematic = CG_DrawCinematic; // +0xd8
    display->runCinematicFrame = CG_RunCinematicFrame; // +0xdc

    // 0x3002da9c..0x3002daa6 push this call's arguments before the assignment
    // block, but the indirect CALL itself is at 0x3002dc99, after every callback
    // store and the DC/menuCount publications below. Preserve execution order,
    // not merely the earlier argument-materialization order.

    // 0x3002dc85: publish the context pointer through the shared ui layer.
    Init_Display(display);

    // 0x3002dc8f: reset the registered-menu/ownerDraw count before loading.
    menuCount = 0;

    // 0x3002dc99: set the default HUD-menu-list cvar.
    cgame_syscall(CG_CVAR_SET, (intptr_t)CG_HUD_FILES_CVAR, (intptr_t)CG_HUD_MENU_FILE_DEFAULT);

    // 0x3002dc9f..0x3002dcbf: read the current cg_hudFiles value, then choose the
    // menu file: the buffer if non-empty, else the built-in default.
    cgame_syscall(CG_CVAR_VARIABLE_STRING_BUFFER, (intptr_t)CG_HUD_FILES_CVAR, (intptr_t)cvarValue, 0x400);
    menuFile = (cvarValue[0] != '\0') ? cvarValue : CG_HUD_MENU_FILE_DEFAULT;

    // 0x3002dcca..0x3002dccc: load the selected HUD menu list.
    CG_LoadMenus(CG_HUD_LOAD_MODE, menuFile);
}
