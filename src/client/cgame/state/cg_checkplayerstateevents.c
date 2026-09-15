// Source: uo_cgame_mp_x86.dll 0x30034ec0..0x30034f4b
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30034ec0_30034f4b.mcode
//
// CG_CheckPlayerstateEvents — fire the client-predicted entity events that the
// new player state records but the old one did not, so the local player sees
// its own events without waiting for the server snapshot.
//
// Name resolution: the .mcode header's guess "SP_script_vehicle" is a pure
// size match from the game_mp spawn-function corpus and is REJECTED — this
// function contains no spawn/vehicle logic. Its behavior is the canonical
// Quake3/CoD CG_CheckPlayerstateEvents: it walks the playerState event ring
// (eventIndex, events[], eventParms[] at +0x88/+0x8c/+0x9c, the same layout the
// recovered playerState_t already models with MAX_PS_EVENTS == 4), diffs the new
// vs. old state, calls the entity-event dispatcher (0x30022810, CG_EntityEvent)
// for each newly-appeared event, and records it into the predicted-event ring
// (cg_predictedEvents[16], cg_predictedEventSequence). The same-module PPC name
// bank lists CG_CheckPlayerstateEvents and CG_EntityEvent, and the machine code
// proves the mapping.
//
// Register ABI (compiler-chosen for this small helper): the new player state
// `ps` arrives in EBX (set by the caller, never written here); the old player
// state `ops` is the single stack argument at [ESP+0xc] after the two prologue
// pushes. The callee ends in a plain RET, so the caller cleans the stack — a
// cdecl-shaped single stack arg plus one register arg. Expressed below as a
// normal two-parameter C function; the EBX/stack split is an ABI detail, not
// source-level behavior.

#include "client/cgame/client_recovered.h"
#include "client/cgame/globals.h"
#include "bg/bg_player_state.h"
#include "cg_predicted_events.h"

#include <string.h>

/* NOT_FROM_ORIGINAL_SOURCE: track locally predicted event occurrences independently of ring position.
 * Four retained events per buffered command bound FX history. Per-step counters distinguish equal
 * events without counting unrelated event types, parameters, weapons or source entities. Snapshot events
 * without local provenance retain their original effects. Event handlers and state updates always run. */
enum {
    CODUOMP_PREDICTED_EVENT_HISTORY = CG_PREDICTED_COMMAND_BACKUP * MAX_PS_EVENTS,
    CODUOMP_PREDICTED_EVENT_COUNTERS = 512
};

typedef struct {
    qboolean valid;
    int32_t sequence, commandNumber, commandTime, simulationTime;
    int32_t event, parm, weapon, sourceEntity;
    uint32_t ordinal;
} coduomp_predicted_event_origin_t;

typedef struct {
    coduomp_predicted_event_origin_t origin;
    int32_t entityNum, presentedWeapon;
    uint32_t playedFx;
} coduomp_predicted_event_record_t;

typedef struct {
    int32_t event, parm, weapon, sourceEntity;
    uint32_t count;
} coduomp_predicted_event_counter_t;

static coduomp_predicted_event_origin_t coduomp_event_origins[MAX_PS_EVENTS];
static coduomp_predicted_event_record_t coduomp_event_history[CODUOMP_PREDICTED_EVENT_HISTORY];
static coduomp_predicted_event_record_t coduomp_event_dispatch;
static unsigned int coduomp_event_dispatch_slot;
static uint32_t coduomp_event_suppressed_fx;
static coduomp_predicted_event_counter_t coduomp_event_counters[CODUOMP_PREDICTED_EVENT_COUNTERS];
static unsigned int coduomp_event_history_cursor, coduomp_event_counter_count;
static int32_t coduomp_event_command, coduomp_event_command_time, coduomp_event_step_time;
static int32_t coduomp_event_source = ENTITYNUM_NONE;
static qboolean coduomp_event_step_valid;

