#ifndef CGAME_BG_PLAYER_STATE_SERVICES_H
#define CGAME_BG_PLAYER_STATE_SERVICES_H

#include "client/cgame/client_recovered.h"

/* NOT_FROM_ORIGINAL_SOURCE: the cgame projection owns the predicted entity
 * number, while the server projection receives an already-numbered entity. */
static inline void bg_compat_player_state_set_entity_number(
    entityState_t *es, const playerState_t *ps)
{
    es->number = (uint16_t)ps->psClientNum;
}

/* NOT_FROM_ORIGINAL_SOURCE: G_PlayerEvent is server-owned; the original cgame
 * bodies have no corresponding side effect. */
static inline void bg_compat_player_event(int32_t entityNum, int32_t event)
{
    (void)entityNum;
    (void)event;
}

#endif
