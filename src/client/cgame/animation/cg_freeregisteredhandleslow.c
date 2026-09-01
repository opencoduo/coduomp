// Source: uo_cgame_mp_x86.dll 0x300163d0..0x300163fe
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_300163d0_300163fe.mcode
//
// CG_FreeRegisteredHandlesLow — release the low block (table indices 0..63) of
// the parallel "DObj info" registration table (cg_dObjInfoKeys[] at 0x30487af8 /
// cg_dObjInfoHandles[] at 0x30488af4) and null both arrays for each freed slot.
//
// For each index i in [0, 64):
//   1. cgame_syscall(CG_SAFE_CLIENT_DOBJ_FREE /* 0xa8 */, i, 1) — ask the engine to
//      release the DObj/model registration keyed by i (flag arg = 1, matching the
//      release-with-flag call form used elsewhere, e.g. 0x30021ea0 / 0x300058f0);
//   2. cg_dObjInfoKeys[i]    = 0;
//   3. cg_dObjInfoHandles[i] = 0;
//
// This is the low-range companion of CG_FreeRegisteredHandlesHigh (0x30016470,
// indices 64..1022); together they cover the full 1023-entry table. Both are
// invoked by the effects/HUD shutdown-reset path CG_ShutdownEffectsAndHud
// (0x3002e390). See the table description in globals.h/globals.c.
//
// NAMING: the .mcode header name "PM_ClearAimDownSightFlag" is a pure size guess
// (win size 0x2e matched a same-size game_mp_uo symbol) and is REJECTED — this
// function has nothing to do with playerState/pmove or an ADS flag bit: it issues
// the DObj-registration RELEASE trap (0xa8) in a loop and nulls the two
// DObj-info arrays. (The mechanical globals export even mislabeled slot 0 of
// those arrays as "pm_clearaimdownsightflag" because this was the first writer to
// touch the datum — see globals.c; the array shape has since been repaired.) The
// name CG_FreeRegisteredHandlesLow is the caller-observed role name already
// recorded in client_recovered.h for 0x300163d0; adopted here. Exact original
// engine/source symbol unproven (no cgame syscall-id name table recovered);
// role-named from behavior + call graph.
//
// ABI: void(void), no arguments. Callee saves ESI/EDI; loop counter in ESI, the
// null value (0) in EDI. Plain RET. The three trap arguments (id, index, flag)
// are pushed and caller-cleaned inside the loop (ADD ESP,0xc), i.e. the cdecl
// cleanup for the *cgame_syscall variadic trap call.

#include "client/cgame/client_recovered.h"
#include "client/cgame/globals.h"

/* Number of low-block table entries freed by this helper (indices 0..63); the
 * high helper at 0x30016470 covers 64..1022. Loop bound: CMP ESI,0x40 / JL. */
void CG_FreeRegisteredHandlesLow(void)
{
    int32_t i;

    for (i = 0; i < CG_DOBJINFO_LOW_COUNT; i++) {
        /* 0x300163d6..0x300163eb: PUSH 1; PUSH i; PUSH 0xa8; CALL [cgame_syscall];
         * ADD ESP,0xc. Release the registration keyed by table index i. */
        cgame_syscall(CG_SAFE_CLIENT_DOBJ_FREE, i, 1);

        /* 0x300163e4 / 0x300163ee: both arrays zeroed for this slot (EDI == 0). */
        cg_dObjInfoKeys[i] = 0;
        cg_dObjInfoHandles[i] = 0;
    }
}
