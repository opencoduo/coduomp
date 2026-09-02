#include "script_value.h"
#include "script_runtime_host.h"
#include "script_memory.h"
#include "script_string.h"
#include "script_variable.h"
#include "compat/coduo_fp_conversion.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#if defined(_MSC_VER)
#include <malloc.h>
#define SCRIPT_ALLOCA(size) _alloca(size)
#else
#define SCRIPT_ALLOCA(size) __builtin_alloca(size)
#endif

enum {
    SCRIPT_TIME_MASK = 0x00ffffff,
    SCRIPT_FLOAT_ABS_MASK = 0x7fffffff,
    SCRIPT_VARIABLE_TYPE_MASK = 31,
    SCRIPT_STRING_RUNTIME_TYPE = 14,
    SCRIPT_VECTOR_COMPONENT_COUNT = 3,
    SCRIPT_SINGLE_CHAR_STRING_SIZE = 2
};

static const float script_floatEqualityEpsilon =
    9.9999999747524270788e-07f; /* 0x358637bd, approximately 1e-6 */

/* Source: CoDUOMP.exe 0x00490e70..0x00490f24.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00490e70_00490f25.mcode. */
uint16_t VM_ConcatenateStrings(const VariableValue values[2])
{
    const char *left =
        SL_ConvertToString((uint16_t)values[0].payload);
    const char *right =
        SL_ConvertToString((uint16_t)values[1].payload);
    size_t leftLength = strlen(left);
    size_t rightLength = strlen(right);
    size_t combinedLength = leftLength + rightLength + 1;
    char *combined = SCRIPT_ALLOCA(combinedLength);

    memcpy(combined, left, leftLength);
    memcpy(combined + leftLength, right, rightLength + 1);

    return SL_GetStringOfLen(combined, 0, combinedLength,
                                    SCRIPT_STRING_RUNTIME_TYPE);
}

/* Source: CoDUOMP.exe 0x00484b90..0x00484c35.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00484b90_00484c36.mcode. */
void GetSizeValue(VariableValue *value)
{
    if (value->type == SCRIPT_VAR_OBJECT) {
        uint16_t object = (uint16_t)value->payload;

        value->type = SCRIPT_VAR_INT;
        value->payload =
            GetVarType(object) == SCRIPT_VAR_ARRAY
                ? script_variableNodes[object]
                      .payload.halves.parentHandle
                : 1;
        RemoveRefToObject(object);
        return;
    }

    if (value->type == SCRIPT_VAR_STRING) {
        uint16_t string = (uint16_t)value->payload;

        value->type = SCRIPT_VAR_INT;
        value->payload = strlen(SL_ConvertToString(string));
        SL_RemoveRefToString(string);
        return;
    }

    Scr_Error(va("size cannot be applied to %s",
                 script_variableTypeNames[value->type]));
}

/* Source: CoDUOMP.exe 0x00484da0..0x00484e60.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00484da0_00484e61.mcode. */
uint16_t CastFieldObject(VariableValue *value)
{
    if (value->type == SCRIPT_VAR_OBJECT) {
        uint16_t object = (uint16_t)value->payload;

        if (IsFieldObject(object) != qfalse) {
            VariableValue objectValue = {
                .payload = object,
                .type = SCRIPT_VAR_OBJECT
            };

            SetVariableValue(script_tempValueHandle,
                                    &objectValue);
            return object;
        }

        Scr_Error(va("%s is not an object",
                     script_variableTypeNames[
                         GetVarType(object)]));
    }

    Scr_Error(va("%s is not an object",
                 script_variableTypeNames[value->type]));
    return 0;
}

/* Source: CoDUOMP.exe 0x004853c0..0x00485624.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004853c0_00485625.mcode. */
void EvalArray(VariableValue *index,
                               VariableValue *container)
{
    if (container->type == SCRIPT_VAR_STRING) {
        if (index->type != SCRIPT_VAR_INT) {
            Scr_Error(va("%s is not a string index",
                         script_variableTypeNames[index->type]));
            return;
        }

        int32_t stringIndex = (int32_t)index->payload;
        uint16_t stringHandle = (uint16_t)container->payload;
        const char *string = SL_ConvertToString(stringHandle);
        if (stringIndex >= 0 &&
            (size_t)stringIndex < strlen(string)) {
            char singleChar[SCRIPT_SINGLE_CHAR_STRING_SIZE];

            singleChar[0] = string[stringIndex];
            singleChar[1] = '\0';
            index->type = SCRIPT_VAR_STRING;
            index->payload = SL_GetStringOfLen(
                singleChar, 0, sizeof(singleChar),
                SCRIPT_STRING_RUNTIME_TYPE);
            SL_RemoveRefToString(stringHandle);
            return;
        }

        Scr_Error(va("string index %d out of range", stringIndex));
        return;
    }

    if (container->type == SCRIPT_VAR_VECTOR) {
        if (index->type != SCRIPT_VAR_INT) {
            Scr_Error(va("%s is not a vector index",
                         script_variableTypeNames[index->type]));
            return;
        }

        uint32_t component = (uint32_t)index->payload;
        if (component < SCRIPT_VECTOR_COMPONENT_COUNT) {
            const float *vector =
                (const float *)container->payload;

            index->payload = 0;
            memcpy(&index->payload, &vector[component], sizeof(float));
            index->type = SCRIPT_VAR_FLOAT;
            RemoveRefToVector((const float *)container->payload);
            return;
        }

        Scr_Error(va("vector index %d out of range",
                     (int32_t)index->payload));
        return;
    }

    if (container->type != SCRIPT_VAR_OBJECT) {
        script_errorParameterIndex = 1;
        Scr_Error(va("%s is not an array, string, or vector",
                     script_variableTypeNames[container->type]));
        return;
    }

    uint16_t array = (uint16_t)container->payload;
    if (GetVarType(array) != SCRIPT_VAR_ARRAY) {
        script_errorParameterIndex = 1;
        Scr_Error(va("%s is not an array",
                     script_variableTypeNames[
                         GetVarType(array)]));
        return;
    }

    uint16_t child;
    if (index->type == SCRIPT_VAR_STRING) {
        uint16_t stringHandle = (uint16_t)index->payload;

        child = FindVariable(array, stringHandle);
        SL_RemoveRefToString(stringHandle);
    } else if (index->type == SCRIPT_VAR_INT) {
        int32_t intIndex = (int32_t)index->payload;

        if (IsValidArrayIndex(intIndex) == qfalse) {
            Scr_Error(va("array index %d out of range", intIndex));
            return;
        }
        child = FindArrayVariable(array, intIndex);
    } else {
        Scr_Error(va("%s is not an array index",
                     script_variableTypeNames[index->type]));
        return;
    }

    if (child == 0) {
        RemoveRefToObject(array);
        index->type = SCRIPT_VAR_UNDEFINED;
        return;
    }

    GetVariableValue(child, index);
    AddRefToValueOfType(index->type, index->u);
    RemoveRefToObject(array);
}

