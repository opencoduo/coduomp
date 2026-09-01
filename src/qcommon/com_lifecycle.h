#ifndef QCOMMON_COM_LIFECYCLE_H
#define QCOMMON_COM_LIFECYCLE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

extern int32_t com_consoleLogFile; /* Windows 0x00981e88; Linux 0x082396bc */

void Com_Shutdown(const char *finalMessage);
void Com_Close(void);

#ifdef __cplusplus
}
#endif

#endif
