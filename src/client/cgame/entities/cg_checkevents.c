// Source: uo_cgame_mp_x86.dll 0x300238e0..0x300239d9
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_300238e0_300239d9.mcode
//
// CG_CheckEvents — fire the entity events that have been queued on one client
// entity (centity_t) since the last time it was checked. This
// is the canonical Quake3/CoD CG_CheckEvents:
//
//   - A pure event entity (currentState.eType > ET_EVENTS) carries exactly one
//     event, encoded as (eType - ET_EVENTS). It is fired once and latched via
//     previousEvent so it never replays.
//   - Otherwise the entity has a queued-event ring (events[]/eventParms[],
//     MAX_ENTITY_EVENTS entries). The function dispatches every event in the
//     (previousEvent, eventSequence] span, temporarily writing each event's parm
//     into currentState.eventParm for the dispatcher to read, then restores the
//     original eventParm and advances previousEvent to eventSequence.
//
// Name resolution: the .mcode header guess "script_method_scriptbuiltin_attach"
// is a pure size match (win size 0xf9 == matched 0xf9) from the game_mp_uo script
// corpus and is REJECTED — there is no script/method/attach behavior here. The
// behavior (eType>ET_EVENTS encoded-event latch + queued event-ring diff calling
// CG_EntityEvent) is exactly CG_CheckEvents, which the same-module cgame_mp PPC
// name bank lists. The two callees are already resolved in the shared header:
// CG_CalcEntityLerpPositions (0x30021d30, lerps the entity's position/angles
// before its events run) and CG_EntityEvent (0x30022810, the event dispatcher).
//
// Register ABI (compiler-chosen): the entity `cent` arrives in ESI (never
// reloaded from the stack — the PUSH ECX at entry is just a scratch slot). Both
// callees take the entity: CG_CalcEntityLerpPositions by a single caller-cleaned
// stack arg; CG_EntityEvent by (self in ECX, event id in EAX, predicted flag on
// the stack). Every dispatch here passes predicted = 0. The callee ends in a plain
// RET, so the ESI-in argument and stack cleanup are ABI details, expressed below
// as an ordinary one-parameter C function.

#include "client/cgame/client_recovered.h"

void CG_CheckEvents(centity_t *cent)
{
    // 0x300238e1 CMP [ESI+0x4],0x10 / 0x300238e7 JLE regular_events:
    // signed compare of currentState.eType against ET_EVENTS.
    if (cent->currentState.eType > ET_EVENTS) {
        // --- pure event entity: fire the encoded event exactly once ---
        // 0x300238e9 EAX = previousEvent; 0x300238f1 JNZ return if already fired.
        if (cent->previousEvent) {
            return;
        }
        // 0x300238f8 previousEvent = 1 (latch); 0x30023902 CALL CG_CalcEntityLerpPositions(cent).
        cent->previousEvent = 1;
        CG_CalcEntityLerpPositions(cent);
        // 0x30023907 EAX = eType; 0x3002390f SUB EAX,0x10 -> event = eType - ET_EVENTS;
        // 0x30023914 CALL CG_EntityEvent(cent, event, predicted=0).
        CG_EntityEvent(cent, cent->currentState.eType - ET_EVENTS, 0);
        return;
    }

    // --- queued event ring ---
    // 0x30023920 EAX = eventSequence; 0x30023928 JNZ process. eventSequence == 0
    // means no events ever queued: clear previousEvent and return.
    uint32_t eventSequenceBits = (uint32_t)cent->currentState.eventSequence;
    int32_t eventSequence = coduo_int32_from_bits(eventSequenceBits);
    if (eventSequenceBits == 0u) {
        // 0x3002392a previousEvent = EAX (== 0).
        cent->previousEvent = 0;
        return;
    }

    // 0x30023934 ECX = previousEvent; 0x3002393a CMP EAX,ECX / 0x3002393c JGE:
    // if eventSequence has wrapped below previousEvent, roll previousEvent back one
    // ring-counter period (256) so the (previousEvent, eventSequence] span is valid.
    // 0x3002393e ADD ECX,0xffffff00 == previousEvent - 0x100.
    if (eventSequence < cent->previousEvent) {
        cent->previousEvent = coduo_int32_from_bits((uint32_t)cent->previousEvent - 256u);
    }

    // 0x3002394a EDI = previousEvent; 0x30023952 ECX = eventSequence - previousEvent;
    // 0x30023954 CMP ECX,0x4 / 0x30023957 JLE: never replay more than MAX_ENTITY_EVENTS
    // events — clamp previousEvent to eventSequence - MAX_ENTITY_EVENTS.
    // 0x30023959 LEA EDX,[EAX-0x4] == eventSequence - 4.
    if (coduo_int32_from_bits(eventSequenceBits - (uint32_t)cent->previousEvent) > MAX_ENTITY_EVENTS) {
        cent->previousEvent = coduo_int32_from_bits(eventSequenceBits - (uint32_t)MAX_ENTITY_EVENTS);
    }

    // 0x30023962 CMP [ESI+0x1f0],EAX / 0x30023968 JGE done: nothing new to fire.
    if (cent->previousEvent < eventSequence) {
        // 0x3002396b CALL CG_CalcEntityLerpPositions(cent) — lerp position/angles
        // once before running this entity's events.
        CG_CalcEntityLerpPositions(cent);

        // 0x30023970 EDI = previousEvent; 0x30023976 EAX = eventSequence (reloaded);
        // 0x3002397c BL = (byte)eventParm — save the original event parm (low byte only).
        uint32_t seqBits = (uint32_t)cent->previousEvent;
        eventSequenceBits = (uint32_t)cent->currentState.eventSequence;
        uint8_t savedEventParm = (uint8_t)cent->currentState.eventParm;

        // 0x30023985 CMP EDI,EAX / 0x30023987 JZ 0x300239c0 skips the loop when the
        // clamped previousEvent already equals eventSequence.
        while (seqBits != eventSequenceBits) {
            // 0x30023990 loop top. 0x30023992 ECX = seq & 3 (ring index).
            uint32_t ring = seqBits & (MAX_ENTITY_EVENTS - 1u);
            // 0x30023995 EAX = events[ring]; 0x3002399c ECX = eventParms[ring].
            uint32_t event = cent->currentState.events[ring];
            // 0x300239a3 currentState.eventParm = eventParms[ring] for the dispatcher.
            cent->currentState.eventParm = cent->currentState.eventParms[ring];
            // 0x300239a9..0x300239ad CG_EntityEvent(cent, events[ring], predicted=0).
            CG_EntityEvent(cent, coduo_int32_from_bits(event), 0);
            // 0x300239b2 EAX = eventSequence (reloaded); 0x300239bb INC EDI;
            // 0x300239bc CMP EDI,EAX / 0x300239be JNZ loop.
            eventSequenceBits = (uint32_t)cent->currentState.eventSequence;
            seqBits += 1u;
        }

        // 0x300239c0 EAX = eventSequence; 0x300239c6 MOVZX EDX,BL;
        // 0x300239c9 currentState.eventParm = (zero-extended) saved parm byte.
        eventSequenceBits = (uint32_t)cent->currentState.eventSequence;
        cent->currentState.eventParm = savedEventParm;
    }

    // 0x300239cf previousEvent = eventSequence.
    cent->previousEvent = coduo_int32_from_bits(eventSequenceBits);
}
