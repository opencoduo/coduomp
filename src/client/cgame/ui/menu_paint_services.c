#include "../client_recovered.h"
#include "../globals.h"

enum {
    CGAME_COMPAT_UNBOUNDED_NAME_LENGTH = 99999
};

/* NOT_FROM_ORIGINAL_SOURCE: the shared item painter calls this target-specific
 * boundary. Stock cgame presentation applies no item translation. */
float client_ui_compat_begin_item_paint(itemDef_t *item)
{
    (void)item;
    return 0.0f;
}

/* NOT_FROM_ORIGINAL_SOURCE: stock cgame presentation has no item translation
 * to close. */
void client_ui_compat_end_item_paint(itemDef_t *item, float offset)
{
    (void)item;
    (void)offset;
}

/* COMPATIBILITY_PATCH (NOT_FROM_ORIGINAL_SOURCE): the retail `Health` menu is
 * declared unconditionally and its three children independently paint the
 * player-health backing, cross, and green bar. Vehicle view supplies its own
 * health composition at the same edge, so the passive HUD pass suppresses the
 * complete player-health menu while in a vehicle. */
static qboolean cgame_compat_hide_player_health_menu(
    const menuDef_t *menu, qboolean passiveHudPass)
{
    (void)menu;
    (void)passiveHudPass;
    return qfalse;
}

/* NOT_FROM_ORIGINAL_SOURCE: apply cgame's optional passive-HUD suppression
 * before the original owner-draw visibility callback. */
qboolean client_ui_compat_should_skip_menu_paint(
    menuDef_t *menu, qboolean passiveHudPass)
{
    return cgame_compat_hide_player_health_menu(menu, passiveHudPass);
}

/* NOT_FROM_ORIGINAL_SOURCE: begin cgame's optional widescreen projection
 * around one invocation of the shared original menu-paint body. */
void client_ui_compat_begin_menu_paint(
    menuDef_t *menu, qboolean passiveHudPass, float *passiveHudOffset,
    float *openMenuPreviousXScale)
{
    (void)menu;
    (void)passiveHudPass;
    *passiveHudOffset = 0.0f;
    *openMenuPreviousXScale = 0.0f;

}

/* NOT_FROM_ORIGINAL_SOURCE: restore the menu window after its border paint;
 * child-item translation remains active until the complete menu is finished. */
void client_ui_compat_finish_menu_window_paint(
    menuDef_t *menu, qboolean passiveHudPass, float passiveHudOffset)
{
    (void)menu;
    (void)passiveHudPass;
    (void)passiveHudOffset;
}

/* NOT_FROM_ORIGINAL_SOURCE: close passive-HUD child translation before the
 * original debug outline, matching the existing compatibility scope. */
void client_ui_compat_finish_menu_items(
    menuDef_t *menu, qboolean passiveHudPass)
{
    (void)menu;
    (void)passiveHudPass;
}

/* NOT_FROM_ORIGINAL_SOURCE: close cgame's open-menu canvas after the original
 * debug outline. */
void client_ui_compat_end_menu_paint(
    menuDef_t *menu, qboolean passiveHudPass, float openMenuPreviousXScale)
{
    (void)menu;
    (void)passiveHudPass;
    (void)openMenuPreviousXScale;
}
