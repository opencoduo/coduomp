#include "../module/ui_functions.h"

// Source: uo_ui_mp_x86.dll 0x40007890..0x400078ac
// Evidence: cgame_mp/mcode/uo_ui_mp_x86/FUN_40007890_400078ac.mcode
// The exact same-module PPC symbol and unique 1024-byte cvar-buffer operation
// identify this function as UI_Cvar_VariableString.
const char *UI_Cvar_VariableString(const char *name)
{
    trap_Cvar_VariableStringBuffer(name, ui_menuFilesBuffer, MAX_STRING_CHARS);
    return ui_menuFilesBuffer;
}
