#include "script_memory.h"
#include "script_notify.h"
#include "script_error_reporting.h"
#include "script_runtime_host.h"
#include "script_string.h"
#include "script_value.h"
#include "script_variable.h"
#include "script_vm.h"

#include <string.h>

enum {
    SCRIPT_NOTIFY_OBJECT_NAME_BASE = 65536,
    SCRIPT_NOTIFY_WAIT_KEY = 131072,
    SCRIPT_NOTIFY_STACK_KEY = 131073,
    SCRIPT_NOTIFY_VARIABLE_TYPE_MASK = 31,
    SCRIPT_NOTIFY_TIME_KEY_MASK = 0x00ffffff,
    SCRIPT_NOTIFY_MT_TAG = 1
};

/* NOT_FROM_ORIGINAL_SOURCE: shared source spelling for the dead-thread type
 * write repeated by 0x00483b60. */
static void coduomp_script_notify_mark_dead_thread(uint16_t thread)
{
    /* The original clears only the low nibble (AND ~0xf at 0x00483ca4 /
     * 0x00483dfb) before setting DEAD_THREAD, not the full 0x1f type mask;
     * identical final state but match the instruction. */
    script_variableNodes[thread].packedTypeIndex &= 0xfffffff0u;
    script_variableNodes[thread].packedTypeIndex |= SCRIPT_VAR_DEAD_THREAD;
}

#if UINTPTR_MAX == UINT32_MAX
_Static_assert(offsetof(VariableStackBuffer, pos) == 4, "i386 notify-frame pos offset changed");
_Static_assert(offsetof(VariableStackBuffer, size) == 8, "i386 notify-frame size offset changed");
_Static_assert(offsetof(VariableStackBuffer, entries) == 12, "i386 notify-frame header size changed");
#endif

/* NOT_FROM_ORIGINAL_SOURCE: typed access to the packed native notify-frame
 * value entries used by the recovered save/free paths. */
static void coduomp_script_notify_read_frame_value(const VariableStackBufferEntry *frameValue, VariableValue *value)
{
    value->type = (script_variable_type_t)frameValue->type;
    value->payload = frameValue->payload;
}

/* NOT_FROM_ORIGINAL_SOURCE: typed write access to the packed native
 * notify-frame entries. */
static void coduomp_script_notify_write_frame_value(VariableStackBufferEntry *frameValue, const VariableValue *value)
{
    frameValue->type = (uint8_t)value->type;
    frameValue->payload = value->payload;
}

/* NOT_FROM_ORIGINAL_SOURCE: source factoring for the repeated removal of a
 * waiter and its now-empty notify containers in 0x0048df00. */
static void coduomp_script_notify_remove_from_notify_bucket(uint16_t objectHandle, uint16_t waitRoot, uint16_t notifyBucket,
                                                            uint16_t notifyName, uint16_t waitThread)
{
    RemoveObjectVariable(notifyBucket, waitThread);
    if (GetArraySize(notifyBucket) != 0) {
        return;
    }
    RemoveVariable(waitRoot, notifyName);
    if (GetArraySize(waitRoot) == 0) {
        RemoveVariable(objectHandle, SCRIPT_NOTIFY_WAIT_KEY);
    }
}

/* NOT_FROM_ORIGINAL_SOURCE: source factoring for the repeated pause-bucket
 * cleanup in 0x0048df00. */
static void coduomp_script_notify_remove_from_pause_bucket(uint16_t parentHandle, uint16_t pauseBucket, uint16_t waitThread)
{
    RemoveObjectVariable(pauseBucket, waitThread);
    if (GetArraySize(pauseBucket) == 0) {
        RemoveObjectVariable(script_pauseArrayHandle, parentHandle);
    }
}

/* NOT_FROM_ORIGINAL_SOURCE: comparison portion factored from the notify
 * dispatcher's waittillmatch path. */
