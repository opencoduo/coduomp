#include "script_runtime.h"

/* Source: CoDUOMP.exe 0x00488120..0x00488160.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00488120_00488161.mcode.
 * This distinct Windows entry has the same object-release operation as the
 * common variable subsystem, but no corresponding retained Linux entry has
 * yet been identified. */
void ScriptVariable_Release(uint16_t object)
{
    script_variable_node_t *node = &script_variableNodes[object];

    if (node->payload.halves.valueOrRefCount != 0) {
        --node->payload.halves.valueOrRefCount;
        return;
    }

    ++node->payload.halves.valueOrRefCount;
    ClearObjectInternal(object);
    RemoveRefToObject(object);
    FreeVariable(object);
}

/* Source: CoDUOMP.exe 0x004884d0..0x004884d2.
 * Name and return type: same-module Mac symbol Scr_GetNumScriptThreads. */
int32_t Scr_GetNumScriptThreads(void)
{
    return 0;
}
