#ifndef QCOMMON_Q_CHECKSUM_H
#define QCOMMON_Q_CHECKSUM_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

uint32_t Com_BlockChecksum(const void *buffer, int32_t length);
uint32_t Com_BlockChecksumKey(const void *buffer, int32_t length,
                              int32_t key);
uint32_t Com_HashKey(const char *text, int32_t length);

#ifdef __cplusplus
}
#endif

#endif
