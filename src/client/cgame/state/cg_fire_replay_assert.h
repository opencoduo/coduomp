#ifndef CG_FIRE_REPLAY_ASSERT_H
#define CG_FIRE_REPLAY_ASSERT_H

#include "qcommon/player_state_types.h"

/* NOT_FROM_ORIGINAL_SOURCE: temporary, intentionally active fire presentation assertion. Remove after diagnosis. */
void coduomp_fire_assert_reset(void);
void coduomp_fire_assert_begin_prediction(const playerState_t *oldState);
void coduomp_fire_assert_begin_command(int32_t commandNumber, int32_t commandTime);
void coduomp_fire_assert_end_command(void);
void coduomp_fire_assert_begin_dispatch(const playerState_t *ps, int32_t sequence);
void coduomp_fire_assert_end_dispatch(void);
void coduomp_fire_assert_present(int32_t entityNum, int32_t weapon, int32_t event, int32_t muzzleTagIndex);

#endif