static qboolean coduomp_script_notify_frame_matches_args(VariableStackBuffer *frame, const VariableValue *argTop, qboolean *appendArgs)
{
    uint8_t *codePos = frame->pos;

    if (codePos[-1] != 'N') {
        *appendArgs = argTop->type != SCRIPT_VAR_CODEPOS ? qtrue : qfalse;
        return qtrue;
    }

    /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
    int32_t remaining = (int8_t)codePos[0];
    const VariableStackBufferEntry *frameValue = frame->entries + (frame->size - remaining);
    const VariableValue *arg = argTop;

    while (remaining != 0) {
        if (arg->type == SCRIPT_VAR_CODEPOS) {
            return qfalse;
        }
        --remaining;

        VariableValue savedValue;
        coduomp_script_notify_read_frame_value(frameValue, &savedValue);
        ++frameValue;

        if (savedValue.type == SCRIPT_VAR_CODEPOS) {
            frame->pos = codePos + 1;
            *appendArgs = qfalse;
            return qtrue;
        }

        AddRefToValueOfType(savedValue.type, savedValue.u);
        VariableValue notifyValue = *arg;
        AddRefToValueOfType(notifyValue.type, notifyValue.u);

        if (CheckEquality(&savedValue, &notifyValue) == qfalse) {
            RuntimeError(frame->pos, (int32_t)(int8_t)codePos[0] - remaining + 2, script_errorMessage, script_errorSource);
            script_errorMessage = NULL;
            script_errorSource = NULL;
            return qfalse;
        }
        if (savedValue.payload == qfalse) {
            return qfalse;
        }
        --arg;
    }

    frame->pos = codePos + 1;
    *appendArgs = qfalse;
    return qtrue;
}

/* NOT_FROM_ORIGINAL_SOURCE: frame-resize and append sequence factored from
 * the notify dispatcher's argument append path. */
static VariableStackBuffer *coduomp_script_notify_append_args_to_frame(VariableStackBuffer *frame, VariableValue *stackBase,
                                                                       VariableValue *argTop)
{
    uint16_t oldCount = frame->size;
    size_t appendCount = (size_t)(argTop - stackBase);
    uint16_t newCount = (uint16_t)(oldCount + appendCount);
    size_t oldSize = offsetof(VariableStackBuffer, entries) + (size_t)oldCount * sizeof(frame->entries[0]);
    size_t newSize = offsetof(VariableStackBuffer, entries) + (size_t)newCount * sizeof(frame->entries[0]);

    if (MT_Realloc(oldSize, newSize) == qfalse) {
#if defined(WINDOWS_BEHAVIOR)
        VariableStackBuffer *newFrame = MT_Alloc(newSize);
#else
        VariableStackBuffer *newFrame = MT_Alloc(newSize, SCRIPT_NOTIFY_MT_TAG);
#endif

        newFrame->time = frame->time;
        newFrame->pos = frame->pos;
        newFrame->localId = frame->localId;
        memcpy(newFrame->entries, frame->entries, (size_t)oldCount * sizeof(frame->entries[0]));
        MT_Free(frame, oldSize);
        frame = newFrame;
    }

    frame->size = newCount;
    VariableStackBufferEntry *frameValue = frame->entries + oldCount;
    for (VariableValue *value = stackBase + 1; value <= argTop; ++value) {
        AddRefToValueOfType(value->type, value->u);
        coduomp_script_notify_write_frame_value(frameValue, value);
        ++frameValue;
    }
    return frame;
}

/* Source: CoDUOMP.exe 0x00483b60..0x00483e46.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00483b60_00483e4b.mcode. */
void KillThread(uint16_t objectHandle)
{
    AddRefToObject(objectHandle);
    ClearObjectInternal(objectHandle);
    RemoveRefToObject(GetSelf(objectHandle));
    uint16_t parentHandle = GetSelf(objectHandle);

    uint16_t pauseSlot = FindObjectVariable(script_pauseArrayHandle, objectHandle);
    if (pauseSlot != 0) {
        uint16_t pauseBucket = FindObject(pauseSlot);

        for (;;) {
            uint16_t child = FindNextSibling(pauseBucket);
            if (child == 0) {
                break;
            }

            uint16_t waitingThread = (uint16_t)GetVariableName(child);
            uint16_t waitingSlot = FindObjectVariable(pauseBucket, waitingThread);
            uint16_t waitObject = (uint16_t)GetVariableValueAddress(waitingSlot)->payload;

            VM_CancelNotify(waitObject, waitingThread);
            coduomp_script_notify_mark_dead_thread(waitingThread);
            RemoveObjectVariable(pauseBucket, waitingThread);
            RemoveRefToObject(objectHandle);
        }

        RemoveObjectVariable(script_pauseArrayHandle, objectHandle);
    }

    if (GetVariableName(objectHandle) != 0) {
        uint16_t parentPauseSlot = FindObjectVariable(script_pauseArrayHandle, parentHandle);
        uint16_t parentPauseBucket = FindObject(parentPauseSlot);
        uint16_t waitingSlot = FindObjectVariable(parentPauseBucket, objectHandle);
        uint16_t waitObject = (uint16_t)GetVariableValueAddress(waitingSlot)->payload;

        VM_CancelNotify(waitObject, objectHandle);
        RemoveObjectVariable(parentPauseBucket, objectHandle);
        if (GetArraySize(parentPauseBucket) == 0) {
            RemoveObjectVariable(script_pauseArrayHandle, parentHandle);
        }
    }

    coduomp_script_notify_mark_dead_thread(objectHandle);
    RemoveRefToObject(objectHandle);
}

