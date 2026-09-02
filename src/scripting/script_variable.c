#include "script_variable.h"
#include "script_runtime_host.h"
#include "script_string.h"

enum {
    SCRIPT_VARIABLE_TYPE_MASK = 31,
    SCRIPT_VARIABLE_NAME_SHIFT = 8,
    SCRIPT_VARIABLE_CHILD_COPY_OCCUPANCY_BITS = 96,
    SCRIPT_VARIABLE_OCCUPIED_MASK = 96,
    SCRIPT_VARIABLE_PRIMARY_BUCKET_BITS = 64,
    SCRIPT_VARIABLE_COLLISION_NODE_BITS = 32,
    SCRIPT_VARIABLE_HASH_MODULUS = 65535,
    SCRIPT_VARIABLE_HASH_PARENT_MULTIPLIER = 31,
    SCRIPT_VARIABLE_OBJECT_KEY_BASE = 65536,
    SCRIPT_VARIABLE_OBJECT_KEY_LIMIT = 131072,
    SCRIPT_VARIABLE_ENTITY_KEY_BIAS = 0x800000,
    SCRIPT_VARIABLE_ENTITY_KEY_MASK = 0xffffff,
    SCRIPT_VARIABLE_ENTITY_KEY_TEST_BIAS = 0x7e0000,
    SCRIPT_VARIABLE_ENTITY_KEY_TEST_LIMIT = 0xfdffff,
    SCRIPT_VARIABLE_PACKED_LOW_BYTE_MASK = 255,
    SCRIPT_CLASS_FIELD_STRING_TYPE = 15,
    SCRIPT_ENTITY_NUM_MASK = 65535
};

/* Source: CoDUOMP.exe 0x00483360..0x004833c5.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00483360_004833c6.mcode. */
void InitVariables(void)
{
    uint16_t previous = 0;

    for (uint32_t index = 1; index < SCRIPT_VARIABLE_NODE_COUNT; ++index) {
        script_variable_node_t *node = &script_variableNodes[index];

        node->packedTypeIndex = 0;
        script_variableNodes[previous].hashOrFreeNext = (uint16_t)index;
        node->payload.halves.valueOrRefCount = previous;
        script_variableIndirections[index].valueIndex = (uint16_t)index;
        previous = (uint16_t)index;
    }

    script_variableNodes[0].packedTypeIndex = 0;
    script_variableNodes[previous].hashOrFreeNext = 0;
    script_variableIndirections[0].valueIndex = 0;
    script_variableNodes[0].payload.halves.valueOrRefCount = previous;
}

/* Source: CoDUOMP.exe 0x004833d0..0x00483437.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004833d0_00483438.mcode. */
void Var_Init(void)
{
    SL_Init();
    InitVariables();
    script_classMapRoot = AllocObject();
    script_entityTypeClassMapRoot = AllocObject();
}

/* Source: CoDUOMP.exe 0x00483450..0x00483465.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00483450_00483466.mcode.
 * Name and argument: exact same-module Mac symbol GetVariableKeyObject. */
uint32_t GetVariableKeyObject(uint16_t handle)
{
    return (script_variableNodes[handle].packedTypeIndex >> SCRIPT_VARIABLE_NAME_SHIFT) - SCRIPT_VARIABLE_OBJECT_KEY_BASE;
}

/* Source: CoDUOMP.exe 0x00483470..0x00483480.
 * Name and argument: exact same-module Mac symbol GetEntityType. */
uint32_t GetEntityType(uint16_t handle)
{
    return script_variableNodes[handle].packedTypeIndex >> SCRIPT_VARIABLE_NAME_SHIFT;
}

/* Source: CoDUOMP.exe 0x00483490..0x00483537.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00483490_00483538.mcode.
 * This low-level entry returns an indirection-table index; zero means that
 * no child with the requested name exists. */
uint16_t FindVariableIndexInternal(uint16_t parent, uint32_t name)
{
    uint16_t bucket = (uint16_t)(((uint32_t)parent * SCRIPT_VARIABLE_HASH_PARENT_MULTIPLIER + name) % SCRIPT_VARIABLE_HASH_MODULUS + 1);
    Variable *bucketSlot = &script_variableIndirections[bucket];
    uint16_t nodeIndex = bucketSlot->valueIndex;
    script_variable_node_t *node = &script_variableNodes[nodeIndex];

    if ((node->packedTypeIndex & SCRIPT_VARIABLE_OCCUPIED_MASK) != SCRIPT_VARIABLE_PRIMARY_BUCKET_BITS) {
        return 0;
    }
    if (GetVariableName(nodeIndex) == name) {
        return bucket;
    }

    uint16_t chainLink = node->hashOrFreeNext;
    Variable *chainSlot = &script_variableIndirections[chainLink];
    nodeIndex = chainSlot->valueIndex;
    while (chainSlot != bucketSlot) {
        if (GetVariableName(nodeIndex) == name) {
            return chainLink;
        }
        chainLink = script_variableNodes[nodeIndex].hashOrFreeNext;
        chainSlot = &script_variableIndirections[chainLink];
        nodeIndex = chainSlot->valueIndex;
    }
    return 0;
}

