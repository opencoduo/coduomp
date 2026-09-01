#ifndef CGAME_BG_TRAJECTORY_BINDING_H
#define CGAME_BG_TRAJECTORY_BINDING_H

#include "client/cgame/client_recovered.h"

/* The original cgame trajectory bodies report invalid types through the
 * client engine's ERR_DROP boundary. */
#define BG_TRAJECTORY_ERROR(format, value) \
    Com_Error(ERR_DROP, (format), (value))

#endif
