#include "script_memory.h"
#include "script_runtime_host.h"
#include "script_serialization.h"
#include "script_temp_memory.h"
#include "script_string.h"
#include "script_value.h"
#include "script_variable.h"

#include "compat/coduo_fp_platform.h"

#if EMULATE_X87
#include "compat/coduo_x87emu.h"
#endif

#include <string.h>

enum {
    SCRIPT_SERIALIZATION_FULL_OBJECT_ID_MAP_BYTES = 131072,
    SCRIPT_SERIALIZATION_VALUE_TYPE_MASK = 31,
    SCRIPT_SERIALIZATION_PERSISTENT_TYPE_MASK = 159,
    SCRIPT_SERIALIZATION_NAME_SHIFT = 8,
    SCRIPT_SERIALIZATION_SHORT_NAME_LIMIT = 65535,
    SCRIPT_SERIALIZATION_OBJECT_NAME_LIMIT = 131071,
    SCRIPT_SERIALIZATION_OBJECT_NAME_BASE = 65536,
    SCRIPT_SERIALIZATION_STRING_TYPE = 14,
    SCRIPT_SERIALIZATION_MT_PERMANENT = 1
};

#if defined(WINDOWS_BEHAVIOR) && defined(LINUX_BEHAVIOR)
#error "Select only one of WINDOWS_BEHAVIOR or LINUX_BEHAVIOR"
#elif !defined(WINDOWS_BEHAVIOR) && !defined(LINUX_BEHAVIOR)
#error "Select WINDOWS_BEHAVIOR or LINUX_BEHAVIOR"
#endif

typedef enum script_serialization_child_name_type_e {
    SCRIPT_CHILD_NAME_SHORT = 0,
    SCRIPT_CHILD_NAME_INT = 1,
    SCRIPT_CHILD_NAME_STRING = 2,
    SCRIPT_CHILD_NAME_OBJECT = 3
} script_serialization_child_name_type_t;

#if UINTPTR_MAX == UINT32_MAX
_Static_assert(offsetof(VariableStackBuffer, pos) == 4, "i386 saved-stack pos offset changed");
_Static_assert(offsetof(VariableStackBuffer, size) == 8, "i386 saved-stack size offset changed");
_Static_assert(offsetof(VariableStackBuffer, entries) == 12, "i386 saved-stack header size changed");
#endif

void ScriptSave_PrepareStack(VariableStackBuffer *frame);
static void ScriptStack_ReadEntryValue(const VariableStackBufferEntry *entry, VariableValue *value);

/* NOT_FROM_ORIGINAL_SOURCE: typed reader for a packed native saved-stack
 * entry. The i386 entry payload is four bytes; native builds retain the same
 * type-byte format while widening payload storage to uintptr_t. */
static void ScriptStack_ReadEntryValue(const VariableStackBufferEntry *entry, VariableValue *value)
{
    value->type = (script_variable_type_t)entry->type;
    value->payload = entry->payload;
}

/* Source: CoDUOMP.exe 0x00486840..0x00486852.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00486840_00486853.mcode.
 * This independent entry is included in Ghidra's fragmented
 * FUN_004864e0_0048699e record. */
void ScriptSave_PrepareValue(script_variable_type_t type, uintptr_t payload)
{
    if (type == SCRIPT_VAR_OBJECT) {
        ScriptSave_PrepareObject((uint16_t)payload);
    } else if (type == SCRIPT_VAR_STACK) {
        ScriptSave_PrepareStack((VariableStackBuffer *)payload);
    }
}

/* Source: CoDUOMP.exe 0x00486860..0x00486880.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00486860_00486881.mcode.
 * This independent entry is included in Ghidra's fragmented
 * FUN_004864e0_0048699e record. */
void ScriptSave_PrepareValueObjectRefs(VariableValue *value)
{
    ScriptSave_PrepareValue((script_variable_type_t)(value->type & SCRIPT_SERIALIZATION_VALUE_TYPE_MASK), value->payload);
}

