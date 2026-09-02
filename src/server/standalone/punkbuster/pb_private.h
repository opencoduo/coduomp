#ifndef CODUO_PB_PRIVATE_H
#define CODUO_PB_PRIVATE_H

#include <stddef.h>
#include <stdint.h>

#include "server/standalone/bindings/coduo_engine_structs.h"

void PB_CallServerSbGlobal(int32_t opcode, int32_t clientNum, uint32_t length, const char *text);
void PB_StartServer(void);
void PB_RunServerFrame(void);
void PB_CallServerSaCommandDrain(void);
void PB_InvokeEventCallback(const char *text, const uint8_t *packetData);
const char *PB_InvokeStringQueryCallback(const char *text, intptr_t arg1, const char *arg2);
void PB_NotifyServerEnabled(void);
void PB_NotifyServerDisabled(void);
void PB_Print(const char *text, int32_t severity);

#endif