/* NOT_FROM_ORIGINAL_SOURCE: different lives and sessions do not share predicted event history. */
void coduomp_predicted_events_reset(void)
{
    memset(coduomp_event_origins, 0, sizeof(coduomp_event_origins));
    memset(coduomp_event_history, 0, sizeof(coduomp_event_history));
    coduomp_event_dispatch.origin.valid = qfalse;
    coduomp_event_history_cursor = 0;
    coduomp_event_counter_count = 0;
    coduomp_event_step_valid = qfalse;
    coduomp_event_source = ENTITYNUM_NONE;
    coduomp_predictable_event_observer = NULL;
}

/* NOT_FROM_ORIGINAL_SOURCE: rebuild local provenance and retire history for commands that cannot replay. */
void coduomp_predicted_events_begin_prediction(int32_t currentCommandNumber)
{
    memset(coduomp_event_origins, 0, sizeof(coduomp_event_origins));
    coduomp_event_dispatch.origin.valid = qfalse;
    coduomp_predictable_event_observer = NULL;
    for (unsigned int i = 0; i < CODUOMP_PREDICTED_EVENT_HISTORY; ++i) {
        coduomp_predicted_event_origin_t *origin = &coduomp_event_history[i].origin;
        if (origin->valid &&
            ((uint32_t)currentCommandNumber - (uint32_t)origin->commandNumber >= CG_PREDICTED_COMMAND_BACKUP ||
             origin->commandTime <= cg_nextSnap->ps.commandTime)) {
            origin->valid = qfalse;
        }
    }
}

/* NOT_FROM_ORIGINAL_SOURCE: every local append gets an occurrence identity before the event sequence advances. */
static void coduomp_predicted_events_record(const playerState_t *ps, int32_t event, int32_t parm)
{
    if (ps != &cg_predictedPlayerState) {
        return;
    }
    coduomp_predicted_event_origin_t *origin = &coduomp_event_origins[(uint32_t)ps->eventIndex & (MAX_PS_EVENTS - 1u)];
    origin->valid = qfalse;
    if (event <= EV_NONE || event >= EV_MAX_EVENTS) {
        return;
    }

    if (!coduomp_event_step_valid || coduomp_event_step_time != ps->commandTime) {
        coduomp_event_counter_count = 0;
        coduomp_event_step_time = ps->commandTime;
        coduomp_event_step_valid = qtrue;
    }
    coduomp_predicted_event_counter_t *counter = NULL;
    for (unsigned int i = 0; i < coduomp_event_counter_count; ++i) {
        coduomp_predicted_event_counter_t *candidate = &coduomp_event_counters[i];
        if (candidate->event == event && candidate->parm == parm && candidate->weapon == ps->currentWeapon &&
            candidate->sourceEntity == coduomp_event_source) {
            counter = candidate;
            break;
        }
    }
    if (counter == NULL) {
        /* Exhausted provenance storage must leave dispatch enabled instead of aliasing different occurrences. */
        if (coduomp_event_counter_count == CODUOMP_PREDICTED_EVENT_COUNTERS) {
            return;
        }
        counter = &coduomp_event_counters[coduomp_event_counter_count++];
        *counter = (coduomp_predicted_event_counter_t) { event, parm, ps->currentWeapon, coduomp_event_source, 0 };
    }
    *origin = (coduomp_predicted_event_origin_t) {
        .valid = qtrue,
        .sequence = ps->eventIndex,
        .commandNumber = coduomp_event_command,
        .commandTime = coduomp_event_command_time,
        .simulationTime = ps->commandTime,
        .event = event,
        .parm = parm,
        .weapon = ps->currentWeapon,
        .sourceEntity = coduomp_event_source,
        .ordinal = counter->count++
    };
}

