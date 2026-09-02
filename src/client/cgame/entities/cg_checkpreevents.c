// Source: uo_cgame_mp_x86.dll 0x300239e0..0x30023ae9
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_300239e0_30023ae9.mcode
//
// CG_CheckPreEvents — fire the events queued on one client entity's
// embedded model sub-entity since the last time it was checked. This is the
// corpse-model / DObj analogue of CG_CheckEvents (0x300238e0): byte-for-byte the
// same event-ring algorithm, but reading the entityState-shaped event fields that
// live inside cent->corpseModelInfo (entityState_t at cent +0xf4) and
// dispatching each event through CG_EntityPreEvent (0x30023690) rather than
// CG_EntityEvent. The two functions form the parent/model pair that mirrors the
// engine's currentState.eType-and-events[] event machinery.
//
//   - A pure event sub-entity (corpseModelInfo.eType > ET_EVENTS) carries exactly
//     one event encoded as (eType - ET_EVENTS); it is fired once and latched via
//     modelPreviousEvent (cent +0x1f4) so it never replays.
//   - Otherwise the model record has a queued-event ring (events[]/eventParms[],
//     MAX_ENTITY_EVENTS entries). The function dispatches every event in the
//     (modelPreviousEvent, eventSequence] span, writing each event's parm into
//     corpseModelInfo.eventParm (cent +0x198) for the dispatcher, then advances
//     modelPreviousEvent to eventSequence. It saves and restores the PARENT
//     entity's currentState.eventParm low byte (cent +0xa4) across the loop —
//     note this is the parent field at +0xa4, distinct from the model field at
//     +0x198 written per-event; both writes are exactly as the machine code does.
//
// Name resolution: the .mcode header guess "CMD_VEH_SetTurretTargetEnt" is a pure
// cross-corpus size match (win 0x109 == matched 0x109) and is REJECTED — there is
// no entity lookup, no angle/aim math, and no vehicle-turret target write here.
// The behavior is exactly CG_CheckEvents specialized to the model sub-entity. The
// Mac CG_CheckPreEvents has the identical two-callee set and multiplicity:
// CG_CalcEntityLerpPositions and CG_EntityPreEvent are each called on the same
// single-event and event-ring paths. This resolves the source name.
//
// Register ABI (compiler-chosen): the entity `cent` arrives in ESI and is never
// reloaded from the stack (the PUSH ECX at entry is just a scratch slot balanced
// by the POP ECX epilogue). CG_CalcEntityLerpPositions takes cent as one
// caller-cleaned stack arg; CG_EntityPreEvent takes cent in EAX with the event
// id and event parm as two caller-cleaned stack args. Both callees end in a plain
// RET, so the ESI-in argument and stack cleanup are ABI details, expressed here as
// an ordinary one-parameter C function.

#include "client/cgame/client_recovered.h"

#include <stdint.h>

/* Ring index mask: (sequence & 3), MAX_ENTITY_EVENTS - 1. Proven at 0x30023aa2
 * (AND EAX,0x3). */
enum {
    MODEL_EVENT_RING_MASK = MAX_ENTITY_EVENTS - 1
};

/* One ring-counter period rolled off modelPreviousEvent when eventSequence has
 * wrapped below it (ADD ECX,0xffffff00 == subtract 0x100). Proven at 0x30023a45. */
enum {
    MODEL_EVENT_SEQUENCE_PERIOD = 0x100
};

