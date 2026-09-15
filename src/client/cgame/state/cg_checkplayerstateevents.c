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
#include "cg_predicted_fire.h"

#include <string.h>

/* NOT_FROM_ORIGINAL_SOURCE: identify locally predicted shots independently of event-ring position.
 * Keep presentation history across replay passes, with space for four retained events per buffered command.
 * Acknowledged commands and commands outside the replay window expire; player-state resets clear the history.
 * Snapshot-owned events have no local producer record and continue through the original presentation path. */
enum { CODUOMP_PREDICTED_FIRE_HISTORY = CG_PREDICTED_COMMAND_BACKUP * MAX_PS_EVENTS };

typedef struct {
    qboolean valid;
    int32_t sequence, commandNumber, commandTime, simulationTime, ordinal;
    int32_t weapon, event, parm;
} coduomp_predicted_fire_origin_t;

typedef struct {
    coduomp_predicted_fire_origin_t origin;
    int32_t entityNum, presentedWeapon, muzzleTagIndex;
} coduomp_predicted_fire_record_t;

static coduomp_predicted_fire_origin_t coduomp_fire_origins[MAX_PS_EVENTS];
static coduomp_predicted_fire_record_t coduomp_fire_history[CODUOMP_PREDICTED_FIRE_HISTORY];
static coduomp_predicted_fire_record_t coduomp_fire_dispatch;
static unsigned int coduomp_fire_history_cursor;
static int32_t coduomp_fire_command, coduomp_fire_command_time, coduomp_fire_ordinal;

/* NOT_FROM_ORIGINAL_SOURCE: shots from different lives or sessions must not share presentation history. */
void coduomp_predicted_fire_reset(void)
{
    memset(coduomp_fire_origins, 0, sizeof(coduomp_fire_origins));
    memset(coduomp_fire_history, 0, sizeof(coduomp_fire_history));
    coduomp_fire_dispatch.origin.valid = qfalse;
    coduomp_fire_history_cursor = 0;
    coduomp_fire_event_observer = NULL;
}

/* NOT_FROM_ORIGINAL_SOURCE: refresh replay provenance and retire shots whose commands cannot be replayed. */
void coduomp_predicted_fire_begin_prediction(int32_t currentCommandNumber)
{
    memset(coduomp_fire_origins, 0, sizeof(coduomp_fire_origins));
    coduomp_fire_dispatch.origin.valid = qfalse;
    coduomp_fire_event_observer = NULL;
    for (unsigned int i = 0; i < CODUOMP_PREDICTED_FIRE_HISTORY; ++i) {
        coduomp_predicted_fire_origin_t *origin = &coduomp_fire_history[i].origin;
        if (origin->valid &&
            ((uint32_t)currentCommandNumber - (uint32_t)origin->commandNumber >= CG_PREDICTED_COMMAND_BACKUP ||
             origin->commandTime <= cg_nextSnap->ps.commandTime)) {
            origin->valid = qfalse;
        }
    }
}

/* NOT_FROM_ORIGINAL_SOURCE: attach command identity before the locally generated fire event advances the ring. */
static void coduomp_predicted_fire_record_event(const playerState_t *ps, int32_t event, int32_t parm)
{
    if (ps != &cg_predictedPlayerState || event < EV_FIRE_WEAPON || event > EV_FIRE_WEAPON_LASTSHOT) {
        return;
    }
    coduomp_predicted_fire_origin_t *origin = &coduomp_fire_origins[(uint32_t)ps->eventIndex & (MAX_PS_EVENTS - 1u)];
    *origin = (coduomp_predicted_fire_origin_t) {
        qtrue, ps->eventIndex, coduomp_fire_command, coduomp_fire_command_time, ps->commandTime,
        coduomp_fire_ordinal++, ps->currentWeapon, event, parm
    };
}