/* Source: CoDUOMP.exe 0x00483540..0x00483855.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00483540_00483856.mcode. */
uint16_t GetVariableIndexInternal(uint16_t parent, uint32_t name)
{
    uint16_t bucket = (uint16_t)(((uint32_t)parent * SCRIPT_VARIABLE_HASH_PARENT_MULTIPLIER + name) % SCRIPT_VARIABLE_HASH_MODULUS + 1);
    Variable *bucketSlot = &script_variableIndirections[bucket];
    uint16_t nodeIndex = bucketSlot->valueIndex;
    script_variable_node_t *node = &script_variableNodes[nodeIndex];
    uint32_t occupancyBits = node->packedTypeIndex & SCRIPT_VARIABLE_OCCUPIED_MASK;
    uint16_t childIndirection = bucket;
    qboolean makePrimaryBucketNode = qtrue;

    if (occupancyBits == SCRIPT_VARIABLE_PRIMARY_BUCKET_BITS) {
        if (GetVariableName(nodeIndex) == name) {
            return bucket;
        }

        childIndirection = node->hashOrFreeNext;
        Variable *chainSlot = &script_variableIndirections[childIndirection];
        uint16_t chainNodeIndex = chainSlot->valueIndex;
        while (chainSlot != bucketSlot) {
            if (GetVariableName(chainNodeIndex) == name) {
                return childIndirection;
            }
            childIndirection = script_variableNodes[chainNodeIndex].hashOrFreeNext;
            chainSlot = &script_variableIndirections[childIndirection];
            chainNodeIndex = chainSlot->valueIndex;
        }

        node = AllocVariable();
        childIndirection = node->hashOrFreeNext;
        node->packedTypeIndex = SCRIPT_VARIABLE_COLLISION_NODE_BITS;
        node->hashOrFreeNext = script_variableNodes[bucketSlot->valueIndex].hashOrFreeNext;
        script_variableNodes[bucketSlot->valueIndex].hashOrFreeNext = childIndirection;
        bucketSlot = &script_variableIndirections[childIndirection];
        makePrimaryBucketNode = qfalse;
    } else if (occupancyBits == 0) {
        uint16_t previousFree = node->payload.halves.valueOrRefCount;
        uint16_t nextFree = node->hashOrFreeNext;

        script_variableNodes[script_variableIndirections[previousFree].valueIndex].hashOrFreeNext = nextFree;
        script_variableNodes[script_variableIndirections[nextFree].valueIndex].payload.halves.valueOrRefCount = previousFree;
    } else {
        script_variable_node_t *newNode = AllocVariable();
        uint16_t newNodeIndex = (uint16_t)(newNode - script_variableNodes);
        uint16_t newIndirection = newNode->hashOrFreeNext;
        script_variable_node_t *oldNode = node;
        uint16_t oldNodeIndex = nodeIndex;
        uint16_t oldPrevious = bucketSlot->previousSibling;
        uint16_t oldNext = oldNode->nextSibling;

        script_variableNodes[script_variableIndirections[oldPrevious].valueIndex].nextSibling = newIndirection;
        script_variableIndirections[oldNext].previousSibling = newIndirection;

        if (occupancyBits == SCRIPT_VARIABLE_COLLISION_NODE_BITS) {
            uint16_t previousCollisionNode = script_variableIndirections[oldNode->hashOrFreeNext].valueIndex;
            while (script_variableNodes[previousCollisionNode].hashOrFreeNext != bucket) {
                previousCollisionNode = script_variableIndirections[script_variableNodes[previousCollisionNode].hashOrFreeNext].valueIndex;
            }
            script_variableNodes[previousCollisionNode].hashOrFreeNext = newIndirection;
        } else {
            oldNode->hashOrFreeNext = newIndirection;
        }

        script_variableIndirections[newIndirection].previousSibling = bucketSlot->previousSibling;
        script_variableIndirections[newIndirection].valueIndex = oldNodeIndex;
        bucketSlot->valueIndex = newNodeIndex;
        node = newNode;
    }

    if (makePrimaryBucketNode != qfalse) {
        node->packedTypeIndex = SCRIPT_VARIABLE_PRIMARY_BUCKET_BITS;
        node->hashOrFreeNext = childIndirection;
    }

    script_variable_node_t *parentNode = &script_variableNodes[parent];
    uint16_t oldFirstChild = parentNode->nextSibling;
    node->nextSibling = oldFirstChild;
    script_variableIndirections[oldFirstChild].previousSibling = childIndirection;
    bucketSlot->previousSibling = parentNode->hashOrFreeNext;
    parentNode->nextSibling = childIndirection;

    node->packedTypeIndex = (uint8_t)node->packedTypeIndex;
    node->packedTypeIndex |= name << SCRIPT_VARIABLE_NAME_SHIFT;

    if ((parentNode->packedTypeIndex & SCRIPT_VARIABLE_TYPE_MASK) == SCRIPT_VAR_ARRAY) {
        ++parentNode->payload.halves.parentHandle;
        if (name < SCRIPT_VARIABLE_OBJECT_KEY_BASE) {
            SL_AddRefToString((uint16_t)name);
        } else if (name < SCRIPT_VARIABLE_OBJECT_KEY_LIMIT) {
            AddRefToObject((uint16_t)name);
        }
    }

    return childIndirection;
}

/* Source: CoDUOMP.exe 0x00484590..0x004845a5.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00484590_004845a6.mcode. */
uint16_t GetVariable(uint16_t parent, uint32_t name)
{
    uint16_t indirection = GetVariableIndexInternal(parent, name);
    return script_variableIndirections[indirection].valueIndex;
}

/* Source: CoDUOMP.exe 0x004845b0..0x004845cd.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004845b0_004845ce.mcode. */
uint16_t GetObjectVariable(uint16_t parent, uint16_t object)
{
    uint16_t indirection = GetVariableIndexInternal(parent, SCRIPT_VARIABLE_OBJECT_KEY_BASE + object);
    return script_variableIndirections[indirection].valueIndex;
}

/* Source: CoDUOMP.exe 0x00484330..0x00484348.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00484330_00484349.mcode. */
uint16_t FindVariable(uint16_t parent, uint32_t name)
{
    uint16_t indirection = FindVariableIndexInternal(parent, name);
    return script_variableIndirections[indirection].valueIndex;
}

/* Source: CoDUOMP.exe 0x00484350..0x00484370.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00484350_00484371.mcode. */
uint16_t FindObjectVariable(uint16_t parent, uint16_t object)
{
    uint16_t indirection = FindVariableIndexInternal(parent, SCRIPT_VARIABLE_OBJECT_KEY_BASE + object);
    return script_variableIndirections[indirection].valueIndex;
}

/* Source: CoDUOMP.exe 0x004842c0..0x004842cf.
 * Evidence: original .text bytes/disassembly; Ghidra left this complete
 * function inside executable_gaps.mcode as UNDEFINED_BYTES.
 * Exact same-module Mac signature: IsValidArrayIndex(unsigned int). */
qboolean IsValidArrayIndex(uint32_t name)
{
    return name + SCRIPT_VARIABLE_ENTITY_KEY_TEST_BIAS <= SCRIPT_VARIABLE_ENTITY_KEY_TEST_LIMIT ? qtrue : qfalse;
}

/* Source: CoDUOMP.exe 0x004842d0..0x004842da.
 * Evidence: original .text bytes/disassembly; Ghidra left this complete
 * function inside executable_gaps.mcode as UNDEFINED_BYTES.
 * Exact same-module Mac signature: GetInternalVariableIndex(unsigned int). */
uint32_t GetInternalVariableIndex(uint32_t name)
{
    return (name - SCRIPT_VARIABLE_ENTITY_KEY_BIAS) & SCRIPT_VARIABLE_ENTITY_KEY_MASK;
}

/* Source: CoDUOMP.exe 0x004842e0..0x004842f7.
 * Evidence: original .text bytes/disassembly; Ghidra left this complete
 * function inside executable_gaps.mcode as UNDEFINED_BYTES. Exact same-module
 * Mac signature: FindArrayVariableIndex(unsigned short, unsigned int). */
