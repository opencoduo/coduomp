// Source: uo_cgame_mp_x86.dll 0x3002e390..0x3002e3fc
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3002e390_3002e3fc.mcode

#include "client/cgame/client_recovered.h"
#include "client/cgame/globals.h"


/*
 * CG_ShutdownEffectsAndHud (0x3002e390)
 *
 * Effect/HUD subsystem shutdown-reset path. Takes no arguments (the prologue's
 * PUSH ECX only reserves a 4-byte scratch slot for the float pushed to the
 * effect-reset trap) and returns nothing (RET with no immediate; all pushed
 * cdecl arguments are reclaimed in one ADD ESP,0x2c).
 *
 * Behaviour, proven instruction-by-instruction against the .mcode:
 *   - cgame trap 0x6d with arg 0                                 (0x3002e393/95, call)
 *   - cgame trap 0xdb (CG_MSS_FADE_ALL_SOUNDS) with target volume 1.0 and
 *     duration 0. The 1.0f is materialised into the scratch slot as its raw IEEE-754
 *     bit pattern (MOV [ESP+8],0x3f800000), re-loaded (MOV EAX,[ESP+8]), then
 *     pushed as a 4-byte word ahead of the zero duration               (0x3002e39b..b4)
 *   - cgame trap 0xec with no arguments                          (0x3002e3b5/ba, call)
 *   - cgame trap 0xc1 (release) with the flame-chunk pool base pointer
 *     cg_flameChunks, which is then nulled                          (0x3002e3c0..d2)
 *   - free every HUD-element handle range and clear the registration table via
 *     the three "free all" helpers                                (0x3002e3dc/e1/e6 calls)
 *   - cgame trap 0xcc with arg 0                                  (0x3002e3eb/ed/f2, call)
 *
 * CoDUOMP.exe's recovered cgame dispatcher identifies these services as
 * CG_R_TRACK_STATISTICS, CG_FX_FREE_ACTIVE,
 * CG_FREE_WEAPON_INFO_MEMORY, and CG_Z_FREE_INTERNAL respectively.
 *
 * Name: the mechanical size-match "RollToQuaternion" (win size 0x6c) is REJECTED
 * — this function performs no quaternion math and no x87 rotation work; it issues
 * a sequence of shutdown/reset traps and frees handle pools. Named by proven role
 * (shutdown-reset of the effect + HUD-element subsystems). It pairs with the
 * snapshot-reset sibling CG_InstallSnapshotResetEffects (0x3003c9d0), which also
 * issues CG_MSS_FADE_ALL_SOUNDS(1.0f, 0). Exact original source name unresolved.
 */

void CG_ShutdownEffectsAndHud(void)
{
    /* trap(0x6d, 0). */
    cgame_syscall(CG_R_TRACK_STATISTICS, 0);

    /* trap(0xdb, 1.0f_bits, 0): restore full master volume immediately. */
    cgame_syscall(CG_MSS_FADE_ALL_SOUNDS, CG_FloatBits(1.0f), 0);

    /* trap(0xec): no arguments. */
    cgame_syscall(CG_FX_FREE_ACTIVE);

    /* trap(0xc1, buffer): hand the flame-chunk pool buffer back to the engine,
     * then null the pointer. (0x30134ce8 resolved to cg_flameChunks by the
     * CG_ClearFlameChunks/CG_InitFlameChunks reconstruction.) */
    cgame_syscall(CG_Z_FREE_INTERNAL, cg_flameChunks);
    cg_flameChunks = NULL;

    /* Free the per-weapon DObj handle band and the full registered-handle table. */
    CG_FreeWeaponDObjHandles();
    CG_FreeRegisteredHandlesLow();
    CG_FreeRegisteredHandlesHigh();

    /* trap(0xcc, 0). */
    cgame_syscall(CG_FREE_WEAPON_INFO_MEMORY, 0);
}
