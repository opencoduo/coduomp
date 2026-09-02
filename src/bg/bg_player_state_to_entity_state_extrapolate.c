// Authoritative bodies: uo_cgame_mp_x86.dll 0x30006980,
// uo_game_mp_x86.dll 0x20006790, game.mp.uo.i386.so RVA 0x00021028.
//
// BG_PlayerStateToEntityStateExtrapolate -- project playerState into an
// entityState whose position is extrapolated for 50 milliseconds. Although
// much of this resembles BG_PlayerStateToEntityState, the retail function has
// its own complete body and several behaviorally different paths. Keep it
// independent so those differences remain visible and auditable.

#include "bg_player_state.h"
#include "bg_player_state_services.h"
#include "math/q_math.h"
#include "compat/coduo_fp_conversion.h"

#include <stddef.h>
#include <stdint.h>

enum {
    BG_PLAYERSTATE_EXTRAPOLATION_MS = 50,
    BG_EXTRAPOLATE_VIEWHEIGHT_LERP_LONG_MS = 400,
    BG_EXTRAPOLATE_VIEWHEIGHT_LERP_SHORT_MS = 200,
    BG_EXTRAPOLATE_MAX_PENDING_EVENTS = 4
};

void BG_PlayerStateToEntityStateExtrapolate(playerState_t *ps,
                                            entityState_t *es,
                                            int32_t time,
                                            qboolean snap)
{
    bg_compat_player_state_set_entity_number(es, ps);

    es->posTrType = TR_LINEAR_STOP;
    es->origin[0] = ps->psOrigin[0];
    es->origin[1] = ps->psOrigin[1];
    es->origin[2] = ps->psOrigin[2];
    es->posTrDelta[0] = ps->velocity[0];
    es->posTrDelta[1] = ps->velocity[1];
    es->posTrDelta[2] = ps->velocity[2];
    es->posTrTime = time;
    es->posTrDuration = BG_PLAYERSTATE_EXTRAPOLATION_MS;

    es->aposTrType = TR_INTERPOLATE;
    es->angles[0] = ps->viewAngles[0];
    es->angles[1] = ps->viewAngles[1];
    es->angles[2] = ps->viewAngles[2];

    /* 0x300069e5..0x300069eb is a direct FILD/FSTP. Unlike the non-
     * extrapolating sibling, this body does not fold values above 128. */
    es->clientInfoLeanYawPayload = (float)(long double)ps->movementDir;
    es->eFlags = ps->entityStateFlags;

    /* Publish one pending predictable-event parm through the direct event
     * slot. SUB/ADD/INC are 32-bit wrapping operations in the retail ABI. */
    int32_t pendingDelta =
        coduo_int32_from_bits((uint32_t)ps->oldEventIndex -
                         (uint32_t)ps->eventIndex);
    if (pendingDelta < 0) {
        int32_t behind =
            coduo_int32_from_bits((uint32_t)ps->eventIndex -
                             (uint32_t)ps->oldEventIndex);
        if (behind > BG_EXTRAPOLATE_MAX_PENDING_EVENTS) {
            ps->oldEventIndex = coduo_int32_from_bits(
                (uint32_t)ps->eventIndex -
                (uint32_t)BG_EXTRAPOLATE_MAX_PENDING_EVENTS);
        }
        es->eventParm =
            (uint8_t)ps->eventParms[(uint32_t)ps->oldEventIndex &
                                    (MAX_PS_EVENTS - 1)];
        ps->oldEventIndex =
            coduo_int32_from_bits((uint32_t)ps->oldEventIndex + 1u);
    } else {
        es->eventParm = 0;
    }

    /* If a reset moved the producer cursor behind the consumer cursor, retail
     * first catches the consumer up to the producer. The comparison uses the
     * signed result of a wrapping 32-bit subtraction. */
    int32_t cursorLead =
        coduo_int32_from_bits((uint32_t)ps->lastEventIndex -
                         (uint32_t)ps->eventIndex);
    if (cursorLead > 0) {
        ps->lastEventIndex = ps->eventIndex;
    }

    /* Transfer every event not present in the positive, sentinel-terminated
     * exclusion list. Each source event and parm is read as an unsigned byte. */
    int32_t eventCursor = ps->lastEventIndex;
    if (eventCursor != ps->eventIndex) {
        const int32_t *eventExclusions = pEventSingleClientList;
        for (;;) {
            uint32_t sourceSlot =
                (uint32_t)eventCursor & (MAX_PS_EVENTS - 1);
            uint8_t event = (uint8_t)ps->events[sourceSlot];

            bg_compat_player_event(es->number, event);

            int32_t exclusionIndex = 0;
            while (eventExclusions[exclusionIndex] > 0 &&
                   eventExclusions[exclusionIndex] != event) {
                exclusionIndex++;
            }

            if (eventExclusions[exclusionIndex] < 0) {
                /* Retail reloads the event byte after the scan and reloads
                 * eventSequence separately for the event and parm stores. */
                uint8_t eventForStore = (uint8_t)ps->events[sourceSlot];
                uint32_t eventOutputSlot =
                    (uint32_t)es->eventSequence & (MAX_PS_EVENTS - 1);
                es->events[eventOutputSlot] = eventForStore;

                uint32_t parmOutputSlot =
                    (uint32_t)es->eventSequence & (MAX_PS_EVENTS - 1);
                uint8_t eventParm = (uint8_t)ps->eventParms[sourceSlot];
                es->eventParms[parmOutputSlot] = eventParm;
                es->eventSequence = coduo_int32_from_bits(
                    (uint32_t)es->eventSequence + 1u);
            }

            /* The producer cursor is reloaded before the local cursor's INC. */
            int32_t liveEventIndex = ps->eventIndex;
            eventCursor = coduo_int32_from_bits((uint32_t)eventCursor + 1u);
            if (eventCursor == liveEventIndex) {
                break;
            }
        }
    }

    ps->lastEventIndex = ps->eventIndex;
    es->weapon = (uint8_t)ps->currentWeapon;
    es->groundEntityNum = (uint16_t)ps->groundEntityNum;

    es->eType = (ps->playerStateFlags & PSF_PLAYER_ENTITY_MASK) != 0
                    ? ET_PLAYER
                    : ET_INVISIBLE;

    /* Snapping occurs only after the complete event projection, and applies to
     * both the copied origin and the copied angles. */
    if (snap) {
        es->origin[0] = (float)coduo_fp_to_i32_extended(es->origin[0]);
        es->origin[1] = (float)coduo_fp_to_i32_extended(es->origin[1]);
        es->origin[2] = (float)coduo_fp_to_i32_extended(es->origin[2]);
        es->angles[0] = (float)coduo_fp_to_i32_extended(es->angles[0]);
        es->angles[1] = (float)coduo_fp_to_i32_extended(es->angles[1]);
        es->angles[2] = (float)coduo_fp_to_i32_extended(es->angles[2]);
    }

    es->legsAnim = ps->legsAnim;
    es->torsoAnim = ps->torsoAnim;
    es->clientNum = ps->psClientNum;

    uint32_t vehicleEFlags = ps->entityStateFlags;
    if ((vehicleEFlags & EF_RESTRICTED_MASK) != 0) {
        es->vehicleEntityNum = ps->viewLockedEntityNum;
    }

    /* Replace the 3-bit position, 3-bit type, and 2-bit motion fields in the
     * already-existing packed word, preserving the other bits at each step. */
    uint32_t vehicleState = (uint32_t)es->vehicleAnimState;
    vehicleState =
        (vehicleState & ~VEHICLE_ANIM_STATE_POS_MASK) |
        ((uint32_t)ps->vehiclePosition << VEHICLE_ANIM_STATE_POS_SHIFT);
    es->vehicleAnimState = (int32_t)vehicleState;

    vehicleState =
        (vehicleState & ~VEHICLE_ANIM_STATE_TYPE_MASK) |
        ((uint32_t)ps->vehicleType << VEHICLE_ANIM_STATE_TYPE_SHIFT);
    es->vehicleAnimState = (int32_t)vehicleState;

    vehicleState =
        (vehicleState & ~VEHICLE_ANIM_STATE_MOTION_MASK) |
        ((uint32_t)ps->vehicleMotion << VEHICLE_ANIM_STATE_MOTION_SHIFT);
    es->vehicleAnimState = (int32_t)vehicleState;

    uint32_t outputEFlags = es->eFlags;
    if (ps->pmType >= PM_TYPE_DEAD) {
        outputEFlags |= EF_DEAD;
    } else {
        outputEFlags &= ~EF_DEAD;
    }
    es->eFlags = outputEFlags;

    outputEFlags = es->eFlags;
    if ((ps->playerStateFlags & PMF_ADS) != 0) {
        outputEFlags |= EF_ADS;
    } else {
        outputEFlags &= ~EF_ADS;
    }
    es->eFlags = outputEFlags;

    es->leanf = ps->leanFraction;

    /* The extrapolating path has no viewHeightLerpTime==0 shortcut. It spills
     * both integer operands to binary32 before dividing, then spills the
     * quotient to binary32 before either clamp comparison. */
    if (ps->viewHeightTarget == ps->proneViewHeight &&
        ps->viewHeightTarget != ps->crouchViewHeight) {
        int32_t duration;
        /* 0x30006c7e compares against proneViewHeight (+0x574) first;
         * 0x30006c92 then compares against crouchViewHeight (+0x578). */
        if (ps->viewHeightLerpTarget == ps->proneViewHeight) {
            duration = BG_EXTRAPOLATE_VIEWHEIGHT_LERP_LONG_MS;
        } else if (ps->viewHeightLerpTarget == ps->crouchViewHeight) {
            duration = ps->viewHeightLerpDown != 0
                           ? BG_EXTRAPOLATE_VIEWHEIGHT_LERP_SHORT_MS
                           : BG_EXTRAPOLATE_VIEWHEIGHT_LERP_LONG_MS;
        } else {
            duration = BG_EXTRAPOLATE_VIEWHEIGHT_LERP_SHORT_MS;
        }

        int32_t elapsed =
            coduo_int32_from_bits((uint32_t)ps->commandTime -
                             (uint32_t)ps->viewHeightLerpTime);
        float elapsedFloat = (float)(long double)elapsed;
        float durationFloat = (float)(long double)duration;
        float fraction =
            (float)((long double)elapsedFloat / (long double)durationFloat);

        if (fraction < 0.0f) {
            fraction = 0.0f;
        } else if (fraction > 1.0f) {
            fraction = 1.0f;
        }

        if (ps->viewHeightLerpDown == 0) {
            fraction = 1.0f - fraction;
        }

        es->viewAngles[0] = fraction * ps->torsoHeight;
        es->viewAngles[1] =
            AngleNormalize180(ps->torsoPitch) * fraction;
        es->viewAngles[2] =
            AngleNormalize180(ps->waistPitch) * fraction;
    } else {
        es->viewAngles[0] = 0.0f;
        es->viewAngles[1] = 0.0f;
        es->viewAngles[2] = 0.0f;
    }
}
