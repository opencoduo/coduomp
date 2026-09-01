#ifndef QCOMMON_INFO_PRIVATE_H
#define QCOMMON_INFO_PRIVATE_H

#include <stdint.h>

#include "com_sprintf.h"
#include "q_string.h"

/* Original qcommon dependencies of the shared info-string subsystem. */
void Com_Error(int32_t level, const char *format, ...);
void Com_Printf(const char *format, ...);

/* Component-owned error routing for the original game-module variation. */
#include "info_error_binding.h"

#endif
