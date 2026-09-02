/*
 * Source reconstruction for the server per-frame update.
 *
 * Binary SHA-256:
 * 4d2391c10e234559ace294fb69d96741cad522653c361f13f626f4dc1891acb7
 */

#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include "recovered_game.h"
#include "g_public.h"
#include "game_globals.h"
#include "g_syscalls.h"
#include "game_functions.h"
#include "scr_vm.h"
#include "level_locals.h"
#include "bg_state.h"
#include "compat/crt/qsort_compat.h"

#define FRAME_MAX_CLIENTS 64
#define GENTITY_FLAG_SKIP_XANIM_UPDATE 0x4000u

/* 0x58da4 SendScoreboardMessageToAllIntermissionClients */
/* VERIFIED_DECOMPILER(0x58da4, 68da4_SendScoreboardMessageToAllIntermissionClients.c, VERIFY-FRAME-RANK-UPDATE-2026-06-17): DATAFLOW_VERIFIED - scoreboard-dirty guard, intermission client scan, scoreboard message argument, and dirty reset checked. */
void SendScoreboardMessageToAllIntermissionClients(void)
{
    level_locals_t *lvl = &level;
    int clientNum;

    if (lvl->scoreboardDirty == 0) {
        return;
    }

    for (clientNum = 0; clientNum < lvl->maxclients; clientNum++) {
        gclient_t *client = &level.clients[clientNum];

        if (client->connectedState == CON_CONNECTED && client->ps.pmType == PM_TYPE_INTERMISSION) {
            DeathmatchScoreboardMessage(&g_entities[clientNum]);
        }
    }

    lvl->scoreboardDirty = 0;
}

/* 0x58e44 SortRanks */
/* VERIFIED_DECOMPILER(0x58e44, 68e44_FUN_00068e44.c, VERIFY-FRAME-RANK-UPDATE-2026-06-17): DATAFLOW_VERIFIED - qsort comparator client-slot lookup, connecting/spectator ordering, score/death tie-breaks, and returns checked. */
static int SortRanks(const void *lhs, const void *rhs)
{
    int leftClientNum = *(const int *)lhs;
    int rightClientNum = *(const int *)rhs;
    const gclient_t *left = &level.clients[leftClientNum];
    const gclient_t *right = &level.clients[rightClientNum];

    /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if (left->connectedState == CON_CONNECTING) {
        return 1;
    }
    if (right->connectedState == CON_CONNECTING) {
        return -1;
    }
    if (left->sessionTeam == TEAM_SPECTATOR && right->sessionTeam == TEAM_SPECTATOR) {
        if (leftClientNum < rightClientNum) {
            return -1;
        }
        if (rightClientNum < leftClientNum) {
            return 1;
        }
        return 0;
    }
    if (left->sessionTeam == TEAM_SPECTATOR) {
        return 1;
    }
    if (right->sessionTeam == TEAM_SPECTATOR) {
        return -1;
    }
    if (right->score < left->score) {
        return -1;
    }
    if (left->score < right->score) {
        return 1;
    }
    if (left->deaths < right->deaths) {
        return -1;
    }
    if (right->deaths < left->deaths) {
        return 1;
    }
    return 0;
}

/* 0x58faf CalculateRanks */
/* VERIFIED_DECOMPILER(0x58faf, 68faf_CalculateRanks.c, VERIFY-FRAME-RANK-UPDATE-2026-06-17): DATAFLOW_VERIFIED - sorted-client rebuild, voting-client count, qsort element size/comparator, and scoreboard dirty flag checked. */
void CalculateRanks(void)
{
    level_locals_t *lvl = &level;
    int clientNum;

    lvl->sortedClientCount = 0;
    lvl->numVotingClients = 0;

    for (clientNum = 0; clientNum < lvl->maxclients; clientNum++) {
        gclient_t *client = &level.clients[clientNum];

        if (client->connectedState != CON_DISCONNECTED) {
            lvl->sortedClients[lvl->sortedClientCount] = clientNum;
            lvl->sortedClientCount++;

            if (client->sessionTeam != TEAM_SPECTATOR && client->connectedState == CON_CONNECTED) {
                lvl->numVotingClients++;
            }
        }
    }

    coduo_qsort(lvl->sortedClients, (size_t)lvl->sortedClientCount, sizeof(lvl->sortedClients[0]), SortRanks);
    lvl->scoreboardDirty = 1;
}

