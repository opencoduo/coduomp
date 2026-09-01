#include "../module/ui_functions.h"

// Source: uo_ui_mp_x86.dll 0x40009e60..0x40009ebf
// Evidence: cgame_mp/mcode/uo_ui_mp_x86/FUN_40009e60_40009ebf.mcode
// Role name: UI_ValidateMapPreviewSelection.
void UI_ValidateMapPreviewSelection(const rectDef_t *rect, qboolean netMap)
{
    int32_t mapIndex = netMap ? ui_currentNetMap : ui_currentMap;

    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if (mapIndex < 0 || mapIndex >= ui_mapCount) {
        if (netMap) {
            ui_currentNetMap = 0;
            trap_Cvar_Set("ui_currentNetMap", "0");
        } else {
            ui_currentMap = 0;
            trap_Cvar_Set("ui_currentMap", "0");
        }
    }
    UI_DrawMapPreview(rect, netMap);
}