/* Source: CoDUOMP.exe 0x004867f0..0x0048683b.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004867f0_0048683c.mcode. */
void ScriptSave_PrepareStack(VariableStackBuffer *frame)
{
    ScriptSave_PrepareObject(frame->localId);

    const VariableStackBufferEntry *entry = frame->entries;
    for (uint16_t count = frame->size; count != 0; --count) {
        VariableValue value;

        ScriptStack_ReadEntryValue(entry, &value);
        ScriptSave_PrepareValue(value.type, value.payload);
        ++entry;
    }
}

/* Source: CoDUOMP.exe 0x004861d0..0x004861f0. */
void WriteByte(uint8_t value)
{
    *TempMalloc(sizeof(value)) = value;
}

/* Source: CoDUOMP.exe 0x00486200..0x0048620f. */
uint8_t ReadByte(void)
{
    return *script_serializationCursor++;
}

/* Source: CoDUOMP.exe 0x00486210..0x00486234. */
void WriteShort(uint16_t value)
{
    uint8_t *out = TempMalloc(sizeof(value));

    memcpy(out, &value, sizeof(value));
}

/* Source: CoDUOMP.exe 0x00486240..0x00486252. */
uint16_t ReadShort(void)
{
    uint16_t value;

    memcpy(&value, script_serializationCursor, sizeof(value));
    script_serializationCursor += sizeof(value);
    return value;
}

/* Source: CoDUOMP.exe 0x00486260..0x004862b3.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00486260_004862b4.mcode. */
void WriteString(uint16_t string)
{
    const char *text = SL_ConvertToString(string);
    size_t size = strlen(text) + 1;

    memcpy(TempMalloc(size), text, size);
}

/* Source: CoDUOMP.exe 0x004862c0..0x004862ed.
 * Evidence: repaired function record
 * coduomp/mcode/CoDUOMP/FUN_004862c0_004862ee.mcode. */
void ScriptSave_WriteOptionalString(uint16_t string)
{
    WriteByte(string != 0 ? 1 : 0);
    if (string != 0) {
        WriteString(string);
    }
}

/* Source: CoDUOMP.exe 0x004862f0..0x00486329.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004862f0_0048632a.mcode. */
uint16_t ReadString(void)
{
    const char *text = (const char *)script_serializationCursor;
    /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
    size_t size = strlen(text) + 1;
    uint16_t string = SL_GetStringOfLen(text, 0, size, SCRIPT_SERIALIZATION_STRING_TYPE);

    script_serializationCursor += size;
    return string;
}

/* Source: CoDUOMP.exe 0x00486330..0x0048637c.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00486330_0048637d.mcode. */
uint16_t ScriptLoad_ReadOptionalString(void)
{
    if (*script_serializationCursor++ == 0) {
        return 0;
    }
    return ReadString();
}

/* Source: CoDUOMP.exe 0x00486380..0x004863a2.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00486380_004863a3.mcode. */
void WriteFloat(float value)
{
    uint8_t *out = TempMalloc(sizeof(value));

    memcpy(out, &value, sizeof(value));
}

/* Source: CoDUOMP.exe 0x004863b0..0x004863bf. */
float ReadFloat(void)
{
    float value;

    memcpy(&value, script_serializationCursor, sizeof(value));
    script_serializationCursor += sizeof(value);
#if EMULATE_X87
    /* CoDUOMP.exe 0x004863b0 and coduo_lnxded 0x080a8f4e return the FLDS
     * result through ST0. Their callers then FSTPS the value, including the
     * original signaling-NaN conversion and status effects. */
    return x87f_store_f32(x87f_load_f32(value));
#else
    return value;
#endif
}

/* Source: CoDUOMP.exe 0x004863c0..0x0048642e.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004863c0_0048642f.mcode. */
void ScriptSave_WriteVector(const vec3_t vector)
{
    WriteFloat(vector[0]);
    WriteFloat(vector[1]);
    WriteFloat(vector[2]);
}

