#include "script_runtime_host.h"
#include "script_runtime_state.h"
#include "script_value.h"
#include "script_variable.h"

/* Source: CoDUOMP.exe 0x00488360..0x004883fe.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00488360_004883ff.mcode. */
void ScriptRuntime_ResetValueRuntime(void)
{
    script_valueStackLimit = &script_valueStack[SCRIPT_VALUE_STACK_COUNT - 1];
    script_valueStackTop = &script_valueStack[0];
    script_callStackDepth = 0;
    script_errorMessage = NULL;
    script_errorSource = NULL;
    script_errorParameterIndex = 0;
    script_forceErrorReport = qfalse;
    script_parameterCount = 0;
    script_valueStackDepth = 0;

    /* RECOVERY_CORRECTION: coduo_lnxded 0x080aa7c9 calls AllocValue at
     * 0x080a69fa. CoDUOMP.exe 0x004883b7..0x004883d9 inlines the same
     * occupied-value allocation. The former Linux Scr_AllocArray call
     * incorrectly created an object and changed the temporary slot's type. */
    script_tempValueHandle = AllocValue();

    script_timeArrayHandle = 0;
    script_pauseArrayHandle = 0;
    script_levelHandle = 0;
    script_animArrayHandle = 0;
    script_gameHandle = 0;
    script_valueStack[0].type = SCRIPT_VAR_CODEPOS;
    script_loopWatchdogWarningFlag = 0;
}

enum {
    SCRIPT_VARIABLE_TYPE_MASK = 0x1f,
    SCRIPT_VARIABLE_NAME_SHIFT = 8
};

/* Source: CoDUOMP.exe 0x00488170..0x00488278.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00488170_00488279.mcode. */
qboolean ScriptRuntime_PruneGameVariableArray(uint16_t handle)
{
    script_variable_node_t *array = &script_variableNodes[handle];

    if ((array->packedTypeIndex & SCRIPT_VARIABLE_TYPE_MASK) != SCRIPT_VAR_ARRAY) {
        return qfalse;
    }

    for (;;) {
        uint16_t child = FindNextSibling(handle);

        for (;;) {
            if (child == 0) {
                return qtrue;
            }

            script_variable_node_t *childNode = &script_variableNodes[child];
            script_variable_type_t childType = (script_variable_type_t)(childNode->packedTypeIndex & SCRIPT_VARIABLE_TYPE_MASK);
            uint32_t childName = childNode->packedTypeIndex >> SCRIPT_VARIABLE_NAME_SHIFT;

            switch (childType) {
            case SCRIPT_VAR_CODEPOS:
            case SCRIPT_VAR_FUNCTION:
            case SCRIPT_VAR_STACK:
            case SCRIPT_VAR_ANIMATION:
                RemoveVariable(handle, childName);
                break;

            case SCRIPT_VAR_OBJECT:
                if (ScriptRuntime_PruneGameVariableArray(childNode->payload.halves.valueOrRefCount) == qfalse) {
                    RemoveVariable(handle, childName);
                    break;
                }

                child = FindNextSibling(child);
                continue;

            default:
                child = FindNextSibling(child);
                continue;
            }

            break;
        }
    }
}

/* Source: CoDUOMP.exe 0x00490bd0..0x00490cc5.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00490bd0_00490cc6.mcode. */
void ScriptRuntime_SetObjectFieldValue(int32_t classNum, int32_t objectNum, int32_t fieldIndex, VariableValue *value)
{
    script_parameterCount = 1;
    script_valueStackTop = value;
    Scr_SetObjectField(classNum, objectNum, fieldIndex);

    while (script_parameterCount != 0) {
        RemoveRefToValue(script_valueStackTop);
        script_valueStackTop--;
        script_parameterCount--;
    }
}

