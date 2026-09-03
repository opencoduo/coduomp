#include "../client_recovered.h"
#include "../globals.h"
#include "client/menu/ui_menu_globals.h"

/* This translation unit is an isolated improved presentation interface. */

/* NOT_FROM_ORIGINAL_SOURCE: classify the loaded ui_mp/hud.menu menu set as
 * stock-authored or mod-authored.
 *
 * The passive-HUD anchor table in cgame_compat_presentation.c keys on the
 * stock menu names (Compass, Health, weaponinfo, ...), but a HUD-restyling
 * mod replaces ui_mp/hud.menu wholesale and REUSES those names for completely
 * re-authored layouts (it must - the engine paints HUD menus by name).
 * Applying the stock anchors to such a menu tears the mod's composition
 * apart: ROTU authors "Health" at the lower LEFT and makes "Compass" a
 * fullscreen container holding screen-centered announcement text, so the
 * stock RIGHT/LEFT anchors throw both to the wrong place.
 *
 * The verdict below is therefore a fingerprint over authored geometry only:
 * every stock hud.menu menu must be present, with its declared window rect,
 * item count, and per-item authored rects exactly as shipped in the retail
 * UO assets (pakuo03.pk3, with the COMPASS_* macros of ui_mp/menudef.h
 * resolved). Any mismatch or absence means a mod owns HUD presentation and
 * the widescreen anchor model must not reinterpret its layout; the
 * presentation fallback for that case is the stock full-width stretch.
 *
 * The comparison uses exact float equality: every stock authored value is
 * dyadic (representable exactly in binary32), and identical source text
 * parses to identical values, so a stock file can never miscompare. */

typedef struct {
    float x;
    float y;
    float w;
    float h;
} cgameCompatAuthoredRect_t;

typedef struct {
    const char *menuName;
    cgameCompatAuthoredRect_t windowRect;
    int32_t itemCount;
    const cgameCompatAuthoredRect_t *itemRects;
} cgameCompatStockHudMenu_t;

/* Item rects hold the parsed client-space values: "rect" writes
 * window.rectClient verbatim and no stock hud.menu item uses "origin". */
static const cgameCompatAuthoredRect_t cgameCompatStockCursorhintsItems[] = {
    { 0.0f, 0.0f, 40.0f, 40.0f },
};

static const cgameCompatAuthoredRect_t cgameCompatStockStanceItems[] = {
    { 0.0f, 0.0f, 40.0f, 40.0f },
};

static const cgameCompatAuthoredRect_t cgameCompatStockTankstatusItems[] = {
    { 0.0f, 0.0f, 40.0f, 40.0f },
    { 20.0f, 20.0f, 40.0f, 40.0f },
    { 0.0f, 0.0f, 40.0f, 40.0f },
    { 0.0f, 0.0f, 40.0f, 40.0f },
};

static const cgameCompatAuthoredRect_t cgameCompatStockJeepstatusItems[] = {
    { 0.0f, 0.0f, 40.0f, 40.0f },
    { 0.0f, 0.0f, 40.0f, 40.0f },
    { 0.0f, 0.0f, 40.0f, 40.0f },
    { 0.0f, 0.0f, 40.0f, 40.0f },
};

static const cgameCompatAuthoredRect_t cgameCompatStockWeaponinfoItems[] = {
    { 242.5f, 10.625f, 320.0f, 20.0f },
    { 557.5f, 1.25f, 80.0f, 40.0f },
    { 557.5f, -19.75f, 80.0f, 40.0f },
    { 537.5f, 10.0f, 20.0f, 20.0f },
    { 537.5f, -10.0f, 20.0f, 20.0f },
    { 242.5f, 25.625f, 320.0f, 30.0f },
    { 570.0f, 24.25f, 55.0f, 40.0f },
    { 570.0f, 4.25f, 55.0f, 40.0f },
};

static const cgameCompatAuthoredRect_t cgameCompatStockHealthItems[] = {
    { 13.0f, 0.0f, 128.0f, 32.0f },
    { 14.0f, 24.0f, 126.0f, 6.0f },
    { 0.0f, 21.0f, 12.0f, 12.0f },
};

/* COMPASS_X -25, COMPASS_Y 345, COMPASS_SIZE 160, COMPASS_NEEDLE_XOFF 60,
 * COMPASS_NEEDLE_YOFF 50, COMPASS_NEEDLE_WIDTH/HEIGHT 40 (ui_mp/menudef.h). */
static const cgameCompatAuthoredRect_t cgameCompatStockCompassItems[] = {
    { 0.0f, 0.0f, 160.0f, 160.0f },
    { 0.0f, 0.0f, 160.0f, 160.0f },
    { 0.0f, 0.0f, 160.0f, 160.0f },
    { 60.0f, 50.0f, 40.0f, 40.0f },
    { 0.0f, 0.0f, 160.0f, 160.0f },
    { 0.0f, 0.0f, 160.0f, 160.0f },
    { 0.0f, 0.0f, 160.0f, 160.0f },
};