/* NOT_FROM_ORIGINAL_SOURCE: observe all simulation substeps and the subsequent trigger prediction for a command. */
void coduomp_predicted_events_begin_command(int32_t commandNumber, int32_t commandTime)
{
    coduomp_event_command = commandNumber;
    coduomp_event_command_time = commandTime;
    coduomp_event_counter_count = 0;
    coduomp_event_step_valid = qfalse;
    coduomp_event_source = ENTITYNUM_NONE;
    coduomp_predictable_event_observer = coduomp_predicted_events_record;
}

/* NOT_FROM_ORIGINAL_SOURCE: source context distinguishes separate entities that produce identical event payloads. */
int32_t coduomp_predicted_events_set_source(int32_t entityNum)
{
    int32_t previousSource = coduomp_event_source;
    coduomp_event_source = entityNum;
    return previousSource;
}

/* NOT_FROM_ORIGINAL_SOURCE: event observation is owned only by the current local prediction command. */
void coduomp_predicted_events_end_command(void)
{
    coduomp_predictable_event_observer = NULL;
    coduomp_event_source = ENTITYNUM_NONE;
}

/* NOT_FROM_ORIGINAL_SOURCE: scope FX history to one event dispatch while allowing its state updates to run.
 * Freeze the previously played groups so all sounds, flashes and muzzle tags in this dispatch stay together. */
void coduomp_predicted_events_begin_dispatch(const playerState_t *ps, int32_t sequence)
{
    coduomp_event_dispatch.origin.valid = qfalse;
    coduomp_event_suppressed_fx = 0;
    unsigned int slot = (uint32_t)sequence & (MAX_PS_EVENTS - 1u);
    const coduomp_predicted_event_origin_t *now = &coduomp_event_origins[slot];
    if (ps != &cg_predictedPlayerState || !now->valid || now->sequence != sequence ||
        now->event != ps->events[slot] || now->parm != ps->eventParms[slot]) {
        return;
    }

    coduomp_event_dispatch = (coduomp_predicted_event_record_t) {
        .origin = *now,
        .entityNum = cg_predictedEventEntity.currentState.number,
        .presentedWeapon = cg_predictedEventEntity.currentState.weapon
    };
    unsigned int available = CODUOMP_PREDICTED_EVENT_HISTORY;
    for (unsigned int i = 0; i < CODUOMP_PREDICTED_EVENT_HISTORY; ++i) {
        const coduomp_predicted_event_record_t *first = &coduomp_event_history[i];
        const coduomp_predicted_event_origin_t *old = &first->origin;
        if (!old->valid) {
            if (available == CODUOMP_PREDICTED_EVENT_HISTORY) {
                available = i;
            }
            continue;
        }
        if (old->commandNumber != now->commandNumber || old->commandTime != now->commandTime ||
            old->simulationTime != now->simulationTime || old->event != now->event || old->parm != now->parm ||
            old->weapon != now->weapon || old->sourceEntity != now->sourceEntity || old->ordinal != now->ordinal ||
            first->entityNum != coduomp_event_dispatch.entityNum ||
            first->presentedWeapon != coduomp_event_dispatch.presentedWeapon) {
            continue;
        }
        coduomp_event_dispatch_slot = i;
        coduomp_event_dispatch.playedFx = first->playedFx;
        coduomp_event_suppressed_fx = first->playedFx;
        return;
    }
    if (available == CODUOMP_PREDICTED_EVENT_HISTORY) {
        available = coduomp_event_history_cursor;
    }
    coduomp_event_dispatch_slot = available;
}

/* NOT_FROM_ORIGINAL_SOURCE: only an FX emission path consumes history; state-only or rejected handlers do not. */
qboolean coduomp_predicted_events_allow_fx(int32_t entityNum, int32_t event, uint32_t fxMask)
{
    if (!coduomp_event_dispatch.origin.valid || coduomp_event_dispatch.entityNum != entityNum ||
        coduomp_event_dispatch.origin.event != event) {
        return qtrue;
    }
    if ((coduomp_event_suppressed_fx & fxMask) != 0) {
        return qfalse;
    }
    coduomp_event_dispatch.playedFx |= fxMask;
    coduomp_event_history[coduomp_event_dispatch_slot] = coduomp_event_dispatch;
    coduomp_event_history_cursor = (coduomp_event_dispatch_slot + 1u) % CODUOMP_PREDICTED_EVENT_HISTORY;
    return qtrue;
}

