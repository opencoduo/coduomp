#include "../module/ui_functions.h"

// Source: uo_ui_mp_x86.dll 0x400086e0..0x4000879b
// Evidence: cgame_mp/mcode/uo_ui_mp_x86/FUN_400086e0_4000879b.mcode
// Role name: UI_AssetCache; exact initialized shader paths and display-context
// handle offsets establish the shared UI asset cache.
void UI_AssetCache(void)
{
    ui_displayContextStorage.context.gradientBar = trap_R_RegisterShaderNoMip(
        "ui/assets/gradientbar2.tga", R_IMAGE_TRACK_UI);
    ui_displayContextStorage.context.scrollBar = trap_R_RegisterShaderNoMip(
        "ui/assets/scrollbar.tga", R_IMAGE_TRACK_UI);
    ui_displayContextStorage.context.scrollBarArrowDown = trap_R_RegisterShaderNoMip(
        "ui/assets/scrollbar_arrow_dwn_a.tga", R_IMAGE_TRACK_UI);
    ui_displayContextStorage.context.scrollBarArrowUp = trap_R_RegisterShaderNoMip(
        "ui/assets/scrollbar_arrow_up_a.tga", R_IMAGE_TRACK_UI);
    ui_displayContextStorage.context.scrollBarArrowLeft = trap_R_RegisterShaderNoMip(
        "ui/assets/scrollbar_arrow_left.tga", R_IMAGE_TRACK_UI);
    ui_displayContextStorage.context.scrollBarArrowRight = trap_R_RegisterShaderNoMip(
        "ui/assets/scrollbar_arrow_right.tga", R_IMAGE_TRACK_UI);
    ui_displayContextStorage.context.scrollBarThumb = trap_R_RegisterShaderNoMip(
        "ui/assets/scrollbar_thumb.tga", R_IMAGE_TRACK_UI);
    ui_displayContextStorage.context.sliderBar = trap_R_RegisterShaderNoMip(
        "ui/assets/slider2.tga", R_IMAGE_TRACK_UI);
    ui_displayContextStorage.context.sliderThumb = trap_R_RegisterShaderNoMip(
        "ui/assets/sliderbutt_1.tga", R_IMAGE_TRACK_UI);
}
