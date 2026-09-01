#include "../module/ui_functions.h"

// Source: uo_ui_mp_x86.dll 0x400079d0..0x400079f9
// Evidence: cgame_mp/mcode/uo_ui_mp_x86/FUN_400079d0_400079f9.mcode
// Role name: UI_AdjustFrom640; exact x/y screen-scale fields and four pointer
// arguments distinguish this early UI rendering helper.
void UI_AdjustFrom640(float *x, float *y, float *width, float *height)
{
    *x *= DC->xscale;
    *y *= DC->yscale;
    *width *= DC->xscale;
    *height *= DC->yscale;
}