uint16_t FindArrayVariableIndex(uint16_t parent, uint32_t name)
{
    return FindVariableIndexInternal(parent, GetInternalVariableIndex(name));
}

/* Source: CoDUOMP.exe 0x00484300..0x00484322.
 * Evidence: original .text bytes/disassembly; Ghidra left this complete
 * function inside executable_gaps.mcode as UNDEFINED_BYTES. */
uint16_t FindArrayVariable(uint16_t parent, int32_t name)
{
    uint16_t indirection = FindArrayVariableIndex(parent, name);
    return script_variableIndirections[indirection].valueIndex;
}

/* Source: CoDUOMP.exe 0x00484380..0x00484394.
 * Evidence: original .text bytes/disassembly; Ghidra left this complete
 * function inside executable_gaps.mcode as UNDEFINED_BYTES. Exact same-module
 * Mac signature: GetArrayVariableIndex(unsigned short, unsigned int). */
uint16_t GetArrayVariableIndex(uint16_t parent, uint32_t name)
{
    return GetVariableIndexInternal(parent, GetInternalVariableIndex(name));
}

/* Source: CoDUOMP.exe 0x00484550..0x0048456f.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00484550_00484570.mcode.
 * Name: exact same-module Mac overload GetArrayVariable__FUsi. The descriptive
 * maintained C name distinguishes it from the unsigned-key overload below. */
uint16_t GetArrayVariable(uint16_t parent, int32_t name)
{
    uint16_t indirection = GetVariableIndexInternal(parent, GetInternalVariableIndex(name));
    return script_variableIndirections[indirection].valueIndex;
}

/* Source: CoDUOMP.exe 0x00484570..0x0048458f.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00484570_00484590.mcode.
 * Name: exact same-module Mac overload GetArrayVariable__FUsUi. MSVC emits
 * the same normalization sequence as the signed-key overload. */
uint16_t GetArrayVariableUnsigned(uint16_t parent, uint32_t name)
{
    uint16_t indirection = GetVariableIndexInternal(parent, (name - SCRIPT_VARIABLE_ENTITY_KEY_BIAS) & SCRIPT_VARIABLE_ENTITY_KEY_MASK);
    return script_variableIndirections[indirection].valueIndex;
}

/* Source: CoDUOMP.exe 0x004843a0..0x004844ba.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004843a0_004844bb.mcode. */
uint16_t GetVariableField(uint16_t parent, uint16_t name)
{
    uint16_t child = FindVariable(parent, name);
    if (child != 0) {
        return child;
    }

    script_variable_node_t *parentNode = &script_variableNodes[parent];
    script_variable_type_t parentType = GetVarType(parent);
    if (parentType >= SCRIPT_VAR_DEAD_THREAD) {
        ClearVariableValue(script_tempValueHandle);
        return script_tempValueHandle;
    }
    if (parentType != SCRIPT_VAR_ENTITY) {
        return GetVariable(parent, name);
    }

    uint16_t classRoot = script_entityTypeUsageRecords[GetVariableName(parent)].classnum;
    uint16_t builtinField = FindArrayVariable(classRoot, name);
    if (builtinField == 0) {
        return GetVariable(parent, name);
    }

    script_variable_node_t *tempNode = &script_variableNodes[script_tempValueHandle];
    ClearVariableValue(script_tempValueHandle);
    /* The original masks with 0xffffff08 (AND EDX,0xffffff08 at 0x0048446e) —
     * clearing the low byte except bit 3 — then ORs 0x8 (0x00484485). Match the
     * literal mask rather than ~0xff; the subsequent OR makes the result equal
     * but the machine code keeps bit 3 in the mask itself. */
    tempNode->packedTypeIndex = (parentNode->packedTypeIndex & 0xffffff08u) | tempNode->packedTypeIndex | SCRIPT_VAR_KEY_VALUE;
    tempNode->payload.halves.valueOrRefCount = parentNode->payload.halves.parentHandle;
    tempNode->payload.halves.parentHandle = script_variableNodes[builtinField].payload.halves.parentHandle;
    return script_tempValueHandle;
}

/* Source: CoDUOMP.exe 0x004844c0..0x00484540.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004844c0_00484541.mcode. */
void ClearVariableField(uint16_t parent, uint16_t name)
{
    if (FindVariable(parent, name) != 0) {
        RemoveVariable(parent, name);
        return;
    }

    script_variable_type_t parentType = GetVarType(parent);
    if (parentType >= SCRIPT_VAR_DEAD_THREAD || parentType != SCRIPT_VAR_ENTITY) {
        return;
    }

    uint16_t classRoot = script_entityTypeUsageRecords[GetVariableName(parent)].classnum;
    if (FindArrayVariable(classRoot, name) != 0) {
        Scr_Error("cannot set entity builtin key/value to undefined");
    }
}

/* Source: CoDUOMP.exe 0x00483860..0x004839c7.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00483860_004839c8.mcode. */
void MakeVariableExternal(Variable *slot, script_variable_node_t *parentNode)
{
    uint16_t indirection = (uint16_t)(slot - script_variableIndirections);
    uint16_t nodeIndex = slot->valueIndex;
    script_variable_node_t *node = &script_variableNodes[nodeIndex];

    if ((parentNode->packedTypeIndex & SCRIPT_VARIABLE_TYPE_MASK) == SCRIPT_VAR_ARRAY) {
        --parentNode->payload.halves.parentHandle;
        uint32_t name = GetVariableName(nodeIndex);

        if (name < SCRIPT_VARIABLE_OBJECT_KEY_BASE) {
            SL_RemoveRefToString((uint16_t)name);
        } else if (name < SCRIPT_VARIABLE_OBJECT_KEY_LIMIT) {
            RemoveRefToObject((uint16_t)name);
        }
    }

    if ((node->packedTypeIndex & SCRIPT_VARIABLE_OCCUPIED_MASK) == SCRIPT_VARIABLE_PRIMARY_BUCKET_BITS) {
        uint16_t replacementIndirection = node->hashOrFreeNext;
        Variable *replacementSlot = &script_variableIndirections[replacementIndirection];

        if (replacementSlot != slot) {
            uint16_t replacementNodeIndex = replacementSlot->valueIndex;
            script_variable_node_t *replacementNode = &script_variableNodes[replacementNodeIndex];
            uint16_t removedPrevious = slot->previousSibling;
            uint16_t removedNext = node->nextSibling;
            uint16_t replacementPrevious = replacementSlot->previousSibling;

            /* The original clears only the collision bit (AND ~0x20 at
             * 0x004838ee) before setting the primary-bucket bit, not the whole
             * occupancy mask; identical result for a promoted chain node but
             * this matches the machine code. */
            replacementNode->packedTypeIndex &= ~(uint32_t)SCRIPT_VARIABLE_COLLISION_NODE_BITS;
            replacementNode->packedTypeIndex |= SCRIPT_VARIABLE_PRIMARY_BUCKET_BITS;

            script_variableIndirections[replacementNode->nextSibling].previousSibling = indirection;
            script_variableNodes[script_variableIndirections[replacementPrevious].valueIndex].nextSibling = indirection;
            script_variableIndirections[removedNext].previousSibling = replacementIndirection;
            script_variableNodes[script_variableIndirections[removedPrevious].valueIndex].nextSibling = replacementIndirection;

            Variable removedSlot = *slot;
            *slot = *replacementSlot;
            *replacementSlot = removedSlot;
            indirection = replacementIndirection;
        }
    } else {
        Variable *previousSlot = slot;
        Variable *chainSlot = slot;

        do {
            previousSlot = chainSlot;
            chainSlot = &script_variableIndirections[node->hashOrFreeNext];
            node = &script_variableNodes[chainSlot->valueIndex];
        } while (chainSlot != slot);

        script_variableNodes[previousSlot->valueIndex].hashOrFreeNext = node->hashOrFreeNext;
        node = &script_variableNodes[nodeIndex];
    }

    /* The original is a bare OR 0x60 (0x0048395f) with no preceding clear;
     * (x & ~0x60) | 0x60 == x | 0x60 for all x, so drop the redundant AND to
     * match the machine code. */
    node->packedTypeIndex |= SCRIPT_VARIABLE_OCCUPIED_MASK;
    node->hashOrFreeNext = indirection;
}