/* Source: CoDUOMP.exe 0x00485630..0x004858ec.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00485630_004858ed.mcode.
 * Name and signature: same-module Mac symbol
 * FindArrayVariableByValue(unsigned short, VariableValue *). The second value slot is
 * scratch storage used only while resolving an engine-backed key/value
 * carrier; values[0] remains the requested array index. */
uint16_t FindArrayVariableByValue(uint16_t handle, VariableValue values[2])
{
    script_variable_node_t *node = &script_variableNodes[handle];
    script_variable_type_t type = GetVarType(handle);
    uintptr_t payload = node->payload.valuePayload;

    while (type == SCRIPT_VAR_KEY_VALUE) {
        VariableValue *resolved = &values[1];

        ScriptRuntime_GetObjectFieldValue(
            (int32_t)GetVariableName(handle),
            node->payload.halves.valueOrRefCount,
            node->payload.halves.parentHandle, resolved);
        type = resolved->type;
        payload = resolved->payload;
        RemoveRefToValue(resolved);
    }

    if (type != SCRIPT_VAR_OBJECT) {
        script_errorParameterIndex = 1;
        Scr_Error(va("%s is not an array",
                     script_variableTypeNames[type]));
        return 0;
    }

    uint16_t array = (uint16_t)payload;
    script_variable_type_t objectType =
        GetVarType(array);
    if (objectType != SCRIPT_VAR_ARRAY) {
        script_errorParameterIndex = 1;
        Scr_Error(va("%s is not an array",
                     script_variableTypeNames[objectType]));
        return 0;
    }

    uint16_t child = 0;
    if (values[0].type == SCRIPT_VAR_INT) {
        int32_t intIndex = (int32_t)values[0].payload;

        if (IsValidArrayIndex(intIndex) == qfalse) {
            Scr_Error(va("array index %d out of range", intIndex));
            return 0;
        }
        child = FindArrayVariable(array, intIndex);
    } else if (values[0].type == SCRIPT_VAR_STRING) {
        uint16_t string = (uint16_t)values[0].payload;

        child = FindVariable(array, string);
        if (child != 0) {
            SL_RemoveRefToString(string);
            return child;
        }
    } else {
        Scr_Error(va("%s is not an array index",
                     script_variableTypeNames[values[0].type]));
        return 0;
    }

    if (child != 0) {
        return child;
    }
    Scr_Error("array index does not exist");
    return 0;
}

/* Source: CoDUOMP.exe 0x00488920..0x004889b5.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00488920_004889cf.mcode. */
void CastVector2(VariableValue values[3])
{
    vec3_t vector;

    for (int32_t index = 2; index >= 0; --index) {
        if (CastFloat(&values[index]) == qfalse) {
            script_errorParameterIndex = index;
            ScriptRuntime_RaiseError();
        } else {
            memcpy(&vector[2 - index], &values[index].payload,
                   sizeof(vector[0]));
        }
    }

    values[0].type = SCRIPT_VAR_VECTOR;
#if defined(WINDOWS_BEHAVIOR)
    script_vector_storage_t *storage = MT_Alloc(sizeof(*storage));
    storage->refCount = 0;
    memcpy(storage->value, vector, sizeof(vector));
    values[0].payload = (uintptr_t)storage->value;
#else
    values[0].payload = (uintptr_t)AllocVectorCopy(vector);
#endif
}

/* Source: CoDUOMP.exe 0x004889d0..0x00488a8c.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004889d0_00488a8d.mcode. */
void ClearVector(VariableValue values[3])
{
    for (int32_t index = 2; index >= 0; --index) {
        RemoveRefToValue(&values[index]);
    }
    values[0].type = SCRIPT_VAR_UNDEFINED;
}

/* Source: CoDUOMP.exe 0x00488ab0..0x00488b4d.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00488ab0_00488b4e.mcode. */
void UnmatchingTypesError(VariableValue *left,
                                        VariableValue *right)
{
    script_errorMessage =
        va("pair has unmatching types '%s' and '%s'",
           script_variableTypeNames[script_coerceLeftType],
           script_variableTypeNames[script_coerceRightType]);
    RemoveRefToValue(left);
    left->type = SCRIPT_VAR_UNDEFINED;
    RemoveRefToValue(right);
    right->type = SCRIPT_VAR_UNDEFINED;
}

/* Source: CoDUOMP.exe 0x00488b90..0x00488cca.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00488b90_00488ccb.mcode. */
qboolean CastWeakerPair(VariableValue *left,
                                     VariableValue *right)
{
    script_coerceLeftType = left->type;
    script_coerceRightType = right->type;

    if (script_coerceLeftType == script_coerceRightType) {
        return qtrue;
    }

    if (script_coerceLeftType < script_coerceRightType) {
        if (script_coerceLeftType == SCRIPT_VAR_STRING) {
            if (script_coerceRightType == SCRIPT_VAR_FLOAT) {
                float value;

                memcpy(&value, &right->payload, sizeof(value));
                right->type = SCRIPT_VAR_STRING;
                right->payload = SL_GetStringForFloat(value);
                return qtrue;
            }
            if (script_coerceRightType == SCRIPT_VAR_VECTOR) {
                const float *vector =
                    (const float *)right->payload;

                right->type = SCRIPT_VAR_STRING;
                right->payload = SL_GetStringForVector(vector);
                RemoveRefToVector(vector);
                return qtrue;
            }
            if (script_coerceRightType == SCRIPT_VAR_INT) {
                right->type = SCRIPT_VAR_STRING;
                right->payload = SL_GetStringForInt(
                    (int32_t)(uint32_t)right->payload);
                return qtrue;
            }
        } else if (script_coerceLeftType != SCRIPT_VAR_FLOAT) {
            UnmatchingTypesError(left, right);
            return qfalse;
        }

        if (script_coerceRightType == SCRIPT_VAR_INT) {
            float value = (float)(int32_t)(uint32_t)right->payload;

            right->payload = 0;
            memcpy(&right->payload, &value, sizeof(value));
            right->type = SCRIPT_VAR_FLOAT;
            return qtrue;
        }
    } else {
        if (script_coerceRightType == SCRIPT_VAR_STRING) {
            if (script_coerceLeftType == SCRIPT_VAR_FLOAT) {
                float value;

                memcpy(&value, &left->payload, sizeof(value));
                left->type = SCRIPT_VAR_STRING;
                left->payload = SL_GetStringForFloat(value);
                return qtrue;
            }
            if (script_coerceLeftType == SCRIPT_VAR_VECTOR) {
                const float *vector =
                    (const float *)left->payload;

                left->type = SCRIPT_VAR_STRING;
                left->payload = SL_GetStringForVector(vector);
                RemoveRefToVector(vector);
                return qtrue;
            }
            if (script_coerceLeftType == SCRIPT_VAR_INT) {
                left->type = SCRIPT_VAR_STRING;
                left->payload = SL_GetStringForInt(
                    (int32_t)(uint32_t)left->payload);
                return qtrue;
            }
        } else if (script_coerceRightType != SCRIPT_VAR_FLOAT) {
            UnmatchingTypesError(left, right);
            return qfalse;
        }

        if (script_coerceLeftType == SCRIPT_VAR_INT) {
            float value = (float)(int32_t)(uint32_t)left->payload;

            left->payload = 0;
            memcpy(&left->payload, &value, sizeof(value));
            left->type = SCRIPT_VAR_FLOAT;
            return qtrue;
        }
    }

    UnmatchingTypesError(left, right);
    return qfalse;
}

