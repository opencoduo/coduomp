// Source: uo_cgame_mp_x86.dll 0x3003c9d0..0x3003ca22
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3003c9d0_3003ca22.mcode

#include "client/cgame/client_recovered.h"
#include "client/cgame/globals.h"


/*
 * CG_InstallSnapshotResetEffects (0x3003c9d0)
 *
 * Installs a newly transitioned snapshot as the current one and resets the
 * client effect-instance pool, then tail-calls the next transition stage
 * (FUN_30034d40).
 *
 * Argument passing: the sole caller (0x3003d3e6, inside the snapshot-transition
 * loop at 0x3003d2d0) does `MOV EAX,ESI; CALL 0x3003c9d0`, so the snapshot
 * pointer arrives in EAX (register-passed, no stack argument). The caller reads
 * `ESI->+0x00` as snapFlags (tests `& 0x4`) and `ESI->+0x08` as a serverTime
 * used for elapsed-time math (`SUB EDX,[EAX+0x8]`), which fixes the two fields
 * this reconstruction relies on.
 *
 * Behaviour, proven instruction-by-instruction:
 *   - store the snapshot pointer into cg_snap                    (MOV [0x30459160],EAX)
 *   - read snap->serverTime (+0x08)                              (MOV EAX,[EAX+8])
 *   - mirror it into the three time bases cg_time, cg_effectTime,
 *     and g_data_pmovesingle_304831b4                            (three MOV stores)
 *   - clear cg_entities[i].currentValid (+0x1e8) for every entity       (zeroing loop)
 *   - issue cgame trap 0xdb with the float 1.0 and flag 0        (syscall)
 *   - tail-call FUN_30034d40 (next transition stage)            (JMP)
 *
 * Name: assigned-by-size guess "DebugDumpAnims" is rejected — this function
 * performs no animation dump; it installs a snapshot, resets the effect pool,
 * and issues a trap. Named here by proven role.
 */

void CG_InstallSnapshotResetEffects(snapshot_t *snap)
{
    int32_t serverTime;
    int i;

    /* cg_snap = snap; (0x30459160 holds the current snapshot pointer) */
    cg_snap = snap;

    /* serverTime = snap->serverTime; then mirror into the time bases. */
    serverTime = snap->serverTime;
    cg_time = (uint32_t)serverTime;
    cg_effectTime = (uint32_t)serverTime;
    cg_physicsTime = serverTime;

    /* 0x3048c8c8 is cg_entities[0]+0x1e8. The loop walks that field with the
     * entity stride to the 0x3052e8c8 sentinel, exactly MAX_GENTITIES times. */
    for (i = 0; i < MAX_GENTITIES; i++) {
        cg_entities[i].currentValid = 0;
    }

    /* trap(0xdb, 1.0f_bits, 0): restore full master volume immediately. */
    cgame_syscall(CG_MSS_FADE_ALL_SOUNDS, CG_FloatBits(1.0f), 0);

    /* Tail call into the next snapshot-transition stage (0x30034d40). */
    CG_SnapshotTransitionStage2();
}
