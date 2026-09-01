#include "ui_functions.h"

// Source: uo_ui_mp_x86.dll 0x400079c0..0x400079c1
// Evidence: cgame_mp/mcode/uo_ui_mp_x86/FUN_400079c0_400079c1.mcode
// One-byte empty callback; the live vmMain/UI_Refresh paths call 0x40010e30.
void UI_EmptyCallback_400079c0(qboolean overlay)
{
    (void)overlay;
}