/* Source: CoDUOMP.exe 0x00486430..0x00486486.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00486430_00486487.mcode. */
uintptr_t ScriptLoad_ReadVector(void)
{
    vec3_t vector;
#if defined(WINDOWS_BEHAVIOR)
    script_vector_storage_t *storage;

    /* CoDUOMP.exe 0x00486430 copies all three dwords without entering the x87
     * domain, preserving every serialized binary32 bit pattern verbatim. */
    memcpy(vector, script_serializationCursor, sizeof(vector));
    script_serializationCursor += sizeof(vector);

    storage = MT_Alloc(sizeof(*storage));
    storage->refCount = 0;
    memcpy(storage->value, vector, sizeof(vector));
    return (uintptr_t)storage->value;
#else
    /* coduo_lnxded 0x080a8fa6 calls ReadFloat and stores each returned ST0
     * value before AllocVectorCopy.  Keep the per-lane conversion boundary. */
    vector[0] = ReadFloat();
    vector[1] = ReadFloat();
    vector[2] = ReadFloat();
    return (uintptr_t)AllocVectorCopy(vector);
#endif
}

/* Source: CoDUOMP.exe 0x00486490..0x004864b2.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00486490_004864b3.mcode. */
void ScriptSave_WriteInt(int32_t value)
{
    uint8_t *out = TempMalloc(sizeof(value));

    memcpy(out, &value, sizeof(value));
}

/* Source: CoDUOMP.exe 0x004864c0..0x004864d1. */
int32_t ScriptLoad_ReadInt(void)
{
    int32_t value;

    memcpy(&value, script_serializationCursor, sizeof(value));
    script_serializationCursor += sizeof(value);
    return value;
}

/* Source: CoDUOMP.exe 0x004864e0..0x00486511.
 * This is the first independent entry included in Ghidra's fragmented
 * FUN_004864e0_0048699e record. */
void ScriptSave_WriteCodepos(const uint8_t *codePos)
{
    int32_t offset = -1;

    if (codePos != NULL) {
        uint32_t offsetBits = (uint32_t)((uintptr_t)codePos - (uintptr_t)script_codeBase);

        memcpy(&offset, &offsetBits, sizeof(offset));
    }

    ScriptSave_WriteInt(offset);
}

/* Source: CoDUOMP.exe 0x00486520..0x00486540.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00486520_00486541.mcode. */
uint8_t *ScriptLoad_ReadCodepos(void)
{
    int32_t offset = ScriptLoad_ReadInt();

    /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
    return offset < 0 ? NULL : (uint8_t *)((uintptr_t)script_codeBase + (uint32_t)offset);
}

/* Source: CoDUOMP.exe 0x00486550..0x0048657e.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00486550_0048657f.mcode. */
void ScriptSave_WriteObject(uint16_t object)
{
    WriteShort(script_variableToObjectId[object]);
}

/* Source: CoDUOMP.exe 0x00486580..0x004865ab.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00486580_004865ac.mcode. */
uint16_t ScriptLoad_ReadObject(void)
{
    /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
    uint16_t object = script_objectIdToVariable[ReadShort()];

    AddRefToObject(object);
    return object;
}

/* Source: CoDUOMP.exe 0x004865b0..0x004865da. */
void ScriptSave_WriteData(const void *data, size_t size)
{
    memcpy(TempMalloc(size), data, size);
}

/* Source: CoDUOMP.exe 0x004865e0..0x004865fb. */
void ScriptLoad_ReadData(void *data, size_t size)
{
    memcpy(data, script_serializationCursor, size);
    script_serializationCursor += size;
}

/* Source: CoDUOMP.exe 0x00486660..0x00486723.
 * This is the second independent entry included in Ghidra's fragmented
 * FUN_004864e0_0048699e record. */
void ScriptSave_WriteStack(VariableStackBuffer *frame)
{
    WriteShort(frame->size);
    ScriptSave_WriteInt((int32_t)frame->time);
    ScriptSave_WriteCodepos(frame->pos);
    ScriptSave_WriteObject(frame->localId);

    const VariableStackBufferEntry *entry = frame->entries;
    for (uint16_t count = frame->size; count != 0; --count) {
        VariableValue value;

        ScriptStack_ReadEntryValue(entry, &value);
        ScriptSave_WriteValue(value.type, value.payload);
        ++entry;
    }
}

