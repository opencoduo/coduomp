#ifndef CODUO_SERVER_SURFACE_TYPES_H
#define CODUO_SERVER_SURFACE_TYPES_H

#include <stdint.h>

int32_t SurfaceTypeFromName(const char *name);
const char *SurfaceTypeToName(int32_t surfaceType);

#endif