/* NOT_FROM_ORIGINAL_SOURCE: identify the original buffered command, including multi-step movement commands. */
void coduomp_predicted_fire_begin_command(int32_t commandNumber, int32_t commandTime)
{
    coduomp_fire_command = commandNumber;
    coduomp_fire_command_time = commandTime;
    coduomp_fire_ordinal = 0;
    coduomp_fire_event_observer = coduomp_predicted_fire_record_event;
}

/* NOT_FROM_ORIGINAL_SOURCE: the observer belongs only to this client prediction command. */
void coduomp_predicted_fire_end_command(void)
{
    coduomp_fire_event_observer = NULL;
}

/* NOT_FROM_ORIGINAL_SOURCE: associate the ensuing weapon presentation with its exact locally produced fire event. */
void coduomp_predicted_fire_begin_dispatch(const playerState_t *ps, int32_t sequence)
{
    coduomp_fire_dispatch.origin.valid = qfalse;
    unsigned int slot = (uint32_t)sequence & (MAX_PS_EVENTS - 1u);
    const coduomp_predicted_fire_origin_t *origin = &coduomp_fire_origins[slot];
    if (ps != &cg_predictedPlayerState || !origin->valid || origin->sequence != sequence ||
        origin->event != ps->events[slot] || origin->parm != ps->eventParms[slot]) {
        return;
    }
    coduomp_fire_dispatch.origin = *origin;
    coduomp_fire_dispatch.entityNum = cg_predictedEventEntity.currentState.number;
}

/* NOT_FROM_ORIGINAL_SOURCE: unrelated entity events must not inherit a shot's identity. */
void coduomp_predicted_fire_end_dispatch(void)
{
    coduomp_fire_dispatch.origin.valid = qfalse;
}

/* NOT_FROM_ORIGINAL_SOURCE: present each matching shot once, even if replay places it at another sequence.
 * The producer's command, simulation time and ordinal distinguish repeated automatic fire; weapon/event/tag
 * differences retain their own presentation. Only fire effects are gated; prediction and event accounting continue. */
qboolean coduomp_predicted_fire_should_present(int32_t entityNum, int32_t weapon, int32_t event, int32_t muzzleTagIndex)
{
    coduomp_predicted_fire_record_t *current = &coduomp_fire_dispatch;
    if (!current->origin.valid || current->entityNum != entityNum || current->origin.event != event) {
        return qtrue;
    }
    current->presentedWeapon = weapon;
    current->muzzleTagIndex = muzzleTagIndex;
    const coduomp_predicted_fire_origin_t *now = &current->origin;
    unsigned int available = CODUOMP_PREDICTED_FIRE_HISTORY;
    for (unsigned int i = 0; i < CODUOMP_PREDICTED_FIRE_HISTORY; ++i) {
        const coduomp_predicted_fire_record_t *first = &coduomp_fire_history[i];
        const coduomp_predicted_fire_origin_t *old = &first->origin;
        if (!old->valid) {
            if (available == CODUOMP_PREDICTED_FIRE_HISTORY) {
                available = i;
            }
            continue;
        }
        if (old->commandNumber != now->commandNumber || old->commandTime != now->commandTime ||
            old->simulationTime != now->simulationTime || old->ordinal != now->ordinal || old->weapon != now->weapon ||
            old->event != now->event || old->parm != now->parm || first->entityNum != entityNum ||
            first->presentedWeapon != weapon || first->muzzleTagIndex != muzzleTagIndex) {
            continue;
        }
        return qfalse;
    }
    if (available == CODUOMP_PREDICTED_FIRE_HISTORY) {
        available = coduomp_fire_history_cursor;
    }
    coduomp_fire_history[available] = *current;
    coduomp_fire_history_cursor = (available + 1u) % CODUOMP_PREDICTED_FIRE_HISTORY;
    return qtrue;
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
        /* NOT_FROM_ORIGINAL_SOURCE: scope the predicted shot identity to this event's weapon presentation. */
        coduomp_predicted_fire_begin_dispatch(ps, i);
        /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered client-module boundary input and state before use. */
        CG_EntityEvent(&cg_predictedEventEntity, event, 1);
        coduomp_predicted_fire_end_dispatch();

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
