#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "compat/coduo_native_x87.h"
#include "compat/coduo_x87emu.h"
#include "script_runtime_host.h"
#include "script_usage.h"
#include "script_variable.h"
#include "compat/crt/qsort_compat.h"

enum {
    SCRIPT_USAGE_STACK_ENTRY_CODEPOS = SCRIPT_VAR_CODEPOS,
    SCRIPT_USAGE_STACK_ENTRY_ARRAY = SCRIPT_VAR_OBJECT,
    SCRIPT_USAGE_STACK_NODE_BITS = 0x60,
    SCRIPT_USAGE_STACK_SIGNATURE_NODE_TYPE = SCRIPT_VAR_STACK,
    SCRIPT_USAGE_PRINT_CHANNEL = 0
};

typedef struct script_usage_stack_signature_s {
    script_codepos_t codePosStack[SCRIPT_CALL_STACK_COUNT];
    int32_t depth;
    float variableUsage;
    float endonUsage;
} script_usage_stack_signature_t;

float Scr_GetObjectUsage(uint16_t object);
float Scr_GetEndonUsage(uint16_t localVars);

/* NOT_FROM_ORIGINAL_SOURCE: typed access to a packed saved-stack entry. */
static void coduomp_script_usage_read_stack_entry_value(
    const VariableStackBufferEntry *entry, VariableValue *value)
{
    value->type = (script_variable_type_t)entry->type;
    value->payload = entry->payload;
}

/* NOT_FROM_ORIGINAL_SOURCE: the diagnostic callstack has a fixed slot count.
 * Keep the current position and newest saved callers, omitting older frames
 * once that presentation capacity is reached. */
static int32_t coduomp_script_usage_collect_code_positions(
    const VariableStackBuffer *frame,
    script_codepos_t codePositions[SCRIPT_CALL_STACK_COUNT])
{
    enum {
        SAVED_CODE_POSITION_CAPACITY = SCRIPT_CALL_STACK_COUNT - 1
    };
    script_codepos_t savedCodePositions[SAVED_CODE_POSITION_CAPACITY];
    uint32_t savedCodePositionCount = 0;

    for (uint16_t entryIndex = 0; entryIndex < frame->size; ++entryIndex) {
        const VariableStackBufferEntry *const entry =
            &frame->entries[entryIndex];
        if (entry->type == SCRIPT_USAGE_STACK_ENTRY_CODEPOS) {
            savedCodePositions[
                savedCodePositionCount % SAVED_CODE_POSITION_CAPACITY] =
                (script_codepos_t)entry->payload;
            savedCodePositionCount++;
        }
    }

    const uint32_t retainedSavedCount =
        savedCodePositionCount < SAVED_CODE_POSITION_CAPACITY
            ? savedCodePositionCount
            : SAVED_CODE_POSITION_CAPACITY;
    codePositions[0] = frame->pos;
    for (uint32_t index = 0; index < retainedSavedCount; ++index) {
        const uint32_t savedIndex = savedCodePositionCount - index - 1;
        codePositions[index + 1] =
            savedCodePositions[savedIndex % SAVED_CODE_POSITION_CAPACITY];
    }
    return (int32_t)retainedSavedCount + 1;
}

/* NOT_FROM_ORIGINAL_SOURCE: stack variables carry live frame pointers. */
#if defined(LINUX_BEHAVIOR)
static VariableStackBuffer *
coduomp_script_usage_stack_frame_for_node(uint16_t handle)
{
    VariableValue value;

    GetVariableValue(handle, &value);
    return (VariableStackBuffer *)value.payload;
}

/* NOT_FROM_ORIGINAL_SOURCE: centralizes recovered entity usage name access. */
static const char *
coduomp_script_usage_entity_type_name(
    const script_class_map_entry_t *usage)
{
    return usage->name;
}
#endif

