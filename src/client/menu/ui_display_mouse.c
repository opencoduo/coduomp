// Sources: uo_cgame_mp_x86.dll 0x3005b1a0..0x3005b24c and
//          uo_ui_mp_x86.dll    0x4001cec0..0x4001cf6c
//
// Display_MouseMove — route cursor motion through the UI menu stack.  The
// size-only mcode name CG_FadeColor is rejected: no color state is accessed.

#include "ui_runtime.h"

#include "compat/coduo_int32_bits.h"
#include "ui_menu_globals.h"

qboolean Display_MouseMove(menuDef_t *menu, int32_t cursorX, int32_t cursorY)
{
    if (menu != NULL) {
        /* ECX is the menu pointer at entry.  0x3005b229-0x3005b238: the two integer
         * stack arguments feed a bare FILD straight into the FADD (FILD [ESP+n];
         * FADD [ECX+m]; FSTP [ECX+m]) -- no intermediate float store -- so no
         * (float) cast here, which would round the int under -std=c11 (Class 4). */
        menu->window.rect.x += cursorX;
        menu->window.rect.y += cursorY;
        Menu_UpdatePosition(menu);
        return qtrue;
    }

    menuDef_t *focused = Menu_GetFocused();
    if (focused != NULL && (focused->window.flags & (int32_t)WINDOW_MODAL) != 0) {
        Menu_HandleMouseMove(focused, (float)cursorX, (float)cursorY);
        return qtrue;
    }

    uint32_t indexBits = (uint32_t)openMenuCount - 1u;
    while (coduo_int32_from_bits(indexBits) >= 0) {
        if (Menu_HandleMouseMove(menuStack[indexBits], (float)cursorX, (float)cursorY)) {
            return qtrue;
        }
        indexBits -= 1u;
    }

    return qtrue;
}
