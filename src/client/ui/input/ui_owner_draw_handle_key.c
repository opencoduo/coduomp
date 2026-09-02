#include "../module/ui_functions.h"

enum {
    UI_OWNERDRAW_HANDICAP = 200,
    UI_OWNERDRAW_GAMETYPE = 205,
    UI_OWNERDRAW_NETSOURCE = 220,
    UI_OWNERDRAW_SERVERFILTER = 222,
    UI_OWNERDRAW_NETGAMETYPE = 245,
    UI_OWNERDRAW_JOINGAMETYPE = 253,
    UI_GAMETYPE_RESET_MAPS = 1
};

// Source: uo_ui_mp_x86.dll 0x4000b5f0..0x4000b656
// Evidence: cgame_mp/mcode/uo_ui_mp_x86/FUN_4000b5f0_4000b656.mcode
// Exact same-module PPC symbol: UI_OwnerDrawHandleKey.
qboolean UI_OwnerDrawHandleKey(int32_t ownerDraw, int32_t flags, float *special, int32_t key)
{
    (void)flags;

    switch (ownerDraw) {
    case UI_OWNERDRAW_HANDICAP:
        return UI_Handicap_HandleKey(0, special, key);
    case UI_OWNERDRAW_GAMETYPE:
        return UI_GameType_HandleKey(UI_GAMETYPE_RESET_MAPS, special, key);
    case UI_OWNERDRAW_NETSOURCE:
        UI_NetSource_HandleKey(0, special, key);
        return qfalse;
    case UI_OWNERDRAW_SERVERFILTER:
        UI_NetFilter_HandleKey(0, special, key);
        return qfalse;
    case UI_OWNERDRAW_NETGAMETYPE:
        return UI_NetGameType_HandleKey(0, special, key);
    case UI_OWNERDRAW_JOINGAMETYPE:
        return UI_JoinGameType_HandleKey(0, special, key);
    default:
        return qfalse;
    }
}
