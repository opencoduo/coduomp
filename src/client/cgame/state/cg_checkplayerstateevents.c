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
#include "cg_fire_replay_assert.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* NOT_FROM_ORIGINAL_SOURCE: temporary fire assertion, explicitly enabled in the normal build for diagnosis.
 * Command number, command endpoint, actual simulation time and fire ordinal identify the producer independently
 * of its event-ring position. Only a second presentation of that same shot at a different sequence aborts.
 * Snapshot-owned events have no local producer record and are deliberately excluded. */
enum { CODUOMP_FIRE_ASSERT_HISTORY = 128 };

typedef struct {
    int32_t commandTime, eventIndex;
    int32_t events[MAX_PS_EVENTS], parms[MAX_PS_EVENTS];
} coduomp_fire_assert_ring_t;

typedef struct {
    qboolean valid;
    int32_t sequence, commandNumber, commandTime, simulationTime, ordinal;
    int32_t weapon, event, parm;
} coduomp_fire_assert_origin_t;

typedef struct {
    coduomp_fire_assert_origin_t origin;
    int32_t entityNum, frame, snapshotTime, presentedWeapon, muzzleTagIndex;
    uint32_t time;
    coduomp_fire_assert_ring_t snapshot, previous, predicted;
} coduomp_fire_assert_record_t;

static coduomp_fire_assert_origin_t coduomp_fire_origins[MAX_PS_EVENTS];
static coduomp_fire_assert_record_t coduomp_fire_history[CODUOMP_FIRE_ASSERT_HISTORY];
static coduomp_fire_assert_record_t coduomp_fire_dispatch;
static coduomp_fire_assert_ring_t coduomp_fire_snapshot, coduomp_fire_previous;
static unsigned int coduomp_fire_history_cursor;
static int32_t coduomp_fire_command, coduomp_fire_command_time, coduomp_fire_ordinal;

/* NOT_FROM_ORIGINAL_SOURCE: compact event-ring evidence for the temporary assertion. */
static coduomp_fire_assert_ring_t coduomp_fire_assert_ring(const playerState_t *ps)
{
    coduomp_fire_assert_ring_t ring;
    ring.commandTime = ps->commandTime;
    ring.eventIndex = ps->eventIndex;
    memcpy(ring.events, ps->events, sizeof(ring.events));
    memcpy(ring.parms, ps->eventParms, sizeof(ring.parms));
    return ring;
}

/* NOT_FROM_ORIGINAL_SOURCE: write failure evidence to both the game console and stderr before aborting. */
static void coduomp_fire_assert_print(const char *format, ...)
{
    char line[MAX_STRING_CHARS];
    va_list args;
    va_start(args, format);
    vsnprintf(line, sizeof(line), format, args);
    va_end(args);
    fputs(line, stderr);
    Com_Printf("%s", line);
}

/* NOT_FROM_ORIGINAL_SOURCE: dump each recorded ring without relying on later mutable player state. */
static void coduomp_fire_assert_dump(const char *label, const coduomp_fire_assert_record_t *record)
{
    const coduomp_fire_assert_origin_t *origin = &record->origin;
    coduomp_fire_assert_print("%s: frame=%d time=%u snapshotTime=%d entity=%d seq=%d cmd=%d commandTime=%d simulationTime=%d ordinal=%d weapon=%d event=%d parm=%d\n",
        label, record->frame, record->time, record->snapshotTime, record->entityNum, origin->sequence,
        origin->commandNumber, origin->commandTime, origin->simulationTime, origin->ordinal, origin->weapon,
        origin->event, origin->parm);
    coduomp_fire_assert_print("presentedWeapon=%d muzzleTagIndex=%d\n", record->presentedWeapon, record->muzzleTagIndex);
    const coduomp_fire_assert_ring_t *rings[] = { &record->snapshot, &record->previous, &record->predicted };
    const char *names[] = { "snapshot", "previous prediction", "new prediction" };
    for (int i = 0; i < 3; ++i) {
        const coduomp_fire_assert_ring_t *ring = rings[i];
        coduomp_fire_assert_print("  %s: commandTime=%d eventIndex=%d slots=[%d/%d %d/%d %d/%d %d/%d]\n",
            names[i], ring->commandTime, ring->eventIndex, ring->events[0], ring->parms[0], ring->events[1], ring->parms[1],
            ring->events[2], ring->parms[2], ring->events[3], ring->parms[3]);
    }
}

/* NOT_FROM_ORIGINAL_SOURCE: clear temporary diagnostic identities at the existing player-state reset boundary. */
void coduomp_fire_assert_reset(void)
{
    memset(coduomp_fire_origins, 0, sizeof(coduomp_fire_origins));
    memset(coduomp_fire_history, 0, sizeof(coduomp_fire_history));
    coduomp_fire_dispatch.origin.valid = qfalse;
    coduomp_fire_history_cursor = 0;
    coduomp_fire_event_observer = NULL;
}

/* NOT_FROM_ORIGINAL_SOURCE: retain only producer identities generated by the current replay pass. */
void coduomp_fire_assert_begin_prediction(const playerState_t *oldState)
{
    memset(coduomp_fire_origins, 0, sizeof(coduomp_fire_origins));
    coduomp_fire_dispatch.origin.valid = qfalse;
    coduomp_fire_snapshot = coduomp_fire_assert_ring(&cg_nextSnap->ps);
    coduomp_fire_previous = coduomp_fire_assert_ring(oldState);
}