/* Source: CoDUOMP.exe 0x0048d990..0x0048da46.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0048d990_0048da47.mcode. */
void VM_ArchiveStack(int32_t valueCount, script_codepos_t codePos, uint16_t threadHandle, uint16_t objectHandle,
                     VariableValue *valueBeforeArgs, uint32_t resumeTimeKey)
{
    size_t frameSize = offsetof(VariableStackBuffer, entries) + (size_t)valueCount * sizeof(VariableStackBufferEntry);
#if defined(WINDOWS_BEHAVIOR)
    VariableStackBuffer *frame = MT_Alloc(frameSize);
#else
    VariableStackBuffer *frame = MT_Alloc(frameSize, SCRIPT_NOTIFY_MT_TAG);
#endif

    AddRefToObject(objectHandle);
    frame->localId = threadHandle;

    int32_t callStackDepth = script_callStackDepth - 1;
    frame->pos = codePos;
    frame->size = (uint16_t)valueCount;
    frame->time = resumeTimeKey;
    script_callStackDepth = callStackDepth;

    VariableStackBufferEntry *frameValue = frame->entries;
    VariableValue *value = valueBeforeArgs + 1;
    while (valueCount != 0) {
        --valueCount;
        frameValue->type = (uint8_t)value->type;
        if (value->type == SCRIPT_VAR_CODEPOS) {
            --callStackDepth;
        }
        frameValue->payload = value->payload;
        ++frameValue;
        ++value;
    }
    script_callStackDepth = callStackDepth;

    uint16_t stackIndirection = GetVariableIndexInternal(objectHandle, SCRIPT_NOTIFY_STACK_KEY);
    uint16_t stackSlot = script_variableIndirections[stackIndirection].valueIndex;
    script_variable_node_t *stackNode = &script_variableNodes[stackSlot];
    stackNode->payload.valuePayload = (uintptr_t)frame;
    stackNode->packedTypeIndex |= SCRIPT_VAR_STACK;
}

/* Source: CoDUOMP.exe 0x0048f6f0..0x0048f7c0.  The Linux dedicated body
 * creates the same four roots in the same order and applies the same
 * 24-bit time-key mask. */
void Scr_InitSystem(uint32_t unused, uint32_t time)
{
    (void)unused;

    script_timeArrayHandle = AllocObject();
    script_pauseArrayHandle = Scr_AllocArray();
    script_levelHandle = AllocObject();
    script_gameHandle = AllocObject();
    script_currentTimeKey = time & SCRIPT_NOTIFY_TIME_KEY_MASK;
}

/* Source: CoDUOMP.exe 0x0048f7d0..0x0048fb35.  The Linux dedicated body
 * performs the same time-list termination, waiter cleanup, and root release. */
