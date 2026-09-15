#ifndef CG_PREDICTED_FIRE_H
#define CG_PREDICTED_FIRE_H

#include "qcommon/player_state_types.h"

/* The engine retains this many commands for client prediction replay. */
enum { CG_PREDICTED_COMMAND_BACKUP = 128 };

/* NOT_FROM_ORIGINAL_SOURCE: client-only shot identities keep prediction replay from repeating fire presentation. */
void coduomp_predicted_fire_reset(void);
void coduomp_predicted_fire_begin_prediction(int32_t currentCommandNumber);
void coduomp_predicted_fire_begin_command(int32_t commandNumber, int32_t commandTime);
void coduomp_predicted_fire_end_command(void);
void coduomp_predicted_fire_begin_dispatch(const playerState_t *ps, int32_t sequence);
void coduomp_predicted_fire_end_dispatch(void);
qboolean coduomp_predicted_fire_should_present(int32_t entityNum, int32_t weapon, int32_t event, int32_t muzzleTagIndex);

#endif