/* Source: CoDUOMP.exe 0x004839d0..0x00483a6c.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004839d0_00483a6d.mcode. */
void ClearObjectInternal(uint16_t object)
{
    script_variable_node_t *objectNode = &script_variableNodes[object];
    Variable *slot = &script_variableIndirections[objectNode->nextSibling];
    uint16_t child = slot->valueIndex;

    while (child != object) {
        MakeVariableExternal(slot, objectNode);
        slot = &script_variableIndirections[script_variableNodes[child].nextSibling];
        child = slot->valueIndex;
    }

    child = script_variableIndirections[objectNode->nextSibling].valueIndex;
    while (child != object) {
        uint16_t next = script_variableIndirections[script_variableNodes[child].nextSibling].valueIndex;
        FreeValueInternal(&script_variableNodes[child]);
        child = next;
    }
}

/* Source: CoDUOMP.exe 0x00483a70..0x00483a90.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00483a70_00483a91.mcode. */
void ClearObject(uint16_t object)
{
    AddRefToObject(object);
    ClearObjectInternal(object);
    RemoveRefToObject(object);
}

/* Source: CoDUOMP.exe 0x00483aa0..0x00483ab0.
 * Evidence: original .text bytes/disassembly; Ghidra left this complete
 * function inside executable_gaps.mcode as UNDEFINED_BYTES. */
uint16_t GetThreadNotifyName(uint16_t thread)
{
    return (uint16_t)GetVariableName(thread);
}

/* Source: CoDUOMP.exe 0x00483ac0..0x00483ae8.
 * Evidence: original .text bytes/disassembly; Ghidra left this complete
 * function inside executable_gaps.mcode as UNDEFINED_BYTES. */
void SetThreadNotifyName(uint16_t thread, uint16_t name)
{
    SL_AddRefToString(name);
    script_variableNodes[thread].packedTypeIndex |= (uint32_t)name << SCRIPT_VARIABLE_NAME_SHIFT;
}

/* Source: CoDUOMP.exe 0x004845d0..0x00484613.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004845d0_00484614.mcode. */
void RemoveVariable(uint16_t parent, uint32_t name)
{
    uint16_t indirection = FindVariableIndexInternal(parent, name);
    uint16_t child = script_variableIndirections[indirection].valueIndex;

    MakeVariableExternal(&script_variableIndirections[indirection], &script_variableNodes[parent]);
    FreeValueInternal(&script_variableNodes[child]);
}

/* Source: CoDUOMP.exe 0x00484620..0x0048466b.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00484620_0048466c.mcode. */
void RemoveObjectVariable(uint16_t parent, uint16_t object)
{
    RemoveVariable(parent, SCRIPT_VARIABLE_OBJECT_KEY_BASE + object);
}

/* Source: CoDUOMP.exe 0x00484670..0x004846c3.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00484670_004846c4.mcode. */
void SafeRemoveArrayVariable(uint16_t parent, int32_t entityNum)
{
    uint32_t name = ((uint32_t)entityNum - SCRIPT_VARIABLE_ENTITY_KEY_BIAS) & SCRIPT_VARIABLE_ENTITY_KEY_MASK;
    uint16_t indirection = FindVariableIndexInternal(parent, name);
    if (indirection == 0) {
        return;
    }

    uint16_t child = script_variableIndirections[indirection].valueIndex;
    MakeVariableExternal(&script_variableIndirections[indirection], &script_variableNodes[parent]);
    FreeValueInternal(&script_variableNodes[child]);
}

/* Source: CoDUOMP.exe 0x004846d0..0x0048471d.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004846d0_0048471e.mcode. */
void RemoveArrayVariable(uint16_t parent, int32_t entityNum)
{
    uint32_t name = ((uint32_t)entityNum - SCRIPT_VARIABLE_ENTITY_KEY_BIAS) & SCRIPT_VARIABLE_ENTITY_KEY_MASK;
    RemoveVariable(parent, name);
}

/* Source: CoDUOMP.exe 0x00484720..0x00484769.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00484720_0048476a.mcode. */
void SafeRemoveVariable(uint16_t parent, uint32_t name)
{
    uint16_t indirection = FindVariableIndexInternal(parent, name);
    if (indirection == 0) {
        return;
    }

    uint16_t child = script_variableIndirections[indirection].valueIndex;
    MakeVariableExternal(&script_variableIndirections[indirection], &script_variableNodes[parent]);
    FreeValueInternal(&script_variableNodes[child]);
}

