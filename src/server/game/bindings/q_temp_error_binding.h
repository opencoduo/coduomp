#ifndef GAME_Q_TEMP_ERROR_BINDING_H
#define GAME_Q_TEMP_ERROR_BINDING_H

#include "qcommon/q_shared_types.h"

/* The Windows game module routes va overflow through G_Error, while the Linux
 * game module uses the common Com_Error(ERR_DROP, ...) entry. */
#if defined(WINDOWS_BEHAVIOR)
void G_Error(const char *format, ...);
#define Q_TEMP_ERROR(message) G_Error((message))
#else
void Com_Error(errorParm_t code, const char *format, ...);
#define Q_TEMP_ERROR(message) Com_Error(ERR_DROP, (message))
#endif

#endif
