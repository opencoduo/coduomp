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

/* NOT_FROM_ORIGINAL_SOURCE: identify weapon FX by producing command and occurrence, independent of ring position.
 * Keep these IDs client-local: playerState_t and the network event format remain unchanged. */
typedef struct {
    qboolean valid;
    int32_t sequence, commandNumber, commandTime, event, parm;
    uint32_t ordinal;
} coduomp_predicted_event_id_t;

typedef struct {
    int32_t event, parm, weapon, entityNum;
    uint32_t ordinal;
} coduomp_presented_weapon_event_t;

typedef struct {
    int32_t commandNumber, commandTime;
    unsigned int count;
    coduomp_presented_weapon_event_t events[MAX_PS_EVENTS];
} coduomp_predicted_command_fx_t;

static coduomp_predicted_event_id_t coduomp_event_ids[MAX_PS_EVENTS];
static coduomp_predicted_command_fx_t coduomp_command_fx[CG_PREDICTED_COMMAND_BACKUP];
static int32_t coduomp_latest_event_command;

/* NOT_FROM_ORIGINAL_SOURCE: different lives and sessions do not share predicted FX history. */
void coduomp_predicted_events_reset(void)
{
    memset(coduomp_event_ids, 0, sizeof(coduomp_event_ids));
    memset(coduomp_command_fx, 0, sizeof(coduomp_command_fx));
    coduomp_latest_event_command = 0;
}

/* NOT_FROM_ORIGINAL_SOURCE: normal prediction replays older commands; only the newest command may reset history. */
void coduomp_predicted_events_begin_prediction(int32_t currentCommandNumber)
{
    if (currentCommandNumber < coduomp_latest_event_command) {
        coduomp_predicted_events_reset();
    }
    coduomp_latest_event_command = currentCommandNumber;
    memset(coduomp_event_ids, 0, sizeof(coduomp_event_ids));
}

/* NOT_FROM_ORIGINAL_SOURCE: collect a command's appended events after Pmove without changing shared event producers. */
void coduomp_predicted_events_record_command(int32_t commandNumber, int32_t commandTime, int32_t firstSequence)
{
    const playerState_t *ps = &cg_predictedPlayerState;
    uint32_t count = (uint32_t)ps->eventIndex - (uint32_t)firstSequence;
    if (count > MAX_PS_EVENTS) {
        /* Overwritten events make occurrence numbers ambiguous. Leave this command unfiltered. */
        memset(coduomp_event_ids, 0, sizeof(coduomp_event_ids));
        return;
    }
    for (uint32_t i = 0; i < count; ++i) {
        uint32_t sequence = (uint32_t)firstSequence + i;
        unsigned int slot = sequence & (MAX_PS_EVENTS - 1u);
        uint32_t ordinal = 0;
        for (uint32_t j = 0; j < i; ++j) {
            unsigned int earlier = ((uint32_t)firstSequence + j) & (MAX_PS_EVENTS - 1u);
            if (ps->events[earlier] == ps->events[slot] && ps->eventParms[earlier] == ps->eventParms[slot]) {
                ++ordinal;
            }
        }
        coduomp_event_ids[slot] = (coduomp_predicted_event_id_t) {
            qtrue, coduo_int32_from_bits(sequence), commandNumber, commandTime,
            ps->events[slot], ps->eventParms[slot], ordinal
        };
    }
}

/* NOT_FROM_ORIGINAL_SOURCE: suppress only repeated local weapon-FX dispatches; mixed/stateful handlers pass through. */
static qboolean coduomp_predicted_weapon_fx_should_present(const playerState_t *ps, int32_t sequence)
{
    unsigned int slot = (uint32_t)sequence & (MAX_PS_EVENTS - 1u);
    const coduomp_predicted_event_id_t *id = &coduomp_event_ids[slot];
    if (ps != &cg_predictedPlayerState || !id->valid || id->sequence != sequence ||
        id->event != ps->events[slot] || id->parm != ps->eventParms[slot]) {
        return qtrue;
    }

    int32_t weapon = cg_predictedEventEntity.currentState.weapon;
    switch (id->event) {
    case EV_FIRE_WEAPON:
    case EV_FIRE_WEAPONB:
    case EV_FIRE_WEAPONC:
    case EV_FIRE_WEAPON_LASTSHOT:
    case EV_FIRE_WEAPON_MG42: {
        /* CG_FireWeapon clears the tag-model flag before resolving a packed weapon override. */
        enum { CODUOMP_FIRE_DRAW_TAG_MODEL = 128 };
        int32_t override = coduo_int32_from_bits((uint32_t)id->parm & ~(uint32_t)CODUOMP_FIRE_DRAW_TAG_MODEL);
        if (override > 0) {
            weapon = override;
        }
        break;
    }
    case EV_FIRE_QUADBARREL_1:
    case EV_FIRE_QUADBARREL_2:
    case EV_RELOAD:
    case EV_RELOAD_FROM_EMPTY:
    case EV_RELOAD_START:
    case EV_RELOAD_END:
    case EV_RECHAMBER_WEAPON:
    case EV_EJECT_BRASS:
        break;
    default:
        return qtrue;
    }
    /* Preserve original validation paths and the gas-fire timestamp update. */
    if (weapon <= 0 || weapon > bg_numWeapons || (uint32_t)weapon >= MAX_WEAPONS ||
        bg_weaponInfos[weapon] == NULL || bg_weaponInfos[weapon]->weaponType == WEAPTYPE_GAS) {
        return qtrue;
    }

    coduomp_predicted_command_fx_t *history = &coduomp_command_fx[(uint32_t)id->commandNumber & (CG_PREDICTED_COMMAND_BACKUP - 1u)];
    if (history->commandNumber != id->commandNumber || history->commandTime != id->commandTime) {
        *history = (coduomp_predicted_command_fx_t) { .commandNumber = id->commandNumber, .commandTime = id->commandTime };
    }
    coduomp_presented_weapon_event_t now = { id->event, id->parm, weapon, cg_predictedEventEntity.currentState.number, id->ordinal };
    for (unsigned int i = 0; i < history->count; ++i) {
        const coduomp_presented_weapon_event_t *old = &history->events[i];
        if (old->event == now.event && old->parm == now.parm && old->weapon == now.weapon &&
            old->entityNum == now.entityNum && old->ordinal == now.ordinal) {
            return qfalse;
        }
    }
    if (history->count < MAX_PS_EVENTS) {
        history->events[history->count++] = now;
    }
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
        /* NOT_FROM_ORIGINAL_SOURCE: skip already presented weapon FX while retaining prediction bookkeeping. */
        if (coduomp_predicted_weapon_fx_should_present(ps, i)) {
            /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered client-module boundary input and state before use. */
            CG_EntityEvent(&cg_predictedEventEntity, event, 1);
        }

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
