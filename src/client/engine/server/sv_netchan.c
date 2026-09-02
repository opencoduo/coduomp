#include "server.h"

#include "../ui/ui_module_loader.h"

#include <stdint.h>

enum {
    SV_PROFILE_DRAW_X = 32,
    SV_PROFILE_DRAW_MODE = 0,
    SV_PROFILE_DRAW_CHAR_WIDTH = 6,
    SV_PROFILE_DRAW_CHAR_HEIGHT = 8,
    SV_PROFILE_DRAW_STYLE = 0
};

/* Source: CoDUOMP.exe 0x00463880..0x0046389f.
 * Evidence: repaired executable-gap record
 * coduomp/mcode/CoDUOMP/FUN_00463880_004638a0.mcode.
 * Name: exact same-module Mac symbol SV_ProfDraw. The Windows optimizer
 * inlines this helper into SV_Netchan_PrintProfileStats, but retains this
 * separately addressable copy. */
void SV_ProfDraw(const char *text, int32_t y)
{
    if (coduo_cgameVm != NULL) {
        (void)VM_Call(
            coduo_cgameVm, CGVM_DRAW_SCALED,
            SV_PROFILE_DRAW_X, y, (intptr_t)text, SV_PROFILE_DRAW_MODE,
            SV_PROFILE_DRAW_CHAR_WIDTH, SV_PROFILE_DRAW_CHAR_HEIGHT,
            SV_PROFILE_DRAW_STYLE, 0, 0, 0, 0, 0);
    }
}