void Scr_ShutdownSystem(uint8_t unused)
{
    (void)unused;

    for (uint16_t child = FindNextSibling(script_timeArrayHandle); child != 0; child = FindNextSibling(child)) {
        VM_TerminateTime(FindObject(child));
    }

    for (;;) {
        uint16_t pauseSlot = FindNextSibling(script_pauseArrayHandle);
        if (pauseSlot == 0) {
            break;
        }

        uint16_t pauseBucket = FindObject(pauseSlot);
        uint16_t waiterSlot = FindNextSibling(pauseBucket);
        uint16_t objectHandle = (uint16_t)GetVariableValueAddress(waiterSlot)->payload;

        AddRefToObject(objectHandle);
        ScriptNotify_StopAllWaiters(objectHandle);
        RemoveRefToObject(objectHandle);
    }

    ClearObject(script_levelHandle);
    RemoveRefToObject(script_levelHandle);
    script_levelHandle = 0;

    ClearObject(script_gameHandle);
    RemoveRefToObject(script_gameHandle);
    script_gameHandle = 0;

    ClearObject(script_timeArrayHandle);
    RemoveRefToObject(script_timeArrayHandle);
    script_timeArrayHandle = 0;

    RemoveRefToObject(script_pauseArrayHandle);
    script_pauseArrayHandle = 0;
}

/* Source: CoDUOMP.exe 0x0048da50..0x0048dc4e.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0048da50_0048dc4f.mcode.
 * Name and signature: exact same-module Mac symbol
 * VM_TerminateStackInternal(unsigned short, VariableStackBuffer const *). */
void VM_TerminateStackInternal(uint16_t objectHandle, VariableStackBuffer *frame)
{
    RemoveVariable(objectHandle, SCRIPT_NOTIFY_STACK_KEY);
    RemoveRefToObject(objectHandle);
    KillThread(frame->localId);
    RemoveRefToObject(frame->localId);

    uint16_t remaining = frame->size;
    const VariableStackBufferEntry *frameValue = frame->entries + remaining;

    while (remaining != 0) {
        VariableValue value;

        --frameValue;
        coduomp_script_notify_read_frame_value(frameValue, &value);
        --remaining;

        if (value.type == SCRIPT_VAR_CODEPOS) {
            VariableValue objectValue;

            --frameValue;
            coduomp_script_notify_read_frame_value(frameValue, &objectValue);
            --remaining;
            KillThread((uint16_t)objectValue.payload);
            RemoveRefToObject((uint16_t)objectValue.payload);
        } else {
            RemoveRefToValue(&value);
        }
    }

    MT_Free(frame, offsetof(VariableStackBuffer, entries) + (size_t)frame->size * sizeof(frame->entries[0]));
}

/* Source: CoDUOMP.exe 0x0048dc70..0x0048dcb7.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0048dc70_0048dcb8.mcode.
 * Name and signature: exact same-module Mac symbol
 * VM_TerminateStack(unsigned short). */
void VM_TerminateStack(uint16_t objectHandle)
{
    uint16_t stackIndirection = FindVariableIndexInternal(objectHandle, SCRIPT_NOTIFY_STACK_KEY);
    uint16_t stackSlot = script_variableIndirections[stackIndirection].valueIndex;

    if (stackSlot != 0) {
        VariableStackBuffer *frame = (VariableStackBuffer *)script_variableNodes[stackSlot].payload.valuePayload;
        VM_TerminateStackInternal(objectHandle, frame);
        return;
    }

    KillThread(objectHandle);
    RemoveRefToObject(objectHandle);
}

/* Source: CoDUOMP.exe 0x0048dcc0..0x0048dd69.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0048dcc0_0048dd6a.mcode.
 * Name and signature: exact same-module Mac symbol
 * VM_CancelNotify(unsigned short, unsigned short). */
void VM_CancelNotify(uint16_t objectHandle, uint16_t waitThread)
{
    uint16_t notifyName = (uint16_t)(script_variableNodes[waitThread].packedTypeIndex >> 8);

    ClearThreadNotifyName(waitThread);

    uint16_t waitIndirection = FindVariableIndexInternal(objectHandle, SCRIPT_NOTIFY_WAIT_KEY);
    uint16_t waitSlot = script_variableIndirections[waitIndirection].valueIndex;
    uint16_t waitRoot = (uint16_t)script_variableNodes[waitSlot].payload.valuePayload;

    uint16_t notifyIndirection = FindVariableIndexInternal(waitRoot, notifyName);
    uint16_t notifySlot = script_variableIndirections[notifyIndirection].valueIndex;
    uint16_t notifyBucket = (uint16_t)script_variableNodes[notifySlot].payload.valuePayload;

    RemoveObjectVariable(notifyBucket, waitThread);
    if (GetArraySize(notifyBucket) == 0) {
        RemoveVariable(waitRoot, notifyName);
        if (GetArraySize(waitRoot) == 0) {
            RemoveVariable(objectHandle, SCRIPT_NOTIFY_WAIT_KEY);
        }
    }
}