/* 0x590e1 ExitLevel */
/* VERIFIED_DECOMPILER(0x590e1, 690e1_ExitLevel.c, VERIFY-FRAME-RANK-UPDATE-2026-06-17): DATAFLOW_VERIFIED - map_rotate command, team-score reset, connected score reset, reconnect state, and log line checked. */
void ExitLevel(void)
{
    level_locals_t *lvl = &level;
    int clientNum;

    trap_SendConsoleCommand(2, "map_rotate\n");
    lvl->teamScoreAxis = 0;
    lvl->teamScoreAllies = 0;

    for (clientNum = 0; clientNum < g_maxclients.integer; clientNum++) {
        gclient_t *client = &level.clients[clientNum];

        if (client->connectedState == CON_CONNECTED) {
            client->score = 0;
        }
    }

    for (clientNum = 0; clientNum < g_maxclients.integer; clientNum++) {
        gclient_t *client = &level.clients[clientNum];

        if (client->connectedState == CON_CONNECTED) {
            client->connectedState = CON_CONNECTING;
        }
    }

    G_LogPrintf("ExitLevel: executed\n");
}

/* 0x59347 CheckVote */
/* VERIFIED_DECOMPILER(0x59347, 69347_CheckVote.c, VERIFY-FRAME-RANK-UPDATE-2026-06-17): DATAFLOW_VERIFIED - delayed vote execution, active-vote time test, pass/fail thresholds, voteExecuteTime store, and configstring clear checked. */
void CheckVote(void)
{
    level_locals_t *lvl = &level;
    int now;
    int passThreshold;

    if (lvl->voteExecuteTime != 0) {
        now = trap_Milliseconds();
        /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
        if (lvl->voteExecuteTime < now) {
            lvl->voteExecuteTime = 0;
            trap_SendConsoleCommand(2, va("%s\n", lvl->voteString));
        }
    }

    if (lvl->voteTime == 0) {
        return;
    }

    now = trap_Milliseconds();
    /* Stock 0x593d3..0x593e1 tests the sign of the wrapping dword
     * subtraction, rather than directly comparing the two signed times. */
    if (coduo_int32_from_bits((uint32_t)now - (uint32_t)lvl->voteTime) < 0) {
        passThreshold = lvl->numVotingClients / 2 + 1;
        if (lvl->voteYes < passThreshold) {
            if (lvl->voteNo <= lvl->numVotingClients - passThreshold) {
                return;
            }
            trap_SendServerCommand(SERVER_COMMAND_ALL_CLIENTS, 0, "e \"GAME_VOTEFAILED\"");
        } else {
            trap_SendServerCommand(SERVER_COMMAND_ALL_CLIENTS, 0, "e \"GAME_VOTEPASSED\"");
            lvl->voteExecuteTime = coduo_int32_from_bits((uint32_t)trap_Milliseconds() + UINT32_C(3000));
        }
    } else {
        trap_SendServerCommand(SERVER_COMMAND_ALL_CLIENTS, 0, "e \"GAME_VOTEFAILED\"");
    }

    lvl->voteTime = 0;
    trap_SetConfigstring(CS_VOTE_TIME, emptyString);
}

/* 0x594d4 CheckMatchTimeout */
/* VERIFIED_DECOMPILER(0x594d4, 694d4_CheckMatchTimeout.c, VERIFY-FRAME-RANK-UPDATE-2026-06-17): DATAFLOW_VERIFIED - recovery-end handling, timeout expiry time test, team bank debit, configstrings, and expiry broadcast checked. */
void CheckMatchTimeout(void)
{
    level_locals_t *lvl = &level;
    int now;

    if (lvl->matchTimeoutRecoveryEndTime != 0) {
        now = trap_Milliseconds();
        /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
        if (lvl->matchTimeoutRecoveryEndTime < now) {
            lvl->matchTimeoutRecoveryEndTime = 0;
            trap_SetConfigstring(CS_TIMEOUT_TIME, emptyString);
            trap_Cvar_Set("timescale", "1");
            return;
        }
    }

    if (lvl->matchTimeoutDuration == 0) {
        return;
    }

    now = trap_Milliseconds();
    /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if (coduo_int32_from_bits((uint32_t)lvl->matchTimeoutStartTime + (uint32_t)lvl->matchTimeoutDuration) <= now) {
        Com_sprintf(lvl->timeoutMessage, sizeof(lvl->timeoutMessage), "PATCH_1_5_TIMEOUT_ENDING\x15");
        if (lvl->matchTimeoutTeam == TEAM_ALLIES) {
            lvl->timeoutCache1 = coduo_int32_from_bits((uint32_t)lvl->timeoutCache1 - (uint32_t)lvl->matchTimeoutDuration);
        } else {
            lvl->timeoutCache2 = coduo_int32_from_bits((uint32_t)lvl->timeoutCache2 - (uint32_t)lvl->matchTimeoutDuration);
        }

        lvl->matchTimeoutDuration = 0;
        lvl->matchTimeoutRecoveryEndTime = coduo_int32_from_bits((uint32_t)trap_Milliseconds() + (uint32_t)g_timeoutRecovery.integer);
        trap_SetConfigstring(CS_TIMEOUT_TIME, va("%i", g_timeoutRecovery.integer));
        trap_SetConfigstring(CS_TIMEOUT_STRING, lvl->timeoutMessage);
        trap_SendServerCommand(SERVER_COMMAND_ALL_CLIENTS, 0, "e \"PATCH_1_5_TIMEOUT_EXPIRED\"");
    }
}

