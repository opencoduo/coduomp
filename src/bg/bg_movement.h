#ifndef BG_MOVEMENT_H
#define BG_MOVEMENT_H

#include "qcommon/player_state_types.h"

#include <stdint.h>

#if defined(WINDOWS_BEHAVIOR)
long double BG_GetSpeed(const playerState_t *ps, int32_t time);
#else
float BG_GetSpeed(const playerState_t *ps, int32_t time);
#endif

#endif