/* Source: CoDUOMP.exe 0x00488cd0..0x00488cd7.
 * Name and source signature: same-module Mac overload
 * CastWeakerPair(VariableValue *). The retained Windows thunk forms the
 * address of values[1] and tail-calls the two-pointer implementation. */
qboolean CastWeakerPairValues(VariableValue values[2])
{
    return CastWeakerPair(&values[0], &values[1]);
}

/* Source: CoDUOMP.exe 0x00488ce0..0x00488e38.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00488ce0_00488e39.mcode. */
qboolean CheckEquality(VariableValue *left,
                                  VariableValue *right)
{
    if (CastWeakerPair(left, right) == qfalse) {
        return qfalse;
    }

    switch (left->type) {
    case SCRIPT_VAR_UNDEFINED:
        left->type = SCRIPT_VAR_INT;
        left->payload = qtrue;
        return qtrue;

    case SCRIPT_VAR_STRING:
    case SCRIPT_VAR_LOCALIZED_STRING: {
        uint16_t leftString = (uint16_t)left->payload;
        uint16_t rightString = (uint16_t)right->payload;

        left->type = SCRIPT_VAR_INT;
        SL_RemoveRefToString(leftString);
        SL_RemoveRefToString(rightString);
        left->payload = leftString == rightString ? qtrue : qfalse;
        return qtrue;
    }

    case SCRIPT_VAR_VECTOR: {
        const float *leftVector = (const float *)left->payload;
        const float *rightVector = (const float *)right->payload;
        qboolean equal =
            leftVector[0] == rightVector[0] &&
                    leftVector[1] == rightVector[1] &&
                    leftVector[2] == rightVector[2]
                ? qtrue
                : qfalse;

        left->type = SCRIPT_VAR_INT;
        RemoveRefToVector((const float *)left->payload);
        RemoveRefToVector((const float *)right->payload);
        left->payload = equal;
        return qtrue;
    }

    case SCRIPT_VAR_FLOAT: {
        float leftValue;
        float rightValue;
        float difference;
        uint32_t differenceBits;

        memcpy(&leftValue, &left->payload, sizeof(leftValue));
        memcpy(&rightValue, &right->payload, sizeof(rightValue));
        difference = leftValue - rightValue;
        memcpy(&differenceBits, &difference, sizeof(differenceBits));
        differenceBits &= SCRIPT_FLOAT_ABS_MASK;
        memcpy(&difference, &differenceBits, sizeof(difference));

        left->type = SCRIPT_VAR_INT;
        left->payload =
            difference < script_floatEqualityEpsilon ? qtrue : qfalse;
        return qtrue;
    }

    case SCRIPT_VAR_INT:
        left->payload =
            (uint32_t)left->payload == (uint32_t)right->payload
                ? qtrue
                : qfalse;
        return qtrue;

    case SCRIPT_VAR_OBJECT: {
        uint16_t leftObject = (uint16_t)left->payload;
        uint16_t rightObject = (uint16_t)right->payload;

        left->type = SCRIPT_VAR_INT;
        RemoveRefToObject(leftObject);
        RemoveRefToObject(rightObject);
        left->payload = leftObject == rightObject ? qtrue : qfalse;
        return qtrue;
    }

    case SCRIPT_VAR_ANIMATION:
        left->type = SCRIPT_VAR_INT;
        left->payload =
            (uint32_t)left->payload == (uint32_t)right->payload
                ? qtrue
                : qfalse;
        return qtrue;

    default:
        UnmatchingTypesError(left, right);
        return qfalse;
    }
}

/* Source: CoDUOMP.exe 0x004884e0..0x00488514.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004884e0_00488515.mcode. */
qboolean ScriptRuntime_StringStartsWithZeroLiteral(const char *text)
{
    while (*text != '\0' && *text <= ' ') {
        ++text;
    }

    if (*text == '-' || *text == '+') {
        ++text;
    }
    if (*text == '0') {
        return qtrue;
    }
    return *text == '.' && text[1] == '0' ? qtrue : qfalse;
}

/* Source: CoDUOMP.exe 0x00488520..0x0048860c.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00488520_0048860d.mcode. */
qboolean CastBool(VariableValue *value)
{
    if (value->type == SCRIPT_VAR_FLOAT) {
        float floatValue;

        memcpy(&floatValue, &value->payload, sizeof(floatValue));
        value->type = SCRIPT_VAR_INT;
        value->payload = floatValue != 0.0f ? qtrue : qfalse;
        return qtrue;
    }

    if (value->type == SCRIPT_VAR_STRING) {
        uint16_t string = (uint16_t)value->payload;
        const char *text = SL_ConvertToString(string);

        value->payload = atoi(text) != 0 ? qtrue : qfalse;
        if (value->payload == qfalse &&
            ScriptRuntime_StringStartsWithZeroLiteral(
                SL_ConvertToString(string)) == qfalse) {
            script_errorMessage =
                va("cannot cast \"%s\" to bool",
                   SL_ConvertToString(string));
            SL_RemoveRefToString(string);
            value->type = SCRIPT_VAR_UNDEFINED;
            return qfalse;
        }

        value->type = SCRIPT_VAR_INT;
        SL_RemoveRefToString(string);
        return qtrue;
    }

    script_errorMessage =
        va("cannot cast %s to bool",
           script_variableTypeNames[value->type]);
    RemoveRefToValue(value);
    value->type = SCRIPT_VAR_UNDEFINED;
    return qfalse;
}

