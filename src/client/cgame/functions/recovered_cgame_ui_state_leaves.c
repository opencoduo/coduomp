// Small cgame/UI state leaves recovered from their complete machine-code bodies.

#include "client/cgame/client_recovered.h"
#include "client/cgame/globals.h"

#include <math.h>

/* NOT_FROM_ORIGINAL_SOURCE: source-only factoring of the three identical menu
 * command leaves below. */
static void cgame_compat_open_active_player_menu(const char *menuName)
{
    if (cg.snap != 0 &&
        (cg.snap->ps.playerStateFlags & PSF_ACTIVE_PLAYER) != 0) {
        (void)cgame_syscall(CG_UI_IS_MENU_OPEN, (intptr_t)menuName);
    }
}

/* The corresponding Mac command-table leaves are CG_QuickMap_f and
 * CG_QuickMessage_f; the Windows menu strings distinguish the two entries. */
void CG_QuickMap_f(void) /* 0x30017750 */
{
    cgame_compat_open_active_player_menu("UIMENU_QUICKMAP");
}

void CG_OpenWMPurchase_f(void) /* 0x30017720 */
{
    cgame_compat_open_active_player_menu("UIMENU_WM_PURCHASE");
}

void CG_QuickMessage_f(void) /* 0x300176f0 */
{
    cgame_compat_open_active_player_menu("UIMENU_WM_QUICKMESSAGE");
}

// Source RVA: 0x30031a60
const char *FraggedByText(void)
{
    if (cg.killerName[0] == '\0')
        return "";
    return va("Fragged by %s", cg.killerName);
}

// Source RVA: 0x30023af0
void CG_UpdateFlameTime(void)
{
    /* FILD signed cg.time; FADD ST0,ST0; floor; _ftol2. The helper preserves
     * the target's signed-64 conversion and low-dword return for every input. */
    cg_flameTime = coduo_fp_to_i32_extended(
        floor((double)cg.time * 2.0));
}

void CG_AddLagometerFrameInfo(void) /* 0x30018a10 */
{
    lagometer.frameSamples[lagometer.frameCount & (LAG_SAMPLES - 1)] =
        coduo_wrap_sub_i32(cg.time, cg.latestSnapshotTime);
    lagometer.frameCount =
        coduo_int32_from_bits((uint32_t)lagometer.frameCount + 1u);
}

void CG_AdjustFrom640(float *x, float *y) /* 0x3001c4c0 */
{
    *x *= cgs.screenXScale;
    *y *= cgs.screenYScale;
}
