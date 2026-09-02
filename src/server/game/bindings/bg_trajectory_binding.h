#ifndef GAME_BG_TRAJECTORY_BINDING_H
#define GAME_BG_TRAJECTORY_BINDING_H

#include "server/game/game_functions.h"

/* The Windows game module uses its game-error boundary, while the Linux game
 * module calls the imported Com_Error slot with ERR_DROP. */
#if defined(WINDOWS_BEHAVIOR)
#define BG_TRAJECTORY_ERROR(format, value) G_Error((format), (value))
#else
#define BG_TRAJECTORY_ERROR(format, value) Com_Error(ERR_DROP, (format), (value))
#endif

#endif
