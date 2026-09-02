// Authoritative bodies: uo_cgame_mp_x86.dll 0x30006590,
// uo_game_mp_x86.dll 0x20006300, game.mp.uo.i386.so RVA 0x00020a35.
//
// BG_PlayerStateToEntityState -- project the mutable playerState into the 0xf4-
// byte network entityState. The .mcode's CG_DrawPlayerCompassPointers label is a
// rejected size-only match: this body copies trajectories, animation/vehicle
// state, stance angles, and predictable events, and its behavior/callers match
// the shared BG_PlayerStateToEntityState routine.

#include "bg_player_state.h"
#include "bg_player_state_services.h"
#include "math/q_math.h"
#include "compat/coduo_fp_conversion.h"

#include <stddef.h>
#include <stdint.h>

enum {
    BG_VIEWHEIGHT_LERP_LONG_MS = 400,
    BG_VIEWHEIGHT_LERP_SHORT_MS = 200,
    BG_MOVEMENT_DIR_FOLD_LIMIT = 128,
    BG_MAX_PENDING_EVENTS = 4
};

static const float BG_MOVEMENT_DIR_FULL_RANGE = 256.0f;

/* The original source table is emitted independently into each module.  The
 * Windows cgame and both game-module bodies contain the same six values and
 * signed terminator; Linux retains the canonical symbol name. */
const int32_t iSingleClientEvents[7] = {EV_STANCE_FORCE_STAND,
                                        EV_STANCE_FORCE_CROUCH,
                                        EV_STANCE_FORCE_PRONE,
                                        EV_STEP_VIEW,
                                        EV_BULLET_HIT_CLIENT_SMALL,
                                        EV_BULLET_HIT_CLIENT_LARGE,
                                        -1};
const int32_t *pEventSingleClientList = iSingleClientEvents;

