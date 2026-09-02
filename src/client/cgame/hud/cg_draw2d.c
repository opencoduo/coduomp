// Source: uo_cgame_mp_x86.dll 0x3001bfe0..0x3001c0f8
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3001bfe0_3001c0f8.mcode

#include "../client_recovered.h"
#include "../globals.h"

#include <stdint.h>

/*
 * CG_Draw2D (0x3001bfe0) — the cgame per-frame screen/HUD draw
 * dispatcher. Called once per rendered frame from the frame entry at 0x3001c440
 * (which first stores the view origin/angles to 0x30487a90.. and runs the
 * world/scene passes, then CALLs this at 0x3001c478). It gates on a few
 * render-state flags, then dispatches the on-screen 2D/HUD drawing based on the
 * local player's movement state cg_snap->ps.pmType (pmType_t).
 *
 * Name adjudication: the .mcode header's mechanical name "Fire_Lead" is REJECTED.
 * That guess is pure size-matching ("win size 0x118, matched size 0x118" from
 * game_mp.dll), which the recovery contract forbids as an identity signal. This
 * function fires no weapon and computes no bullet lead: it reads no weapon
 * definition, does no vector/x87 endpoint math, and instead reads cg_snap and a
 * set of screen-state gate flags and issues a fixed sequence of no-argument HUD
 * draw calls plus one cgame draw trap. It is a HUD/scene draw dispatcher keyed on
 * pmType. The Mac cgame symbol CG_Draw2D has the same set of all 14 recovered
 * direct game-function callees, including the HUD, scoreboard, vote, lagometer,
 * menu, and screen-fade draws. That cross-architecture call fingerprint resolves
 * the exact source name; the old exporter owner labels near 0x3005d8xx were only
 * mechanical first-toucher guesses.
 *
 * Control flow, proven instruction-by-instruction against the .mcode:
 *
 * Entry guards (0x3001bfe1..0x3001c00f):
 *   - if g_cgScreenSuppressFlag != 0  -> return immediately (bare RET,
 *     0x3001c0f6): no drawing and NOT even the fade-overlay tail. (JNZ 0x3001c0f6)
 *   - if cg_lockedViewFace != 0                -> same bare return. (JNZ 0x3001c0f6)
 *   - if g_cgScreenReadyState != 0    -> jump to the fade-overlay tail
 *     (0x3001c0f1: CG_DrawScreenFadeOverlay then return). (JNZ 0x3001c0f1)
 *   - if cg_draw2D_vmCvar.integer == 0    -> same fade-overlay tail; the flag
 *     must be SET to proceed to drawing. (JZ 0x3001c0f1)
 *
 * Always-run pre-pass once past the guards (0x3001c015..0x3001c01f):
 *   CG_UpdateScreenFade(); CG_DrawFlashDamage(); CG_DrawScreenBlend();
 *
 * pmType dispatch (EAX = cg_snap->ps.pmType, MOV EAX,[cg_snap]; MOV EAX,[EAX+0x10]):
 *   - PM_TYPE_INTERMISSION (5): CG_DrawIntermission(); POP ECX; RET — this branch
 *     returns directly, skipping the fade-overlay tail. (0x3001c02f..0x3001c037)
 *   - PM_TYPE_SPECTATOR (4): CG_DrawSpectatorMessage(); CG_DrawSpectatorHud();
 *     CG_DrawTeamInfo(); then the scoreboard gate: if
 *     cg_drawStatus_vmCvar.integer == 0 jump straight to the scoreboard tail
 *     at 0x3001c091 (skipping the overlay/hud-elem block), else jump to
 *     CG_DrawHudElems() at 0x3001c08c (skipping Menu_PaintAll + lagometer).
 *     (0x3001c038..0x3001c055)
 *   - alive/playing states (psPmType < 6, i.e. 0..3 after 4 and 5 are handled;
 *     CMP 6 / JGE): CG_DrawCrosshair(); CG_DrawSpectatorHud(); and if the
 *     scoreboard gate is set, CG_DrawWeaponSelect(). Falls through to the common tail.
 *     (0x3001c057..0x3001c072)
 *   - dead states (psPmType >= 6): JGE skips straight to the common tail.
 *
 * Common tail from 0x3001c074:
 *   CG_DrawTeamInfo(); then if cg_drawStatus_vmCvar.integer != 0:
 *     Menu_PaintAll(); CG_DrawLagometer(); (fall into) CG_DrawHudElems();
 *   else jump past those to the scoreboard block. (The SPECTATOR branch's
 *   0x3001c08c entry lands on CG_DrawHudElems() only; its 0x3001c091 entry lands
 *   just after, on the scoreboard block.)
 *
 * Scoreboard block from 0x3001c091:
 *   if (!CG_DrawScoreboard()) { CG_ResetScreenFadeA(); CG_ResetScreenFadeB(); }
 *   CG_DrawCrosshair(); CG_DrawMatchTimeout(); CG_DrawObjectiveHud();
 *   CG_DrawTimerHud(); CG_DrawInfoScreens(); CG_DrawWeaponStance();
 *   if (!CG_DrawScoreboard()) {              // called a SECOND time, 0x3001c0c2
 *       CG_DrawSpawnOverlay();
 *       CG_DrawSlidingFadeElement(); CG_DrawScoreboardFadeElement();
 *       CG_DrawDebugFadeElement();  CG_DrawFixedFadeElement();
 *       cgame_syscall(CG_NOTIFY_PLAYER_SPAWNED, 100);      // trap(0x1e, 100), 0x3001c0e4
 *   }
 *   // when the 2nd CG_DrawScoreboard() returns nonzero, JNZ jumps to the tail
 *   // (0x3001c0f1), skipping the just-spawned overlay + fade elements + trap.
 *
 * Fade-overlay tail (0x3001c0f1): CG_DrawScreenFadeOverlay(); then POP ECX; RET.
 * Reached by every non-fully-suppressed path (the 3rd/4th guards and the end of
 * the full draw), but NOT by the two bare-return suppress guards nor the
 * intermission branch.
 *
 * ABI: no incoming source arguments. The prologue PUSH ECX / epilogue POP ECX is a
 * 4-byte scratch reservation (the compiler's stack alignment for the sole outgoing
 * trap-arg push pair), not a source-level value; the function returns void.
 * Every callee here is invoked with a bare CALL (no argument setup, no post-call
 * ADD ESP) and its return is used only where noted (the two CG_DrawScoreboard
 * calls), so each is modeled as void f(void) except CG_DrawScoreboard (qboolean).
 * The callee identities are documented in client_recovered.h.
 */
