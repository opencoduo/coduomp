#include "script_value.h"

#include "script_memory.h"
#include "script_runtime_host.h"
#include "script_string.h"
#include "script_variable.h"

#include <string.h>

enum {
    SCRIPT_VECTOR_ALLOC_TAG = 2
};

/* NOT_FROM_ORIGINAL_SOURCE: defined native spelling of the original word
 * INC/DEC operations. */
static int16_t coduomp_script_vector_ref_from_bits(uint16_t bits)
{
    int16_t value;

    memcpy(&value, &bits, sizeof(value));
    return value;
}

/* Source: CoDUOMP.exe 0x00484150..0x0048416a and coduo_lnxded
 * 0x080a6c6a..0x080a6c8a.  The Linux allocator's second argument is its
 * original unused allocation tag. */
#if defined(WINDOWS_BEHAVIOR)
float *AllocVector(void)
{
    script_vector_storage_t *storage = MT_Alloc(sizeof(*storage));

    storage->refCount = 0;
    return storage->value;
}
#else
float *AllocVector(void)
{
    script_vector_storage_t *storage =
        MT_Alloc(sizeof(*storage), SCRIPT_VECTOR_ALLOC_TAG);

    storage->refCount = 0;
    return storage->value;
}
#endif

/* Source: CoDUOMP.exe 0x00484170..0x0048419a and the corresponding Linux
 * body immediately following AllocVector. Name and signature: exact Mac
 * overload AllocVector(const float *). */
float *AllocVectorCopy(const vec3_t value)
{
    float *copy = AllocVector();

    memcpy(copy, value, sizeof(vec3_t));
    return copy;
}

/* Source: CoDUOMP.exe 0x004841a0..0x004841b6 and coduo_lnxded
 * 0x080a6cbd..0x080a6cf6. */
void AddRefToVector(const float *vector)
{
    if ((uintptr_t)vector - (uintptr_t)script_vectorLocalPoolBase >=
        script_vectorLocalPoolByteCount) {
        script_vector_storage_t *storage =
            SCRIPT_VECTOR_STORAGE_FROM_VALUE(vector);
        /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
        if ((uint16_t)storage->refCount == SCRIPT_REFERENCE_COUNT_MAX) {
            Com_Error(ERR_DROP, "\x15" "script vector reference count overflow");
        }
        storage->refCount = coduomp_script_vector_ref_from_bits(
            (uint16_t)((uint16_t)storage->refCount + 1u));
    }
}

/* Source: CoDUOMP.exe 0x004841c0..0x004841f8 and coduo_lnxded
 * 0x080a6cfc..0x080a6d33. */
void RemoveRefToVector(const float *vector)
{
    uintptr_t vectorPayload = (uintptr_t)vector;

    if (vectorPayload - (uintptr_t)script_vectorLocalPoolBase <
        script_vectorLocalPoolByteCount) {
        return;
    }

    script_vector_storage_t *storage =
        SCRIPT_VECTOR_STORAGE_FROM_PAYLOAD(vectorPayload);
    if (storage->refCount != 0) {
        storage->refCount = coduomp_script_vector_ref_from_bits(
            (uint16_t)((uint16_t)storage->refCount - 1u));
        return;
    }

    MT_Free(storage, sizeof(*storage));
}

/* Source: CoDUOMP.exe 0x00484200..0x0048424b and the equivalent Linux
 * value-reference dispatcher. The original C++ overload takes a
 * VariableUnion by value; the C suffix only disambiguates that overload. */
void AddRefToValueOfType(script_variable_type_t type, VariableUnion value)
{
    if (type == SCRIPT_VAR_VECTOR) {
        AddRefToVector((const float *)value.payload);
        return;
    }

    if (type < SCRIPT_VAR_FLOAT) {
        if (type > SCRIPT_VAR_UNDEFINED) {
            SL_AddRefToString((uint16_t)value.payload);
        }
        return;
    }

    if (type == SCRIPT_VAR_OBJECT) {
        AddRefToObject((uint16_t)value.payload);
    }
}

/* Source: CoDUOMP.exe 0x00482f30..0x00482f3c. Exact Mac overload:
 * AddRefToValue(VariableValue *). */
void AddRefToValue(const VariableValue *value)
{
    AddRefToValueOfType(value->type, value->u);
}

/* Source: CoDUOMP.exe 0x00484270..0x00484295 and the equivalent Linux
 * value-reference dispatcher. */
void RemoveRefToValueOfType(script_variable_type_t type, VariableUnion value)
{
    switch (type) {
    case SCRIPT_VAR_STRING:
    case SCRIPT_VAR_LOCALIZED_STRING:
        SL_RemoveRefToString((uint16_t)value.payload);
        break;
    case SCRIPT_VAR_VECTOR:
        RemoveRefToVector((const float *)value.payload);
        break;
    case SCRIPT_VAR_OBJECT:
        RemoveRefToObject((uint16_t)value.payload);
        break;
    default:
        break;
    }
}

/* Source: CoDUOMP.exe 0x00482f40..0x00482f67. Exact Mac overload:
 * RemoveRefToValue(VariableValue *). */
void RemoveRefToValue(VariableValue *value)
{
    RemoveRefToValueOfType(value->type, value->u);
}