/* Source: CoDUOMP.exe 0x0048dd70..0x0048def2.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0048dd70_0048def3.mcode.
 * Name and signature: exact same-module Mac symbol
 * VM_Terminate(unsigned short). */
void VM_Terminate(uint16_t objectHandle)
{
    uint16_t stackIndirection = FindVariableIndexInternal(objectHandle, SCRIPT_NOTIFY_STACK_KEY);
    uint16_t stackSlot = script_variableIndirections[stackIndirection].valueIndex;
    if (stackSlot == 0) {
        return;
    }

    VariableStackBuffer *frame = (VariableStackBuffer *)script_variableNodes[stackSlot].payload.valuePayload;
    uint16_t ownerHandle = script_variableNodes[objectHandle].payload.halves.parentHandle;

    uint16_t pauseOwnerIndirection = FindVariableIndexInternal(script_pauseArrayHandle, SCRIPT_NOTIFY_OBJECT_NAME_BASE + ownerHandle);
    uint16_t pauseOwnerSlot = script_variableIndirections[pauseOwnerIndirection].valueIndex;
    if (pauseOwnerSlot != 0) {
        uint16_t pauseOwnerBucket = (uint16_t)script_variableNodes[pauseOwnerSlot].payload.valuePayload;
        uint16_t pauseThreadIndirection = FindVariableIndexInternal(pauseOwnerBucket, SCRIPT_NOTIFY_OBJECT_NAME_BASE + objectHandle);
        uint16_t pauseThreadSlot = script_variableIndirections[pauseThreadIndirection].valueIndex;

        if (pauseThreadSlot != 0) {
            uint16_t waitObject = (uint16_t)script_variableNodes[pauseThreadSlot].payload.valuePayload;

            VM_CancelNotify(waitObject, objectHandle);
            AddRefToObject(objectHandle);
            RemoveObjectVariable(pauseOwnerBucket, objectHandle);
            if (GetArraySize(pauseOwnerBucket) == 0) {
                RemoveObjectVariable(script_pauseArrayHandle, ownerHandle);
            }
            VM_TerminateStackInternal(objectHandle, frame);
            return;
        }
    }

    uint32_t resumeTimeKey = frame->time;
    uint16_t timeIndirection = FindVariableIndexInternal(script_timeArrayHandle, resumeTimeKey);
    uint16_t timeSlot = script_variableIndirections[timeIndirection].valueIndex;
    uint16_t timeBucket = (uint16_t)script_variableNodes[timeSlot].payload.valuePayload;

    AddRefToObject(objectHandle);
    RemoveObjectVariable(timeBucket, objectHandle);
    if (GetArraySize(timeBucket) == 0 && resumeTimeKey != script_currentTimeKey) {
        RemoveVariable(script_timeArrayHandle, resumeTimeKey);
    }
    VM_TerminateStackInternal(objectHandle, frame);
}