int32_t ThreadInfoCompare(const void *left, const void *right)
{
    const script_usage_stack_signature_t *leftStack = left;
    const script_usage_stack_signature_t *rightStack = right;
    int32_t sharedDepth = leftStack->depth < rightStack->depth
                              ? leftStack->depth
                              : rightStack->depth;

    for (int32_t index = 0; index < sharedDepth; ++index) {
        script_codepos_t leftCodePos = leftStack->codePosStack[index];
        script_codepos_t rightCodePos = rightStack->codePosStack[index];

        if (leftCodePos != rightCodePos) {
#if UINTPTR_MAX == UINT32_MAX
            uint32_t difference =
                (uint32_t)(uintptr_t)leftCodePos -
                (uint32_t)(uintptr_t)rightCodePos;
            int32_t result;

            memcpy(&result, &difference, sizeof(result));
            return result;
#else
            return (uintptr_t)leftCodePos > (uintptr_t)rightCodePos ? 1 : -1;
#endif
        }
    }

    uint32_t difference =
        (uint32_t)leftStack->depth - (uint32_t)rightStack->depth;
    int32_t result;

    memcpy(&result, &difference, sizeof(result));
    return result;
}

float Scr_GetEntryUsage(script_variable_type_t type,
                        VariableUnion value)
{
    uint16_t object = (uint16_t)value.payload;

#if defined(WINDOWS_BEHAVIOR)
    /* CoDUOMP.exe 0x00486891 compares the unmasked low type byte. */
    if ((uint8_t)type == SCRIPT_VAR_OBJECT &&
#else
    if (type == SCRIPT_VAR_OBJECT &&
#endif
        GetVarType(object) == SCRIPT_VAR_ARRAY) {
        /* Stock 0x80a940f adds 1.0 to the fild-exact count (no float cast). */
        return Scr_GetObjectUsage(object) /
               (script_variableNodes[object].payload.halves.valueOrRefCount + 1.0f);
    }

    return 0.0f;
}

#if !(EMULATE_X87 && defined(LINUX_BEHAVIOR))
/* Linux coduo_lnxded 0x080a944f returns the sum as raw st(0) with no float
 * store.  The long-double carrier preserves that unspilled x87 result for the
 * non-emulated Linux build; Windows likewise performs the expression without
 * an intervening binary32 spill. */
long double
Scr_GetEntryUsageForNode(script_variable_node_t *node)
{
    return 1.0f + Scr_GetEntryUsage(
                      (script_variable_type_t)(
                          node->packedTypeIndex &
                          SCRIPT_VARIABLE_NODE_TYPE_MASK),
                      node->payload);
}
#endif

float Scr_GetEndonUsage(uint16_t localVars)
{
    uint16_t pauseEntry =
        FindObjectVariable(script_pauseArrayHandle, localVars);

    if (pauseEntry == 0) {
        return 0.0f;
    }

    return Scr_GetObjectUsage(FindObject(pauseEntry));
}

float Scr_GetObjectUsage(uint16_t object)
{
    float usage = 1.0f;

    for (uint16_t child = FindNextSibling(object);
         child != 0; child = FindNextSibling(child)) {
#if EMULATE_X87 && defined(LINUX_BEHAVIOR)
        float entryUsage = Scr_GetEntryUsage(
            (script_variable_type_t)(
                script_variableNodes[child].packedTypeIndex &
                SCRIPT_VARIABLE_NODE_TYPE_MASK),
            script_variableNodes[child].payload);
        usage = x87f_store_f32(x87f_add(
            x87f_load_f32(usage),
            x87f_add(x87f_load_f32(1.0f),
                     x87f_load_f32(entryUsage))));
#else
        usage += Scr_GetEntryUsageForNode(&script_variableNodes[child]);
#endif
    }

    return usage;
}

float Scr_GetThreadUsage(const VariableStackBuffer *frame,
                         float *endonUsage)
{
    float variableUsage = Scr_GetObjectUsage(frame->localId);

    *endonUsage = Scr_GetEndonUsage(frame->localId);

    uint32_t remaining = frame->size;
    const VariableStackBufferEntry *entry = frame->entries + remaining;

    while (remaining != 0) {
        uint8_t type = entry[-1].type;

        if (type == SCRIPT_USAGE_STACK_ENTRY_CODEPOS) {
            VariableValue priorValue;

            /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
            coduomp_script_usage_read_stack_entry_value(
                entry - 2,
                &priorValue);
            uint16_t localVars =
                (uint16_t)priorValue.payload;

            entry -= 2;
            remaining -= 2;
            variableUsage += Scr_GetObjectUsage(localVars);
            *endonUsage += Scr_GetEndonUsage(localVars);
        } else {
            VariableValue value;

            coduomp_script_usage_read_stack_entry_value(
                entry - 1,
                &value);
            variableUsage += Scr_GetEntryUsage(type, value.u);
            entry--;
            remaining--;
        }
    }

    return variableUsage;
}

#if defined(LINUX_BEHAVIOR)
void Scr_DumpScriptThreads(void)
{
    script_usage_stack_signature_t *signatures =
        Z_MallocInternal((size_t)SCRIPT_VARIABLE_NODE_COUNT *
                 sizeof(*signatures));
    int32_t signatureCount = 0;

    for (uint32_t handle = 1; handle < SCRIPT_VARIABLE_NODE_COUNT;
         ++handle) {
        script_variable_node_t *node = &script_variableNodes[handle];

        if ((node->packedTypeIndex & SCRIPT_USAGE_STACK_NODE_BITS) == 0 ||
            (node->packedTypeIndex & SCRIPT_VARIABLE_NODE_TYPE_MASK) !=
                SCRIPT_USAGE_STACK_SIGNATURE_NODE_TYPE) {
            continue;
        }

        script_usage_stack_signature_t *signature =
            &signatures[signatureCount++];
        VariableStackBuffer *frame =
            coduomp_script_usage_stack_frame_for_node((uint16_t)handle);
        const int32_t depth = coduomp_script_usage_collect_code_positions(
            frame, signature->codePosStack);
        signature->variableUsage =
            Scr_GetThreadUsage(frame, &signature->endonUsage);
        signature->depth = depth;
    }

    coduo_qsort(signatures, signatureCount, sizeof(*signatures),
                ThreadInfoCompare);

    Com_Printf("********************************\n");
    int32_t index = 0;
    while (index < signatureCount) {
        script_usage_stack_signature_t *signature = &signatures[index];
        int32_t count = 0;
        float variableUsage = 0.0f;
        float endonUsage = 0.0f;

        do {
            count++;
            variableUsage += signatures[index].variableUsage;
            endonUsage += signatures[index].endonUsage;
            index++;
        } while (index < signatureCount &&
                 ThreadInfoCompare(signature, &signatures[index]) == 0);

#if defined(__x86_64__)
        coduo_x87_truncation_control_t conversionControl;
        int32_t variableUsageInteger = CODUO_X87_TRUNCATE_I32_FIRST(
            &conversionControl, (long double)variableUsage);
        int32_t endonUsageInteger = CODUO_X87_TRUNCATE_I32_NEXT(
            &conversionControl, (long double)endonUsage);
#else
        int32_t variableUsageInteger = (int32_t)variableUsage;
        int32_t endonUsageInteger = (int32_t)endonUsage;
#endif
        Com_Printf("count: %d, var usage: %d, endon usage: %d\n", count,
                   variableUsageInteger, endonUsageInteger);
        Scr_PrintPrevCodePos(SCRIPT_USAGE_PRINT_CHANNEL,
                                 signature->codePosStack[0], 0);
        for (int32_t stackIndex = 1; stackIndex < signature->depth;
             ++stackIndex) {
            Com_Printf("called from:\n");
            Scr_PrintPrevCodePos(SCRIPT_USAGE_PRINT_CHANNEL,
                                     signature->codePosStack[stackIndex], 0);
        }
    }

    Z_FreeInternal(signatures);
    Com_Printf("********************************\n");

    for (uint32_t usageIndex = 0;
         usageIndex < script_entityTypeUsageCount; ++usageIndex) {
        uint16_t classEntry =
            FindVariable(script_entityTypeClassMapRoot, usageIndex);

        if (classEntry == 0) {
            continue;
        }

        float variableUsage = 0.0f;
        int32_t count = 0;
        uint16_t classObject =
            FindObject(classEntry);

        for (uint16_t child =
                 FindNextSibling(classObject);
             child != 0; child = FindNextSibling(child)) {
            count++;
            variableUsage += Scr_GetObjectUsage(
                script_variableNodes[child].payload.halves.valueOrRefCount);
        }

#if defined(__x86_64__)
        int32_t variableUsageInteger =
            CODUO_X87_TRUNCATE_I32((long double)variableUsage);
#else
        int32_t variableUsageInteger = (int32_t)variableUsage;
#endif
        Com_Printf("ent type '%s'... count: %d, var usage: %d\n",
                   coduomp_script_usage_entity_type_name(
                       &script_entityTypeUsageRecords[usageIndex]),
                   count, variableUsageInteger);
    }

    Com_Printf("********************************\n");
}
#else
/* Source: CoDUOMP.exe 0x00482fe0..0x0048334c.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00482fe0_0048334d.mcode. */
void Scr_DumpScriptThreads(void)
{
    script_usage_stack_signature_t *signatures =
        malloc((size_t)SCRIPT_VARIABLE_NODE_COUNT * sizeof(*signatures));
    if (signatures == NULL) {
        SCRIPT_OUT_OF_MEMORY();
        return;
    }
    memset(signatures, 0,
           (size_t)SCRIPT_VARIABLE_NODE_COUNT * sizeof(*signatures));

    int32_t signatureCount = 0;
    for (uint32_t handle = 1; handle < SCRIPT_VARIABLE_NODE_COUNT;
         ++handle) {
        script_variable_node_t *node = &script_variableNodes[handle];

        if ((node->packedTypeIndex & SCRIPT_USAGE_STACK_NODE_BITS) == 0 ||
            (node->packedTypeIndex & SCRIPT_VARIABLE_NODE_TYPE_MASK) !=
                SCRIPT_USAGE_STACK_SIGNATURE_NODE_TYPE) {
            continue;
        }

        const VariableStackBuffer *frame =
            (const VariableStackBuffer *)node->payload.valuePayload;
        script_usage_stack_signature_t *signature =
            &signatures[signatureCount++];
        signature->variableUsage =
            Scr_GetThreadUsage(frame, &signature->endonUsage);
        signature->depth = coduomp_script_usage_collect_code_positions(
            frame, signature->codePosStack);
    }

    coduo_qsort(signatures, (size_t)signatureCount, sizeof(*signatures),
                ThreadInfoCompare);

    Com_Printf("********************************\n");
    for (int32_t first = 0; first < signatureCount;) {
        script_usage_stack_signature_t *signature = &signatures[first];
        int32_t next = first;
        int32_t count = 0;
        long double variableUsage = 0.0L;
        long double endonUsage = 0.0L;

        do {
            variableUsage += signatures[next].variableUsage;
            endonUsage += signatures[next].endonUsage;
            ++count;
            ++next;
        } while (next < signatureCount &&
                 ThreadInfoCompare(signature, &signatures[next]) == 0);

        Com_Printf("count: %d, var usage: %d, endon usage: %d\n",
                   count, (int32_t)variableUsage, (int32_t)endonUsage);
        Scr_PrintPrevCodePos(
            SCRIPT_USAGE_PRINT_CHANNEL, signature->codePosStack[0], 0);
        for (int32_t stackIndex = 1; stackIndex < signature->depth;
             ++stackIndex) {
            Com_Printf("called from:\n");
            Scr_PrintPrevCodePos(
                SCRIPT_USAGE_PRINT_CHANNEL,
                signature->codePosStack[stackIndex], 0);
        }
        first = next;
    }

    free(signatures);
    Com_Printf("********************************\n");

    for (uint32_t usageIndex = 0;
         usageIndex < script_entityTypeUsageCount; ++usageIndex) {
        uint16_t classEntry = FindVariable(
            script_entityTypeClassMapRoot, usageIndex);
        if (classEntry == 0) {
            continue;
        }

        uint16_t classObject = FindObject(classEntry);
        int32_t count = 0;
        float variableUsage = 0.0f;
        for (uint16_t child = FindNextSibling(classObject);
             child != 0; child = FindNextSibling(child)) {
            ++count;
            variableUsage += Scr_GetObjectUsage(
                script_variableNodes[child]
                    .payload.halves.valueOrRefCount);
        }

        Com_Printf("ent type '%s'... count: %d, var usage: %d\n",
                   script_entityTypeUsageRecords[usageIndex].name,
                   count, (int32_t)variableUsage);
    }
    Com_Printf("********************************\n");
}
#endif

void Scr_DumpScriptVariables(void)
{
}

int32_t Scr_GetNumScriptVars(void)
{
    return 0;
}
