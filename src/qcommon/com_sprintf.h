#ifndef QCOMMON_COM_SPRINTF_H
#define QCOMMON_COM_SPRINTF_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int32_t Com_sprintf(char *destination, size_t destinationSize,
                    const char *format, ...);

#ifdef __cplusplus
}
#endif

#endif
