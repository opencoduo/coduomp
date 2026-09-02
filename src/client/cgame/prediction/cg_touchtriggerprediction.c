// Source: uo_cgame_mp_x86.dll 0x30035710..0x300357c2
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30035710_300357c2.mcode
//
// CG_TouchTriggerPrediction (0x30035710) — the per-frame consumer of the
// trigger list cg_triggerEntities[0..cg_numTriggerEntities) that
// CG_BuildSolidList (0x30035030) populates. It is a sibling of
// CG_PointContents (0x30035420) and walks the same centity_t
// source objects, with mode-gated predicted item touching.
//
// High-level behavior proven from the machine code:
//   - Read cg_predictedPlayerState.pmType (0x304831c8). If it is >= 6, or if it is not one of {0,1,4},
//     do nothing this frame (the loop is only entered for modes 0, 1, and 4).
//   - EBP = (cg_predictedPlayerState.pmType == 4) is precomputed as the item-touch gate.
//   - For each entity in cg_triggerEntities[0..cg_numTriggerEntities):
//       * If entity->currentState.eType == 3 AND cg_predictedPlayerState.pmType != 4, run the predicted item
//         touch CG_TouchItem(entity) and move to the next entity.
//       * Otherwise, if entity->currentState.solid == SOLID_BMODEL, resolve its inline model;
//         on a non-zero handle, issue CG_CM_BOX_TRACE into a local trace-result
//         buffer, passing the shared view vector (0x304831d8, twice) and this
//         pmove bounds buffers (cg_pmove.mins/maxs), the
//         shader handle, and a trailing -1.
//
// Naming: the .mcode header carries the SIZE-GUESS name "SnapVectorTowards" (matched
// only on byte size 0xb2). REJECTED: SnapVectorTowards is a pure vector-rounding math
// helper with no system calls and no global-array walk. This function issues two
// collision-model system calls, iterates the trigger list, and calls CG_TouchItem.
// The replacement name is confirmed by the same-module symbol-bearing Mac binary.
//
// ABI: no arguments, no return value; plain RET (no RET imm). SUB ESP,0x30 reserves
// the 48-byte trace_t out buffer for the projection trap. EBP/ESI/EDI are
// callee-saved (pushed lazily and popped on the taken exits). ESI is the loop index
// i; EDI holds the current entity pointer, then is reused to hold the shader handle.
//
// cg_triggerEntities[] is declared as centity_t*[256]. The entity pointer is
// forwarded directly to CG_TouchItem through its canonical centity type.

#include <stddef.h>
#include <stdint.h>

#include "client/cgame/client_recovered.h"
#include "client/cgame/globals.h"

// Offsets this function proves against the machine code (i386, 4-byte pointers):
_Static_assert(offsetof(centity_t, currentState.eType) == 0x04, "centity_t.eType at +0x04 (CMP [EDI+4],3)");
_Static_assert(offsetof(centity_t, currentState.itemIndex) == 0x8c, "centity_t.itemIndex at +0x8c (MOV EDI,[EDI+0x8c])");
_Static_assert(offsetof(centity_t, currentState.solid) == 0xa0, "centity_t.solid at +0xa0 (CMP [EDI+0xa0],0xffffff)");

void CG_TouchTriggerPrediction(void)
{
    // 30035710..3003571b: modes at or beyond PM_TYPE_DEAD do not process triggers.
    int32_t mode = cg_predictedPlayerState.pmType;
    if (mode >= PM_TYPE_DEAD) {
        return;
    }

    // 30035721..3003572c: spectator mode suppresses predicted item pickup but
    // still admits inline brush triggers. The equality is materialized with SETZ.
    int32_t noItemTouch = (mode == PM_TYPE_SPECTATOR);

    // 30035729..30035737: only normal, linked, and spectator modes enter the loop.
    if (mode != PM_TYPE_NORMAL && mode != PM_TYPE_LINKED && !noItemTouch) {
        return;
    }

    // 3003573d..30035747: signed loop bound; if count <= 0 the loop body is skipped.
    for (int32_t i = 0; i < cg_numTriggerEntities; i++) {
        // 30035750: EDI = cg_triggerEntities[i].
        centity_t *entity = cg_triggerEntities[i];

        // 30035757..3003575f: ET_ITEM entities are handled by predicted item
        // touch, but only when the mode does not suppress it (mode != 4).
        if (entity->currentState.eType == ET_ITEM && !noItemTouch) {
            // 30035761: CG_TouchItem(entity) — entity forwarded in EDI. 30035766
            // jumps past the projection path to the loop tail.
            CG_TouchItem(entity);
            continue;
        }

        // 30035768..30035772: only inline brush models take the trace path.
        if (entity->currentState.solid != SOLID_BMODEL) {
            continue;
        }

        // 30035774..30035788: resolve the inline collision model.
        int32_t inlineModel = (int32_t)cgame_syscall(CG_CM_INLINE_MODEL, entity->currentState.itemIndex);
        if (inlineModel == 0) {
            continue;
        }

        // 3003578a..300357ae: trace the predicted player bounds against the model.
        // 48-byte trace-result buffer (SUB ESP,0x30 reserves it). Push order
        // (program order) is -1, inlineModel, &cg_pmove.maxs, &cg_pmove.mins,
        // cg_predictedPlayerState.psOrigin (the shared view-origin vec3 at 0x304831d8,
        // pushed twice), &out, id; so on the callee side the argument order after the
        // trap id is (out, view, view, argA, argB, handle, -1).
        trace_t out;
        cgame_syscall(CG_CM_BOX_TRACE, (intptr_t)&out, (intptr_t)cg_predictedPlayerState.psOrigin,
                      (intptr_t)cg_predictedPlayerState.psOrigin, (intptr_t)cg_pmove.mins, (intptr_t)cg_pmove.maxs, inlineModel, -1);
    }
}