void CG_Draw2D(void)
{
    int32_t pmType;

    /* Entry guards. */
    if (g_cgScreenSuppressFlag != 0) {
        return; /* bare RET at 0x3001c0f6 — no fade-overlay tail */
    }
    if (cg_lockedViewFace != 0) {
        return; /* bare RET at 0x3001c0f6 */
    }
    if (g_cgScreenReadyState != 0 ||
        cg_draw2D_vmCvar.integer == 0) {
        CG_ScreenFade(); /* fade-overlay tail at 0x3001c0f1 */
        return;
    }

    /* Always-run pre-pass. */
    CG_UpdateScreenFade();
    CG_DrawFlashDamage();
    CG_DrawDamageDirectionIndicators(); /* 0x3001a980 (was provisionally CG_DrawScreenBlend) */

    pmType = cg_snap->ps.pmType; /* MOV EAX,[cg_snap]; MOV EAX,[EAX+0x10] */

    if (pmType == PM_TYPE_INTERMISSION) {
        CG_DrawIntermission();
        return; /* direct RET at 0x3001c037 — skips the fade-overlay tail */
    }

    if (pmType == PM_TYPE_SPECTATOR) {
        CG_DrawSpectatorMessage();
        CG_DrawCrosshairNames();
        CG_DrawTeamInfo();
        if (cg_drawStatus_vmCvar.integer != 0) {
            /* JMP 0x3001c08c: draw hud elements only, skip Menu_PaintAll +
             * lagometer, then continue into the scoreboard block. */
            CG_DrawHudElems();
        }
        /* else JZ 0x3001c091: skip the overlay/hud-elem block entirely and fall
         * straight into the scoreboard block below. */
    } else if (pmType < PM_TYPE_DEAD) {
        /* alive/playing states 0..3 (PM_TYPE_NORMAL..PM_TYPE_UFO). */
        CG_DrawCrosshair();
        CG_DrawCrosshairNames();
        if (cg_drawStatus_vmCvar.integer != 0) {
            CG_DrawWeaponSelect();
        }
        /* fall through to the common tail below. */
        CG_DrawTeamInfo(); /* 0x3001c074 */
        if (cg_drawStatus_vmCvar.integer != 0) {
            Menu_PaintAll();
            CG_VoiceMenuTimeout();
            CG_DrawHudElems();
        }
    } else {
        /* dead states (psPmType >= 6): JGE straight to the common tail. */
        CG_DrawTeamInfo(); /* 0x3001c074 */
        if (cg_drawStatus_vmCvar.integer != 0) {
            Menu_PaintAll();
            CG_VoiceMenuTimeout();
            CG_DrawHudElems();
        }
    }

    /* Scoreboard block (0x3001c091). */
    if (!CG_DrawScoreboard()) {
        CG_DrawSpectatorFollowHints();
        CG_DrawFollowingMessage();
    }
    CG_DrawVote();
    CG_DrawMatchTimeout();  /* 0x3001bbd0, reconstructed: match-timeout/paused overlay */
    CG_DrawLagometer();
    CG_DrawExpiringIconGrid();
    CG_DrawInfoScreens();   /* 0x3001b360: was mis-named CG_DrawScoresHud; developer info-overlay dispatcher */
    CG_DrawDebugOverlays();

    if (!CG_DrawScoreboard()) { /* second call, 0x3001c0c2 */
        CG_DrawCenterString();
        CG_DrawSlidingFadeElement();
        CG_DrawScoreboardFadeElement();
        CG_DrawDebugFadeElement();
        CG_DrawFixedFadeElement();
        cgame_syscall(CG_NOTIFY_PLAYER_SPAWNED, 100); /* trap(0x1e, 100) at 0x3001c0e4 */
    }

    CG_ScreenFade(); /* fade-overlay tail at 0x3001c0f1 */
}