/* NOT_FROM_ORIGINAL_SOURCE: unrelated entity events must not inherit the local event's FX suppression. */
void coduomp_predicted_events_end_dispatch(void)
{
    coduomp_event_dispatch.origin.valid = qfalse;
}

void CG_CheckPlayerstateEvents(playerState_t *ps, playerState_t *ops)
{
    // for (i = ps->eventIndex - MAX_PS_EVENTS; i < ps->eventIndex; i++)
    //   0x30034ec1 EAX = ps->eventIndex; 0x30034ecd ESI = EAX - 4 (MAX_PS_EVENTS);
    //   0x30034ed0 CMP ESI,EAX / JGE exit is the signed loop-entry test i < eventIndex;
    //   0x30034f41 INC ESI / 0x30034f42 CMP / JL is the signed loop-continue test.
    int32_t i = coduo_int32_from_bits(
        (uint32_t)ps->eventIndex - (uint32_t)MAX_PS_EVENTS);
    while (i < ps->eventIndex) {
        int32_t event;

        // 0x30034ed5 EAX = ops->eventIndex; CMP ESI,EAX / JGE process:
        // events with i >= ops->eventIndex are always new -> always processed.
        if (i < ops->eventIndex) {
            // 0x30034edf EAX = ops->eventIndex - MAX_PS_EVENTS; CMP ESI,EAX / JLE skip:
            // events older than the old state's ring window are dropped.
            int32_t oldestRetained = coduo_int32_from_bits(
                (uint32_t)ops->eventIndex - (uint32_t)MAX_PS_EVENTS);
            if (i <= oldestRetained) {
                goto next_event;
            }
            // 0x30034ef2/0x30034ef5 compare the two rings at (i & 3); JZ skip:
            // an unchanged slot is not a new event.
            int32_t compareRing =
                (int32_t)((uint32_t)i & (MAX_PS_EVENTS - 1u));
            if (ps->events[compareRing] == ops->events[compareRing]) {
                goto next_event;
            }
        }

        // 0x30034eff EDI = ps->events[i & 3]; 0x30034f06 EDX = ps->eventParms[i & 3].
        int32_t ring =
            (int32_t)((uint32_t)i & (MAX_PS_EVENTS - 1u));
        event = ps->events[ring];

        // 0x30034f16 store eventParm into the predicted-event entity's
        // currentState.eventParm slot (+0xa4, aliased in centity_t as fxId)
        // BEFORE dispatch; 0x30034f0d..0x30034f1c call CG_EntityEvent(self=ECX, event=EAX,
        // predicted=1 on the stack).
        cg_predictedEventEntity.currentState.eventParm = ps->eventParms[ring];
        /* NOT_FROM_ORIGINAL_SOURCE: scope duplicate FX suppression; the handler and its state updates still run. */
        coduomp_predicted_events_begin_dispatch(ps, i);
        /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered client-module boundary input and state before use. */
        CG_EntityEvent(&cg_predictedEventEntity, event, 1);
        coduomp_predicted_events_end_dispatch();

        // 0x30034f23/0x30034f26 cg_predictedEvents[i & 0xf] = event;
        cg_predictedEvents[(int32_t)((uint32_t)i & (MAX_PREDICTED_EVENTS - 1u))] = event;

        // 0x30034f2d..0x30034f36 cg_predictedEventSequence++;
        cg_predictedEventSequence = coduo_int32_from_bits(
            (uint32_t)cg_predictedEventSequence + 1u);

next_event:
        /* INC ESI and INC EAX are target dword operations; retain their
         * modulo-2^32 behavior rather than invoking signed-overflow UB. */
        i = coduo_int32_from_bits((uint32_t)i + 1u);
    }
}
