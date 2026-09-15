#ifndef CG_PREDICTED_EVENTS_H
#define CG_PREDICTED_EVENTS_H

#include "qcommon/player_state_types.h"

/* The engine retains this many commands for client prediction replay. */
enum { CG_PREDICTED_COMMAND_BACKUP = 128 };

/* NOT_FROM_ORIGINAL_SOURCE: FX groups are independent; state-only handlers do not consume FX history. */
enum {
    CODUOMP_PREDICTED_FX_SOUND = 1u << 0,
    CODUOMP_PREDICTED_FX_VISUAL = 1u << 1,
    CODUOMP_PREDICTED_FX_WEAPON_FIRE = 1u << 2
};

/* NOT_FROM_ORIGINAL_SOURCE: local event occurrence identities keep prediction replay from repeating FX. */
void coduomp_predicted_events_reset(void);
void coduomp_predicted_events_begin_prediction(int32_t currentCommandNumber);
void coduomp_predicted_events_begin_command(int32_t commandNumber, int32_t commandTime);
int32_t coduomp_predicted_events_set_source(int32_t entityNum);
void coduomp_predicted_events_end_command(void);
void coduomp_predicted_events_begin_dispatch(const playerState_t *ps, int32_t sequence);
void coduomp_predicted_events_end_dispatch(void);
qboolean coduomp_predicted_events_allow_fx(int32_t entityNum, int32_t event, uint32_t fxMask);

#endif
