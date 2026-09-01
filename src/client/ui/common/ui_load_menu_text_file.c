#include "../module/ui_functions.h"

enum {
    UI_MENU_TEXT_BUFFER_SIZE = 32768
};

// Source: uo_ui_mp_x86.dll 0x40057bd8..0x4005fbd7.
static char uiMenuTextBuffer[UI_MENU_TEXT_BUFFER_SIZE];
// Source: uo_ui_mp_x86.dll data 0x401c46a4.
static char *uiMenuTextFailureResult;

// Source: uo_ui_mp_x86.dll 0x40008c30..0x40008cd6
// Evidence: cgame_mp/mcode/uo_ui_mp_x86/FUN_40008c30_40008cd6.mcode
// Role name: UI_LoadMenuTextFile; exact menu diagnostics, 32 KiB bound, shared
// buffer, and filesystem syscall path establish the behavior.
char *UI_LoadMenuTextFile(const char *filename)
{
    int32_t handle;
    int32_t length = trap_FS_FOpenFile(filename, &handle, FS_READ);

    if (handle == 0) {
        trap_Print(va("^1menu file not found: %s, using default\n", filename));
        return uiMenuTextFailureResult;
    }
    if (length >= UI_MENU_TEXT_BUFFER_SIZE) {
        trap_Print(va("^1menu file too large: %s is %i, max allowed is %i",
                      filename, length, UI_MENU_TEXT_BUFFER_SIZE));
        trap_FS_FCloseFile(handle);
        return uiMenuTextFailureResult;
    }

    trap_FS_Read(uiMenuTextBuffer, length, handle);
    uiMenuTextBuffer[length] = '\0';
    trap_FS_FCloseFile(handle);
    return uiMenuTextBuffer;
}