/* Source: CoDUOMP.exe 0x00486730..0x004867e9.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00486730_004867ea.mcode. */
VariableStackBuffer *ScriptLoad_ReadStack(void)
{
    uint16_t entryCount = ReadShort();
    size_t frameSize = offsetof(VariableStackBuffer, entries) + (size_t)entryCount * sizeof(VariableStackBufferEntry);
#if defined(WINDOWS_BEHAVIOR)
    VariableStackBuffer *frame = MT_Alloc(frameSize);
#else
    VariableStackBuffer *frame = MT_Alloc(frameSize, SCRIPT_SERIALIZATION_MT_PERMANENT);
#endif

    frame->size = entryCount;
    frame->time = (uint32_t)ScriptLoad_ReadInt();
    frame->pos = ScriptLoad_ReadCodepos();
    frame->localId = ScriptLoad_ReadObject();

    VariableStackBufferEntry *entry = frame->entries;
    for (uint16_t count = entryCount; count != 0; --count) {
        VariableValue value;

        ScriptLoad_ReadValue(&value);
        entry->type = (uint8_t)value.type;
        entry->payload = value.payload;
        ++entry;
    }
    return frame;
}

/* Source: CoDUOMP.exe 0x00486900..0x0048699d.
 * This is the third independent entry included in Ghidra's fragmented
 * FUN_004864e0_0048699e record. */
void ScriptSave_WriteValue(script_variable_type_t type, uintptr_t payload)
{
    WriteByte((uint8_t)type);

    switch (type & SCRIPT_SERIALIZATION_VALUE_TYPE_MASK) {
    case SCRIPT_VAR_STRING:
    case SCRIPT_VAR_LOCALIZED_STRING:
        WriteString((uint16_t)payload);
        break;
    case SCRIPT_VAR_VECTOR:
        ScriptSave_WriteVector((const float *)payload);
        break;
    case SCRIPT_VAR_FLOAT: {
        float value;

        memcpy(&value, &payload, sizeof(value));
        WriteFloat(value);
        break;
    }
    case SCRIPT_VAR_INT:
    case SCRIPT_VAR_ANIMATION:
        ScriptSave_WriteInt((int32_t)(uint32_t)payload);
        break;
    case SCRIPT_VAR_CODEPOS:
    case SCRIPT_VAR_FUNCTION:
        ScriptSave_WriteCodepos((const uint8_t *)payload);
        break;
    case SCRIPT_VAR_OBJECT:
        ScriptSave_WriteObject((uint16_t)payload);
        break;
    case SCRIPT_VAR_STACK:
        ScriptSave_WriteStack((VariableStackBuffer *)payload);
        break;
    default:
        break;
    }
}

/* Source: CoDUOMP.exe 0x004869d0..0x00486a7d.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004869d0_00486a7e.mcode. */
void ScriptSave_WriteChildValue(const VariableValue *value, uint32_t name, qboolean parentIsArray)
{
    ScriptSave_WriteValue(value->type, value->payload);

    if (parentIsArray != qfalse && name <= SCRIPT_SERIALIZATION_OBJECT_NAME_LIMIT) {
        if (name <= SCRIPT_SERIALIZATION_SHORT_NAME_LIMIT) {
            WriteByte(SCRIPT_CHILD_NAME_STRING);
            WriteString((uint16_t)name);
        } else {
            WriteByte(SCRIPT_CHILD_NAME_OBJECT);
            ScriptSave_WriteObject((uint16_t)name);
        }
    } else if (name <= SCRIPT_SERIALIZATION_SHORT_NAME_LIMIT) {
        WriteByte(SCRIPT_CHILD_NAME_SHORT);
        WriteShort((uint16_t)name);
    } else {
        WriteByte(SCRIPT_CHILD_NAME_INT);
        ScriptSave_WriteInt((int32_t)name);
    }
}

/* Source: CoDUOMP.exe 0x00487530..0x00487558.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00487530_00487559.mcode.
 * Role name: the Mac traceback table has no separate symbol. MSVC retains
 * this root-value writer and also inlines it into Scr_SavePost. */
