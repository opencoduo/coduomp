#ifndef QCOMMON_COM_TIME_H
#define QCOMMON_COM_TIME_H

#include "qtime_types.h"

#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

time_t Com_RealTime(qtime_t *qtime);

#ifdef __cplusplus
}
#endif

#endif
