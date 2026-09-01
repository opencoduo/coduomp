#ifndef CGAME_BG_WEAPON_BINDING_H
#define CGAME_BG_WEAPON_BINDING_H

#include "client/cgame/client_recovered.h"

/* NOT_FROM_ORIGINAL_SOURCE: local dependency boundary for the shared original
 * BG bodies.  Cgame has no shared-ammo developer print at this call site. */
#define BG_WEAPON_PRINT(...) Com_Printf(__VA_ARGS__)
#define BG_WEAPON_ERROR(...) Com_Error(ERR_DROP, __VA_ARGS__)
#define BG_WEAPON_DEBUG(...) Com_DPrintf(__VA_ARGS__)
#define BG_WEAPON_SHARED_AMMO_DEBUG(...) ((void)0)

#endif
