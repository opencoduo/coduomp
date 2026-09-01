// Sources: uo_cgame_mp_x86.dll 0x30057680..0x300576cd and
//          uo_ui_mp_x86.dll    0x400191e0..0x4001922d

#include "ui_runtime.h"

extern displayContextDef_t *DC;

/* Draw an item's asset one pixel inside its rectangle on every side. */
void Item_Asset_Paint(itemDef_t *item)
{
    if (item != NULL) {
        qhandle_t asset = item->asset; /* 0x30057684 */
        float height = (float)((long double)item->window.rect.h - 2.0f);
        displayContextDef_t *display = DC; /* 0x30057693 */
        float width = (float)((long double)item->window.rect.w - 2.0f);
        float y = (float)((long double)item->window.rect.y + 1.0f);
        float x = (float)((long double)item->window.rect.x + 1.0f);
        display->drawHandlePic(x, y, width, height, asset);
    }
}
