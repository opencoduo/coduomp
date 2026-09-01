#ifndef CODUOMP_COLLISION_MAP_LOAD_SERVICES_H
#define CODUOMP_COLLISION_MAP_LOAD_SERVICES_H

#include "client/engine/system_platform.h"

/* NOT_FROM_ORIGINAL_SOURCE: bind the shared terrain loader to the Win32
 * message-pump/yield sequence inlined in CoDUOMP.exe 0x0041d01b..0x0041d070. */
#define COLLISION_MAP_LOADING_KEEPALIVE() coduomp_loading_keepalive()

/* Diagnostic hooks are absent from every normal client build. */
#define COLLISION_MAP_LOAD_DIAGNOSTICS_BEGIN(mapName) ((void)(mapName))
#define COLLISION_MAP_LOAD_DIAGNOSTICS_END(mapName) ((void)(mapName))

#endif
