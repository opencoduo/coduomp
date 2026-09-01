#ifndef SHARED_SERVER_STARTUP_H
#define SHARED_SERVER_STARTUP_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void SV_CreateBaseline(void);
void SV_BoundMaxClients(int32_t minimumClientCount);
void SV_Startup(void);
void SV_ChangeMaxClients(void);
void SV_SetExpectedHunkUsage(const char *mapBspPath);
void SV_ClearServer(void);
void SV_InitCvar(void);
void SV_SpawnServer(const char *serverName);

#ifdef __cplusplus
}
#endif

#endif