void ScriptSave_WriteRootValue(void)
{
    script_variable_node_t *node = &script_variableNodes[script_animArrayHandle];

    ScriptSave_WriteValue((script_variable_type_t)(node->packedTypeIndex & SCRIPT_SERIALIZATION_PERSISTENT_TYPE_MASK),
                          node->payload.valuePayload);
}

/* Source: CoDUOMP.exe 0x00486a80..0x00486af2.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00486a80_00486af3.mcode. */
void ScriptLoad_ReadValue(VariableValue *value)
{
    value->type = *script_serializationCursor++;

    switch (value->type & SCRIPT_SERIALIZATION_VALUE_TYPE_MASK) {
    case SCRIPT_VAR_STRING:
    case SCRIPT_VAR_LOCALIZED_STRING:
        value->payload = ReadString();
        break;
    case SCRIPT_VAR_VECTOR:
        value->payload = ScriptLoad_ReadVector();
        break;
    case SCRIPT_VAR_FLOAT: {
        /* Reconstruction transcription fix: CoDUOMP.exe
         * 0x00486abb..0x00486ac8 and coduo_lnxded
         * 0x080a95f0..0x080a95fa both load/store one binary32 value and exit
         * this switch arm. The former Windows branch omitted that exit and
         * incorrectly fell through to the integer reader. */
        float floatValue = ReadFloat();

        value->payload = 0;
        memcpy(&value->payload, &floatValue, sizeof(floatValue));
        break;
    }
    case SCRIPT_VAR_INT:
    case SCRIPT_VAR_ANIMATION:
        value->payload = (uint32_t)ScriptLoad_ReadInt();
        break;
    case SCRIPT_VAR_CODEPOS:
    case SCRIPT_VAR_FUNCTION:
        value->payload = (uintptr_t)ScriptLoad_ReadCodepos();
        break;
    case SCRIPT_VAR_OBJECT:
        value->payload = ScriptLoad_ReadObject();
        break;
    case SCRIPT_VAR_STACK:
        value->payload = (uintptr_t)ScriptLoad_ReadStack();
        break;
    default:
        /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
        break;
    }
}

/* Source: CoDUOMP.exe 0x00486b20..0x00486b7c.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00486b20_00486b7d.mcode. */
uint32_t ScriptLoad_ReadChildValue(VariableValue *value)
{
    ScriptLoad_ReadValue(value);

    script_serialization_child_name_type_t nameType = (script_serialization_child_name_type_t)*script_serializationCursor++;
    switch (nameType) {
    case SCRIPT_CHILD_NAME_SHORT:
        return ReadShort();
    case SCRIPT_CHILD_NAME_INT:
        return (uint32_t)ScriptLoad_ReadInt();
    case SCRIPT_CHILD_NAME_STRING:
        return ReadString();
    case SCRIPT_CHILD_NAME_OBJECT:
        return SCRIPT_SERIALIZATION_OBJECT_NAME_BASE + ScriptLoad_ReadObject();
    default:
        return 0;
    }
}

