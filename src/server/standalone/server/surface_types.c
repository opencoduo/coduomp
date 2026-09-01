#include "surface_types.h"

static const char surfaceTypeDefaultName[] = "default";

int32_t SurfaceTypeFromName(const char *name)
{
    (void)name;
    return -1;
}

const char *SurfaceTypeToName(int32_t surfaceType)
{
    (void)surfaceType;
    return surfaceTypeDefaultName;
}
