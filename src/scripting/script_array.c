#include "script_value.h"
#include "script_runtime_host.h"
#include "script_string.h"
#include "script_variable.h"

enum {
    SCRIPT_IMPORT_ERROR_PARAMETER_CONTAINER = 1
};

/* NOT_FROM_ORIGINAL_SOURCE: resolves the engine-backed key/value carrier used
 * at the start of both original indexed-mutation functions. */
static void ScriptImport_ResolveKeyValue(script_variable_node_t *node,
                                         VariableValue *resolved)
{
    ScriptRuntime_GetObjectFieldValue(
        (int32_t)GetVariableName(
            (uint16_t)(node - script_variableNodes)),
        node->payload.halves.valueOrRefCount,
        node->payload.halves.parentHandle, resolved);
}

/* Source: CoDUOMP.exe 0x00485910..0x00485c77.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00485910_00485c78.mcode. */
uint16_t EvalArrayRef(uint16_t handle,
                      VariableValue index[2])
{
    script_variable_node_t *node = &script_variableNodes[handle];
    script_variable_type_t type = GetVarType(handle);
    uintptr_t payload = node->payload.valuePayload;

    if (type == SCRIPT_VAR_UNDEFINED) {
        type = SCRIPT_VAR_OBJECT;
        payload = Scr_AllocArray();
        node->packedTypeIndex |= SCRIPT_VAR_OBJECT;
        node->payload.valuePayload = payload;
    } else {
        while (type == SCRIPT_VAR_KEY_VALUE) {
            VariableValue *resolved = index + 1;

            ScriptImport_ResolveKeyValue(node, resolved);
            type = resolved->type;
            payload = resolved->payload;
            RemoveRefToValue(resolved);
        }

        if (type != SCRIPT_VAR_OBJECT) {
            script_errorParameterIndex =
                SCRIPT_IMPORT_ERROR_PARAMETER_CONTAINER;
            if (type == SCRIPT_VAR_STRING) {
                Scr_Error(
                    "string characters cannot be individually changed");
                return 0;
            }
            if (type == SCRIPT_VAR_VECTOR) {
                Scr_Error(
                    "vector components cannot be individually changed");
                return 0;
            }
            Scr_Error(va("%s is not an array",
                         script_variableTypeNames[type]));
            return 0;
        }
    }

    uint16_t object = (uint16_t)payload;
    script_variable_type_t objectType = GetVarType(object);
    if (objectType != SCRIPT_VAR_ARRAY) {
        script_errorParameterIndex =
            SCRIPT_IMPORT_ERROR_PARAMETER_CONTAINER;
        Scr_Error(va("%s is not an array",
                     script_variableTypeNames[objectType]));
        return 0;
    }

    if (script_variableNodes[object]
            .payload.halves.valueOrRefCount != 0) {
        uint16_t oldObject = object;

        RemoveRefToObject(oldObject);
        object = Scr_AllocArray();
        CopyArray(oldObject, object);
        /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
        node->payload.valuePayload = object;
    }

    if (index->type == SCRIPT_VAR_INT) {
        int32_t intIndex = (int32_t)index->payload;

        if (IsValidArrayIndex(intIndex) == qfalse) {
            Scr_Error(va("array index %d out of range", intIndex));
            return 0;
        }
        return GetArrayVariable(object, intIndex);
    }

    if (index->type == SCRIPT_VAR_STRING) {
        uint16_t string = (uint16_t)index->payload;
        uint16_t child = GetVariable(object, string);

        SL_RemoveRefToString(string);
        return child;
    }

    Scr_Error(va("%s is not an array index",
                 script_variableTypeNames[index->type]));
    return 0;
}

/* Source: CoDUOMP.exe 0x00485ca0..0x00485fd2.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00485ca0_00485fd3.mcode. */
void ClearArray(uint16_t handle,
                VariableValue index[2])
{
    script_variable_node_t *node = &script_variableNodes[handle];
    script_variable_type_t type = GetVarType(handle);
    uintptr_t payload = node->payload.valuePayload;

    while (type == SCRIPT_VAR_KEY_VALUE) {
        VariableValue *resolved = index + 1;

        ScriptImport_ResolveKeyValue(node, resolved);
        type = resolved->type;
        payload = resolved->payload;
        RemoveRefToValue(resolved);
    }

    if (type != SCRIPT_VAR_OBJECT) {
        script_errorParameterIndex =
            SCRIPT_IMPORT_ERROR_PARAMETER_CONTAINER;
        Scr_Error(va("%s is not an array",
                     script_variableTypeNames[type]));
        return;
    }

    uint16_t object = (uint16_t)payload;
    script_variable_type_t objectType = GetVarType(object);
    if (objectType != SCRIPT_VAR_ARRAY) {
        script_errorParameterIndex =
            SCRIPT_IMPORT_ERROR_PARAMETER_CONTAINER;
        Scr_Error(va("%s is not an array",
                     script_variableTypeNames[objectType]));
        return;
    }

    if (script_variableNodes[object]
            .payload.halves.valueOrRefCount != 0) {
        uint16_t oldObject = object;

        RemoveRefToObject(oldObject);
        object = Scr_AllocArray();
        CopyArray(oldObject, object);
        /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
        node->payload.valuePayload = object;
    }

    if (index->type == SCRIPT_VAR_INT) {
        int32_t intIndex = (int32_t)index->payload;

        if (IsValidArrayIndex(intIndex) == qfalse) {
            Scr_Error(va("array index %d out of range", intIndex));
            return;
        }
        SafeRemoveArrayVariable(object, intIndex);
        return;
    }

    if (index->type == SCRIPT_VAR_STRING) {
        uint16_t string = (uint16_t)index->payload;

        SafeRemoveVariable(object, string);
        SL_RemoveRefToString(string);
        return;
    }

    Scr_Error(va("%s is not an array index",
                 script_variableTypeNames[index->type]));
}