/* Source: CoDUOMP.exe 0x00488610..0x004886d9.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00488610_004886da.mcode. */
qboolean CastInt(VariableValue *value)
{
    if (value->type == SCRIPT_VAR_INT) {
        return qtrue;
    }

    if (value->type == SCRIPT_VAR_FLOAT) {
        float floatValue;

        memcpy(&floatValue, &value->payload, sizeof(floatValue));
        value->type = SCRIPT_VAR_INT;
        value->payload = coduo_fp_to_u32_extended(
            (long double)floatValue);
        return qtrue;
    }

    if (value->type == SCRIPT_VAR_STRING) {
        uint16_t string = (uint16_t)value->payload;
        const char *text = SL_ConvertToString(string);
        int32_t intValue = atoi(text);

        value->payload = (uint32_t)intValue;
        if (intValue == 0 &&
            ScriptRuntime_StringStartsWithZeroLiteral(
                SL_ConvertToString(string)) == qfalse) {
            script_errorMessage =
                va("cannot cast \"%s\" to int",
                   SL_ConvertToString(string));
            SL_RemoveRefToString(string);
            value->type = SCRIPT_VAR_UNDEFINED;
            return qfalse;
        }

        value->type = SCRIPT_VAR_INT;
        SL_RemoveRefToString(string);
        return qtrue;
    }

    script_errorMessage =
        va("cannot cast %s to int",
           script_variableTypeNames[value->type]);
    RemoveRefToValue(value);
    value->type = SCRIPT_VAR_UNDEFINED;
    return qfalse;
}

/* Source: CoDUOMP.exe 0x004886e0..0x004887b5.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004886e0_004887b6.mcode. */
#if defined(WINDOWS_BEHAVIOR)
qboolean CastFloat(VariableValue *value)
{
    if (value->type == SCRIPT_VAR_FLOAT) {
        return qtrue;
    }

    if (value->type == SCRIPT_VAR_INT) {
        float floatValue = (float)(int32_t)(uint32_t)value->payload;

        value->type = SCRIPT_VAR_FLOAT;
        memcpy(&value->payload, &floatValue, sizeof(floatValue));
        return qtrue;
    }

    if (value->type == SCRIPT_VAR_STRING) {
        uint16_t string = (uint16_t)value->payload;
        /* atof returns a double; the original stores the float32-narrowed
         * value to the payload (FST) but performs the zero test on the
         * un-narrowed double still in ST(0) (FUCOMPP at 0x00488731). Testing
         * the narrowed float instead wrongly rejects sub-float-min nonzero
         * strings like "1e-300", which the DLL accepts as float 0.0. */
        double parsedValue = atof(SL_ConvertToString(string));
        float floatValue = (float)parsedValue;

        memcpy(&value->payload, &floatValue, sizeof(floatValue));
        if (parsedValue == 0.0 &&
            ScriptRuntime_StringStartsWithZeroLiteral(
                SL_ConvertToString(string)) == qfalse) {
            script_errorMessage =
                va("cannot cast \"%s\" to float",
                   SL_ConvertToString(string));
            SL_RemoveRefToString(string);
            value->type = SCRIPT_VAR_UNDEFINED;
            return qfalse;
        }

        value->type = SCRIPT_VAR_FLOAT;
        SL_RemoveRefToString(string);
        return qtrue;
    }

    script_errorMessage =
        va("cannot cast %s to float",
           script_variableTypeNames[value->type]);
    RemoveRefToValue(value);
    value->type = SCRIPT_VAR_UNDEFINED;
    return qfalse;
}
#else
/* coduo_lnxded 0x080aac34..0x080aad7a stores atof's result to the binary32
 * payload at 0x080aaca7, reloads that payload at 0x080aacac, and performs the
 * zero comparison on the reloaded value. */
qboolean CastFloat(VariableValue *value)
{
    if (value->type == SCRIPT_VAR_FLOAT) {
        return qtrue;
    }

    if (value->type == SCRIPT_VAR_INT) {
        float floatValue = (float)(int32_t)(uint32_t)value->payload;

        value->type = SCRIPT_VAR_FLOAT;
        memcpy(&value->payload, &floatValue, sizeof(floatValue));
        return qtrue;
    }

    if (value->type == SCRIPT_VAR_STRING) {
        uint16_t string = (uint16_t)value->payload;
        float floatValue =
            (float)atof(SL_ConvertToString(string));

        memcpy(&value->payload, &floatValue, sizeof(floatValue));
        if (floatValue == 0.0f &&
            ScriptRuntime_StringStartsWithZeroLiteral(
                SL_ConvertToString(string)) == qfalse) {
            script_errorMessage =
                va("cannot cast \"%s\" to float",
                   SL_ConvertToString(string));
            SL_RemoveRefToString(string);
            value->type = SCRIPT_VAR_UNDEFINED;
            return qfalse;
        }

        value->type = SCRIPT_VAR_FLOAT;
        SL_RemoveRefToString(string);
        return qtrue;
    }

    script_errorMessage =
        va("cannot cast %s to float",
           script_variableTypeNames[value->type]);
    RemoveRefToValue(value);
    value->type = SCRIPT_VAR_UNDEFINED;
    return qfalse;
}
#endif

/* Source: CoDUOMP.exe 0x004887c0..0x00488852.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004887c0_00488853.mcode. */
qboolean CastString(VariableValue *value)
{
    switch (value->type) {
    case SCRIPT_VAR_STRING:
        return qtrue;
    case SCRIPT_VAR_INT:
        value->type = SCRIPT_VAR_STRING;
        value->payload =
            SL_GetStringForInt((int32_t)(uint32_t)value->payload);
        return qtrue;
    case SCRIPT_VAR_FLOAT: {
        float floatValue;

        memcpy(&floatValue, &value->payload, sizeof(floatValue));
        value->type = SCRIPT_VAR_STRING;
        value->payload = SL_GetStringForFloat(floatValue);
        return qtrue;
    }
    case SCRIPT_VAR_VECTOR: {
        const float *vector = (const float *)value->payload;

        value->type = SCRIPT_VAR_STRING;
        value->payload = SL_GetStringForVector(vector);
        RemoveRefToVector(vector);
        return qtrue;
    }
    default:
        script_errorMessage =
            va("cannot cast %s to string",
               script_variableTypeNames[value->type]);
        RemoveRefToValue(value);
        value->type = SCRIPT_VAR_UNDEFINED;
        return qfalse;
    }
}

/* Source: CoDUOMP.exe 0x00488860..0x00488895.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00488860_00488896.mcode.
 * Name: exact same-module Mac symbol CastIString. The original operation is
 * a type assertion: it does not convert ordinary strings to localized
 * strings. */
qboolean CastIString(VariableValue *value)
{
    if (value->type == SCRIPT_VAR_LOCALIZED_STRING)
        return qtrue;

    script_errorMessage =
        va("cannot cast %s to istring",
           script_variableTypeNames[value->type]);
    RemoveRefToValue(value);
    value->type = SCRIPT_VAR_UNDEFINED;
    return qfalse;
}

