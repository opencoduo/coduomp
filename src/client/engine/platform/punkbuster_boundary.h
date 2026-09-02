#ifndef CODUOMP_PUNKBUSTER_BOUNDARY_H
#define CODUOMP_PUNKBUSTER_BOUNDARY_H

#include "../q_shared.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void PB_InvokeEventCallback(const char *address, const uint8_t *packetData);
void PB_CallServerSaCommandDrain(void);
void PB_InitializeClient(void *applicationInstance);
void PB_InitializeServer(void);
void PB_ProcessClientEvents(void);
void PB_ProcessServerEvents(void);
void PB_SetClientEnabled(qboolean enabled);
typedef enum pbClientConnectingEvent_e {
    PB_CLIENT_CONNECTING_CHALLENGE = 1,
    PB_CLIENT_CONNECTING_REQUEST = 2
} pbClientConnectingEvent_t;
void PbClientConnecting(pbClientConnectingEvent_t event, char *packet, int32_t *packetLength);
qboolean PB_ClientTrapConsole(const char *text);
void PB_DispatchClientConsoleCommand(const char *text);
void PB_DispatchServerConsoleCommand(const char *text);
void PbClientCompleteCommand(char *command, int32_t commandCapacity);
void PbServerCompleteCommand(char *command, int32_t commandCapacity);
void PbMsgToScreen(const char *prefix, const char *message);
void set_sv_punkbuster(const char *value);

#ifdef __cplusplus
}
#endif

#endif