/* Source: CoDUOMP.exe 0x00486f70..0x004871b2.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00486f70_004871b3.mcode. */
void ScriptSave_WriteObjectRecord(uint16_t object)
{
    script_variable_node_t *objectNode = &script_variableNodes[object];
    script_variable_type_t savedType = (script_variable_type_t)(objectNode->packedTypeIndex & SCRIPT_SERIALIZATION_PERSISTENT_TYPE_MASK);
    script_variable_type_t baseType = savedType & SCRIPT_SERIALIZATION_VALUE_TYPE_MASK;

    WriteByte((uint8_t)savedType);

    if (baseType == SCRIPT_VAR_THREAD) {
        ScriptSave_WriteObject(objectNode->payload.halves.parentHandle);
        ScriptSave_WriteOptionalString((uint16_t)(objectNode->packedTypeIndex >> SCRIPT_SERIALIZATION_NAME_SHIFT));
    } else if (baseType == SCRIPT_VAR_ENTITY || baseType == SCRIPT_VAR_DEAD_ENTITY) {
        WriteShort(objectNode->payload.halves.parentHandle);
        WriteShort((uint16_t)(objectNode->packedTypeIndex >> SCRIPT_SERIALIZATION_NAME_SHIFT));
    }

    qboolean parentIsArray = baseType == SCRIPT_VAR_ARRAY ? qtrue : qfalse;
    uint16_t childCount = 0;
    for (uint16_t child = FindNextSibling(object); child != 0; child = FindNextSibling(child)) {
        ++childCount;
    }
    WriteShort(childCount);

    for (uint16_t child = FindNextSibling(object); child != 0; child = FindNextSibling(child)) {
        script_variable_node_t *childNode = &script_variableNodes[child];
        VariableValue value = {.payload = childNode->payload.valuePayload,
                               .type = (script_variable_type_t)(childNode->packedTypeIndex & SCRIPT_SERIALIZATION_PERSISTENT_TYPE_MASK)};

        ScriptSave_WriteChildValue(&value, childNode->packedTypeIndex >> SCRIPT_SERIALIZATION_NAME_SHIFT, parentIsArray);
    }
}

/* Source: CoDUOMP.exe 0x004871c0..0x0048737a.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004871c0_0048737b.mcode. */
void ScriptLoad_ReadObjectRecord(uint16_t object)
{
    script_variable_node_t *objectNode = &script_variableNodes[object];
    script_variable_type_t savedType = (script_variable_type_t)*script_serializationCursor++;

    objectNode->packedTypeIndex &= ~(uint32_t)SCRIPT_SERIALIZATION_VALUE_TYPE_MASK;
    objectNode->packedTypeIndex |= (uint32_t)savedType;

    switch (savedType & SCRIPT_SERIALIZATION_VALUE_TYPE_MASK) {
    case SCRIPT_VAR_THREAD:
        objectNode->payload.halves.parentHandle = ScriptLoad_ReadObject();
        objectNode->packedTypeIndex |= (uint32_t)ScriptLoad_ReadOptionalString() << SCRIPT_SERIALIZATION_NAME_SHIFT;
        break;
    case SCRIPT_VAR_ENTITY:
    case SCRIPT_VAR_DEAD_ENTITY:
        objectNode->payload.halves.parentHandle = ReadShort();
        objectNode->packedTypeIndex |= (uint32_t)ReadShort() << SCRIPT_SERIALIZATION_NAME_SHIFT;
        break;
    case SCRIPT_VAR_ARRAY:
        objectNode->payload.halves.parentHandle = 0;
        break;
    default:
        break;
    }

    qboolean parentIsArray = (savedType & SCRIPT_SERIALIZATION_VALUE_TYPE_MASK) == SCRIPT_VAR_ARRAY ? qtrue : qfalse;
    uint16_t childCount = ReadShort();

    for (uint16_t index = 0; index < childCount; ++index) {
        VariableValue value;
        uint32_t name = ScriptLoad_ReadChildValue(&value);
        uint16_t childIndirection = GetVariableIndexInternal(object, name);
        uint16_t child = script_variableIndirections[childIndirection].valueIndex;
        script_variable_node_t *childNode = &script_variableNodes[child];

        if (parentIsArray != qfalse) {
            if (name <= SCRIPT_SERIALIZATION_SHORT_NAME_LIMIT) {
                SL_RemoveRefToString((uint16_t)name);
            } else if (name <= SCRIPT_SERIALIZATION_OBJECT_NAME_LIMIT) {
                RemoveRefToObject((uint16_t)name);
            }
        }

        childNode->packedTypeIndex |= (uint32_t)value.type;
        childNode->payload.valuePayload = value.payload;
    }
}

/* Source: CoDUOMP.exe 0x00487560..0x004875b9.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00487560_004875ba.mcode. */
void ScriptLoad_ReadRootValue(void)
{
    uint16_t valueHandle = AllocValue();
    VariableValue value;

    /* CoDUOMP.exe 0x00487560 inlines AllocValue as AllocVariable plus the
     * occupied flags. coduo_lnxded 0x080a9e6e calls AllocValue at 0x080a69fa;
     * the old Linux reconstruction incorrectly allocated an array object. */
    script_animArrayHandle = valueHandle;
    ScriptLoad_ReadValue(&value);
    SetNewVariableValue(script_animArrayHandle, &value);
}

