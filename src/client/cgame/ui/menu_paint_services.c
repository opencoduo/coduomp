#include "../client_recovered.h"
#include "../globals.h"

enum {
    CGAME_COMPAT_UNBOUNDED_NAME_LENGTH = 99999
};

/* COMPATIBILITY_PATCH (NOT_FROM_ORIGINAL_SOURCE): the retail `Health` menu is
 * declared unconditionally and its three children independently paint the
 * player-health backing, cross, and green bar. Vehicle view supplies its own
 * health composition at the same edge, so the passive HUD pass suppresses the
 * complete player-health menu while in a vehicle. */
static qboolean cgame_compat_hide_player_health_menu(
    const menuDef_t *menu, qboolean passiveHudPass)
{
    if (passiveHudPass == qfalse || menu == NULL ||
        (cg_predictedPlayerState.entityStateFlags & EF_IN_VEHICLE) == 0 ||
        menu->window.name == NULL) {
        return qfalse;
    }

    return Q_stricmpn(menu->window.name, "Health",
                      CGAME_COMPAT_UNBOUNDED_NAME_LENGTH) == 0
               ? qtrue : qfalse;
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

    if (passiveHudPass == qfalse) {
        *openMenuPreviousXScale = cgame_compat_begin_open_menu_canvas();
    } else if (menu->fullScreen == 0) {
        *passiveHudOffset = cgame_compat_begin_passive_hud_menu(menu);
        menu->window.rect.x += *passiveHudOffset;
    }
}

/* NOT_FROM_ORIGINAL_SOURCE: restore the menu window after its border paint;
 * child-item translation remains active until the complete menu is finished. */
void client_ui_compat_finish_menu_window_paint(
    menuDef_t *menu, qboolean passiveHudPass, float passiveHudOffset)
{
    if (passiveHudPass != qfalse && menu->fullScreen == 0) {
        menu->window.rect.x -= passiveHudOffset;
    }
}

/* NOT_FROM_ORIGINAL_SOURCE: close passive-HUD child translation before the
 * original debug outline, matching the existing compatibility scope. */
void client_ui_compat_finish_menu_items(
    menuDef_t *menu, qboolean passiveHudPass)
{
    if (passiveHudPass != qfalse && menu->fullScreen == 0) {
        cgame_compat_end_passive_hud_menu();
    }
}

/* NOT_FROM_ORIGINAL_SOURCE: close cgame's open-menu canvas after the original
 * debug outline. */
void client_ui_compat_end_menu_paint(
    menuDef_t *menu, qboolean passiveHudPass, float openMenuPreviousXScale)
{
    (void)menu;
    if (passiveHudPass == qfalse) {
        cgame_compat_end_open_menu_canvas(openMenuPreviousXScale);
    }
}