/* Source: CoDUOMP.exe 0x00483e50..0x00483eb8.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00483e50_00483eb9.mcode. */
script_variable_node_t *AllocVariable(void)
{
    uint16_t freeIndirection = script_variableNodes[0].hashOrFreeNext;

    if (freeIndirection == 0) {
        Scr_TerminalError("exceeded maximum number of script variables");
    }

    uint16_t nodeIndex = script_variableIndirections[freeIndirection].valueIndex;
    script_variable_node_t *node = &script_variableNodes[nodeIndex];
    uint16_t nextIndirection = node->hashOrFreeNext;

    script_variableNodes[0].hashOrFreeNext = nextIndirection;
    uint16_t nextNode = script_variableIndirections[nextIndirection].valueIndex;
    script_variableNodes[nextNode].payload.halves.valueOrRefCount = 0;

    node->hashOrFreeNext = freeIndirection;
    node->nextSibling = freeIndirection;
    script_variableIndirections[freeIndirection].previousSibling = freeIndirection;
    return node;
}

/* Source: CoDUOMP.exe 0x00483ec0..0x00483f3e.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00483ec0_00483f3f.mcode. */
void FreeVariable(uint16_t handle)
{
    script_variable_node_t *node = &script_variableNodes[handle];
    uint16_t selfIndirection = node->hashOrFreeNext;
    uint16_t nextSibling = node->nextSibling;
    uint16_t previousSibling = script_variableIndirections[selfIndirection].previousSibling;

    script_variableIndirections[nextSibling].previousSibling = previousSibling;
    uint16_t previousNode = script_variableIndirections[previousSibling].valueIndex;
    script_variableNodes[previousNode].nextSibling = nextSibling;

    node->packedTypeIndex &= ~(uint32_t)SCRIPT_VARIABLE_OCCUPIED_MASK;
    uint16_t freeHead = script_variableNodes[0].hashOrFreeNext;
    node->hashOrFreeNext = freeHead;
    node->payload.halves.valueOrRefCount = 0;

    uint16_t freeHeadNode = script_variableIndirections[freeHead].valueIndex;
    script_variableNodes[freeHeadNode].payload.halves.valueOrRefCount = selfIndirection;
    script_variableNodes[0].hashOrFreeNext = selfIndirection;
}

/* Source: CoDUOMP.exe 0x00483f40..0x00483f63.
 * Evidence: original .text bytes/disassembly; Ghidra left this complete
 * function inside executable_gaps.mcode as UNDEFINED_BYTES. */
uint16_t AllocValue(void)
{
    script_variable_node_t *node = AllocVariable();

    node->packedTypeIndex = SCRIPT_VARIABLE_OCCUPIED_MASK;
    return (uint16_t)(node - script_variableNodes);
}

/* Source: CoDUOMP.exe 0x00483f70..0x00483f98.
 * Evidence: original .text bytes/disassembly; Ghidra left this complete
 * function inside executable_gaps.mcode as UNDEFINED_BYTES. */
uint16_t AllocObject(void)
{
    script_variable_node_t *node = AllocVariable();

    node->packedTypeIndex = SCRIPT_VARIABLE_OCCUPIED_MASK | SCRIPT_VAR_STRUCT;
    node->payload.halves.valueOrRefCount = 0;
    return (uint16_t)(node - script_variableNodes);
}

/* Source: CoDUOMP.exe 0x00483fa0..0x00483fd7.
 * Evidence: original .text bytes/disassembly; Ghidra left this complete
 * function inside executable_gaps.mcode as UNDEFINED_BYTES. */
uint16_t AllocEntity(int32_t classNum, uint16_t entityNum)
{
    script_variable_node_t *node = AllocVariable();

    node->payload.halves.parentHandle = entityNum;
    node->payload.halves.valueOrRefCount = 0;
    node->packedTypeIndex = ((uint32_t)classNum << SCRIPT_VARIABLE_NAME_SHIFT) | SCRIPT_VARIABLE_OCCUPIED_MASK | SCRIPT_VAR_ENTITY;
    return (uint16_t)(node - script_variableNodes);
}

/* Source: CoDUOMP.exe 0x00483fe0..0x0048400e.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00483fe0_0048400f.mcode. */
uint16_t Scr_AllocArray(void)
{
    script_variable_node_t *node = AllocVariable();

    node->packedTypeIndex = SCRIPT_VARIABLE_OCCUPIED_MASK | SCRIPT_VAR_ARRAY;
    node->payload.halves.valueOrRefCount = 0;
    node->payload.halves.parentHandle = 0;
    return (uint16_t)(node - script_variableNodes);
}

/* Source: CoDUOMP.exe 0x00484010..0x00484041.
 * Evidence: original .text bytes/disassembly; Ghidra left this complete
 * function inside executable_gaps.mcode as UNDEFINED_BYTES. */
uint16_t AllocThread(uint16_t parent)
{
    script_variable_node_t *node = AllocVariable();

    node->payload.halves.parentHandle = parent;
    node->packedTypeIndex = SCRIPT_VARIABLE_OCCUPIED_MASK | SCRIPT_VAR_THREAD;
    node->payload.halves.valueOrRefCount = 0;
    return (uint16_t)(node - script_variableNodes);
}

/* Source: CoDUOMP.exe 0x00484050..0x0048405e.
 * Evidence: original .text bytes/disassembly; Ghidra left this complete
 * function inside executable_gaps.mcode as UNDEFINED_BYTES. */
uint16_t GetSelf(uint16_t object)
{
    return script_variableNodes[object].payload.halves.parentHandle;
}

/* Source: CoDUOMP.exe 0x00484060..0x004840ac.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00484060_004840ad.mcode. */
void FreeValueInternal(script_variable_node_t *node)
{
    /* The Mac symbol exposes VariableValueInternal *, whose payload/status
     * pair is the first eight target bytes of this complete variable node.
     * Retaining the complete C type here avoids invalid prefix-type aliasing
     * and preserves the native links that follow it. */
    switch ((script_variable_type_t)(node->packedTypeIndex & SCRIPT_VARIABLE_TYPE_MASK)) {
    case SCRIPT_VAR_STRING:
    case SCRIPT_VAR_LOCALIZED_STRING:
        SL_RemoveRefToString((uint16_t)node->payload.valuePayload);
        break;
    case SCRIPT_VAR_VECTOR:
        RemoveRefToVector((const float *)node->payload.valuePayload);
        break;
    case SCRIPT_VAR_OBJECT:
        RemoveRefToObject((uint16_t)node->payload.valuePayload);
        break;
    default:
        break;
    }
    FreeVariable((uint16_t)(node - script_variableNodes));
}

/* Source: CoDUOMP.exe 0x004840d0..0x004840e1.
 * Name and argument: exact same-module Mac symbol FreeValue(uint16_t). */
void FreeValue(uint16_t handle)
{
    FreeValueInternal(&script_variableNodes[handle]);
}

/* Source: CoDUOMP.exe 0x004840f0..0x004840fe.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004840f0_004840ff.mcode.
 * Name and argument: exact same-module Mac symbol AddRefToObject. */
