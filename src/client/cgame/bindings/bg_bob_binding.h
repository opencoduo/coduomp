#ifndef CGAME_BG_BOB_BINDING_H
#define CGAME_BG_BOB_BINDING_H

#include "qcommon/q_shared_types.h"

extern vmCvar_t cg_bobAmplitudeStanding_vmCvar;
extern vmCvar_t cg_bobAmplitudeDucked_vmCvar;
extern vmCvar_t cg_bobAmplitudeProne_vmCvar;

/* NOT_FROM_ORIGINAL_SOURCE: local cvar ownership edge for the shared original
 * bob-wave bodies. */
#define BG_BOB_AMPLITUDE_STANDING (cg_bobAmplitudeStanding_vmCvar.value)
#define BG_BOB_AMPLITUDE_DUCKED (cg_bobAmplitudeDucked_vmCvar.value)
#define BG_BOB_AMPLITUDE_PRONE (cg_bobAmplitudeProne_vmCvar.value)

#endif
