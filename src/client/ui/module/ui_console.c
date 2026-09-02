#include "../abi/ui_module_abi.h"
#include "ui_functions.h"
#include "ui_globals.h"

enum {
    UI_CONSOLE_COMMAND_COMPARE_LIMIT = 99999
};

// Source: uo_ui_mp_x86.dll 0x40007870..0x4000788c
// Evidence: cgame_mp/mcode/uo_ui_mp_x86/FUN_40007870_4000788c.mcode
// Same-module PPC symbol: UI_Argv.
const char *UI_Argv(int32_t index)
{
    trap_Argv(index, ui_argvBuffer, MAX_STRING_CHARS);
    return ui_argvBuffer;
}

// Source: uo_ui_mp_x86.dll 0x400078e0..0x400079b1
// Evidence: cgame_mp/mcode/uo_ui_mp_x86/FUN_400078e0_400079b1.mcode
// Same-module PPC symbol/call graph: UI_ConsoleCommand.
qboolean UI_ConsoleCommand(int32_t realtime)
{
    const char *command;

    ui_displayContextStorage.context.frameTime = realtime - ui_displayContextStorage.context.realTime;
    ui_displayContextStorage.context.realTime = realtime;
    command = UI_Argv(0);

    if (Q_stricmpn("ui_test", command, UI_CONSOLE_COMMAND_COMPARE_LIMIT) == 0) {
        UI_ShowPostGame(qtrue);
        /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
    }
    if (Q_stricmpn("ui_report", command, UI_CONSOLE_COMMAND_COMPARE_LIMIT) == 0) {
        UI_Report();
        return qtrue;
    }
    if (Q_stricmpn("ui_load", command, UI_CONSOLE_COMMAND_COMPARE_LIMIT) == 0) {
        UI_Load();
        return qtrue;
    }
    if (Q_stricmpn("ui_cache", command, UI_CONSOLE_COMMAND_COMPARE_LIMIT) == 0) {
        UI_Cache_f();
        return qtrue;
    }

    return Q_stricmpn("ui_cdkey", command, UI_CONSOLE_COMMAND_COMPARE_LIMIT) == 0;
}