/* Source: CoDUOMP.exe 0x004888a0..0x004888d5.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004888a0_004888d6.mcode.
 * Name: exact same-module Mac symbol CastVector. */
qboolean CastVector(VariableValue *value)
{
    if (value->type == SCRIPT_VAR_VECTOR)
        return qtrue;

    script_errorMessage =
        va("cannot cast %s to vector",
           script_variableTypeNames[value->type]);
    RemoveRefToValue(value);
    value->type = SCRIPT_VAR_UNDEFINED;
    return qfalse;
}

/* Source: CoDUOMP.exe 0x004888e0..0x00488915.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004888e0_00488916.mcode.
 * Name: exact same-module Mac symbol CastPointer. "Pointer" is the script
 * VM's object-handle value type, not a native host pointer. */
qboolean CastPointer(VariableValue *value)
{
    if (value->type == SCRIPT_VAR_OBJECT)
        return qtrue;

    script_errorMessage =
        va("cannot cast %s to object",
           script_variableTypeNames[value->type]);
    RemoveRefToValue(value);
    value->type = SCRIPT_VAR_UNDEFINED;
    return qfalse;
}

/* Source: CoDUOMP.exe 0x0048fb50..0x0048fbf2.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0048fb50_0048fbf3.mcode. */
qboolean Scr_GetBool(uint32_t index)
{
    if (index < script_parameterCount) {
        VariableValue *value = &script_valueStackTop[-(int32_t)index];
        if (value->type == SCRIPT_VAR_INT) {
            return value->payload != 0 ? qtrue : qfalse;
        }
        if (CastBool(value) != qfalse) {
            return value->payload != 0 ? qtrue : qfalse;
        }

        script_errorParameterIndex = (int32_t)index + 1;
        ScriptRuntime_RaiseError();
    }

    Scr_Error(va("parameter %d does not exist", index + 1));
    return qfalse;
}

/* Source: CoDUOMP.exe 0x0048fc00..0x0048fc8e.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0048fc00_0048fc8f.mcode. */
int32_t Scr_GetInt(uint32_t index)
{
    if (index < script_parameterCount) {
        VariableValue *value = &script_valueStackTop[-(int32_t)index];
        if (CastInt(value) != qfalse) {
            return (int32_t)(uint32_t)value->payload;
        }

        script_errorParameterIndex = (int32_t)index + 1;
        ScriptRuntime_RaiseError();
    }

    Scr_Error(va("parameter %d does not exist", index + 1));
    return 0;
}

/* Source: CoDUOMP.exe 0x0048ff00..0x0048ff92.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0048ff00_0048ff93.mcode. */
float Scr_GetFloat(uint32_t index)
{
    if (index < script_parameterCount) {
        VariableValue *value = &script_valueStackTop[-(int32_t)index];
        if (CastFloat(value) != qfalse) {
            float result;

            memcpy(&result, &value->payload, sizeof(result));
            return result;
        }

        script_errorParameterIndex = (int32_t)index + 1;
        ScriptRuntime_RaiseError();
    }

    Scr_Error(va("parameter %d does not exist", index + 1));
    return 0.0f;
}

/* Source: CoDUOMP.exe 0x0048ef50..0x0048f08b.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0048ef50_0048f08c.mcode.
 * The original inlines the type-specific release switch implemented by
 * RemoveRefToValue. */
void IncInParam(void)
{
    while (script_parameterCount != 0) {
        RemoveRefToValue(script_valueStackTop);
        --script_valueStackTop;
        --script_parameterCount;
    }

    ++script_valueStackTop;
    ++script_valueStackDepth;
    if (script_valueStackTop > script_valueStackLimit) {
#if defined(WINDOWS_BEHAVIOR)
        Com_Error(ERR_DROP,
                  "\x15Internal script stack overflow");
#else
        Com_Error(ERR_DROP, "Internal script stack overflow");
#endif
    }
}

/* Source: CoDUOMP.exe 0x00490650..0x00490667.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00490650_00490668.mcode. */
void Scr_AddBool(qboolean value)
{
    IncInParam();
    script_valueStackTop->type = SCRIPT_VAR_INT;
    script_valueStackTop->payload = (uintptr_t)(uint32_t)value;
}

/* Source: CoDUOMP.exe 0x00490670..0x00490687.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00490670_00490688.mcode. */
void Scr_AddInt(int32_t value)
{
    IncInParam();
    script_valueStackTop->type = SCRIPT_VAR_INT;
    script_valueStackTop->payload = (uintptr_t)(uint32_t)value;
}

/* Source: CoDUOMP.exe 0x00490690..0x004906a7.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00490690_004906a8.mcode. */
void Scr_AddFloat(float value)
{
    uint32_t payload;

    IncInParam();
    memcpy(&payload, &value, sizeof(payload));
    script_valueStackTop->type = SCRIPT_VAR_FLOAT;
    script_valueStackTop->payload = payload;
}

/* Source: CoDUOMP.exe 0x004906b0..0x004906c7.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004906b0_004906c8.mcode. */
void Scr_AddAnim(uint32_t value)
{
    IncInParam();
    script_valueStackTop->type = SCRIPT_VAR_ANIMATION;
    script_valueStackTop->payload = value;
}

/* Source: CoDUOMP.exe 0x004906d0..0x004906e1.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004906d0_004906e2.mcode. */
void Scr_AddUndefined(void)
{
    IncInParam();
    script_valueStackTop->type = SCRIPT_VAR_UNDEFINED;
}

/* Source: CoDUOMP.exe 0x004906f0..0x00490717.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004906f0_00490718.mcode. */
void Scr_AddObject(uint16_t object)
{
    IncInParam();
    script_valueStackTop->type = SCRIPT_VAR_OBJECT;
    script_valueStackTop->payload = object;
    AddRefToObject(object);
}

/* Source: CoDUOMP.exe 0x00490720..0x00490758.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00490720_00490759.mcode. */
void Scr_AddEntityNum(int32_t entityNum, int32_t classNum)
{
    Scr_AddObject(Scr_GetEntityId(entityNum, classNum));
}

/* Source: CoDUOMP.exe 0x00490760..0x004907e6.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00490760_004907e7.mcode. */
void Scr_AddStruct(void)
{
    uint16_t object = AllocObject();

    Scr_AddObject(object);
    RemoveRefToObject(object);
}

/* Source: CoDUOMP.exe 0x004907f0..0x00490832.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004907f0_00490833.mcode. */
void Scr_AddString(const char *value)
{
    IncInParam();
    script_valueStackTop->type = SCRIPT_VAR_STRING;
    script_valueStackTop->payload = SL_GetString(value, 0);
}