void AddRefToObject(uint16_t object)
{
    uint16_t *const referenceCount = &script_variableNodes[object].payload.halves.valueOrRefCount;

    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if (*referenceCount == SCRIPT_REFERENCE_COUNT_MAX) {
        Com_Error(ERR_DROP, "\x15"
                            "script object reference count overflow");
    }
    ++*referenceCount;
}

/* Source: CoDUOMP.exe 0x00484100..0x00484142.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00484100_00484143.mcode. */
void RemoveRefToObject(uint16_t object)
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

/* Source: CoDUOMP.exe 0x00483af0..0x00483b50.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00483af0_00483b51.mcode. */
void ClearThreadNotifyName(uint16_t thread)
{
    script_variable_node_t *node = &script_variableNodes[thread];
    uint16_t waitName = (uint16_t)(node->packedTypeIndex >> SCRIPT_VARIABLE_NAME_SHIFT);

    SL_RemoveRefToString(waitName);
    node->packedTypeIndex &= 0xff;
}

/* Source: CoDUOMP.exe 0x00484d80..0x00484d99.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00484d80_00484d9a.mcode. */
qboolean Scr_IsThreadAlive(uint16_t handle)
{
    return (script_variableNodes[handle].packedTypeIndex & SCRIPT_VARIABLE_TYPE_MASK) == SCRIPT_VAR_THREAD ? qtrue : qfalse;
}

/* Source: CoDUOMP.exe 0x00484e70..0x00484e80.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00484e70_00484e81.mcode. */
script_variable_type_t GetVarType(uint16_t handle)
{
    return (script_variable_type_t)(script_variableNodes[handle].packedTypeIndex & SCRIPT_VARIABLE_TYPE_MASK);
}

/* Source: CoDUOMP.exe 0x00485ff0..0x00486028.
 * The Ghidra export split its arithmetic tail at 0x00486013 into a false
 * independent function. The complete body is the source operation used by
 * SCRIPT_OP_NEW_ARRAY: install a newly allocated array object into a value. */
void GetEmptyArray(VariableValue *value)
{
    value->type = SCRIPT_VAR_OBJECT;
    value->payload = Scr_AllocArray();
}

/* Source: CoDUOMP.exe 0x00486030..0x0048607d.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00486030_0048607e.mcode.
 * Name and signature: same-module Mac symbol SetEmptyArray(unsigned short). */
void SetEmptyArray(uint16_t handle)
{
    script_variableNodes[handle].packedTypeIndex |= SCRIPT_VAR_OBJECT;
    script_variableNodes[handle].payload.halves.valueOrRefCount = Scr_AllocArray();
}

/* Source: CoDUOMP.exe 0x00486080..0x0048608e.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00486080_0048608f.mcode.
 * Name and signature: same-module Mac symbol GetEntnum(unsigned short).
 * This separately retained accessor reads the same parent/entnum word as
 * GetSelf. */
uint16_t GetEntnum(uint16_t handle)
{
    return script_variableNodes[handle].payload.halves.parentHandle;
}

/* Source: CoDUOMP.exe 0x004848a0..0x004848fb.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004848a0_004848fc.mcode. */
void SetVariableValue(uint16_t handle, const VariableValue *value)
{
    script_variable_node_t *node = &script_variableNodes[handle];
    VariableValue oldValue = {.payload = node->payload.valuePayload,
                              .type = (script_variable_type_t)(node->packedTypeIndex & SCRIPT_VARIABLE_TYPE_MASK)};

    RemoveRefToValue(&oldValue);
    node->packedTypeIndex &= ~(uint32_t)SCRIPT_VARIABLE_TYPE_MASK;
    node->packedTypeIndex |= (uint32_t)value->type;
    node->payload.valuePayload = value->payload;
}

/* Source: CoDUOMP.exe 0x00484920..0x00484937.
 * Evidence: original .text bytes/disassembly; Ghidra left this complete
 * function inside executable_gaps.mcode as UNDEFINED_BYTES. */
void SetNewVariableValue(uint16_t handle, const VariableValue *value)
{
    script_variable_node_t *node = &script_variableNodes[handle];

    node->packedTypeIndex |= (uint32_t)value->type;
    node->payload.valuePayload = value->payload;
}

/* Source: CoDUOMP.exe 0x00484940..0x0048494d.
 * Evidence: original .text bytes/disassembly; Ghidra left this complete
 * function inside executable_gaps.mcode as UNDEFINED_BYTES. */
VariableValue *GetVariableValueAddress(uint16_t handle)
{
    /* VariableValue's alias-capable declaration makes this canonical prefix
     * view valid when native optimization performs type-based alias analysis. */
    return (VariableValue *)&script_variableNodes[handle];
}

/* Source: CoDUOMP.exe 0x00484c40..0x00484c4e.
 * Evidence: original .text bytes/disassembly; Ghidra left this complete
 * function inside executable_gaps.mcode as UNDEFINED_BYTES. */
uint16_t GetArraySize(uint16_t handle)
{
    return script_variableNodes[handle].payload.halves.parentHandle;
}

/* Source: CoDUOMP.exe 0x00484c50..0x00484c7d.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00484c50_00484c7e.mcode. */
uint16_t FindNextSibling(uint16_t handle)
{
    uint16_t link = script_variableNodes[handle].nextSibling;
    uint16_t node = script_variableIndirections[link].valueIndex;

    return (script_variableNodes[node].packedTypeIndex & SCRIPT_VARIABLE_TYPE_MASK) < SCRIPT_VAR_THREAD ? node : 0;
}

/* Source: CoDUOMP.exe 0x00484c80..0x00484c90.
 * Evidence: original .text bytes/disassembly; Ghidra left this complete
 * function inside executable_gaps.mcode as UNDEFINED_BYTES. */
uint32_t GetVariableName(uint16_t handle)
{
    return script_variableNodes[handle].packedTypeIndex >> SCRIPT_VARIABLE_NAME_SHIFT;
}

/* Source: CoDUOMP.exe 0x00484a70..0x00484a8e.
 * Evidence: original .text bytes/disassembly; Ghidra left this complete
 * function inside executable_gaps.mcode as UNDEFINED_BYTES. */
void GetVariableValue(uint16_t handle, VariableValue *value)
{
    script_variable_node_t *node = &script_variableNodes[handle];

    value->type = (script_variable_type_t)(node->packedTypeIndex & SCRIPT_VARIABLE_TYPE_MASK);
    value->payload = node->payload.valuePayload;
}

