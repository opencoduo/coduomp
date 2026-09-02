#ifndef QCOMMON_COM_REDIRECT_H
#define QCOMMON_COM_REDIRECT_H

#include "command_types.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

extern char *com_redirectBuffer;
extern int32_t com_redirectBufferSize;
extern com_redirect_flush_t com_redirectFlush;

void Com_BeginRedirect(char *buffer, int32_t bufferSize, com_redirect_flush_t flush);
void Com_EndRedirect(void);

#ifdef __cplusplus
}
#endif

#endif