/* Source: CoDUOMP.exe 0x00490840..0x00490882.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00490840_00490883.mcode. */
void Scr_AddIString(const char *value)
{
    IncInParam();
    script_valueStackTop->type = SCRIPT_VAR_LOCALIZED_STRING;
    script_valueStackTop->payload = SL_GetString(value, 0);
}

/* Source: CoDUOMP.exe 0x00490890..0x004908b9.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00490890_004908ba.mcode. */
void Scr_AddConstString(uint16_t value)
{
    IncInParam();
    script_valueStackTop->type = SCRIPT_VAR_STRING;
    script_valueStackTop->payload = value;
    SL_AddRefToString(value);
}

/* Source: CoDUOMP.exe 0x004908c0..0x00490907.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004908c0_00490908.mcode. */
void Scr_AddVector(const vec3_t vector)
{
#if defined(WINDOWS_BEHAVIOR)
    script_vector_storage_t *storage;
#endif

    IncInParam();
    script_valueStackTop->type = SCRIPT_VAR_VECTOR;
#if defined(WINDOWS_BEHAVIOR)
    storage = MT_Alloc(sizeof(*storage));
    storage->refCount = 0;
    storage->value[0] = vector[0];
    storage->value[1] = vector[1];
    storage->value[2] = vector[2];
    script_valueStackTop->payload = (uintptr_t)storage->value;
#else
    script_valueStackTop->payload = (uintptr_t)AllocVectorCopy(vector);
#endif
}

/* Source: CoDUOMP.exe 0x00490910..0x00490958.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00490910_00490959.mcode. */
void Scr_MakeArray(void)
{
    IncInParam();
    script_valueStackTop->type = SCRIPT_VAR_OBJECT;
    script_valueStackTop->payload = Scr_AllocArray();
}

/* Source: CoDUOMP.exe 0x00490960..0x004909d5.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00490960_004909d6.mcode. */
void Scr_AddArray(void)
{
    --script_valueStackTop;
    --script_valueStackDepth;

    uint16_t array = (uint16_t)script_valueStackTop->payload;
    uint16_t child = GetArrayVariable(
        array,
        script_variableNodes[array].payload.halves.parentHandle);
    SetNewVariableValue(child, &script_valueStackTop[1]);
}

/* Source: CoDUOMP.exe 0x004909e0..0x00490a40.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004909e0_00490a41.mcode. */
void Scr_AddArrayStringIndexed(uint16_t name)
{
    --script_valueStackTop;
    --script_valueStackDepth;

    uint16_t array = (uint16_t)script_valueStackTop->payload;
    uint16_t child = GetVariable(array, name);
    SetNewVariableValue(child, &script_valueStackTop[1]);
}

/* Source: CoDUOMP.exe 0x00490040..0x00490062.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00490040_00490063.mcode. */
#if defined(WINDOWS_BEHAVIOR)
const char *Scr_GetString(uint32_t index)
{
    uint16_t string = Scr_GetConstString(index);

    return string != 0 ? SL_ConvertToString(string) : NULL;
}
#else
/* coduo_lnxded 0x080afbaa..0x080afbed zero-extends the returned handle and
 * calls SL_ConvertToString unconditionally. */
const char *Scr_GetString(uint32_t index)
{
    return SL_ConvertToString(Scr_GetConstString(index));
}
#endif

/* Source: CoDUOMP.exe 0x0048ffa0..0x00490030.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0048ffa0_00490031.mcode. */
uint16_t Scr_GetConstString(uint32_t index)
{
    if (index < script_parameterCount) {
        VariableValue *value = &script_valueStackTop[-(int32_t)index];
        if (CastString(value) != qfalse) {
            return (uint16_t)value->payload;
        }

        script_errorParameterIndex = (int32_t)index + 1;
        ScriptRuntime_RaiseError();
    }

    Scr_Error(va("parameter %d does not exist", index + 1));
    return 0;
}

/* Source: CoDUOMP.exe 0x00490070..0x004901b7.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00490070_004901b8.mcode. */
#if defined(WINDOWS_BEHAVIOR)
const char *Scr_GetDebugString(uint32_t index)
{
    if (index >= script_parameterCount) {
        Scr_Error(va("parameter %d does not exist", index + 1));
        return NULL;
    }

    VariableValue *value = &script_valueStackTop[-(int32_t)index];
    script_variable_type_t originalType = value->type;
    script_variable_type_t debugType = originalType;

    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if (originalType == SCRIPT_VAR_LOCALIZED_STRING) {
        script_errorMessage = NULL;
        script_errorSource = NULL;
        script_errorParameterIndex = 0;
        return SL_ConvertToString((uint16_t)value->payload);
    }

    if (originalType == SCRIPT_VAR_OBJECT) {
        debugType = GetVarType((uint16_t)value->payload);
    }

    const char *debugString = script_variableTypeNames[debugType];
    if (CastString(value) != qfalse) {
        /* The original tests the coerced handle and returns NULL when it is 0
         * (CMP AX,CX / JZ 0x004900e2) rather than resolving handle 0 to a
         * non-NULL &stringTable[0] pointer. Matches the sibling Scr_GetString. */
        uint16_t string = (uint16_t)value->payload;
        return string != 0 ? SL_ConvertToString(string) : NULL;
    }

    script_errorMessage = NULL;
    script_errorSource = NULL;
    script_errorParameterIndex = 0;

    if (originalType == SCRIPT_VAR_ANIMATION) {
        scr_anim_t animRef;

        memcpy(&animRef, &value->payload, sizeof(animRef));
        debugString = XAnimGetAnimName(
            Scr_GetAnims(animRef.treeIndex), animRef.animIndex);
    }

    return debugString;
}
#else
const char *Scr_GetDebugString(uint32_t index)
{
    if (index >= script_parameterCount) {
        Scr_Error(va("parameter %d does not exist", index + 1));
        return NULL;
    }

    VariableValue *value = &script_valueStackTop[-(int32_t)index];
    script_variable_type_t originalType = value->type;
    script_variable_type_t debugType = originalType;

    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if (originalType == SCRIPT_VAR_LOCALIZED_STRING) {
        script_errorMessage = NULL;
        script_errorSource = NULL;
        script_errorParameterIndex = 0;
        return SL_ConvertToString((uint16_t)value->payload);
    }

    if (originalType == SCRIPT_VAR_OBJECT) {
        debugType = GetVarType((uint16_t)value->payload);
    }

    const char *debugString = script_variableTypeNames[debugType];
    if (CastString(value) != qfalse) {
        return SL_ConvertToString((uint16_t)value->payload);
    }

    script_errorMessage = NULL;
    script_errorSource = NULL;
    script_errorParameterIndex = 0;

    if (originalType == SCRIPT_VAR_ANIMATION) {
        scr_anim_t animRef;

        memcpy(&animRef, &value->payload, sizeof(animRef));
        debugString = XAnimGetAnimName(
            Scr_GetAnims(animRef.treeIndex), animRef.animIndex);
    }

    return debugString;
}
#endif

