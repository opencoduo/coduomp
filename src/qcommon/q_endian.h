#ifndef QCOMMON_Q_ENDIAN_H
#define QCOMMON_Q_ENDIAN_H

#include "q_shared_types.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int16_t BigShort(int16_t value);
int32_t BigLong(int32_t value);
qint64 BigLong64(qint64 value);
qint64 LittleLong64(qint64 value);
float BigFloat(float value);
int16_t ShortSwap(int16_t value);
int16_t ShortNoSwap(int16_t value);
int32_t LongSwap(int32_t value);
int32_t LongNoSwap(int32_t value);
qint64 Long64Swap(qint64 value);
qint64 Long64NoSwap(qint64 value);
float FloatSwap(float value);
float FloatNoSwap(float value);
void Swap_Init(void);

#ifdef __cplusplus
}
#endif

#endif
