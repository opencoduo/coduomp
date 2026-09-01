// Source: uo_cgame_mp_x86.dll 0x30016470..0x300164ab
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30016470_300164ab.mcode
//
// CG_FreeRegisteredHandlesHigh — release the high block (table indices 64..1022)
// of the parallel "DObj info" registration table (cg_dObjInfoKeys[] at 0x30487af8
// / cg_dObjInfoHandles[] at 0x30488af4) and null both arrays for each freed slot.
//
// For each index i in [64, 1023):
//   1. cgame_syscall(CG_SAFE_CLIENT_DOBJ_FREE /* 0xa8 */, i, 1) — ask the engine to
//      release the DObj/model registration keyed by i (flag arg = 1, matching the
//      release-with-flag call form used by the low companion 0x300163d0);
//   2. cg_dObjInfoKeys[i]    = 0;
//   3. cg_dObjInfoHandles[i] = 0;
//
// This is the high-range companion of CG_FreeRegisteredHandlesLow (0x300163d0,
// indices 0..63); together they cover the full 1023-entry table. Both are invoked
// by the effects/HUD shutdown-reset path CG_ShutdownEffectsAndHud (0x3002e390).
// See the table description in globals.h/globals.c and the sibling .c.
//
// NAMING: the .mcode header name "Use_Static" is a pure size guess (win size 0x3b
// matched a same-size game_mp_uo symbol) and is REJECTED — the body has nothing to
// do with any "static" allocation: it issues the DObj-registration RELEASE trap
// (0xa8) in a loop over the upper table range and nulls the two DObj-info arrays.
// The name CG_FreeRegisteredHandlesHigh is the caller-observed role name already
// recorded in client_recovered.h for 0x30016470; adopted here. Exact original
// engine/source symbol unproven (no cgame syscall-id name table recovered);
// role-named from behavior + call graph.
//
// ABI: void(void), no arguments. Callee saves ESI/EDI; loop counter in ESI
// (0x40..0x3fe), the null value (0) in EDI. Plain RET. The three trap arguments
// (id, index, flag) are pushed and caller-cleaned inside the loop (ADD ESP,0xc),
// i.e. the cdecl cleanup for the *cgame_syscall variadic trap call. Loop bound:
// CMP ESI,0x3ff / JL, so the last freed index is 0x3fe (1022).

#include "client/cgame/client_recovered.h"
#include "client/cgame/globals.h"

/* First high-block index freed by this helper (MOV ESI,0x40); the low helper at
 * 0x300163d0 covers 0..63. */
/* One past the last freed index (CMP ESI,0x3ff / JL): indices run 64..1022, i.e.
 * the full 1023-entry table minus the low block. */
void CG_FreeRegisteredHandlesHigh(void)
{
    int32_t i;

    for (i = CG_DOBJINFO_LOW_COUNT; i < ENTITYNUM_NONE; i++) {
        /* 0x30016480..0x30016495: PUSH 1; PUSH i; PUSH 0xa8; CALL [cgame_syscall];
         * ADD ESP,0xc. Release the registration keyed by table index i. */
        cgame_syscall(CG_SAFE_CLIENT_DOBJ_FREE, i, 1);

        /* 0x3001648e / 0x30016498: both arrays zeroed for this slot (EDI == 0). */
        cg_dObjInfoKeys[i] = 0;
        cg_dObjInfoHandles[i] = 0;
    }
}
