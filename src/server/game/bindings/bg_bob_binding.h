#ifndef GAME_BG_BOB_BINDING_H
#define GAME_BG_BOB_BINDING_H

#include "qcommon/q_shared_types.h"

extern vmCvar_t bg_bobAmplitudeStanding;
extern vmCvar_t bg_bobAmplitudeDucked;
extern vmCvar_t bg_bobAmplitudeProne;

/* NOT_FROM_ORIGINAL_SOURCE: local cvar ownership edge for the shared original
 * bob-wave bodies. */
#define BG_BOB_AMPLITUDE_STANDING (bg_bobAmplitudeStanding.value)
#define BG_BOB_AMPLITUDE_DUCKED (bg_bobAmplitudeDucked.value)
#define BG_BOB_AMPLITUDE_PRONE (bg_bobAmplitudeProne.value)

#endif