/* 0x596a4 G_UpdateObjectiveToClients */
/* VERIFIED_DECOMPILER(0x596a4, 696a4_G_UpdateObjectiveToClients.c, VERIFY-FRAME-RANK-UPDATE-2026-06-17): DATAFLOW_VERIFIED - linked-client scan, objective visibility clear/copy by state/team, and 16-entry copy offsets checked. */
void G_UpdateObjectiveToClients(void)
{
    level_locals_t *lvl = &level;
    int clientNum;
    int objectiveIndex;

    for (clientNum = 0; clientNum < lvl->maxclients; clientNum++) {
        gentity_t *ent = &lvl->gentities[clientNum];
        gclient_t *client;
        int team;

        if (ent->linked == 0) {
            continue;
        }

        client = ent->client;
        team = client->sessionTeam;
        for (objectiveIndex = 0; objectiveIndex < PLAYERSTATE_OBJECTIVE_COUNT; objectiveIndex++) {
            objective_t *objective = &lvl->objectives[objectiveIndex];

            if (objective->state == OBJECTIVE_STATE_EMPTY || (objective->teamNum != TEAM_FREE && objective->teamNum != team)) {
                client->ps.objectives[objectiveIndex].state = OBJECTIVE_STATE_EMPTY;
            } else {
                client->ps.objectives[objectiveIndex] = *objective;
            }
        }
    }
}

/* 0x597e4 G_UpdateHudElemsToClients */
/* VERIFIED_DECOMPILER(0x597e4, 697e4_G_UpdateHudElemsToClients.c, VERIFY-FRAME-RANK-UPDATE-2026-06-17): DATAFLOW_VERIFIED - linked-client scan and HudElem_UpdateClient client/entity/updateFlags argument order checked. */
void G_UpdateHudElemsToClients(void)
{
    level_locals_t *lvl = &level;
    int clientNum;

    for (clientNum = 0; clientNum < lvl->maxclients; clientNum++) {
        gentity_t *ent = &lvl->gentities[clientNum];

        if (ent->linked != 0) {
            HudElem_UpdateClient(ent->client, ent->s.number, 3);
        }
    }
}

/* 0x598d9 DebugDumpAnims */
/* VERIFIED_DECOMPILER(0x598d9, 698d9_DebugDumpAnims.c, VERIFY-FRAME-RANK-UPDATE-2026-06-17): DATAFLOW_VERIFIED - g_dumpAnims signed bounds check and entity-stride DObj display argument checked. */
void DebugDumpAnims(void)
{
    if (g_dumpAnims.integer >= 0 && g_dumpAnims.integer < (int)MAX_GENTITIES) {
        trap_DObjDisplayAnim(&level.gentities[g_dumpAnims.integer]);
    }
}

/* 0x5992c G_XAnimUpdateEnt */
/* VERIFIED_DECOMPILER(0x5992c, 6992c_G_XAnimUpdateEnt.c, VERIFY-FRAME-RANK-UPDATE-2026-06-17): DATAFLOW_VERIFIED - linked/non-scriptClass 0x4000 loop, G_DObjUpdateServerTime arguments, and script thread pump checked. */
void G_XAnimUpdateEnt(gentity_t *ent)
{
    while (ent->linked != 0 && (ent->flags & GENTITY_FLAG_SKIP_XANIM_UPDATE) == 0 && G_DObjUpdateServerTime(ent, 1) != 0) {
        Scr_RunCurrentThreads();
    }
}