void BG_PlayerStateToEntityState(playerState_t *ps, entityState_t *es, qboolean snap)
{
    /* Follow/spectator states are rendered as a player; other states use the
     * client-reserved no-op entity type. The NEG/SBB expression yields 1 or 7. */
    es->eType = (ps->playerStateFlags & PSF_PLAYER_ENTITY_MASK) != 0 ? ET_PLAYER : ET_INVISIBLE;
    bg_compat_player_state_set_entity_number(es, ps);

    es->posTrType = TR_INTERPOLATE;
    es->origin[0] = ps->psOrigin[0];
    es->origin[1] = ps->psOrigin[1];
    es->origin[2] = ps->psOrigin[2];
    if (snap) {
        es->origin[0] = (float)coduo_fp_to_i32_extended(es->origin[0]);
        es->origin[1] = (float)coduo_fp_to_i32_extended(es->origin[1]);
        es->origin[2] = (float)coduo_fp_to_i32_extended(es->origin[2]);
    }

    es->aposTrType = TR_INTERPOLATE;
    es->angles[0] = ps->viewAngles[0];
    es->angles[1] = ps->viewAngles[1];
    es->angles[2] = ps->viewAngles[2];
    if (snap) {
        es->angles[0] = (float)coduo_fp_to_i32_extended(es->angles[0]);
        es->angles[1] = (float)coduo_fp_to_i32_extended(es->angles[1]);
        es->angles[2] = (float)coduo_fp_to_i32_extended(es->angles[2]);
    }

    int32_t movementDir = ps->movementDir;
    long double movementYaw = (long double)movementDir;
    if (movementDir > BG_MOVEMENT_DIR_FOLD_LIMIT) {
        movementYaw -= (long double)BG_MOVEMENT_DIR_FULL_RANGE;
    }
    es->clientInfoLeanYawPayload = (float)movementYaw;

    es->legsAnim = ps->legsAnim;
    es->torsoAnim = ps->torsoAnim;
    es->clientNum = ps->psClientNum;
    es->eFlags = ps->entityStateFlags;

    uint32_t vehicleEFlags = ps->entityStateFlags; /* 0x300066ac reload */
    if ((vehicleEFlags & EF_RESTRICTED_MASK) != 0) {
        es->vehicleEntityNum = ps->viewLockedEntityNum;
    }

    /* Preserve unrelated bits in the packed vehicle state while replacing the
     * 3-bit position, 3-bit type, and 2-bit animation-pitch fields in order. */
    uint32_t vehicleState = (uint32_t)es->vehicleAnimState;
    vehicleState = (vehicleState & ~VEHICLE_ANIM_STATE_POS_MASK) | ((uint32_t)ps->vehiclePosition << VEHICLE_ANIM_STATE_POS_SHIFT);
    es->vehicleAnimState = (int32_t)vehicleState;
    vehicleState = (vehicleState & ~VEHICLE_ANIM_STATE_TYPE_MASK) | ((uint32_t)ps->vehicleType << VEHICLE_ANIM_STATE_TYPE_SHIFT);
    es->vehicleAnimState = (int32_t)vehicleState;
    vehicleState = (vehicleState & ~VEHICLE_ANIM_STATE_MOTION_MASK) | ((uint32_t)ps->vehicleMotion << VEHICLE_ANIM_STATE_MOTION_SHIFT);
    es->vehicleAnimState = (int32_t)vehicleState;

    if (ps->pmType >= PM_TYPE_DEAD) {
        es->eFlags |= EF_DEAD;
    } else {
        es->eFlags &= ~EF_DEAD;
    }

    if ((ps->playerStateFlags & PMF_ADS) != 0) {
        es->eFlags |= EF_ADS;
    } else {
        es->eFlags &= ~EF_ADS;
    }

    es->leanf = ps->leanFraction;

    /* The three output view angles carry the prone lean payload only at the
     * configured prone viewheight; every other stance writes zeroes. */
    if (ps->viewHeightTarget == ps->proneViewHeight && ps->viewHeightTarget != ps->crouchViewHeight) {
        float fraction;
        if (ps->viewHeightLerpTime == 0) {
            fraction = 1.0f;
        } else {
            int32_t duration;
            if (ps->viewHeightLerpTarget == ps->proneViewHeight) {
                duration = BG_VIEWHEIGHT_LERP_LONG_MS;
            } else if (ps->viewHeightLerpTarget == ps->crouchViewHeight) {
                duration = ps->viewHeightLerpDown != 0 ? BG_VIEWHEIGHT_LERP_SHORT_MS : BG_VIEWHEIGHT_LERP_LONG_MS;
            } else {
                duration = BG_VIEWHEIGHT_LERP_SHORT_MS;
            }

            int32_t elapsed = coduo_int32_from_bits((uint32_t)ps->commandTime - (uint32_t)ps->viewHeightLerpTime);
            /* 0x300067aa..0x300067db: FILD/FIDIV leaves the exact quotient in
             * st0 while FST writes the rounded float used by later paths. The
             * first (negative) compare consumes the retained quotient; only
             * the upper-bound compare reloads the rounded copy. */
            long double fractionWide = (long double)elapsed / (long double)duration;
            fraction = (float)fractionWide;
            if (fractionWide < 0.0L) {
                fraction = 0.0f;
            } else if (fraction > 1.0f) {
                fraction = 1.0f;
            }

            if (ps->viewHeightLerpDown == 0) {
                fraction = 1.0f - fraction;
            }
        }

        es->viewAngles[0] = fraction * ps->torsoHeight;
        es->viewAngles[1] = AngleNormalize180(ps->torsoPitch) * fraction;
        es->viewAngles[2] = AngleNormalize180(ps->waistPitch) * fraction;
    } else {
        es->viewAngles[0] = 0.0f;
        es->viewAngles[1] = 0.0f;
        es->viewAngles[2] = 0.0f;
    }

    /* Publish one pending predictable-event parm through the direct event slot.
     * If its cursor fell over four entries behind, catch it up to the oldest
     * event still resident in the four-slot player-state ring. */
    int32_t pendingDelta = coduo_int32_from_bits((uint32_t)ps->oldEventIndex - (uint32_t)ps->eventIndex);
    if (pendingDelta < 0) {
        int32_t behind = coduo_int32_from_bits((uint32_t)ps->eventIndex - (uint32_t)ps->oldEventIndex);
        if (behind > BG_MAX_PENDING_EVENTS) {
            ps->oldEventIndex = coduo_int32_from_bits((uint32_t)ps->eventIndex - (uint32_t)BG_MAX_PENDING_EVENTS);
        }
        es->eventParm = (uint8_t)ps->eventParms[(uint32_t)ps->oldEventIndex & (MAX_PS_EVENTS - 1)];
        ps->oldEventIndex = coduo_int32_from_bits((uint32_t)ps->oldEventIndex + 1u);
    } else {
        es->eventParm = 0;
    }

    /* Transfer every event not present in the six-entry exclusion list. Input
     * events/parms are byte-truncated exactly as the MOVZX byte loads prove. */
    int32_t eventCursor = ps->lastEventIndex;
    if (eventCursor != ps->eventIndex) {
        const int32_t *eventExclusions = pEventSingleClientList;
        for (;;) {
            uint32_t sourceSlot = (uint32_t)eventCursor & (MAX_PS_EVENTS - 1);
            uint8_t event = (uint8_t)ps->events[sourceSlot];

            bg_compat_player_event(es->number, event);

            int32_t exclusionIndex = 0;
            while (eventExclusions[exclusionIndex] > 0 && eventExclusions[exclusionIndex] != event) {
                exclusionIndex++;
            }

            if (eventExclusions[exclusionIndex] < 0) {
                /* Retail reloads the source event byte after scanning. It also
                 * reloads eventSequence between the event and parm stores. */
                uint8_t eventForStore = (uint8_t)ps->events[sourceSlot];
                uint32_t eventOutputSlot = (uint32_t)es->eventSequence & (MAX_PS_EVENTS - 1);
                es->events[eventOutputSlot] = eventForStore;

                uint32_t parmOutputSlot = (uint32_t)es->eventSequence & (MAX_PS_EVENTS - 1);
                uint8_t eventParm = (uint8_t)ps->eventParms[sourceSlot];
                es->eventParms[parmOutputSlot] = eventParm;
                es->eventSequence = coduo_int32_from_bits((uint32_t)es->eventSequence + 1u);
            }

            /* 0x3000693e snapshots the live producer cursor before INC EBX. */
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
}
