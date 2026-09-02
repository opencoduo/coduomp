// Source: uo_cgame_mp_x86.dll 0x30034f50..0x30034fd7
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30034f50_30034fd7.mcode

#include "../client_recovered.h"

/* Re-dispatch predicted events whose value changed after prediction.  Only
 * events still represented by the 16-entry prediction history can be checked. */
void CG_CheckChangedPredictableEvents(playerState_t *ps)
{
    int32_t sequence = coduo_int32_from_bits(
        (uint32_t)ps->eventIndex - (uint32_t)MAX_PS_EVENTS);
    while (sequence < ps->eventIndex) {
        int32_t predictedSequence = cg_predictedEventSequence;
        int32_t oldestPredicted = coduo_int32_from_bits(
            (uint32_t)predictedSequence -
            (uint32_t)MAX_PREDICTED_EVENTS);
        if (sequence >= predictedSequence ||
            sequence <= oldestPredicted) {
            goto next_sequence;
        }

        const int32_t psSlot = sequence & (MAX_PS_EVENTS - 1);
        const int32_t predictedSlot = sequence & (MAX_PREDICTED_EVENTS - 1);
        const int32_t event = ps->events[psSlot];
        if (event == cg_predictedEvents[predictedSlot]) {
            goto next_sequence;
        }

        cg_predictedEventEntity.currentState.eventParm = ps->eventParms[psSlot];
        /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered client-module boundary input and state before use. */
        CG_EntityEvent(&cg_predictedEventEntity,
                       event, qtrue);
        int32_t showMiss = cg_showmiss_vmCvar.integer;
        cg_predictedEvents[predictedSlot] = event;
        if (showMiss != 0) {
            Com_PrintMessage("WARNING: changed predicted event\n");
        }

next_sequence:
        sequence = coduo_int32_from_bits((uint32_t)sequence + 1u);
    }
}
