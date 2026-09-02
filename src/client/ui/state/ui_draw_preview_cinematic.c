#include "../module/ui_functions.h"

enum {
    UI_CINEMATIC_DISABLED = -2,
    UI_PREVIEW_CINEMATIC_FLAGS = 10
};

// Source: uo_ui_mp_x86.dll 0x40009cf0..0x40009d8b
// Evidence: cgame_mp/mcode/uo_ui_mp_x86/FUN_40009cf0_40009d8b.mcode
// Exact same-module PPC symbol: UI_DrawPreviewCinematic.
void UI_DrawPreviewCinematic(const rectDef_t *rect)
{
    if (ui_previewMovie <= UI_CINEMATIC_DISABLED) {
        return;
    }

    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if (ui_movieIndex < 0 || ui_movieIndex >= ui_movieCount) {
        if (ui_previewMovie >= 0) {
            trap_CIN_StopCinematic(ui_previewMovie);
        }
        ui_previewMovie = UI_CINEMATIC_DISABLED;
        return;
    }
    ui_previewMovie = trap_CIN_PlayCinematic(va("%s.roq", ui_movieNames[ui_movieIndex]), 0, 0, 0, 0, UI_PREVIEW_CINEMATIC_FLAGS);
    if (ui_previewMovie < 0) {
        ui_previewMovie = UI_CINEMATIC_DISABLED;
        return;
    }

    trap_CIN_RunCinematic(ui_previewMovie);
    trap_CIN_SetExtents(ui_previewMovie, (int32_t)rect->x, (int32_t)rect->y, (int32_t)rect->w, (int32_t)rect->h);
    trap_CIN_DrawCinematic(ui_previewMovie);
}