/* Source: CoDUOMP.exe 0x00487380..0x00487527.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00487380_00487528.mcode. */
void Scr_SavePre(void)
{
    script_variableToObjectId = Hunk_AllocateTempMemoryHighInternal(SCRIPT_SERIALIZATION_FULL_OBJECT_ID_MAP_BYTES);
    script_objectIdToVariable = Hunk_AllocateTempMemoryHighInternal(SCRIPT_SERIALIZATION_FULL_OBJECT_ID_MAP_BYTES);
    Com_Memset(script_variableToObjectId, 0, SCRIPT_SERIALIZATION_FULL_OBJECT_ID_MAP_BYTES);
    script_savedObjectCount = 0;

    ScriptSave_PrepareObject(script_levelHandle);
    ScriptSave_PrepareObject(script_gameHandle);
    ScriptSave_PrepareObject(script_timeArrayHandle);
    ScriptSave_PrepareObject(script_pauseArrayHandle);

    for (uint32_t index = 0; index < script_entityTypeUsageCount; ++index) {
        uint16_t entityType = FindObject(FindVariable(script_entityTypeClassMapRoot, index));
        ScriptSave_PrepareObject(entityType);

        uint16_t classType = FindObject(FindVariable(script_classMapRoot, index));
        ScriptSave_PrepareObject(classType);
    }

    script_variable_node_t *animNode = &script_variableNodes[script_animArrayHandle];
    VariableValue animValue = {.payload = animNode->payload.valuePayload, .type = (script_variable_type_t)animNode->packedTypeIndex};
    ScriptSave_PrepareValueObjectRefs(&animValue);
}

/* Source: CoDUOMP.exe 0x004875c0..0x0048789e.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004875c0_0048789f.mcode. */
void Scr_SavePost(void)
{
    ScriptSave_WriteInt((int32_t)script_currentTimeKey);
    WriteShort(script_savedObjectCount);

    for (uint32_t objectId = 1; objectId <= script_savedObjectCount; ++objectId) {
        ScriptSave_WriteObjectRecord(script_objectIdToVariable[objectId]);
    }

    ScriptSave_WriteRootValue();
    ScriptSave_WriteObject(script_levelHandle);
    ScriptSave_WriteObject(script_gameHandle);
    ScriptSave_WriteObject(script_timeArrayHandle);
    ScriptSave_WriteObject(script_pauseArrayHandle);

    for (uint32_t index = 0; index < script_entityTypeUsageCount; ++index) {
        uint16_t entityType = FindObject(FindVariable(script_entityTypeClassMapRoot, index));
        ScriptSave_WriteObject(entityType);
    }
}

/* Source: CoDUOMP.exe 0x004878a0..0x004878aa. */
void Scr_SaveShutdown(void)
{
    Hunk_ClearTempMemoryHigh();
}

