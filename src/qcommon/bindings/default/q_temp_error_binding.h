#ifndef QCOMMON_Q_TEMP_ERROR_BINDING_H
#define QCOMMON_Q_TEMP_ERROR_BINDING_H

#include "qcommon/q_shared_types.h"

void Com_Error(errorParm_t code, const char *format, ...);

#define Q_TEMP_ERROR(message) Com_Error(ERR_DROP, (message))

#endif