void CG_CheckPreEvents(centity_t *cent)
{
    entityState_t *model = &cent->corpseModelInfo;

    // 0x300239e1 CMP [ESI+0xf8],0x10 / 0x300239ea JLE regular: signed compare of
    // the model sub-entity's eType against ET_EVENTS.
    if (model->eType > ET_EVENTS) {
        // --- pure event sub-entity: fire the encoded event exactly once ---
        // 0x300239ec EAX = modelPreviousEvent; 0x300239f4 JNZ return if already fired.
        if (cent->modelPreviousEvent) {
            return;
        }
        // 0x300239fb modelPreviousEvent = 1 (latch);
        // 0x30023a05 CALL CG_CalcEntityLerpPositions(cent).
        cent->modelPreviousEvent = 1;
        CG_CalcEntityLerpPositions(cent);
        // 0x30023a0a EAX = eType; 0x30023a13 SUB EAX,0x10 -> event = eType - ET_EVENTS;
        // 0x30023a16 PUSH 0 (parm); 0x30023a18 PUSH EAX (event); 0x30023a1b CALL
        // CG_EntityPreEvent(cent, event, eventParm=0).
        CG_EntityPreEvent(cent, model->eType - ET_EVENTS, 0);
        return;
    }

    // --- queued event ring ---
    // 0x30023a27 EAX = eventSequence; 0x30023a2f JNZ process. eventSequence == 0
    // means no events ever queued: clear modelPreviousEvent and return.
    int32_t eventSequence = model->eventSequence;
    if (eventSequence == 0) {
        // 0x30023a31 modelPreviousEvent = EAX (== 0).
        cent->modelPreviousEvent = 0;
        return;
    }

    // 0x30023a3b ECX = modelPreviousEvent; 0x30023a41 CMP EAX,ECX / 0x30023a43 JGE:
    // if eventSequence has wrapped below modelPreviousEvent, roll modelPreviousEvent
    // back one ring-counter period (0x100) so the (modelPreviousEvent, eventSequence]
    // span is valid. 0x30023a45 ADD ECX,0xffffff00 == modelPreviousEvent - 0x100.
    if (eventSequence < cent->modelPreviousEvent) {
        cent->modelPreviousEvent = coduo_int32_from_bits((uint32_t)cent->modelPreviousEvent - (uint32_t)MODEL_EVENT_SEQUENCE_PERIOD);
    }

    // 0x30023a51 EDI = modelPreviousEvent; 0x30023a59 ECX = eventSequence -
    // modelPreviousEvent; 0x30023a5b CMP ECX,0x4 / 0x30023a5e JLE: never replay more
    // than MAX_ENTITY_EVENTS events — clamp modelPreviousEvent to
    // eventSequence - MAX_ENTITY_EVENTS. 0x30023a60 LEA EDX,[EAX-0x4].
    uint32_t gapBits = (uint32_t)eventSequence - (uint32_t)cent->modelPreviousEvent;
    int32_t signedGap = coduo_int32_from_bits(gapBits);
    if (signedGap > MAX_ENTITY_EVENTS) {
        cent->modelPreviousEvent = coduo_int32_from_bits((uint32_t)eventSequence - (uint32_t)MAX_ENTITY_EVENTS);
    }

    // 0x30023a69 CMP [ESI+0x1f4],EAX / 0x30023a6f JL work: process only when
    // modelPreviousEvent < eventSequence; otherwise (>=) 0x30023a71 latches
    // modelPreviousEvent = eventSequence and returns.
    if (cent->modelPreviousEvent < eventSequence) {
        // 0x30023a7c CALL CG_CalcEntityLerpPositions(cent) — lerp position/angles
        // once before running this sub-entity's events.
        CG_CalcEntityLerpPositions(cent);

        // 0x30023a81 EDI = modelPreviousEvent; 0x30023a87 EAX = eventSequence
        // (reloaded); 0x30023a8d BL = (byte)cent->currentState.eventParm — save the PARENT
        // entity's currentState.eventParm low byte (cent +0xa4).
        int32_t seq = cent->modelPreviousEvent;
        eventSequence = model->eventSequence;
        uint8_t savedParentEventParm = (uint8_t)cent->currentState.eventParm;

        // 0x30023a96 CMP EDI,EAX / 0x30023a98 JZ 0x30023ad0 skips the loop when the
        // clamped modelPreviousEvent already equals eventSequence.
        while (seq != eventSequence) {
            // 0x30023aa0 loop top. 0x30023aa2 EAX = seq & 3 (ring index).
            int32_t ring = seq & MODEL_EVENT_RING_MASK;
            // 0x30023aa5 ECX = events[ring]; 0x30023aac EAX = eventParms[ring].
            uint32_t event = model->eventBits[ring];
            uint32_t parm = model->eventParmBitsRing[ring];
            // 0x30023ab4 corpseModelInfo.eventParm (cent +0x198) = eventParms[ring]
            // for the dispatcher to read.
            model->eventParmBits = parm;
            // 0x30023ab3 PUSH EAX (parm) / 0x30023aba PUSH ECX (event) /
            // 0x30023abd CALL CG_EntityPreEvent(cent, events[ring], eventParms[ring]).
            CG_EntityPreEvent(cent, coduo_int32_from_bits(event), coduo_int32_from_bits(parm));
            // 0x30023ac2 EAX = eventSequence (reloaded); 0x30023acb INC EDI;
            // 0x30023acc CMP EDI,EAX / 0x30023ace JNZ loop.
            eventSequence = model->eventSequence;
            seq = coduo_int32_from_bits((uint32_t)seq + 1u);
        }

        // 0x30023ad0 ECX = eventSequence; 0x30023ad6 MOVZX EAX,BL; 0x30023ad9
        // cent->currentState.eventParm = (zero-extended) saved parent parm byte (restore +0xa4).
        eventSequence = model->eventSequence;
        cent->currentState.eventParm = savedParentEventParm;
    }

    // 0x30023a71 / 0x30023adf modelPreviousEvent = eventSequence.
    cent->modelPreviousEvent = eventSequence;
}