/* 0x59983 G_XAnimUpdate */
/* VERIFIED_DECOMPILER(0x59983, 69983_G_XAnimUpdate.c, VERIFY-FRAME-RANK-UPDATE-2026-06-17): DATAFLOW_VERIFIED - linked entity server-time init pass, frameTime 0.001 scale, and full per-entity XAnim update pass checked. */
void G_XAnimUpdate(void)
{
    level_locals_t *lvl = &level;
    gentity_t *ent;
    int entityNum;

    ent = g_entities;
    for (entityNum = 0; entityNum < lvl->num_entities; entityNum++) {
        if (ent->linked != 0) {
            /* 0x599c7: bare fild of frameTime, no float32 rounding of the int. */
            trap_DObjInitServerTime(ent, (float)((long double)lvl->frameTime * (long double)0.001f));
        }
        ent = &ent[1];
    }

    ent = g_entities;
    for (entityNum = 0; entityNum < lvl->num_entities; entityNum++) {
        G_XAnimUpdateEnt(ent);
        ent = &ent[1];
    }
}

/* ------------------------------------------------------------------ */
/*  0x59c21  G_RunFrame                                               */
/* ------------------------------------------------------------------ */

/*
 * Server per-frame update.  Called from vmMain GAME_RUN_FRAME.
 *
 * param_1 = current level.time in milliseconds.
 *
 * This is the spine of the server simulation loop:
 *  1. Update timing globals
 *  2. Process entity notify-watch list (script entity-change triggers)
 *  3. Run pending script threads
 *  4. Update animations, check cvar-driven bounds/viewheight changes
 *  5. Run entity think functions
 *  6. Update objectives, hudelems, vehicles to clients
 *  7. Run client end-frame
 *  8. Check teams, votes, match timeout, scoreboard
 */
