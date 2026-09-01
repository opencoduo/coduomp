#include "../module/ui_functions.h"

enum {
    UI_OWNERDRAW_HANDICAP = 200,
    UI_OWNERDRAW_GAMETYPE = 205,
    UI_OWNERDRAW_MAP_PREVIEW_NET_ALIAS = 206,
    UI_OWNERDRAW_NETSOURCE = 220,
    UI_OWNERDRAW_NET_MAP_PREVIEW = 221,
    UI_OWNERDRAW_SERVERFILTER = 222,
    UI_OWNERDRAW_MAP_PREVIEW = 244,
    UI_OWNERDRAW_NETGAMETYPE = 245,
    UI_OWNERDRAW_NET_MAP_CINEMATIC = 246,
    UI_OWNERDRAW_REFRESH_DATE = 247,
    UI_OWNERDRAW_GLINFO = 249,
    UI_OWNERDRAW_KEY_STATUS = 250,
    UI_OWNERDRAW_REFRESH_TOTALS = 251,
    UI_OWNERDRAW_JOINGAMETYPE = 253,
    UI_OWNERDRAW_PREVIEW_CINEMATIC = 254,
    UI_OWNERDRAW_NET_MAP_PREVIEW_ALIAS = 255
};

// Source: uo_ui_mp_x86.dll 0x4000abf0..0x4000afce
// Evidence: cgame_mp/mcode/uo_ui_mp_x86/FUN_4000abf0_4000afce.mcode
// Exact same-module PPC symbol: UI_OwnerDraw.
void UI_OwnerDraw(float x, float y, float width, float height,
                  float textX, float textY, int32_t ownerDraw,
                  int32_t ownerDrawFlags, int32_t alignment, float special,
                  int32_t font, float textScale, vec4_t color,
                  qhandle_t background, int32_t textStyle)
{
    rectDef_t rect = {x + textX, y + textY, width, height};

    (void)ownerDrawFlags;
    (void)alignment;
    (void)special;
    (void)background;

    switch (ownerDraw) {
    case UI_OWNERDRAW_HANDICAP:
        UI_DrawHandicap(&rect, font, textScale, color, textStyle);
        break;
    case UI_OWNERDRAW_GAMETYPE:
        UI_DrawGameType(&rect, font, textScale, color, textStyle);
        break;
    case UI_OWNERDRAW_MAP_PREVIEW_NET_ALIAS:
        UI_DrawMapPreview(&rect, qtrue);
        break;
    case UI_OWNERDRAW_NETSOURCE:
        UI_DrawNetSource(&rect, font, textScale, color, textStyle);
        break;
    case UI_OWNERDRAW_NET_MAP_PREVIEW:
        UI_DrawNetMapPreview(&rect);
        break;
    case UI_OWNERDRAW_SERVERFILTER:
        UI_DrawNetFilter(&rect, font, textScale, color, textStyle);
        break;
    case UI_OWNERDRAW_MAP_PREVIEW:
        UI_ValidateMapPreviewSelection(&rect, qfalse);
        break;
    case UI_OWNERDRAW_NETGAMETYPE:
        UI_DrawNetGameType(&rect, font, textScale, color, textStyle);
        break;
    case UI_OWNERDRAW_NET_MAP_CINEMATIC:
        UI_DrawNetMapCinematic(&rect);
        break;
    case UI_OWNERDRAW_REFRESH_DATE:
        UI_DrawServerRefreshDate(&rect, font, textScale, color, textStyle);
        break;
    case UI_OWNERDRAW_GLINFO:
        UI_DrawGLInfo(&rect, font, textScale, color, textStyle);
        break;
    case UI_OWNERDRAW_KEY_STATUS:
        UI_DrawKeyBindStatus(&rect, font, textScale, color, textStyle);
        break;
    case UI_OWNERDRAW_REFRESH_TOTALS:
        UI_DrawServerRefreshTotals(&rect, font, textScale, color, textStyle);
        break;
    case UI_OWNERDRAW_JOINGAMETYPE:
        UI_DrawJoinGameType(&rect, font, textScale, color, textStyle);
        break;
    case UI_OWNERDRAW_PREVIEW_CINEMATIC:
        UI_DrawPreviewCinematic(&rect);
        break;
    case UI_OWNERDRAW_NET_MAP_PREVIEW_ALIAS:
        UI_ValidateMapPreviewSelection(&rect, qtrue);
        break;
    default:
        break;
    }
}
