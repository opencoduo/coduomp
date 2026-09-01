// Source: uo_cgame_mp_x86.dll 0x3003cc10..0x3003d21c
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3003cc10_3003d21c.mcode
//
// CG_SetNextSnap - install and transition the incoming snapshot. The former
// CG_FakeTrajectoryEffects label was a size-only match; the cg_nextSnap store,
// interpolation math, client-state refresh, and entity transition graph prove
// the same-module CG_SetNextSnap identity.

#include "client/cgame/client_recovered.h"
#include "client/cgame/globals.h"

#include <stdint.h>
#include <string.h>

void CG_SetNextSnap(snapshot_t *snap)
{
    cg_nextSnap = snap;
    cgame_syscall(CG_DOBJ_INVALIDATE_SKELS);

    if (snap->serverTime == cg_snap->serverTime) {
        cg_frameInterpolation = 0.0f;
    } else {
        /* 0x3003cc54 FILD num / 0x3003cc58 FIDIV den: the numerator is loaded as an
         * integer and divided by the integer denominator operand in 80-bit (both
         * exact) -- this is NOT a float/float divide, so keep the operands wide.
         * 0x3003cc5c FST (store-and-keep) rounds the quotient into
         * cg_frameInterpolation while 0x3003cc62 FCOMP tests the UNROUNDED st0
         * against 0.0f -- so the sign test must read the wide quotient, not the
         * float lvalue. */
        int32_t numerator = coduo_int32_from_bits(
            cg_time - (uint32_t)cg_snap->serverTime);
        int32_t denominator = coduo_int32_from_bits(
            (uint32_t)snap->serverTime - (uint32_t)cg_snap->serverTime);
        long double frac = (long double)numerator / (long double)denominator;
        cg_frameInterpolation = (float)frac;
        if (frac < 0.0f) {
            cg_frameInterpolation = 0.0f;
        }
    }

    CG_ExecuteNewServerCommands(snap->serverCommandSequence);

    for (int32_t i = 0; i < snap->numClients; i++) {
        clientState_t *client = &snap->clients[i];
        clientInfo_t *state = &bgs.clientinfo[client->clientNum];

        state->obituaryTeam = state->infoValid != 0
                                ? state->team : client->team;
        state->infoValid = 1;
        state->moduleState.active = 1;
        state->clientNum = client->clientNum;
        state->team = client->team;

        if (strcmp(state->name, client->name) != 0) {
            if (state->name[0] != '\0') {
                const char *renamed = CG_SafeTranslateString_Internal("cgame", "CGAME_PLAYERRENAMES");
                const char *message = va("%s^7 %s %s", state->name,
                                         renamed, client->name);
                cgame_syscall(CG_GAME_MESSAGE, (intptr_t)message,
                              cg_gameMessageWidth_vmCvar.integer);
            }
            Q_strncpyz(state->name, client->name,
                       sizeof(state->name));
        }

        {
            const char *name = CG_ConfigString(client->modelindex + 405);
            if (strcmp(state->modelName, name) != 0) {
                Q_strncpyz(state->modelName, name, sizeof(state->modelName));
                state->dobjNeedsUpdate = 1;
            }
        }

        for (int32_t part = 0; part < 6; part++) {
            const char *modelName =
                CG_ConfigString(client->attachModelIndex[part] + 405);
            const char *skinName =
                CG_ConfigString(client->attachTagIndex[part] + 117);

            if (strcmp(state->attachModelNames[part], modelName) != 0) {
                Q_strncpyz(state->attachModelNames[part], modelName,
                           sizeof(state->attachModelNames[part]));
                state->dobjNeedsUpdate = 1;
            }
            if (strcmp(state->attachTagNames[part], skinName) != 0) {
                Q_strncpyz(state->attachTagNames[part], skinName,
                           sizeof(state->attachTagNames[part]));
                state->dobjNeedsUpdate = 1;
            }
        }
    }

    CG_RefreshWeaponInfosForConfigString(
        CG_ConfigString(snap->ps.viewModelIndex + 405));

    cg_crosshairHealthEntNum = snap->ps.stats[STAT_IDENT_CLIENT_NUM];
    cg_crosshairHealth = snap->ps.stats[STAT_IDENT_CLIENT_HEALTH];

    if (snap->ps.psClientNum != cg_snap->ps.psClientNum) {
        cg_entities[cg_snap->ps.psClientNum].currentValid = 0;
    }

    qboolean spawnCountChanged =
        snap->ps.stats[STAT_SPAWN_COUNT] !=
            cg_snap->ps.stats[STAT_SPAWN_COUNT];

    if ((snap->ps.playerStateFlags & PSF_PLAYER_ENTITY_MASK) != 0) {
        centity_t *cent = &cg_entities[snap->ps.psClientNum];
        qboolean resetPlayerState =
            cg_initialSnapshotPending != 0 ||
            spawnCountChanged ||
            snap->ps.psClientNum != cg_snap->ps.psClientNum;

        BG_PlayerStateToEntityState(&snap->ps, &cent->nextState, qfalse);

        if (resetPlayerState || cent->currentValid == 0 ||
            ((cent->nextState.eFlags ^ cent->currentState.eFlags) & EF_DOBJ_STATE_CHANGED) != 0) {
            memcpy(&cg_snap->ps, &snap->ps,
                   sizeof(playerState_t));
            CG_TransitionEntity(cent);

            /* 0x3003d009..0x3003d031 selects the reset branch from the
             * old cg_snap state before the REP MOVSD at 0x3003d08f. The
             * previous reconstruction repeated these comparisons after the
             * copy, when health and clientNum necessarily matched, and
             * therefore skipped CG_SnapshotTransitionStage2 on a spawn. */
            if (resetPlayerState) {
                CG_SnapshotTransitionStage2();
            } else {
                cg_predictedError[0] = 0.0f;
                cg_predictedError[1] = 0.0f;
                cg_predictedError[2] = 0.0f;
            }
        }
    } else if (cg_initialSnapshotPending != 0 ||
               spawnCountChanged ||
               snap->ps.psClientNum != cg_snap->ps.psClientNum) {
        memcpy(&cg_snap->ps, &snap->ps,
               sizeof(playerState_t));
        CG_SnapshotTransitionStage2();
    }

    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if (spawnCountChanged) {
        int32_t currentWeapon = cg_predictedPlayerState.currentWeapon;

        CG_ResetWeaponAnimTrees(&cg_predictedPlayerState);
        if (currentWeapon > 0 && currentWeapon <= bg_numWeapons) {
            cg_weaponInfos[currentWeapon].lastRunAnim = -1;
        }
    }

    for (int32_t i = 0; i < snap->numEntities; i++) {
        entityState_t *entity = &snap->entities[i];
        centity_t *cent = &cg_entities[entity->number];

        memcpy(&cent->nextState, entity, sizeof(*entity));
        if (cent->currentValid == 0 ||
            ((cent->nextState.eFlags ^ cent->currentState.eFlags) & EF_DOBJ_STATE_CHANGED) != 0) {
            CG_TransitionEntity(cent);
        }
    }

    for (int32_t i = 0; i < snap->numClients; i++) {
        centity_t *cent = &cg_entities[snap->clients[i].clientNum];
        if (cent->currentValid != 0) {
            const int32_t entityClientNum = cent->nextState.clientNum;
            /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered client-module boundary input and state before use. */
            if ((uint32_t)entityClientNum >= (uint32_t)MAX_CLIENTS) {
                Com_Error(ERR_DROP,
                          "\x15" "CG_SetNextSnap: "
                          "invalid entity client number %i",
                          entityClientNum);
                return;
            }
            intptr_t handle = cgame_syscall(CG_DOBJ_GET_HANDLE,
                                            entityClientNum);
            clientInfo_t *anim = &bgs.clientinfo[entityClientNum];
            CG_BuildCorpseDObjModels(
                anim,
                handle, &cent->nextState,
                cent->corpseTagState);
        }
    }

    CG_BuildSolidList();
    for (int32_t i = 0; i < snap->numEntities; i++) {
        CG_CheckPreEvents(&cg_entities[snap->entities[i].number]);
    }
}
