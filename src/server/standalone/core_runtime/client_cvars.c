#include "core_runtime_private.h"
#include "filesystem/filesystem.h"

cvar_t *cl_shownet;

void CL_Init(void)
{
    cl_shownet = Cvar_Get("cl_shownet", "0", CVAR_TEMP);
    cl_language = Cvar_Get("cl_language", "0", CVAR_ARCHIVE);
}