/* VERIFIED_DECOMPILER(0x59c21, 69c21_G_RunFrame.c, VERIFY-FRAME-RANK-UPDATE-2026-06-17): DATAFLOW_VERIFIED - timing update, notify-watch removal loop, script thread pump, animation/cvar refresh, entity frame dispatch, client end-frame, and final frame checks checked. */
void G_RunFrame(int levelTimeIn)
{
    char notified[MAX_GENTITIES];
    int hadMatch;
    int i;
    gentity_t *ent;
    int oldTime;
    uint8_t notifyPassMarker;
    int watchCount;
    level_trigger_notify_watch_entry_t *watch;
    gentity_t *ent1;
    gentity_t *ent2;
    int entNum1;
    level_locals_t *lvl = &level;
    level_trigger_notify_watch_entry_t *notifyWatchBase = lvl->triggerNotifyWatch;

    /* ---- Timing ---- */
    oldTime = lvl->time;
    lvl->framenum = coduo_int32_from_bits((uint32_t)lvl->framenum + UINT32_C(1)); /* level.framenum++ */
    lvl->previousTime = lvl->time; /* level.previousTime = old time */
    lvl->time = levelTimeIn; /* level.time = now */
    bg.frameTime = coduo_int32_from_bits((uint32_t)levelTimeIn - (uint32_t)oldTime); /* frame delta (msec) */
    bg.time = levelTimeIn;
    bg.levelFrameTime = levelTimeIn;
    lvl->frameTime = bg.frameTime; /* level.frameTime */

    G_UpdateCvars();

    /* ---- Entity notify-watch loop ---- */
    memset(notified, 0, sizeof(notified));
    hadMatch = 0;
    notifyPassMarker = 0;

    do {
        hadMatch = 0;
        notifyPassMarker++;
        watchCount = lvl->notifyWatchCount;

        for (i = 0; i < watchCount; i++) {
            watch = &notifyWatchBase[i];
            entNum1 = watch->entNum1;
            ent1 = &g_entities[entNum1];

            if (ent1->validationToken == watch->token1) {
                ent2 = &g_entities[watch->entNum2];
                if (ent2->validationToken != watch->token2) {
                    goto remove_entry;
                }
                /* Tokens still match — check if already notified this pass */
                if ((uint8_t)notified[entNum1] != notifyPassMarker) {
                    notified[entNum1] = (char)notifyPassMarker;
                    Scr_AddEntity(ent2);
                    Scr_Notify(ent1, scr_const_trigger, 1);
                    goto remove_entry;
                }
                hadMatch = 1;
            } else {
            remove_entry:
                /* Remove entry by swapping with last */
                lvl->notifyWatchCount = lvl->notifyWatchCount - 1;
                i = i - 1;
                watchCount = lvl->notifyWatchCount;
                *watch = notifyWatchBase[watchCount];
            }
        }

        Scr_RunCurrentThreads();

        if (hadMatch == 0) {
            lvl->notifyWatchCount = 0;

            /* ---- Animation update ---- */
            G_XAnimUpdate();
            Scr_SetTime(lvl->time);

            /* ---- Check bounds cvar changes ---- */
            /* VERIFIED_DECOMPILER(0x59c21, 69c21_G_RunFrame.c, VERIFY-FRAME-RANK-UPDATE-2026-06-17): DATAFLOW_VERIFIED - cached bounds slots compare against cvar.value floats, not integer. */
            if (lvl->cachedBoundsWidth != g_bounds_width.value || lvl->cachedBoundsHeightStanding != g_bounds_height_standing.value) {
                lvl->cachedBoundsWidth = g_bounds_width.value;
                lvl->cachedBoundsHeightStanding = g_bounds_height_standing.value;
                G_SetPlayerSize();

                ent = (gentity_t *)g_entities;
                for (i = 0; i < FRAME_MAX_CLIENTS; i++) {
                    if (ent->linked != 0) {
                        /* Copy new bounds to entity */
                        ent->mins[0] = playerMins[0];
                        ent->mins[1] = playerMins[1];
                        ent->mins[2] = playerMins[2];
                        ent->maxs[0] = playerMaxs[0];
                        ent->maxs[1] = playerMaxs[1];
                        ent->maxs[2] = playerMaxs[2];

                        /* Copy to client playerMins/playerMaxs/viewheights */
                        gclient_t *cl = ent->client;
                        cl->ps.playerMins[0] = ent->mins[0];
                        cl->ps.playerMins[1] = ent->mins[1];
                        cl->ps.playerMins[2] = ent->mins[2];
                        cl->ps.playerMaxs[0] = ent->maxs[0];
                        cl->ps.playerMaxs[1] = ent->maxs[1];
                        cl->ps.playerMaxs[2] = ent->maxs[2];
                        trap_LinkEntity(ent);
                    }
                    ent = &ent[1];
                }
            }

            /* ---- Check viewheight cvar changes ---- */
            /* VERIFIED_DECOMPILER(0x59c21, 69c21_G_RunFrame.c, VERIFY-FRAME-RANK-UPDATE-2026-06-17): DATAFLOW_VERIFIED - cached viewheight slots compare against cvar.value floats and client slots receive cvar.integer. */
            if (lvl->cachedViewheightStanding != bg_viewheight_standing.value ||
                lvl->cachedViewheightCrouched != bg_viewheight_crouched.value || lvl->cachedViewheightProne != bg_viewheight_prone.value) {
                lvl->cachedViewheightStanding = bg_viewheight_standing.value;
                lvl->cachedViewheightCrouched = bg_viewheight_crouched.value;
                lvl->cachedViewheightProne = bg_viewheight_prone.value;

                ent = g_entities;
                for (i = 0; i < FRAME_MAX_CLIENTS; i++) {
                    if (ent->linked != 0) {
                        gclient_t *cl = ent->client;
                        cl->ps.proneViewHeight = bg_viewheight_prone.integer;
                        cl->ps.crouchViewHeight = bg_viewheight_crouched.integer;
                        cl->ps.standViewHeight = bg_viewheight_standing.integer;
                    }
                    ent = &ent[1];
                }
            }

            /* ---- Entity think loop ---- */
            ent = g_entities;
            for (i = 0; i < lvl->num_entities; i++) {
                if (ent->linked != 0) {
                    if (ent->linkInfo != NULL) {
                        G_RunFrameForEntity(ent->linkInfo->parent);
                    }
                    G_RunFrameForEntity(ent);
                }
                ent = &ent[1];
            }

            /* ---- Post-entity updates ---- */
            G_UpdateObjectiveToClients();
            G_UpdateHudElemsToClients();
            G_VehicleClientThink();

            /* ---- Client end-frame loop ---- */
            ent = g_entities;
            for (i = 0; i < lvl->maxclients; i++) {
                if (ent->linked != 0) {
                    ClientEndFrame(ent);
                }
                ent = &ent[1];
            }

            /* ---- End-of-frame checks ---- */
            CheckTeamStatus();
            CheckVote();
            CheckMatchTimeout();
            SendScoreboardMessageToAllIntermissionClients();

            /* Debug entity listing */
            if (g_listEntity.integer != 0) {
                ent = g_entities;
                for (i = 0; i < (int)MAX_GENTITIES; i++) {
                    G_Printf("%4i: %s\n", i, SL_ConvertToString(ent->scriptClassname));
                    ent = &ent[1];
                }
                trap_Cvar_Set("g_listEntity", zeroString);
            }

            /* Save registered items if dirty */
            if (lvl->registeredItemsDirty != 0) {
                SaveRegisteredItems();
            }

            DebugDumpAnims();
            return;
        }
    } while (1);
}
