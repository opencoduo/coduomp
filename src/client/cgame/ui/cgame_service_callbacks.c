// Source: uo_cgame_mp_x86.dll display/cinematic and HUD-selection callbacks.
// Evidence: corresponding cgame_mp/mcode/uo_cgame_mp_x86/FUN_*.mcode records.

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "client/cgame/client_recovered.h"
#include "client/cgame/globals.h"

long double CG_Cvar_Get(const char *name) /* 0x3002d530 */
{
    char buffer[128] = {0};
    cgame_syscall(CG_CVAR_VARIABLE_STRING_BUFFER, (intptr_t)name,
                  (intptr_t)buffer, sizeof(buffer));
    /* 0x3002d56f CALL atof; authoritative Win32 returns immediately with the
     * binary64 value still in ST0.  Keep that value in the display-context
     * carrier; consumers perform their own proven binary32/binary64 stores. */
    return (long double)atof(buffer);
}

int32_t CG_OwnerDrawWidth(int32_t ownerDraw, int32_t font,
                          float scale) /* 0x3002d590 */
{
    enum {
        OWNERDRAW_WIDTH_GAMETYPE = 39,
        OWNERDRAW_WIDTH_FRAGGED_BY = 50
    };

    if (ownerDraw == OWNERDRAW_WIDTH_GAMETYPE)
        return coduo_int32_from_bits((uint32_t)cgame_syscall(
            CG_R_TEXT_WIDTH, (intptr_t)cgs_gametype, font,
            CG_FloatBits(scale), 0));
    if (ownerDraw != OWNERDRAW_WIDTH_FRAGGED_BY) return 0;
    const char *text = cg_fraggedByName[0] != '\0'
        ? va("Fragged by %s", cg_fraggedByName) : g_str_empty;
    return coduo_int32_from_bits((uint32_t)cgame_syscall(
        CG_R_TEXT_WIDTH, (intptr_t)text, font, CG_FloatBits(scale), 0));
}

int32_t CG_PlayCinematic(const char *name, float x, float y,
                         float w, float h) /* 0x3002d610 */
{
    enum { CG_CINEMATIC_PLAY_FLAGS = 1 << 1 }; /* CIN_LOOP */

    /* Four FLD/_ftol2 pairs execute in h,w,y,x order before the trap call. */
    const int32_t hInt = coduo_fp_to_i32_extended(h);
    const int32_t wInt = coduo_fp_to_i32_extended(w);
    const int32_t yInt = coduo_fp_to_i32_extended(y);
    const int32_t xInt = coduo_fp_to_i32_extended(x);

    return (int32_t)cgame_syscall(CG_CIN_PLAY_CINEMATIC, (intptr_t)name,
                                  xInt, yInt, wInt, hInt,
                                  CG_CINEMATIC_PLAY_FLAGS);
}

void CG_StopCinematic(int32_t handle) /* 0x3002d650 */
{
    cgame_syscall(CG_CIN_STOP_CINEMATIC, handle);
}

void CG_DrawCinematic(int32_t handle, float x, float y,
                      float w, float h) /* 0x3002d670 */
{
    /* The set-extents path has the same ordered h,w,y,x conversion chain. */
    const int32_t hInt = coduo_fp_to_i32_extended(h);
    const int32_t wInt = coduo_fp_to_i32_extended(w);
    const int32_t yInt = coduo_fp_to_i32_extended(y);
    const int32_t xInt = coduo_fp_to_i32_extended(x);

    cgame_syscall(CG_CIN_SET_EXTENTS, handle,
                  xInt, yInt, wInt, hInt);
    cgame_syscall(CG_CIN_DRAW_CINEMATIC, handle);
}

void CG_RunCinematicFrame(int32_t handle) /* 0x3002d6c0 */
{
    cgame_syscall(CG_CIN_RUN_CINEMATIC, handle);
}

static void cgame_compat_publish_selected_player(void)
{
    /* NOT_FROM_ORIGINAL_SOURCE: source factoring of the identical publish tail
     * in the three original selection callbacks. */
    const int32_t selected = cg_currentSelectedPlayer_vmCvar.integer;

    if (selected >= 0 && selected < cg_hudEmitCount)
        trap_Cvar_Set("cg_selectedPlayer", va("%d", cg_hudEmitClientTable[selected]));
}

void CG_SelectedPlayerPublish(void) /* 0x3002ea90 */
{
    cgame_compat_publish_selected_player();
}

void CG_SelectedPlayerClamp(void) /* 0x3002ead0 */
{
    if (cg_currentSelectedPlayer_vmCvar.integer < 0 || cg_currentSelectedPlayer_vmCvar.integer >= cg_hudEmitCount) cg_currentSelectedPlayer_vmCvar.integer = 0;
}

void CG_SelectedPlayerNext(void) /* 0x3002eaf0 */
{
    int32_t selected = cg_currentSelectedPlayer_vmCvar.integer;
    const int32_t count = cg_hudEmitCount;

    if (selected < 0 || selected >= count)
        selected = 0;
    else
        selected = coduo_int32_from_bits((uint32_t)selected + 1u);

    cg_currentSelectedPlayer_vmCvar.integer = selected;
    if (selected >= 0 && selected < count)
        trap_Cvar_Set("cg_selectedPlayer", va("%d", cg_hudEmitClientTable[selected]));
}

void CG_SelectedPlayerPrev(void) /* 0x3002eb40 */
{
    int32_t selected = cg_currentSelectedPlayer_vmCvar.integer;
    const int32_t count = cg_hudEmitCount;

    if (selected > 0 && selected < count)
        selected = coduo_int32_from_bits((uint32_t)selected - 1u);
    else
        selected = count;

    cg_currentSelectedPlayer_vmCvar.integer = selected;
    if (selected >= 0 && selected < count)
        trap_Cvar_Set("cg_selectedPlayer", va("%d", cg_hudEmitClientTable[selected]));
}
