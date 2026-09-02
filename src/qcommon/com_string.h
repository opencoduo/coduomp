#ifndef QCOMMON_COM_STRING_H
#define QCOMMON_COM_STRING_H

#include "q_shared_types.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int32_t Com_AddToString(const char *source, char *destination, int32_t offset, int32_t limit, qboolean addQuotes);

#ifdef __cplusplus
}
#endif

#endif
