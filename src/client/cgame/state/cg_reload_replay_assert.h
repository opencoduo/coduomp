#ifndef CG_RELOAD_REPLAY_ASSERT_H
#define CG_RELOAD_REPLAY_ASSERT_H

#include "qcommon/player_state_types.h"

/* NOT_FROM_ORIGINAL_SOURCE: temporary, intentionally active reload presentation assertion. Remove after diagnosis. */
void coduomp_reload_assert_reset(void);
void coduomp_reload_assert_begin_prediction(const playerState_t *oldState);
void coduomp_reload_assert_begin_command(int32_t commandNumber, int32_t commandTime);
void coduomp_reload_assert_end_command(void);
void coduomp_reload_assert_begin_dispatch(const playerState_t *ps, int32_t sequence);
void coduomp_reload_assert_end_dispatch(void);
void coduomp_reload_assert_sound(int32_t entityNum, const char *aliasName);

#endif
