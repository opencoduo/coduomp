/*
 * Source reconstruction for player death handling.
 *
 * Binary SHA-256:
 * 4d2391c10e234559ace294fb69d96741cad522653c361f13f626f4dc1891acb7
 */

#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include "recovered_game.h"
#include "g_public.h"
#include "game_globals.h"
#include "g_syscalls.h"
#include "game_functions.h"
#include "scr_vm.h"
#include "compat/coduo_x87emu.h" /* defines EMULATE_X87; x87 shim when it is 1 */

#define PLAYER_DIE_STUCK_CONTENTS 0x04000000u
#define PLAYER_DIE_TURRET_PS_FLAGS 0x00006000u
/* ------------------------------------------------------------------ */
/*  0x4f972  player_die                                               */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x4f972, 5f972_player_die.c, VERIFY-FIRST-FUNCTIONS-2026-06-17): DATAFLOW_VERIFIED; pmType gate, turret/vehicle attacker resolution, death notify, spectator inactivity reset, weapon attribution, grenade random denominator/origin/direction, death anim/event, kill notify, spectator score loop, death contents/angles, unlink/link, health clear, and die callback clear checked against current decompiler output.
 *
 * Handle player death.
 *
 * This function is called when a player's health drops below 1. It:
 *  1. Notifies script system of death
 *  2. Resolves attacker (handles vehicle kills)
 *  3. Launches grenade if player had grenade launcher
 *  4. Plays death animation
 *  5. Adds death event
 *  6. Notifies script system of kill
 *  7. Updates score for spectators watching the player
 *  8. Sets entity state for death
 *  9. Calls LookAtKiller to set death yaw
 * 10. Unlinks and relinks entity
 *
 * RECOVERED(UO-GAME-UNK-0208): This function has complex vehicle kill
 * resolution logic for collision kills and vehicle/turret weapon attribution.
 */
void player_die(gentity_t *self, gentity_t *inflictor, gentity_t *attacker, int damage, int mod, int weapon, const float *dir,
                int hitLocation)
{
    gclient_t *client = self->client;
    int i;
    vec3_t grenadeDir;
    vec3_t grenadeStart;

    /* Check if player is in a valid state to die */
    if (client->ps.pmType < PM_TYPE_DEAD) {
        /* Resolve turret/vehicle attackers to their controlling player. */
        if ((attacker->s.eType == ET_TURRET || attacker->s.eType == ET_VEHICLE) && attacker->passEntityNum != ENTITYNUM_NONE) {
            attacker = &g_entities[attacker->passEntityNum];
        }

        /* Notify script system of death */
        Scr_AddEntity(attacker);
        Scr_Notify(self, scr_const_death, 1);

        /* Reset the spectator inactivity timer after the death notification. */
        self->client->spectatorInactivityTime = -1;

        if (weapon != 0 && attacker->client != NULL) {
            if ((attacker->client->ps.entityStateFlags & PLAYER_DIE_TURRET_PS_FLAGS) != 0) {
                gentity_t *turret = &g_entities[attacker->s.vehicleEntityNum];

                if (turret->s.eType == ET_TURRET) {
                    weapon = turret->s.weapon;
                }
            }
        }

        /* Store attacker in entity */
        self->attacker = attacker;

        /* Launch grenade if player had grenade launcher */
        if (self->client->ps.grenadeTimeLeft != 0) {
            /* Generate random grenade direction */
            grenadeDir[0] = (float)coduo_server_rand_signed_unit();
            grenadeDir[1] = (float)coduo_server_rand_signed_unit();
            grenadeDir[0] *= 160.0f;
            grenadeDir[1] *= 160.0f;
            grenadeDir[2] = (float)coduo_server_rand_unit();
            grenadeDir[2] *= 160.0f;

            /* Set grenade start position from entity currentOrigin */
            grenadeStart[0] = self->currentOrigin[0];
            grenadeStart[1] = self->currentOrigin[1];
            grenadeStart[2] = self->currentOrigin[2] + 40.0f;

            /* Grenade damage uses the entity-state weapon slot. */
            fire_grenade(self, grenadeStart, grenadeDir, self->s.weapon);
        }

        /* Play death animation */
        BG_AnimScriptEvent(&self->client->ps, ANIM_EVENT_DEATH, qfalse, qtrue);

        /* Add death event */
        G_AddEvent(self, EV_DEATH, 0);

        /* Notify script system of kill */
        Scr_PlayerKilled(self, inflictor, attacker, damage, mod, weapon, dir, hitLocation);

        /* Update score for spectators watching this player */
        for (i = 0; i < level.maxclients; i++) {
            gclient_t *otherClient = &level.clients[i];
            if (otherClient->connectedState == 2 && otherClient->sessionState == 2 && otherClient->archiveClient == self->s.number) {
                Cmd_Score_f(&g_entities[i]);
            }
        }

        self->takeDamage = 1;

        /* Set death collision contents and clear roll before looking at the killer. */
        self->scriptContents = PLAYER_DIE_STUCK_CONTENTS;
        self->currentAngles[2] = 0.0f;

        /* Look at killer */
        LookAtKiller(self, inflictor, attacker);

        /* Copy death angles from entity to client */
        self->client->ps.viewAngles[0] = self->currentAngles[0];
        self->client->ps.viewAngles[1] = self->currentAngles[1];
        self->client->ps.viewAngles[2] = self->currentAngles[2];

        /* Clear client sound field */
        self->s.clientSound = 0;

        /* Unlink entity */
        trap_UnlinkEntity(self);

        /* Set death-related bounding box adjustment */
        self->maxs[2] = 30.0f;

        /* Relink entity */
        trap_LinkEntity(self);

        /* Clear health */
        self->health = 0;

        /* Clear die callback */
        self->die = NULL;

        /* Relink entity again */
        trap_LinkEntity(self);
    }
}
