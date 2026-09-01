#ifndef SHARED_SERVER_XMODEL_H
#define SHARED_SERVER_XMODEL_H

#include "qcommon/xmodel_types.h"

#ifdef __cplusplus
extern "C" {
#endif

XModel *SV_XModelGet(const char *name);

#ifdef __cplusplus
}
#endif

#endif