/* Source: CoDUOMP.exe 0x0048fc90..0x0048fdc8.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0048fc90_0048fdc9.mcode. */
scr_anim_t Scr_GetAnim(uint32_t index, XAnimTree *runtimeTree)
{
    scr_anim_t animRef = {0, 0};

    if (index < script_parameterCount) {
        VariableValue *value = &script_valueStackTop[-(int32_t)index];
        if (value->type == SCRIPT_VAR_ANIMATION) {
            memcpy(&animRef, &value->payload, sizeof(animRef));
            if (runtimeTree == NULL) {
                return animRef;
            }

            XAnim *animTree =
                script_animTrees[xanim_activePoolPayloadSlot]
                                [animRef.treeIndex];
            if (animTree == runtimeTree->sourceTree) {
                return animRef;
            }

            script_errorMessage =
                va("anim '%s' in animtree '%s' does not belong to the "
                   "entity's animtree '%s'",
                   XAnimGetAnimName(animTree, animRef.animIndex),
                   animTree->name, runtimeTree->sourceTree->name);
        } else {
            script_errorMessage =
                va("cannot cast %s to anim",
                   script_variableTypeNames[value->type]);
        }

        RemoveRefToValue(value);
        value->type = SCRIPT_VAR_UNDEFINED;
        script_errorParameterIndex = (int32_t)index + 1;
        ScriptRuntime_RaiseError();
    }

    Scr_Error(va("parameter %d does not exist", index + 1));
    return animRef;
}

/* Source: CoDUOMP.exe 0x0048fdd0..0x0048fefe.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0048fdd0_0048feff.mcode. */
#if defined(WINDOWS_BEHAVIOR)
XAnim *Scr_GetAnimTree(uint32_t index)
{
    int32_t slot = xanim_activePoolPayloadSlot;

    if (index < script_parameterCount) {
        VariableValue *value = &script_valueStackTop[-(int32_t)index];
        if (value->type == SCRIPT_VAR_INT) {
            uint32_t treeIndex = (uint32_t)value->payload;
            if (treeIndex <= (uint32_t)script_animTreeCounts[slot] &&
                script_animTrees[slot][treeIndex] != NULL) {
                return script_animTrees[slot][treeIndex];
            }
            script_errorMessage = "bad anim tree";
        } else {
            script_errorMessage =
                va("cannot cast %s to animtree",
                   script_variableTypeNames[value->type]);
        }

        RemoveRefToValue(value);
        value->type = SCRIPT_VAR_UNDEFINED;
        script_errorParameterIndex = (int32_t)index + 1;
        ScriptRuntime_RaiseError();
    }

    Scr_Error(va("parameter %d does not exist", index + 1));
    return script_animTrees[slot][0];
}
#else
/* The Linux source ABI returns the same one-pointer value as its retained
 * script_anim_tree_ref_t aggregate. Keep the complete function under the
 * behavior gate because its i386 calling convention uses an aggregate return. */
script_anim_tree_ref_t Scr_GetAnimTree(uint32_t index)
{
    int32_t slot = xanim_activePoolPayloadSlot;

    if (index < script_parameterCount) {
        VariableValue *value = &script_valueStackTop[-(int32_t)index];
        if (value->type == SCRIPT_VAR_INT) {
            uint32_t treeIndex = (uint32_t)value->payload;
            if (treeIndex <= (uint32_t)script_animTreeCounts[slot] &&
                script_animTrees[slot][treeIndex] != NULL) {
                return (script_anim_tree_ref_t){
                    script_animTrees[slot][treeIndex]};
            }
            script_errorMessage = "bad anim tree";
        } else {
            script_errorMessage =
                va("cannot cast %s to animtree",
                   script_variableTypeNames[value->type]);
        }

        RemoveRefToValue(value);
        value->type = SCRIPT_VAR_UNDEFINED;
        script_errorParameterIndex = (int32_t)index + 1;
        ScriptRuntime_RaiseError();
    }

    Scr_Error(va("parameter %d does not exist", index + 1));
    return (script_anim_tree_ref_t){script_animTrees[slot][0]};
}
#endif

/* Source: CoDUOMP.exe 0x00479010..0x00479038.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00479010_00479039.mcode. */
uint32_t CODUO_SCRIPT_CDECL Scr_GetAnimsIndex(XAnim *tree)
{
    int32_t slot = xanim_activePoolPayloadSlot;

    for (int32_t index = script_animTreeCounts[slot]; index != 0; --index) {
        if (script_animTrees[slot][index] == tree) {
            return (uint32_t)index;
        }
    }
    return 0;
}

/* Source: CoDUOMP.exe 0x00479040..0x00479055.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00479040_00479056.mcode. */
XAnim *Scr_GetAnims(uint32_t treeIndex)
{
    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if (treeIndex >= SCRIPT_ANIM_TREE_SLOT_COUNT) {
        Com_Error(ERR_DROP,
                  "\x15Scr_GetAnims: animation tree index %u out of range",
                  treeIndex);
        return NULL;
    }

    return script_animTrees[xanim_activePoolPayloadSlot][treeIndex];
}

/* Source: CoDUOMP.exe 0x00490d60..0x00490e4a.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00490d60_00490e4b.mcode. */
void Scr_SetTime(uint32_t time)
{
    time &= SCRIPT_TIME_MASK;
    if ((int32_t)(time - script_currentTimeKey) <= 0) {
        script_currentTimeKey = time;
        return;
    }

    do {
        VM_SetTime();
        script_currentTimeKey =
            (script_currentTimeKey + 1U) & SCRIPT_TIME_MASK;
    } while (script_currentTimeKey != time);
}

/* Source: CoDUOMP.exe 0x00490d20..0x00490d5a.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00490d20_00490d5b.mcode. */
void Scr_SetDynamicEntityField(int32_t entityNum, int32_t classNum,
                               uint16_t fieldName)
{
    uint16_t entity = Scr_GetEntityId(entityNum, classNum);
    uint16_t field =
        GetVariableField(entity, fieldName);

    SetVariableFieldValue(field, script_valueStackTop);
    --script_valueStackTop;
    script_valueStackDepth = 0;
}

/* Source: CoDUOMP.exe 0x00490270..0x00490292.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00490270_00490293.mcode. */
#if defined(WINDOWS_BEHAVIOR)
const char *Scr_GetIString(uint32_t index)
{
    uint16_t string = Scr_GetConstIString(index);

    return string != 0 ? SL_ConvertToString(string) : NULL;
}
#else
const char *Scr_GetIString(uint32_t index)
{
    return SL_ConvertToString(Scr_GetConstIString(index));
}
#endif