/* NOT_FROM_ORIGINAL_SOURCE: observe fire production without treating simulation replay as presentation. */
static void coduomp_fire_assert_record_event(const playerState_t *ps, int32_t event, int32_t parm)
{
    if (ps != &cg_predictedPlayerState || event < EV_FIRE_WEAPON || event > EV_FIRE_WEAPON_LASTSHOT) {
        return;
    }
    coduomp_fire_assert_origin_t *origin = &coduomp_fire_origins[(uint32_t)ps->eventIndex & (MAX_PS_EVENTS - 1u)];
    *origin = (coduomp_fire_assert_origin_t) {
        qtrue, ps->eventIndex, coduomp_fire_command, coduomp_fire_command_time, ps->commandTime,
        coduomp_fire_ordinal++, ps->currentWeapon, event, parm
    };
}

/* NOT_FROM_ORIGINAL_SOURCE: identify the original buffered command, including multi-step movement commands. */
void coduomp_fire_assert_begin_command(int32_t commandNumber, int32_t commandTime)
{
    coduomp_fire_command = commandNumber;
    coduomp_fire_command_time = commandTime;
    coduomp_fire_ordinal = 0;
    coduomp_fire_event_observer = coduomp_fire_assert_record_event;
}

/* NOT_FROM_ORIGINAL_SOURCE: the observer belongs only to this client prediction command. */
void coduomp_fire_assert_end_command(void)
{
    coduomp_fire_event_observer = NULL;
}

/* NOT_FROM_ORIGINAL_SOURCE: associate the ensuing weapon presentation with its exact locally produced fire event. */
void coduomp_fire_assert_begin_dispatch(const playerState_t *ps, int32_t sequence)
{
    coduomp_fire_dispatch.origin.valid = qfalse;
    unsigned int slot = (uint32_t)sequence & (MAX_PS_EVENTS - 1u);
    const coduomp_fire_assert_origin_t *origin = &coduomp_fire_origins[slot];
    if (ps != &cg_predictedPlayerState || !origin->valid || origin->sequence != sequence ||
        origin->event != ps->events[slot] || origin->parm != ps->eventParms[slot]) {
        return;
    }
    coduomp_fire_dispatch.origin = *origin;
    coduomp_fire_dispatch.entityNum = cg_predictedEventEntity.currentState.number;
    coduomp_fire_dispatch.frame = cg_clientFrame;
    coduomp_fire_dispatch.time = cg_time;
    coduomp_fire_dispatch.snapshotTime = cg_nextSnap->serverTime;
    coduomp_fire_dispatch.snapshot = coduomp_fire_snapshot;
    coduomp_fire_dispatch.previous = coduomp_fire_previous;
    coduomp_fire_dispatch.predicted = coduomp_fire_assert_ring(ps);
}

/* NOT_FROM_ORIGINAL_SOURCE: unrelated entity events must not inherit a shot's identity. */
void coduomp_fire_assert_end_dispatch(void)
{
    coduomp_fire_dispatch.origin.valid = qfalse;
}

/* NOT_FROM_ORIGINAL_SOURCE: temporary always-active assertion before weapon recoil/flash/sound; independent of NDEBUG. */
void coduomp_fire_assert_present(int32_t entityNum, int32_t weapon, int32_t event, int32_t muzzleTagIndex)
{
    coduomp_fire_assert_record_t *current = &coduomp_fire_dispatch;
    if (!current->origin.valid || current->entityNum != entityNum || current->origin.event != event) {
        return;
    }
    current->presentedWeapon = weapon;
    current->muzzleTagIndex = muzzleTagIndex;
    const coduomp_fire_assert_origin_t *now = &current->origin;
    for (unsigned int i = 0; i < CODUOMP_FIRE_ASSERT_HISTORY; ++i) {
        const coduomp_fire_assert_record_t *first = &coduomp_fire_history[i];
        const coduomp_fire_assert_origin_t *old = &first->origin;
        if (!old->valid || old->commandNumber != now->commandNumber || old->commandTime != now->commandTime ||
            old->simulationTime != now->simulationTime || old->ordinal != now->ordinal || old->weapon != now->weapon ||
            old->event != now->event || old->parm != now->parm || first->entityNum != entityNum ||
            first->presentedWeapon != weapon || first->muzzleTagIndex != muzzleTagIndex) {
            continue;
        }
        if (old->sequence != now->sequence) {
            coduomp_fire_assert_print("ASSERT: predicted weapon-fire presented twice after event sequence changed\n");
            coduomp_fire_assert_dump("first presentation", first);
            coduomp_fire_assert_dump("duplicate presentation", current);
            fflush(NULL);
            abort();
        }
        return;
    }
    coduomp_fire_history[coduomp_fire_history_cursor++ % CODUOMP_FIRE_ASSERT_HISTORY] = *current;
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
        /* NOT_FROM_ORIGINAL_SOURCE: temporary fire assertion applies only to weapon presentation during this dispatch. */
        coduomp_fire_assert_begin_dispatch(ps, i);
        /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered client-module boundary input and state before use. */
        CG_EntityEvent(&cg_predictedEventEntity, event, 1);
        coduomp_fire_assert_end_dispatch();

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
