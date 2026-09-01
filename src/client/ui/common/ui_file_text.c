#include "../module/ui_functions.h"

enum {
    UI_FILE_TEXT_BUFFER_SIZE = 4096
};

// Source: uo_ui_mp_x86.dll 0x400563d8..0x400573d7.
static char ui_fileTextBuffer[UI_FILE_TEXT_BUFFER_SIZE];

// Source: uo_ui_mp_x86.dll 0x4000eb90..0x4000ebe4
// Evidence: cgame_mp/mcode/uo_ui_mp_x86/FUN_4000eb90_4000ebe4.mcode
// Exact same-module PPC symbol: UI_FileText.
const char *UI_FileText(const char *filename)
{
    int32_t handle;
    int32_t length = trap_FS_FOpenFile(filename, &handle, FS_READ);

    if (handle == 0) {
        return NULL;
    }

    /* NOT_FROM_ORIGINAL_SOURCE: the complete file and its terminator must fit
     * the fixed text buffer; otherwise preserve the existing NULL result. */
    if (length < 0 || length >= UI_FILE_TEXT_BUFFER_SIZE) {
        trap_Print(va("^1text file has invalid length: %s is %i, max allowed is %i\n", filename, length, UI_FILE_TEXT_BUFFER_SIZE - 1));
        trap_FS_FCloseFile(handle);
        return NULL;
    }

    trap_FS_Read(ui_fileTextBuffer, length, handle);
    ui_fileTextBuffer[length] = '\0';
    trap_FS_FCloseFile(handle);
    return ui_fileTextBuffer;
}
