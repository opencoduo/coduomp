#ifndef GAME_BG_WEAPON_BINDING_H
#define GAME_BG_WEAPON_BINDING_H

#include "server/game/game_functions.h"

/* NOT_FROM_ORIGINAL_SOURCE: local dependency boundary for the shared original
 * BG bodies.  Windows game parse errors call G_Error without an errorParm_t;
 * Linux game uses Com_Error(ERR_DROP, ...).  Both retained game modules emit
 * the same developer-only print lines. */
#define BG_WEAPON_PRINT(...) Com_Printf(__VA_ARGS__)
#if defined(WINDOWS_BEHAVIOR)
#define BG_WEAPON_ERROR(...) G_Error(__VA_ARGS__)
#else
#define BG_WEAPON_ERROR(...) Com_Error(ERR_DROP, __VA_ARGS__)
#endif
#define BG_WEAPON_DEBUG(...) Com_DPrintf(__VA_ARGS__)
#define BG_WEAPON_SHARED_AMMO_DEBUG(...) Com_DPrintf(__VA_ARGS__)

#endif
