#include "ui_functions.h"
#include "ui_globals.h"

#include <stddef.h>

enum {
    UI_FPS_SAMPLE_COUNT = 4,
    UI_FPS_SAMPLE_MILLISECONDS = 4000,
    UI_CURSOR_HALF_SIZE = 16,
    UI_CURSOR_SIZE = 32
};

// Source: uo_ui_mp_x86.dll 0x40008a90..0x40008bf0
// Evidence: cgame_mp/mcode/uo_ui_mp_x86/FUN_40008a90_40008bf0.mcode
// Same-module PPC symbol/call graph: _UI_Refresh.
void UI_Refresh(int32_t realtime)
{
    int32_t sampleTotal;

    ui_displayContextStorage.context.frameTime = realtime - ui_displayContextStorage.context.realTime;
    ui_displayContextStorage.context.realTime = realtime;
    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    ui_frameSamples[(uint32_t)ui_frameSampleCount % UI_FPS_SAMPLE_COUNT] = ui_displayContextStorage.context.frameTime;
    ui_frameSampleCount = coduo_int32_from_bits((uint32_t)ui_frameSampleCount + 1u);

    if (ui_frameSampleCount > UI_FPS_SAMPLE_COUNT) {
        sampleTotal = ui_frameSamples[0] + ui_frameSamples[1] + ui_frameSamples[2] + ui_frameSamples[3];
        if (sampleTotal == 0) {
            sampleTotal = 1;
        }
        ui_displayContextStorage.context.fps = (float)(UI_FPS_SAMPLE_MILLISECONDS / sampleTotal);
    }

    UI_UpdateCvars();
    if (trap_Cvar_VariableValue("ui_dl_running") != 0.0f) {
        UI_DrawConnectScreen(qfalse);
        return;
    }

    if (Menu_Count() > 0) {
        Menu_PaintAll();
        UI_DoServerRefresh();
        UI_BuildServerStatus(qfalse);
        UI_BuildFindPlayerList(qfalse);
    }

    UI_SetColor(NULL);

    if (Menu_Count() > 0) {
        UI_DrawHandlePic((float)(ui_displayContextStorage.context.cursorx - UI_CURSOR_HALF_SIZE),
                         (float)(ui_displayContextStorage.context.cursory - UI_CURSOR_HALF_SIZE), (float)UI_CURSOR_SIZE,
                         (float)UI_CURSOR_SIZE, ui_displayContextStorage.context.cursor);
    }
}
