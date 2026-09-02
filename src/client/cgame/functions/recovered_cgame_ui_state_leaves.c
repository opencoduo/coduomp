// Small cgame/UI state leaves recovered from their complete machine-code bodies.

#include "client/cgame/client_recovered.h"
#include "client/cgame/globals.h"

#include <math.h>

/* NOT_FROM_ORIGINAL_SOURCE: source-only factoring of the three identical menu
 * command leaves below. */
static void cgame_compat_open_active_player_menu(const char *menuName)
{
    if (cg_snap != 0 && (cg_snap->ps.playerStateFlags & PSF_ACTIVE_PLAYER) != 0) {
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
    if (cg_fraggedByName[0] == '\0')
        return "";
    return va("Fragged by %s", cg_fraggedByName);
}

// Source RVA: 0x30023af0
void CG_UpdateFlameTime(void)
{
    /* FILD signed cg_time; FADD ST0,ST0; floor; _ftol2. The helper preserves
     * the target's signed-64 conversion and low-dword return for every input. */
    cg_flameTime = (uint32_t)coduo_fp_to_i32_extended(floor((double)coduo_int32_from_bits(cg_time) * 2.0));
}

void CG_AddLagometerFrameInfo(void) /* 0x30018a10 */
{
    cg_lagometerFrameSamples[cg_lagometerFrameCount & (LAG_SAMPLES - 1)] =
        coduo_int32_from_bits((uint32_t)cg_time - (uint32_t)cg_latestSnapshotServerTime);
    cg_lagometerFrameCount = coduo_int32_from_bits((uint32_t)cg_lagometerFrameCount + 1u);
}

void CG_AdjustFrom640(float *x, float *y) /* 0x3001c4c0 */
{
    *x *= cgs_screenXScale;
    *y *= cgs_screenYScale;
}
