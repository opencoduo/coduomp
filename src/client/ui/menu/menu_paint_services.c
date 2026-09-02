#include "../module/ui_functions.h"

/* NOT_FROM_ORIGINAL_SOURCE: UI has no cgame passive-HUD suppression. */
qboolean client_ui_compat_should_skip_menu_paint(menuDef_t *menu, qboolean passiveHudPass)
{
    (void)menu;
    (void)passiveHudPass;
    return qfalse;
}

/* NOT_FROM_ORIGINAL_SOURCE: UI has no cgame widescreen projection around the
 * shared original menu-paint body. */
void client_ui_compat_begin_menu_paint(menuDef_t *menu, qboolean passiveHudPass, float *passiveHudOffset, float *openMenuPreviousXScale)
{
    (void)menu;
    (void)passiveHudPass;
    *passiveHudOffset = 0.0f;
    *openMenuPreviousXScale = 0.0f;
}

/* NOT_FROM_ORIGINAL_SOURCE: UI has no temporary menu-window projection. */
void client_ui_compat_finish_menu_window_paint(menuDef_t *menu, qboolean passiveHudPass, float passiveHudOffset)
{
    (void)menu;
    (void)passiveHudPass;
    (void)passiveHudOffset;
}

/* NOT_FROM_ORIGINAL_SOURCE: UI has no passive-HUD item scope to close. */
void client_ui_compat_finish_menu_items(menuDef_t *menu, qboolean passiveHudPass)
{
    (void)menu;
    (void)passiveHudPass;
}

/* NOT_FROM_ORIGINAL_SOURCE: UI has no cgame menu projection to close. */
void client_ui_compat_end_menu_paint(menuDef_t *menu, qboolean passiveHudPass, float openMenuPreviousXScale)
{
    (void)menu;
    (void)passiveHudPass;
    (void)openMenuPreviousXScale;
}