/* Source: CoDUOMP.exe 0x0048df00..0x0048e36d.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0048df00_0048e36e.mcode. */
void VM_Notify(uint16_t objectHandle, uint16_t notifyName, VariableValue *argTop)
{
    uint16_t waitIndirection = FindVariableIndexInternal(objectHandle, SCRIPT_NOTIFY_WAIT_KEY);
    uint16_t waitSlot = script_variableIndirections[waitIndirection].valueIndex;
    if (waitSlot == 0) {
        return;
    }
    uint16_t waitRoot = (uint16_t)script_variableNodes[waitSlot].payload.valuePayload;

    uint16_t notifyIndirection = FindVariableIndexInternal(waitRoot, notifyName);
    uint16_t notifySlot = script_variableIndirections[notifyIndirection].valueIndex;
    if (notifySlot == 0) {
        return;
    }
    uint16_t notifyBucket = (uint16_t)script_variableNodes[notifySlot].payload.valuePayload;
    AddRefToObject(notifyBucket);

    uint16_t child = FindNextSibling(notifyBucket);
    while (child != 0) {
        uint16_t waitThread = (uint16_t)(script_variableNodes[child].packedTypeIndex >> 8);
        uint16_t parentHandle = script_variableNodes[waitThread].payload.halves.parentHandle;

        uint16_t pauseSlot = FindObjectVariable(script_pauseArrayHandle, parentHandle);
        uint16_t pauseBucket = FindObject(pauseSlot);

        uint16_t stackSlot = FindVariable(waitThread, SCRIPT_NOTIFY_STACK_KEY);
        if (stackSlot == 0) {
            ClearThreadNotifyName(waitThread);
            AddRefToObject(waitThread);
            coduomp_script_notify_remove_from_notify_bucket(objectHandle, waitRoot, notifyBucket, notifyName, waitThread);
            coduomp_script_notify_remove_from_pause_bucket(parentHandle, pauseBucket, waitThread);
            AddRefToObject(parentHandle);
            KillThread(waitThread);
            RemoveRefToObject(waitThread);
            VM_Terminate(parentHandle);
            RemoveRefToObject(parentHandle);
            child = FindNextSibling(notifyBucket);
            continue;
        }

        script_variable_node_t *stackNode = &script_variableNodes[stackSlot];
        VariableStackBuffer *frame = (VariableStackBuffer *)stackNode->payload.valuePayload;
        qboolean appendArgs;
        if (coduomp_script_notify_frame_matches_args(frame, argTop, &appendArgs) == qfalse) {
            child = FindNextSibling(child);
            continue;
        }

        ClearThreadNotifyName(waitThread);
        coduomp_script_notify_remove_from_notify_bucket(objectHandle, waitRoot, notifyBucket, notifyName, waitThread);

        frame->time = script_currentTimeKey;
        uint16_t timeSlot = GetVariable(script_timeArrayHandle, script_currentTimeKey);
        uint16_t timeBucket = GetArray(timeSlot);
        (void)GetObjectVariable(timeBucket, waitThread);

        coduomp_script_notify_remove_from_pause_bucket(parentHandle, pauseBucket, waitThread);

        if (appendArgs != qfalse) {
            VariableValue *stackBase = argTop;
            while (stackBase->type != SCRIPT_VAR_CODEPOS) {
                --stackBase;
            }
            frame = coduomp_script_notify_append_args_to_frame(frame, stackBase, argTop);
            stackNode->payload.valuePayload = (uintptr_t)frame;
        }

        child = FindNextSibling(notifyBucket);
    }

    RemoveRefToObject(notifyBucket);
}

/* Source: CoDUOMP.exe 0x0048e8a0..0x0048e9ef.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0048e8a0_0048e9f0.mcode. */
void ScriptNotify_StopAllWaiters(uint16_t objectHandle)
{
    for (;;) {
        uint16_t waitIndirection = FindVariableIndexInternal(objectHandle, SCRIPT_NOTIFY_WAIT_KEY);
        uint16_t waitSlot = script_variableIndirections[waitIndirection].valueIndex;
        if (waitSlot == 0) {
            return;
        }

        uint16_t waitRoot = (uint16_t)script_variableNodes[waitSlot].payload.valuePayload;
        uint16_t notifySlot = FindNextSibling(waitRoot);
        if (notifySlot == 0) {
            return;
        }

        uint16_t notifyBucket = (uint16_t)script_variableNodes[notifySlot].payload.valuePayload;
        uint16_t waiterSlot = FindNextSibling(notifyBucket);
        if (waiterSlot == 0) {
            return;
        }

        uint16_t waitingThread = (uint16_t)GetVariableName(waiterSlot);
        AddRefToObject(waitingThread);

        uint16_t stackIndirection = FindVariableIndexInternal(waitingThread, SCRIPT_NOTIFY_STACK_KEY);
        uint16_t stackSlot = script_variableIndirections[stackIndirection].valueIndex;
        if (stackSlot != 0) {
            VariableStackBuffer *frame = (VariableStackBuffer *)script_variableNodes[stackSlot].payload.valuePayload;
            VM_TerminateStackInternal(waitingThread, frame);
        } else {
            KillThread(waitingThread);
            RemoveRefToObject(waitingThread);
        }
    }
}

