#include "../module/ui_functions.h"
#include "../module/ui_globals.h"

enum {
    UI_INVALID_SOURCE_HANDLE = 0
};

// Source: uo_ui_mp_x86.dll 0x40009750..0x400098d2
// Evidence: cgame_mp/mcode/uo_ui_mp_x86/FUN_40009750_400098d2.mcode
// Exact same-module PPC symbol and call graph: UI_LoadMenus.
void UI_LoadMenus(const char *menuFile, qboolean reset, int32_t loadMode)
{
    int32_t startTime = trap_Milliseconds();
    int32_t sourceHandle = trap_PC_LoadSource(menuFile);
    pc_token_t token;

    if (sourceHandle == UI_INVALID_SOURCE_HANDLE) {
        trap_Error(va("^3menu file not found: %s, using default\n", menuFile));
        sourceHandle = trap_PC_LoadSource("ui_mp/menus.txt");
        if (sourceHandle == UI_INVALID_SOURCE_HANDLE) {
            trap_Error(va("^1default menu file not found: ui_mp/menus.txt, "
                          "unable to continue!\n",
                          menuFile));
        }
    }

    ui_menuLoadActive = qtrue;
    if (reset) {
        menuCount = 0;
    }

    while (trap_PC_ReadToken(sourceHandle, &token)) {
        if (token.string[0] == '\0' || token.string[0] == '}') {
            break;
        }
        if (Q_stricmp(token.string, "loadmenu") == 0) {
            if (!Load_Menu(sourceHandle, loadMode)) {
                break;
            }
        }
    }

    Com_DPrintf("UI menu load time = %d milli seconds\n",
                trap_Milliseconds() - startTime);
    trap_PC_FreeSource(sourceHandle);
}
