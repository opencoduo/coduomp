#ifndef CG_PREDICTED_EVENTS_H
#define CG_PREDICTED_EVENTS_H

#include <stdint.h>

/* The engine retains this many commands for client prediction replay. */
enum { CG_PREDICTED_COMMAND_BACKUP = 128 };

/* NOT_FROM_ORIGINAL_SOURCE: local event occurrence identities keep prediction replay from repeating weapon FX. */
void coduomp_predicted_events_reset(void);
void coduomp_predicted_events_begin_prediction(int32_t currentCommandNumber);
void coduomp_predicted_events_record_command(int32_t commandNumber, int32_t commandTime, int32_t firstSequence);

#endif
