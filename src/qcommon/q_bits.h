#ifndef QCOMMON_Q_BITS_H
#define QCOMMON_Q_BITS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int32_t Com_BitCheck(const uint32_t *bits, int32_t bit);
void Com_BitSet(uint32_t *bits, int32_t bit);
void Com_BitClear(uint32_t *bits, int32_t bit);
int32_t FloatAsInt(float value);

#ifdef __cplusplus
}
#endif

#endif
