#ifndef CODUOMP_SERVER_GAME_LIFECYCLE_SERVICES_H
#define CODUOMP_SERVER_GAME_LIFECYCLE_SERVICES_H

#include "client/engine/system_event.h"

/* CoDUOMP.exe 0x0045eb77 and 0x0045eba1 call the client message pump around
 * GAME_INIT. Keep that client-only presentation dependency out of the shared
 * server lifecycle implementation. */
#define SERVER_GAME_LOADING_KEEPALIVE() Sys_PumpEvents()

#endif
