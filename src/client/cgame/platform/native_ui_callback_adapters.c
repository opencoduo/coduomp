#include "../client_recovered.h"

/*
 * NOT_FROM_ORIGINAL_SOURCE: native-ABI adapters between ui_shared's semantic
 * float callback signatures and the recovered i386 trap wrappers, whose float
 * arguments are represented as their original opaque 32-bit syscall words.
 * On i386 both forms occupy the same stack dword. Register-based 64-bit ABIs
 * require this explicit bit conversion at the callback boundary.
 */
void cgame_compat_ui_draw_stretch_pic(
    float x, float y, float w, float h,
    float s1, float t1, float s2, float t2, qhandle_t shaderHandle)
{
    (void)trap_R_DrawStretchPic(
        CG_FloatBits(x), CG_FloatBits(y), CG_FloatBits(w), CG_FloatBits(h),
        CG_FloatBits(s1), CG_FloatBits(t1), CG_FloatBits(s2), CG_FloatBits(t2),
        shaderHandle);
}

void cgame_compat_ui_draw_text(float x, float y, int32_t font,
                               float scale, const vec4_t color,
                               const char *text, float fixedAdvance,
                               int32_t limit, int32_t textStyle)
{
    (void)trap_R_Text_Paint(CG_FloatBits(x), CG_FloatBits(y), font,
                            CG_FloatBits(scale), (intptr_t)color,
                            (intptr_t)text, CG_FloatBits(fixedAdvance),
                            limit, textStyle);
}

int32_t cgame_compat_ui_text_width(const char *text, int32_t font,
                                   float scale, int32_t limit)
{
    return trap_R_Text_Width(text, font, CG_FloatBits(scale), limit);
}

int32_t cgame_compat_ui_text_height(int32_t font, float scale)
{
    return trap_R_Text_Height(font, CG_FloatBits(scale));
}

void cgame_compat_ui_draw_text_with_cursor(
    float x, float y, int32_t font, float scale, const vec4_t color,
    const char *text, int32_t cursorPos, int8_t cursorChar,
    int32_t limit, int32_t textStyle)
{
    (void)trap_R_Text_PaintWithCursor(
        CG_FloatBits(x), CG_FloatBits(y), font, CG_FloatBits(scale),
        (intptr_t)color, (intptr_t)text, cursorPos, cursorChar,
        limit, textStyle);
}
