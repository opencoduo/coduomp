#include "server/standalone/bindings/coduo_engine_structs.h"
#include "../animation/dobj_private.h"
#include "../animation/xanim_private.h"
#include "../scripting/script_runtime_private.h"
#include "core_runtime_private.h"

void Com_InitServerRuntimePools(void)
{
    Com_ShutdownDObj();
    DObjShutdown();
    XAnimShutdown();
    Scr_Shutdown();
    Com_InitScriptRuntime();
    XAnimInit();
    DObjInit();
    Com_InitDObj();
}