/* Source: CoDUOMP.exe 0x00484a90..0x00484b63.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00484a90_00484b64.mcode. */
void GetVariableFieldValue(uint16_t handle, VariableValue *value)
{
    script_variable_node_t *node = &script_variableNodes[handle];
    script_variable_type_t type = (script_variable_type_t)(node->packedTypeIndex & SCRIPT_VARIABLE_TYPE_MASK);

    if (type == SCRIPT_VAR_KEY_VALUE) {
        ScriptRuntime_GetObjectFieldValue((int32_t)GetVariableName(handle), node->payload.halves.valueOrRefCount,
                                          node->payload.halves.parentHandle, value);

        if (value->type == SCRIPT_VAR_OBJECT && GetVarType((uint16_t)value->payload) == SCRIPT_VAR_ARRAY) {
            uint16_t source = (uint16_t)value->payload;

            RemoveRefToObject(source);
            value->payload = Scr_AllocArray();
            CopyArray(source, (uint16_t)value->payload);
        }
        return;
    }

    GetVariableValue(handle, value);
    AddRefToValueOfType(value->type, value->u);
}

/* Source: CoDUOMP.exe 0x00484ca0..0x00484cee.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00484ca0_00484cef.mcode. */
uint16_t GetObject(uint16_t handle)
{
    script_variable_node_t *node = &script_variableNodes[handle];

    if ((node->packedTypeIndex & SCRIPT_VARIABLE_TYPE_MASK) == SCRIPT_VAR_UNDEFINED) {
        node->packedTypeIndex |= SCRIPT_VAR_OBJECT;
        node->payload.halves.valueOrRefCount = AllocObject();
    }

    return node->payload.halves.valueOrRefCount;
}

/* Source: CoDUOMP.exe 0x00484cf0..0x00484d44.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00484cf0_00484d45.mcode. */
uint16_t GetArray(uint16_t handle)
{
    script_variable_node_t *node = &script_variableNodes[handle];

    if ((node->packedTypeIndex & SCRIPT_VARIABLE_TYPE_MASK) == SCRIPT_VAR_UNDEFINED) {
        node->packedTypeIndex |= SCRIPT_VAR_OBJECT;
        node->payload.halves.valueOrRefCount = Scr_AllocArray();
    }

    return node->payload.halves.valueOrRefCount;
}

/* Source: CoDUOMP.exe 0x00484d50..0x00484d5e.
 * Evidence: original .text bytes/disassembly; Ghidra left this complete
 * function inside executable_gaps.mcode as UNDEFINED_BYTES. */
uint16_t FindObject(uint16_t handle)
{
    return script_variableNodes[handle].payload.halves.valueOrRefCount;
}

/* Source: CoDUOMP.exe 0x00484d60..0x00484d77.
 * Evidence: original .text bytes/disassembly; Ghidra left this complete
 * function inside executable_gaps.mcode as UNDEFINED_BYTES. */
qboolean IsFieldObject(uint16_t handle)
{
    return GetVarType(handle) < SCRIPT_VAR_ARRAY ? qtrue : qfalse;
}

/* Source: CoDUOMP.exe 0x00484950..0x004849a7.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00484950_004849a8.mcode. */
void ClearVariableValue(uint16_t handle)
{
    script_variable_node_t *node = &script_variableNodes[handle];
    VariableValue oldValue = {.payload = node->payload.valuePayload,
                              .type = (script_variable_type_t)(node->packedTypeIndex & SCRIPT_VARIABLE_TYPE_MASK)};

    RemoveRefToValue(&oldValue);
    node->packedTypeIndex &= SCRIPT_VARIABLE_CHILD_COPY_OCCUPANCY_BITS;
}

/* Source: CoDUOMP.exe 0x004849d0..0x00484a45.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004849d0_00484a46.mcode. */
void SetVariableFieldValue(uint16_t handle, VariableValue *value)
{
    script_variable_node_t *node = &script_variableNodes[handle];
    script_variable_type_t type = (script_variable_type_t)(node->packedTypeIndex & SCRIPT_VARIABLE_TYPE_MASK);

    if (type == SCRIPT_VAR_KEY_VALUE) {
        ScriptRuntime_SetObjectFieldValue((int32_t)GetVariableName(handle), node->payload.halves.valueOrRefCount,
                                          node->payload.halves.parentHandle, value);
        return;
    }

    SetVariableValue(handle, value);
}

/* Source: CoDUOMP.exe 0x00484e90..0x00484f44.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00484e90_00484f45.mcode. */
void Scr_FreeEntityNum(int32_t entityNum, int32_t classNum)
{
    if (script_runtimeActive == qfalse) {
        return;
    }

    uint16_t classMapSlot = FindVariable(script_entityTypeClassMapRoot, (uint32_t)classNum);
    uint16_t classMap = FindObject(classMapSlot);
    uint16_t entitySlot = FindArrayVariable(classMap, entityNum);
    if (entitySlot == 0) {
        return;
    }

    uint16_t entityObject = script_variableNodes[entitySlot].payload.halves.valueOrRefCount;
    script_variableNodes[entityObject].packedTypeIndex &= ~(uint32_t)SCRIPT_VARIABLE_TYPE_MASK;
    script_variableNodes[entityObject].packedTypeIndex |= SCRIPT_VAR_DEAD_ENTITY;

    AddRefToObject(entityObject);
    ScriptNotify_StopAllWaiters(entityObject);
    ClearObjectInternal(entityObject);
    RemoveRefToObject(entityObject);
    RemoveArrayVariable(classMap, entityNum);
}

/* Source: CoDUOMP.exe 0x00484f50..0x00485060.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00484f50_00485061.mcode. */
void Scr_SetClassMap(script_class_map_entry_t *records, uint32_t count)
{
    script_entityTypeUsageRecords = records;
    script_entityTypeUsageCount = count;

    for (uint32_t index = 0; index < count; ++index) {
        uint16_t entityMapSlot = GetVariable(script_entityTypeClassMapRoot, index);
        GetArray(entityMapSlot);

        uint16_t classFieldSlot = GetVariable(script_classMapRoot, index);
        records[index].classnum = GetArray(classFieldSlot);
    }
}

/* Source: CoDUOMP.exe 0x00485070..0x00485138.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00485070_00485139.mcode. */
void Scr_RemoveClassMap(void)
{
    if (script_runtimeActive == qfalse) {
        return;
    }

    for (uint32_t index = 0; index < script_entityTypeUsageCount; ++index) {
        SafeRemoveVariable(script_classMapRoot, index);
        SafeRemoveVariable(script_entityTypeClassMapRoot, index);
    }
}