/* Source: CoDUOMP.exe 0x004878b0..0x00487ba6.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004878b0_00487ba7.mcode. */
void Scr_LoadPre(void)
{
    uint32_t saveDataSize;

    script_serializationCursor = Scr_LoadRead(sizeof(saveDataSize));
    memcpy(&saveDataSize, script_serializationCursor, sizeof(saveDataSize));
    /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
    script_serializationCursor = Scr_LoadRead(saveDataSize);

    script_currentTimeKey = (uint32_t)ScriptLoad_ReadInt();
    script_savedObjectCount = ReadShort();
    script_variableToObjectId = Hunk_AllocateTempMemoryInternal(SCRIPT_SERIALIZATION_FULL_OBJECT_ID_MAP_BYTES);
    script_objectIdToVariable =
        Hunk_AllocateTempMemoryInternal(((size_t)script_savedObjectCount + 1) * sizeof(script_objectIdToVariable[0]));

    for (uint32_t objectId = 1; objectId <= script_savedObjectCount; ++objectId) {
        uint16_t object = AllocObject();

        script_objectIdToVariable[objectId] = object;
        script_variableToObjectId[object] = (uint16_t)objectId;
    }

    for (uint32_t objectId = 1; objectId <= script_savedObjectCount; ++objectId) {
        ScriptLoad_ReadObjectRecord(script_objectIdToVariable[objectId]);
    }

    ScriptLoad_ReadRootValue();
    script_levelHandle = ScriptLoad_ReadObject();
    script_gameHandle = ScriptLoad_ReadObject();
    script_timeArrayHandle = ScriptLoad_ReadObject();
    script_pauseArrayHandle = ScriptLoad_ReadObject();

    for (uint32_t index = 0; index < script_entityTypeUsageCount; ++index) {
        VariableValue value = {.payload = ScriptLoad_ReadObject(), .type = SCRIPT_VAR_OBJECT};
        uint16_t classEntry = FindVariable(script_entityTypeClassMapRoot, index);

        SetVariableValue(classEntry, &value);
    }
}

/* Source: CoDUOMP.exe 0x00486b90..0x00486cc0.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00486b90_00486cc1.mcode. */
void ScriptSave_PrepareObject(uint16_t object)
{
    if (script_variableToObjectId[object] != 0) {
        return;
    }

    script_savedObjectCount++;
    script_variableToObjectId[object] = script_savedObjectCount;
    script_objectIdToVariable[script_savedObjectCount] = object;

    script_variable_node_t *objectNode = &script_variableNodes[object];
    uint32_t packedTypeIndex = objectNode->packedTypeIndex;
    qboolean parentIsArray = (packedTypeIndex & SCRIPT_SERIALIZATION_VALUE_TYPE_MASK) == SCRIPT_VAR_ARRAY ? qtrue : qfalse;

    for (uint16_t child = FindNextSibling(object); child != 0; child = FindNextSibling(child)) {
        script_variable_node_t *childNode = &script_variableNodes[child];

        if (parentIsArray != qfalse) {
            uint32_t name = childNode->packedTypeIndex >> SCRIPT_SERIALIZATION_NAME_SHIFT;
            if (name > SCRIPT_SERIALIZATION_SHORT_NAME_LIMIT && name <= SCRIPT_SERIALIZATION_OBJECT_NAME_LIMIT) {
                ScriptSave_PrepareObject((uint16_t)name);
            }
        }

        VariableValue childValue = {.payload = childNode->payload.valuePayload, .type = (script_variable_type_t)childNode->packedTypeIndex};
        ScriptSave_PrepareValueObjectRefs(&childValue);
    }

    if ((packedTypeIndex & SCRIPT_SERIALIZATION_VALUE_TYPE_MASK) == SCRIPT_VAR_THREAD) {
        ScriptSave_PrepareObject(objectNode->payload.halves.parentHandle);
    }
}

/* Source: CoDUOMP.exe 0x00486600..0x00486626.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00486600_00486627.mcode. */
uint16_t Scr_ConvertThreadToSave(uint16_t thread)
{
    if (thread == 0) {
        return 0;
    }

    ScriptSave_PrepareObject(thread);
    return script_variableToObjectId[thread];
}

/* Source: CoDUOMP.exe 0x00486630..0x00486655.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00486630_00486656.mcode. */
uint16_t Scr_ConvertThreadFromLoad(uint16_t objectId)
{
    if (objectId == 0) {
        return 0;
    }

    uint16_t thread = script_objectIdToVariable[objectId];
    AddRefToObject(thread);
    return thread;
}

/* Source: CoDUOMP.exe 0x00487bb0..0x00487cb8.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00487bb0_00487cb9.mcode. */
void Scr_LoadShutdown(void)
{
    for (uint32_t objectId = 1; objectId <= script_savedObjectCount; ++objectId) {
        RemoveRefToObject(script_objectIdToVariable[objectId]);
    }

    Hunk_FreeTempMemory(script_objectIdToVariable);
    Hunk_FreeTempMemory(script_variableToObjectId);
}