/* Source: CoDUOMP.exe 0x0048e9f0..0x0048eb6a.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0048e9f0_0048eb6f.mcode. */
void VM_TerminateTime(uint16_t threadList)
{
    AddRefToObject(threadList);

    for (;;) {
        uint16_t child = FindNextSibling(threadList);
        if (child == 0) {
            break;
        }

        uint16_t thread = (uint16_t)GetVariableName(child);
        AddRefToObject(thread);
        RemoveObjectVariable(threadList, thread);

        uint16_t stackIndirection = FindVariableIndexInternal(thread, SCRIPT_NOTIFY_STACK_KEY);
        uint16_t stackSlot = script_variableIndirections[stackIndirection].valueIndex;
        if (stackSlot != 0) {
            VariableStackBuffer *frame = (VariableStackBuffer *)script_variableNodes[stackSlot].payload.valuePayload;
            VM_TerminateStackInternal(thread, frame);
        } else {
            KillThread(thread);
            RemoveRefToObject(thread);
        }
    }

    RemoveRefToObject(threadList);
}

/* Source: CoDUOMP.exe 0x0048eb70..0x0048ef24.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0048eb70_0048ef29.mcode. */
void VM_Resume(uint16_t threadList)
{
    Scr_ResetTimeout();
    AddRefToObject(threadList);

    for (;;) {
        uint16_t child = FindNextSibling(threadList);
        if (child == 0) {
            break;
        }

        uint16_t objectHandle = (uint16_t)GetVariableName(child);
        RemoveObjectVariable(threadList, objectHandle);

        uint16_t stackIndirection = FindVariableIndexInternal(objectHandle, SCRIPT_NOTIFY_STACK_KEY);
        uint16_t stackSlot = script_variableIndirections[stackIndirection].valueIndex;
        VariableStackBuffer *frame = (VariableStackBuffer *)script_variableNodes[stackSlot].payload.valuePayload;

        RemoveVariable(objectHandle, SCRIPT_NOTIFY_STACK_KEY);

        uint16_t savedValueCount = frame->size;
        uint16_t valueCount = savedValueCount;
        uint8_t *codePos = frame->pos;
        uint16_t threadHandle = frame->localId;
        const VariableStackBufferEntry *frameValue = frame->entries;
        VariableValue *stackTop = &script_valueStack[0];

        while (valueCount != 0) {
            ++stackTop;
            --valueCount;
            coduomp_script_notify_read_frame_value(frameValue, stackTop);
            ++frameValue;

            if (stackTop->type == SCRIPT_VAR_CODEPOS) {
                script_callStackCodepos[script_callStackDepth] = (uint8_t *)stackTop->payload;
                ++script_callStackDepth;
            }
        }

        script_callStackCodepos[script_callStackDepth] = codePos;
        ++script_callStackDepth;

        MT_Free(frame, offsetof(VariableStackBuffer, entries) + (size_t)savedValueCount * sizeof(frame->entries[0]));

        /* The original reads the thread node's parentHandle field (+2,
         * 0xaa6b6a) and type-checks THAT node (MOVZX word[thread*12+2] then
         * dword[parent*12+4] & 0x1f at 0x0048ed13-0x0048ed25) — i.e.
         * GetType(GetParentHandle(threadHandle)). The prior reconstruction read
         * hashOrFreeNext (+8) through the indirection table, a different
         * field and table, so the >= ARRAY branch dispatched on the wrong
         * node. */
        uint16_t parentNode = GetSelf(threadHandle);
        if (GetVarType(parentNode) >= SCRIPT_VAR_ARRAY) {
            for (;;) {
                KillThread(threadHandle);
                RemoveRefToObject(threadHandle);

                while (stackTop->type != SCRIPT_VAR_CODEPOS) {
                    RemoveRefToValue(stackTop);
                    --stackTop;
                }

                --script_callStackDepth;
                if (stackTop == &script_valueStack[0]) {
                    break;
                }

                threadHandle = (uint16_t)stackTop[-1].payload;
                stackTop -= 2;
            }
        } else {
            uint16_t result = VM_Execute(stackTop, codePos, threadHandle, objectHandle, &script_valueStack[0]);
            RemoveRefToObject(result);
            RemoveRefToValue(&script_valueStack[1]);
        }
    }

    RemoveRefToObject(threadList);
    ClearVariableValue(script_tempValueHandle);
}
