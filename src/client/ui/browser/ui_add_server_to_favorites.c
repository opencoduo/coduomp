#include "../module/ui_functions.h"
#include "../module/ui_globals.h"

enum {
    UI_LAN_ADD_SERVER_DUPLICATE = 0,
    UI_LAN_ADD_SERVER_LIST_FULL = -1,
    UI_LAN_ADD_SERVER_BAD_ADDRESS = -2
};

// Source: uo_ui_mp_x86.dll 0x4000c010..0x4000c166
// Evidence: cgame_mp/mcode/uo_ui_mp_x86/FUN_4000c010_4000c166.mcode
// Exact same-module PPC symbol: UI_AddServerToFavoritesList.
void UI_AddServerToFavoritesList(const char *name, const char *address)
{
    int32_t result;

    if (name[0] == '\0') {
        Com_Printf("%s\n", UI_SafeTranslateString("EXE_FAVORITENAMEEMPTY"));
        trap_Cvar_Set("ui_favorite_message", "@EXE_FAVORITENAMEEMPTY");
        return;
    }

    if (address[0] == '\0') {
        Com_Printf("%s\n",
                   UI_SafeTranslateString("EXE_FAVORITEADDRESSEMPTY"));
        trap_Cvar_Set("ui_favorite_message", "@EXE_FAVORITEADDRESSEMPTY");
        return;
    }

    result = trap_LAN_AddServer(LAN_SERVER_SOURCE_FAVORITES, name, address);
    if (result == UI_LAN_ADD_SERVER_DUPLICATE) {
        Com_Printf("%s\n", UI_SafeTranslateString("EXE_FAVORITEINLIST"));
        trap_Cvar_Set("ui_favorite_message", "@EXE_FAVORITEINLIST");
        return;
    }

    if (result == UI_LAN_ADD_SERVER_LIST_FULL) {
        Com_Printf("%s\n", UI_SafeTranslateString("EXE_FAVORITELISTFULL"));
        trap_Cvar_Set("ui_favorite_message", "@EXE_FAVORITELISTFULL");
        return;
    }

    if (result == UI_LAN_ADD_SERVER_BAD_ADDRESS) {
        Com_Printf("%s\n", UI_SafeTranslateString("EXE_BADSERVERADDRESS"));
        trap_Cvar_Set("ui_favorite_message", "@EXE_BADSERVERADDRESS");
        return;
    }

    /* NOT_FROM_ORIGINAL_SOURCE: the translated message is display data, not a
     * variadic format contract. */
    Com_Printf("%s\n", UI_SafeTranslateString("EXE_FAVORITEADDED"));
    trap_Cvar_Set("ui_favorite_message", "@EXE_FAVORITEADDED");
}
