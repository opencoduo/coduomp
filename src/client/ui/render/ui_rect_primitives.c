#include "../module/ui_functions.h"

// Source: uo_ui_mp_x86.dll 0x40007b30..0x40007b9c
// Evidence: cgame_mp/mcode/uo_ui_mp_x86/FUN_40007b30_40007b9c.mcode
// Same-module PPC role: UI_FillRect.
void UI_FillRect(float x, float y, float width, float height,
                 const vec4_t color)
{
    trap_R_SetColor(color);
    trap_R_DrawStretchPic(x * ui_displayContextStorage.context.xscale,
                          y * ui_displayContextStorage.context.yscale,
                          width * ui_displayContextStorage.context.xscale,
                          height * ui_displayContextStorage.context.yscale,
                          0.0f, 0.0f, 0.0f, 0.0f,
                          ui_displayContextStorage.context.whiteShader);
    trap_R_SetColor(NULL);
}

// Source: uo_ui_mp_x86.dll 0x40007ba0..0x40007c33
// Evidence: cgame_mp/mcode/uo_ui_mp_x86/FUN_40007ba0_40007c33.mcode
// Role name: UI_DrawSides.
void UI_DrawSides(float x, float y, float width, float height)
{
    x *= ui_displayContextStorage.context.xscale;
    y *= ui_displayContextStorage.context.yscale;
    width *= ui_displayContextStorage.context.xscale;
    height *= ui_displayContextStorage.context.yscale;
    trap_R_DrawStretchPic(x, y, 1.0f, height,
                          0.0f, 0.0f, 0.0f, 0.0f,
                          ui_displayContextStorage.context.whiteShader);
    trap_R_DrawStretchPic(x + width - 1.0f, y, 1.0f, height,
                          0.0f, 0.0f, 0.0f, 0.0f,
                          ui_displayContextStorage.context.whiteShader);
}

// Source: uo_ui_mp_x86.dll 0x40007c40..0x40007cd3
// Evidence: cgame_mp/mcode/uo_ui_mp_x86/FUN_40007c40_40007cd3.mcode
// Role name: UI_DrawTopBottom.
void UI_DrawTopBottom(float x, float y, float width, float height)
{
    x *= ui_displayContextStorage.context.xscale;
    y *= ui_displayContextStorage.context.yscale;
    width *= ui_displayContextStorage.context.xscale;
    height *= ui_displayContextStorage.context.yscale;
    trap_R_DrawStretchPic(x, y, width, 1.0f,
                          0.0f, 0.0f, 0.0f, 0.0f,
                          ui_displayContextStorage.context.whiteShader);
    trap_R_DrawStretchPic(x, y + height - 1.0f, width, 1.0f,
                          0.0f, 0.0f, 0.0f, 0.0f,
                          ui_displayContextStorage.context.whiteShader);
}

// Source: uo_ui_mp_x86.dll 0x40007ce0..0x40007d21
// Evidence: cgame_mp/mcode/uo_ui_mp_x86/FUN_40007ce0_40007d21.mcode
// Role name: UI_DrawRect.
void UI_DrawRect(float x, float y, float width, float height,
                 const vec4_t color)
{
    trap_R_SetColor(color);
    UI_DrawTopBottom(x, y, width, height);
    UI_DrawSides(x, y, width, height);
    trap_R_SetColor(NULL);
}

// Source: uo_ui_mp_x86.dll 0x40007d30..0x40007d41
// Evidence: cgame_mp/mcode/uo_ui_mp_x86/FUN_40007d30_40007d41.mcode
// Same-module PPC symbol: UI_SetColor.
void UI_SetColor(const vec4_t rgba)
{
    trap_R_SetColor(rgba);
}

// Source: uo_ui_mp_x86.dll 0x40007d50..0x40007d5a
// Evidence: cgame_mp/mcode/uo_ui_mp_x86/FUN_40007d50_40007d5a.mcode
// Role name: UI_UpdateScreen.
void UI_UpdateScreen(void)
{
    trap_UpdateScreen();
}