static const cgameCompatStockHudMenu_t cgameCompatStockHudMenus[] = {
    { "Cursorhints", { 300.0f, 325.0f, 40.0f, 40.0f }, 1,
      cgameCompatStockCursorhintsItems },
    { "stance", { 100.0f, 434.375f, 40.0f, 40.0f }, 1,
      cgameCompatStockStanceItems },
    { "tankstatus", { 100.0f, 434.375f, 40.0f, 40.0f }, 4,
      cgameCompatStockTankstatusItems },
    { "jeepstatus", { 100.0f, 434.375f, 40.0f, 40.0f }, 4,
      cgameCompatStockJeepstatusItems },
    { "weaponinfo", { 0.0f, 420.375f, 640.0f, 40.0f }, 8,
      cgameCompatStockWeaponinfoItems },
    { "Health", { 488.0f, 436.0f, 0.0f, 0.0f }, 3,
      cgameCompatStockHealthItems },
    { "Compass", { -25.0f, 345.0f, 160.0f, 160.0f }, 7,
      cgameCompatStockCompassItems },
};

enum {
    CGAME_COMPAT_STOCK_HUD_MENU_COUNT =
        (int32_t)(sizeof(cgameCompatStockHudMenus) /
                  sizeof(cgameCompatStockHudMenus[0])),
    CGAME_COMPAT_AUTHORSHIP_NAME_LIMIT = 99999
};

/* NOT_FROM_ORIGINAL_SOURCE_STORAGE_FILE: qtrue once the loaded HUD menu set
 * deviates from the stock fingerprint. Defaults to stock (legacy behavior)
 * until the HUD menu list has actually been loaded and classified. */
static qboolean cgameCompatHudMenusModAuthored;

/* NOT_FROM_ORIGINAL_SOURCE: dllEntry-lifecycle reset, called from
 * cgame_compat_reset_presentation_state. */
void cgame_compat_reset_hud_menu_authorship(void)
{
    cgameCompatHudMenusModAuthored = qfalse;
}

qboolean cgame_compat_hud_menus_are_mod_authored(void)
{
    return cgameCompatHudMenusModAuthored;
}

static qboolean cgame_compat_rects_equal(const cgameCompatAuthoredRect_t *expected,
                                         const rectDef_t *actual)
{
    return expected->x == actual->x && expected->y == actual->y &&
                   expected->w == actual->w && expected->h == actual->h
               ? qtrue
               : qfalse;
}

static qboolean cgame_compat_menu_matches_stock(
    const menuDef_t *menu, const cgameCompatStockHudMenu_t *stock)
{
    if (cgame_compat_rects_equal(&stock->windowRect, &menu->window.rect) ==
            qfalse ||
        menu->itemCount != stock->itemCount) {
        return qfalse;
    }

    for (int32_t index = 0; index < stock->itemCount; ++index) {
        const itemDef_t *item = menu->items[index];

        if (item == NULL ||
            cgame_compat_rects_equal(&stock->itemRects[index],
                                     &item->window.rectClient) == qfalse) {
            return qfalse;
        }
    }

    return qtrue;
}

/* NOT_FROM_ORIGINAL_SOURCE: classify the registered menu set after every
 * CG_LoadMenus pass. Stock verdict requires every stock menu name to be
 * present and every menu bearing a stock name to match its fingerprint;
 * duplicate declarations (a mod trick to shadow a stock menu) must each
 * match. The verdict is a function of authored geometry only and is
 * published to the presentation layer immediately so the first drawn frame
 * already uses the resolved presentation. */
void cgame_compat_evaluate_hud_menu_authorship(void)
{
    qboolean stockNameSeen[CGAME_COMPAT_STOCK_HUD_MENU_COUNT] = { qfalse };
    qboolean modAuthored = qfalse;

    for (int32_t menuIndex = 0; menuIndex < menuCount && menuIndex < MAX_MENUS;
         ++menuIndex) {
        const menuDef_t *menu = &Menus[menuIndex];

        if (menu->window.name == NULL)
            continue;

        for (int32_t stockIndex = 0;
             stockIndex < CGAME_COMPAT_STOCK_HUD_MENU_COUNT; ++stockIndex) {
            const cgameCompatStockHudMenu_t *stock =
                &cgameCompatStockHudMenus[stockIndex];

            if (Q_stricmpn(menu->window.name, stock->menuName,
                           CGAME_COMPAT_AUTHORSHIP_NAME_LIMIT) != 0) {
                continue;
            }

            stockNameSeen[stockIndex] = qtrue;
            if (cgame_compat_menu_matches_stock(menu, stock) == qfalse)
                modAuthored = qtrue;
            break;
        }
    }

    for (int32_t stockIndex = 0;
         stockIndex < CGAME_COMPAT_STOCK_HUD_MENU_COUNT; ++stockIndex) {
        if (stockNameSeen[stockIndex] == qfalse)
            modAuthored = qtrue;
    }

    cgameCompatHudMenusModAuthored = modAuthored;
    cgame_compat_publish_hud_presentation();
}
