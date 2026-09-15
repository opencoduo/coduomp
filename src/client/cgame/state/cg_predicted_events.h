#ifndef CG_PREDICTED_EVENTS_H
#define CG_PREDICTED_EVENTS_H

#include "qcommon/player_state_types.h"

/* The engine retains this many commands for client prediction replay. */
enum { CG_PREDICTED_COMMAND_BACKUP = 128 };

/* NOT_FROM_ORIGINAL_SOURCE: local event occurrence identities keep prediction replay from repeating dispatch. */
void coduomp_predicted_events_reset(void);
void coduomp_predicted_events_begin_prediction(int32_t currentCommandNumber);
void coduomp_predicted_events_begin_command(int32_t commandNumber, int32_t commandTime);
int32_t coduomp_predicted_events_set_source(int32_t entityNum);
void coduomp_predicted_events_end_command(void);

#endif
