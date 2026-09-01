#ifndef QCOMMON_COM_FRAME_H
#define QCOMMON_COM_FRAME_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int32_t Com_ClampMsec(int32_t msec);
int32_t Com_ModifyMsec(int32_t msec);

#ifdef __cplusplus
}
#endif

#endif