// Source: uo_ui_mp_x86.dll 0x40007d60..0x40007e27
// Evidence: cgame_mp/mcode/uo_ui_mp_x86/FUN_40007d60_40007e27.mcode
// Role name: UI_DrawTextBox.
void UI_DrawTextBox(int32_t x, int32_t y, int32_t width, int32_t lines)
{
    static const vec4_t black = {0.0f, 0.0f, 0.0f, 1.0f};
    static const vec4_t white = {1.0f, 1.0f, 1.0f, 1.0f};
    /* The DLL FILDs each integer straight into the add/multiply
     * (0x40007d60/0x40007d7d/0x40007d96/0x40007da9); an explicit (float) cast
     * would round it before the arithmetic, so keep the conversion implicit. */
    float drawX = x + 8.0f;
    float drawY = y + 8.0f;
    float drawWidth = (width + 1.0f) * 16.0f;
    float drawHeight = (lines + 1.0f) * 16.0f;

    UI_FillRect(drawX, drawY, drawWidth, drawHeight, black);
    UI_DrawRect(drawX, drawY, drawWidth, drawHeight, white);
}

// Source: uo_ui_mp_x86.dll 0x400087a0..0x4000883d
// Evidence: cgame_mp/mcode/uo_ui_mp_x86/FUN_400087a0_4000883d.mcode
// Role name: UI_DrawSides; the suffix distinguishes this variable-thickness
// source variant from the fixed-one-pixel sibling above.
void UI_DrawSidesWithSize(float x, float y, float width, float height,
                          float size)
{
    x *= ui_displayContextStorage.context.xscale;
    y *= ui_displayContextStorage.context.yscale;
    width *= ui_displayContextStorage.context.xscale;
    height *= ui_displayContextStorage.context.yscale;
    size *= ui_displayContextStorage.context.xscale;
    trap_R_DrawStretchPic(x, y, size, height,
                          0.0f, 0.0f, 0.0f, 0.0f,
                          ui_displayContextStorage.context.whiteShader);
    trap_R_DrawStretchPic(x + width - size, y, size, height,
                          0.0f, 0.0f, 0.0f, 0.0f,
                          ui_displayContextStorage.context.whiteShader);
}

// Source: uo_ui_mp_x86.dll 0x40008840..0x400088dd
// Evidence: cgame_mp/mcode/uo_ui_mp_x86/FUN_40008840_400088dd.mcode
// Role name: UI_DrawTopBottom; variable thickness is y-scaled.
void UI_DrawTopBottomWithSize(float x, float y, float width, float height,
                              float size)
{
    x *= ui_displayContextStorage.context.xscale;
    y *= ui_displayContextStorage.context.yscale;
    width *= ui_displayContextStorage.context.xscale;
    height *= ui_displayContextStorage.context.yscale;
    size *= ui_displayContextStorage.context.yscale;
    trap_R_DrawStretchPic(x, y, width, size,
                          0.0f, 0.0f, 0.0f, 0.0f,
                          ui_displayContextStorage.context.whiteShader);
    trap_R_DrawStretchPic(x, y + height - size, width, size,
                          0.0f, 0.0f, 0.0f, 0.0f,
                          ui_displayContextStorage.context.whiteShader);
}

// Source: uo_ui_mp_x86.dll 0x400088e0..0x4000892f
// Evidence: cgame_mp/mcode/uo_ui_mp_x86/FUN_400088e0_4000892f.mcode
// Role name: UI_DrawRect; variable-thickness source variant.
void UI_DrawRectWithSize(float x, float y, float width, float height,
                         float size, const vec4_t color)
{
    trap_R_SetColor(color);
    UI_DrawTopBottomWithSize(x, y, width, height, size);
    UI_DrawSidesWithSize(x, y, width, height, size);
    trap_R_SetColor(NULL);
}