/* Source: CoDUOMP.exe 0x004901c0..0x0049026d.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004901c0_0049026e.mcode. */
uint16_t Scr_GetConstIString(uint32_t index)
{
    if (index < script_parameterCount) {
        VariableValue *value = &script_valueStackTop[-(int32_t)index];
        if (CastIString(value) != qfalse) {
            return (uint16_t)value->payload;
        }

        script_errorParameterIndex = (int32_t)index + 1;
        ScriptRuntime_RaiseError();
    }

    Scr_Error(va("parameter %d does not exist", index + 1));
    return 0;
}

/* Source: CoDUOMP.exe 0x00490530..0x00490594.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00490530_00490595.mcode. */
script_variable_type_t Scr_GetType(uint32_t index)
{
    if (index < script_parameterCount) {
        return script_valueStackTop[-(int32_t)index].type;
    }

    Scr_Error(va("parameter %d does not exist", index + 1));
    return SCRIPT_VAR_UNDEFINED;
}

/* Source: CoDUOMP.exe 0x004905a0..0x00490635.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004905a0_00490636.mcode. */
script_variable_type_t Scr_GetPointerType(uint32_t index)
{
    if (index < script_parameterCount) {
        VariableValue *value = &script_valueStackTop[-(int32_t)index];
        if (value->type == SCRIPT_VAR_OBJECT) {
            return GetVarType((uint16_t)value->payload);
        }

        Scr_Error(va("parameter %d is not a pointer", index + 1));
    }

    Scr_Error(va("parameter %d does not exist", index + 1));
    return SCRIPT_VAR_UNDEFINED;
}

/* Source: CoDUOMP.exe 0x004902a0..0x00490361.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004902a0_00490362.mcode. */
void Scr_GetVector(uint32_t index, vec3_t vector)
{
    if (index < script_parameterCount) {
        VariableValue *value = &script_valueStackTop[-(int32_t)index];
        if (CastVector(value) != qfalse) {
            const float *source = (const float *)value->payload;

            vector[0] = source[0];
            vector[1] = source[1];
            vector[2] = source[2];
            return;
        }

        script_errorParameterIndex = (int32_t)index + 1;
        ScriptRuntime_RaiseError();
    }

    Scr_Error(va("parameter %d does not exist", index + 1));
}

/* Source: CoDUOMP.exe 0x00490370..0x004903fd.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00490370_004903fe.mcode. */
uint32_t Scr_GetFunc(uint32_t index)
{
    if (index < script_parameterCount) {
        VariableValue *value = &script_valueStackTop[-(int32_t)index];
        if (value->type == SCRIPT_VAR_FUNCTION) {
            const uint8_t *codePos = (const uint8_t *)value->payload;

            return (uint32_t)((uintptr_t)codePos -
                              (uintptr_t)script_codeBase);
        }

        script_errorMessage = "not a function";
        script_errorParameterIndex = (int32_t)index + 1;
        ScriptRuntime_RaiseError();
    }

    Scr_Error(va("parameter %d does not exist", index + 1));
    return 0;
}

/* Source: CoDUOMP.exe 0x00490400..0x00490528.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00490400_00490529.mcode.
 * The Windows VM import declares the entity number as a 16-bit result.  The
 * Linux game-import slot and game-module callers declare the same bounded
 * value as uint32_t.  Preserve that public ABI distinction even though both
 * bodies return the same zero-extended 16-bit entity field. */
#if defined(WINDOWS_BEHAVIOR)
uint16_t
#else
uint32_t
#endif
Scr_GetEntityNum(uint32_t index, int32_t *classNum)
{
    if (index < script_parameterCount) {
        VariableValue *value = &script_valueStackTop[-(int32_t)index];
        if (CastPointer(value) != qfalse) {
            uint16_t object = (uint16_t)value->payload;
            script_variable_node_t *node = &script_variableNodes[object];
            script_variable_type_t objectType =
                (script_variable_type_t)(node->packedTypeIndex &
                                         SCRIPT_VARIABLE_TYPE_MASK);

            if (objectType == SCRIPT_VAR_ENTITY) {
                *classNum = (int32_t)(node->packedTypeIndex >> 8);
                return node->payload.halves.parentHandle;
            }

            script_errorMessage =
                va("cannot cast %s to entity",
                   script_variableTypeNames[objectType]);
        }

        script_errorParameterIndex = (int32_t)index + 1;
        ScriptRuntime_RaiseError();
    }

    Scr_Error(va("parameter %d does not exist", index + 1));
    return 0;
}

/* Source: CoDUOMP.exe 0x00490a50..0x00490a8b.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00490a50_00490a8c.mcode. */
void Scr_Error(const char *message)
{
    script_errorMessage = message;
    ScriptRuntime_RaiseError();
}

/* Source: CoDUOMP.exe 0x00490ae0..0x00490b25.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00490ae0_00490b26.mcode. */
void Scr_TerminalError(const char *message)
{
    Scr_DumpScriptThreads();
#if defined(LINUX_BEHAVIOR)
    Scr_DumpScriptVariables();
#endif
    script_forceErrorReport = qtrue;
    Scr_Error(message);
}

/* Source: CoDUOMP.exe 0x00490a90..0x00490ad6.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00490a90_00490ad7.mcode. */
void Scr_ErrorWithDialogMessage(const char *message, const char *source)
{
    script_errorSource = source;
    script_errorMessage = message;
    ScriptRuntime_RaiseError();
}

/* Source: CoDUOMP.exe 0x00490b30..0x00490b76.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00490b30_00490b77.mcode. */
void Scr_ParamError(int32_t index, const char *message)
{
    script_errorParameterIndex = index + 1;
    script_errorMessage = message;
    ScriptRuntime_RaiseError();
}

/* Source: CoDUOMP.exe 0x00490b80..0x00490bc5.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00490b80_00490bc6.mcode. */
void Scr_ObjectError(const char *message)
{
    script_errorParameterIndex = -1;
    script_errorMessage = message;
    ScriptRuntime_RaiseError();
}

#if defined(WINDOWS_BEHAVIOR)
qboolean Scr_IsSystemActive(qboolean unused)
#else
qboolean Scr_IsSystemActive(uint8_t unused)
#endif
{
    (void)unused;
    return script_timeArrayHandle != 0 ? qtrue : qfalse;
}

uint32_t Scr_GetNumParam(void)
{
    return script_parameterCount;
}

void Scr_RunCurrentThreads(void)
{
    VM_SetTime();
}

void Scr_ResetTimeout(void)
{
    script_loopWatchdogTick = (uint32_t)rdtsc();
}
