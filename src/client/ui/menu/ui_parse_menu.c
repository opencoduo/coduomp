#include "../module/ui_functions.h"

#include <string.h>

enum {
    UI_MENU_KEYWORD_COMPARE_LIMIT = 99999
};

// Source: uo_ui_mp_x86.dll 0x40009300..0x400093f4
// Evidence: cgame_mp/mcode/uo_ui_mp_x86/FUN_40009300_400093f4.mcode
// Exact same-module PPC symbol and call graph: UI_ParseMenu.
qboolean UI_ParseMenu(const char *filename, int32_t loadMode)
{
    int32_t sourceHandle;
    pc_token_t token;

    Com_DPrintf("Parsing menu file:%s\n", filename);
    sourceHandle = trap_PC_LoadSource(filename);
    if (sourceHandle == 0) {
        return qfalse;
    }

    memset(&token, 0, sizeof(token));
    while (trap_PC_ReadToken(sourceHandle, &token)) {
        if (token.string[0] == '}') {
            break;
        }
        if (Q_stricmpn("assetGlobalDef", token.string, UI_MENU_KEYWORD_COMPARE_LIMIT) == 0) {
            if (!Asset_Parse(sourceHandle, loadMode)) {
                break;
            }
        } else if (Q_stricmpn("menudef", token.string, UI_MENU_KEYWORD_COMPARE_LIMIT) == 0) {
            Menu_New(sourceHandle, loadMode);
        }
        memset(&token, 0, sizeof(token));
    }

    trap_PC_FreeSource(sourceHandle);
    return qtrue;
}