/* Source: CoDUOMP.exe 0x00490cf0..0x00490d1e.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00490cf0_00490d1f.mcode. */
void ScriptRuntime_GetObjectFieldValue(int32_t classNum, int32_t objectNum, int32_t fieldIndex, VariableValue *value)
{
    script_valueStackTop = value - 1;
    value->type = SCRIPT_VAR_UNDEFINED;
    Scr_GetObjectField(classNum, objectNum, fieldIndex);
    script_valueStackDepth = 0;
}

/* Source: CoDUOMP.exe 0x004882e0..0x0048831a and coduo_lnxded
 * 0x080aa6a6..0x080aa6f9.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004882e0_0048831b.mcode. */
void Scr_FreeGameVariable(qboolean shutdown)
{
    if (shutdown == qfalse) {
        ScriptRuntime_PruneGameVariableArray(script_variableNodes[script_animArrayHandle].payload.halves.valueOrRefCount);
        return;
    }

    FreeValue(script_animArrayHandle);
    script_animArrayHandle = 0;
}

/* Source: CoDUOMP.exe 0x00488400..0x0048844c and coduo_lnxded
 * 0x080aa818..0x080aa870.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00488400_0048844d.mcode.
 * Name and signature: the game-module VM callback ABI names this Scr_Init;
 * the same source identity is independently present in the Mac engine. The
 * five post-reset stores address the same load state and animation-tree root
 * in both authoritative targets. */
void Scr_Init(int32_t debugReport, int32_t developerScript, int32_t developer)
{
    script_runtimeDebugReportFlag = debugReport;
    script_runtimeDeveloperScriptFlag = developerScript;
    script_runtimeDeveloperFlag = developer;
    Var_Init();
    ScriptRuntime_ResetValueRuntime();
    script_loadScriptsActive = qfalse;
    script_loadAnimTreesActive = qfalse;
    script_loadScriptHandleRoot = 0;
    script_loadScriptCodeRoot = 0;
    script_animTreeRoot = 0;
    script_runtimeActive = qtrue;
}

/* Source: CoDUOMP.exe 0x00488450..0x00488460 and coduo_lnxded
 * 0x080aa872..0x080aa88e.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00488450_00488461.mcode.
 * Name: game-module VM callback Scr_Shutdown and same-version Mac symbol. */
void Scr_Shutdown(void)
{
    if (script_runtimeActive != qfalse) {
        script_runtimeActive = qfalse;
    }
}

/* Source: CoDUOMP.exe 0x00488470..0x0048847d and coduo_lnxded
 * 0x080aa890..0x080aa8a3.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00488470_0048847e.mcode.
 * Name: game-module VM callback Scr_Abort and same-version Mac symbol. */
void Scr_Abort(void)
{
    script_timeArrayHandle = 0;
    script_runtimeActive = qfalse;
}

/* Source: CoDUOMP.exe 0x00488480..0x00488489 and coduo_lnxded
 * 0x080aa8a4..0x080aa8b1.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00488480_0048848a.mcode.
 * Name and signature: game-module VM callback Scr_SetLoading. */
void Scr_SetLoading(int32_t enabled)
{
    script_loopWatchdogWarningFlag = enabled;
}

/* Source: CoDUOMP.exe 0x004882a0..0x004882dd and coduo_lnxded
 * 0x080aa678..0x080aa6a5.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004882a0_004882de.mcode.
 * The exact callback name says "GameVariable"; the allocated value is the
 * special `anim` array consumed by VM opcodes 0x0c and 0x0f. */
void Scr_AllocGameVariable(void)
{
    if (script_animArrayHandle == 0) {
        script_animArrayHandle = AllocValue();
        SetEmptyArray(script_animArrayHandle);
    }
}

/* Source: CoDUOMP.exe 0x00488320..0x00488344 and coduo_lnxded
 * 0x080aa6fa..0x080aa72a.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00488320_00488345.mcode. */
void Scr_GetChecksum(uint32_t checksum[3])
{
    checksum[0] = script_sourceChecksum;
    checksum[1] = script_sourceBufferOffset;
    checksum[2] = (uint32_t)((uintptr_t)script_sourceBufferEnd - (uintptr_t)script_sourceBufferStart);
}