/* Source: CoDUOMP.exe 0x00485140..0x0048524e.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00485140_0048524f.mcode. */
void Scr_AddClassField(uint16_t classRoot, const char *name, uint16_t offset)
{
    uint16_t tempHash = SL_FindCanonicalString(name);
    if (tempHash != 0) {
        uint16_t field = GetArrayVariableUnsigned(classRoot, tempHash);
        script_variableNodes[field].packedTypeIndex &= ~(uint32_t)SCRIPT_VARIABLE_TYPE_MASK;
        script_variableNodes[field].packedTypeIndex |= SCRIPT_VAR_INT;
        script_variableNodes[field].payload.halves.parentHandle = offset;
    }

    uint16_t string = SL_GetString_(name, 0, SCRIPT_CLASS_FIELD_STRING_TYPE);
    uint16_t field = GetVariable(classRoot, string);
    SL_RemoveRefToString(string);
    script_variableNodes[field].packedTypeIndex &= ~(uint32_t)SCRIPT_VARIABLE_TYPE_MASK;
    script_variableNodes[field].packedTypeIndex |= SCRIPT_VAR_INT;
    script_variableNodes[field].payload.halves.parentHandle = offset;
}

/* Source: CoDUOMP.exe 0x00485250..0x0048529a.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00485250_0048529b.mcode. */
uint32_t Scr_GetOffset(uint16_t classRoot, const char *name)
{
    uint16_t string = SL_ConvertFromString(name);
    uint16_t field = FindVariable(classRoot, string);

    if (field == 0) {
        return UINT32_MAX;
    }
    return script_variableNodes[field].payload.halves.parentHandle;
}

/* Source: CoDUOMP.exe 0x00485300..0x004853b3.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00485300_004853b4.mcode. */
uint16_t Scr_GetEntityId(int32_t entityNum, int32_t classNum)
{
    uint16_t classMapSlot = FindVariable(script_entityTypeClassMapRoot, (uint32_t)classNum);
    uint16_t classMap = FindObject(classMapSlot);
    uint16_t entitySlot = GetArrayVariable(classMap, entityNum);
    script_variable_node_t *slotNode = &script_variableNodes[entitySlot];

    if ((slotNode->packedTypeIndex & SCRIPT_VARIABLE_TYPE_MASK) == SCRIPT_VAR_UNDEFINED) {
        uint16_t entityObject = AllocEntity(classNum, (uint16_t)(entityNum & SCRIPT_ENTITY_NUM_MASK));
        slotNode->packedTypeIndex |= SCRIPT_VAR_OBJECT;
        slotNode->payload.halves.valueOrRefCount = entityObject;
        return entityObject;
    }

    return slotNode->payload.halves.valueOrRefCount;
}

/* Source: CoDUOMP.exe 0x004852a0..0x004852f6.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004852a0_004852f7.mcode. */
uint16_t FindEntityId(int32_t entityNum, int32_t classNum)
{
    uint16_t classMapSlot = FindVariable(script_entityTypeClassMapRoot, (uint32_t)classNum);
    uint16_t classMap = FindObject(classMapSlot);
    uint16_t entitySlot = FindArrayVariable(classMap, entityNum);

    if (entitySlot == 0) {
        return 0;
    }

    return script_variableNodes[entitySlot].payload.halves.valueOrRefCount;
}

/* Source: CoDUOMP.exe 0x00486090..0x0048615e.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00486090_0048615f.mcode. */
void CopyEntity(uint16_t source, uint16_t dest)
{
    uint16_t child = FindNextSibling(source);

    while (child != 0) {
        script_variable_node_t *sourceNode = &script_variableNodes[child];
        uint32_t sourceName = sourceNode->packedTypeIndex >> SCRIPT_VARIABLE_NAME_SHIFT;

        if (sourceName != SCRIPT_VARIABLE_OBJECT_KEY_LIMIT) {
            uint16_t destIndirection = GetVariableIndexInternal(dest, sourceName);
            script_variable_node_t *destNode = &script_variableNodes[script_variableIndirections[destIndirection].valueIndex];
            uint32_t copiedPackedType = sourceNode->packedTypeIndex & ~(uint32_t)SCRIPT_VARIABLE_CHILD_COPY_OCCUPANCY_BITS;
            script_variable_type_t copiedType = (script_variable_type_t)(copiedPackedType & SCRIPT_VARIABLE_TYPE_MASK);

            destNode->packedTypeIndex |= copiedPackedType;
            destNode->payload.valuePayload = sourceNode->payload.valuePayload;
            AddRefToValueOfType(copiedType, sourceNode->payload);
        }

        child = FindNextSibling(child);
    }
}

/* Source: CoDUOMP.exe 0x00484770..0x00484899.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00484770_0048489a.mcode. */
void CopyArray(uint16_t source, uint16_t dest)
{
    uint16_t child = script_variableIndirections[script_variableNodes[source].nextSibling].valueIndex;

    while (child != source) {
        script_variable_node_t *childNode = &script_variableNodes[child];
        script_variable_type_t childType = GetVarType(child);
        uint16_t destChild = GetVariable(dest, GetVariableName(child));
        script_variable_node_t *destNode = &script_variableNodes[destChild];

        destNode->packedTypeIndex |= (uint32_t)childType;
        if (childType == SCRIPT_VAR_OBJECT) {
            uint16_t childObject = childNode->payload.halves.valueOrRefCount;

            if (GetVarType(childObject) == SCRIPT_VAR_ARRAY) {
                destNode->payload.halves.valueOrRefCount = Scr_AllocArray();
                CopyArray(childObject, destNode->payload.halves.valueOrRefCount);
            } else {
                destNode->payload.halves.valueOrRefCount = childObject;
                AddRefToObject(childObject);
            }
        } else {
            destNode->payload.valuePayload = childNode->payload.valuePayload;
            AddRefToValueOfType(childType, childNode->payload);
        }

        child = script_variableIndirections[script_variableNodes[child].nextSibling].valueIndex;
    }
}

/* Source: CoDUOMP.exe 0x00486160..0x004861c0.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00486160_004861c1.mcode. */
void Scr_CopyEntityNum(int32_t sourceEntityNum, int32_t destEntityNum, int32_t classNum)
{
    uint16_t source = FindEntityId(sourceEntityNum, classNum);

    if (source == 0 || FindNextSibling(source) == 0) {
        return;
    }

    uint16_t dest = Scr_GetEntityId(destEntityNum, classNum);
    CopyEntity(source, dest);
}

/* Source: coduo_lnxded 0x080aa558..0x080aa56d.  Name, argument, and sole
 * RemoveRefToObject call independently confirmed by the exact Mac engine
 * symbol Scr_FreeValue at 0x100b9040. */
void Scr_FreeValue(uint16_t handle)
{
    RemoveRefToObject(handle);
}
