// Source: uo_cgame_mp_x86.dll 0x3005a600..0x3005a62a.
// The UI-DLL body at 0x4001c220 consumes the parsed integer but does not set
// WINDOW_VISIBLE, so this genuinely module-specific handler remains local.

#include "client/cgame/client_recovered.h"

qboolean MenuParse_visible(menuDef_t *menu, int handle)
{
    int visible;

    if (!PC_Int_Parse(handle, &visible)) {
        return qfalse;
    }
    if (visible) {
        menu->window.flags |= WINDOW_VISIBLE;
    }
    return qtrue;
}
