#ifndef GAME_BG_PLAYER_STATE_SERVICES_H
#define GAME_BG_PLAYER_STATE_SERVICES_H

#include "server/game/recovered_game.h"
#include "server/game/game_functions.h"

/* NOT_FROM_ORIGINAL_SOURCE: the server entity number is assigned by entity
 * ownership code before the shared BG projection runs. */
static inline void bg_compat_player_state_set_entity_number(entityState_t *es, const playerState_t *ps)
{
    (void)es;
    (void)ps;
}

/* NOT_FROM_ORIGINAL_SOURCE: preserve the game-module event side effect at the
 * shared player-state projection boundary. */
static inline void bg_compat_player_event(int32_t entityNum, int32_t event)
{
    G_PlayerEvent(entityNum, event);
}

#endif
