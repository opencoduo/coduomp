#include "script_notify.h"
#include "script_error_reporting.h"
#include "script_runtime_host.h"
#include "script_thread.h"
#include "script_value.h"
#include "script_variable.h"
#include "script_vm.h"

/* Source: CoDUOMP.exe 0x0048f0b0..0x0048f3a2.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0048f0b0_0048f3a3.mcode. */
uint16_t VM_ExecuteThread(uint16_t parent, script_codepos_t codePos, uint32_t paramCount)
{
    while (script_parameterCount != 0) {
        RemoveRefToValue(script_valueStackTop);
        script_valueStackTop--;
        script_parameterCount--;
    }

    VariableValue *stackBase = &script_valueStackTop[-(int32_t)paramCount];
    uint32_t savedDepth = script_valueStackDepth - paramCount;

    AddRefToObject(parent);
    uint16_t thread = AllocThread(parent);

    if (script_callStackDepth < SCRIPT_CALL_STACK_COUNT) {
        script_callStackCodepos[script_callStackDepth] = codePos;
        script_callStackDepth++;

        script_variable_type_t savedType = stackBase->type;
        stackBase->type = SCRIPT_VAR_CODEPOS;
        script_valueStackDepth = 0;

        thread = VM_Execute(script_valueStackTop, codePos, thread, thread, stackBase);

        stackBase->type = savedType;
        script_valueStackTop = stackBase + 1;
        script_valueStackDepth = savedDepth + 1;
        ClearVariableValue(script_tempValueHandle);
        return thread;
    }

    KillThread(thread);
    script_valueStackDepth = savedDepth + 1;
    uint32_t valuesToRelease = savedDepth;
    while (valuesToRelease != 0) {
        RemoveRefToValue(script_valueStackTop);
        script_valueStackTop--;
        valuesToRelease--;
    }

    script_valueStackTop++;
    script_valueStackTop->type = SCRIPT_VAR_UNDEFINED;
    RuntimeError(codePos, 0, "script stack overflow (too many embedded function calls)", NULL);
    return thread;
}

/* Source: CoDUOMP.exe 0x0048f540..0x0048f5b5.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0048f540_0048f5b6.mcode. */
void Scr_AddExecThread(uint32_t codeOffset, uint32_t paramCount)
{
    if (script_callStackDepth == 0) {
        Scr_ResetTimeout();
    }

    script_codepos_t codePos = script_codeBase + codeOffset;
#if defined(LINUX_BEHAVIOR)
    ScriptCode_IsLoadedCodePos(codePos);
#endif
    uint16_t thread = VM_ExecuteThread(script_levelHandle, codePos, paramCount);
    RemoveRefToObject(thread);
}

/* Source: CoDUOMP.exe 0x0048f5c0..0x0048f63f.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0048f5c0_0048f640.mcode. */
void Scr_AddExecEntThreadNum(int32_t entityNum, int32_t classNum, uint32_t codeOffset, uint32_t paramCount)
{
    if (script_callStackDepth == 0) {
        Scr_ResetTimeout();
    }

    uint16_t object = Scr_GetEntityId(entityNum, classNum);
    script_codepos_t codePos = script_codeBase + codeOffset;
#if defined(LINUX_BEHAVIOR)
    ScriptCode_IsLoadedCodePos(codePos);
#endif
    uint16_t thread = VM_ExecuteThread(object, codePos, paramCount);
    RemoveRefToObject(thread);
}

/* Source: CoDUOMP.exe 0x0048f3e0..0x0048f467.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0048f3e0_0048f468.mcode. */
uint16_t Scr_ExecThread(uint32_t codeOffset, uint32_t paramCount)
{
    if (script_callStackDepth == 0) {
        Scr_ResetTimeout();
    }

    script_codepos_t codePos = script_codeBase + codeOffset;
#if defined(LINUX_BEHAVIOR)
    ScriptCode_IsLoadedCodePos(codePos);
#endif
    uint16_t thread = VM_ExecuteThread(script_levelHandle, codePos, paramCount);

    RemoveRefToValue(script_valueStackTop);
    script_valueStackTop->type = SCRIPT_VAR_UNDEFINED;
    script_valueStackTop--;
    script_valueStackDepth--;
    return thread;
}

/* Source: CoDUOMP.exe 0x0048f490..0x0048f521.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0048f490_0048f522.mcode. */
uint16_t Scr_ExecEntThreadNum(int32_t entityNum, int32_t classNum, uint32_t codeOffset, uint32_t paramCount)
{
    if (script_callStackDepth == 0) {
        Scr_ResetTimeout();
    }

    uint16_t object = Scr_GetEntityId(entityNum, classNum);
    script_codepos_t codePos = script_codeBase + codeOffset;
#if defined(LINUX_BEHAVIOR)
    ScriptCode_IsLoadedCodePos(codePos);
#endif
    uint16_t thread = VM_ExecuteThread(object, codePos, paramCount);

    RemoveRefToValue(script_valueStackTop);
    script_valueStackTop->type = SCRIPT_VAR_UNDEFINED;
    script_valueStackTop--;
    script_valueStackDepth--;
    return thread;
}

/* Source: CoDUOMP.exe 0x0048f640..0x0048f682.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0048f640_0048f683.mcode. */
void Scr_FreeThread(uint16_t thread)
{
    RemoveRefToObject(thread);
}

/* Source: CoDUOMP.exe 0x0048f690..0x0048f6e4.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0048f690_0048f6e5.mcode. */
void VM_SetTime(void)
{
    if (script_timeArrayHandle == 0) {
        return;
    }

    uint16_t timeSlot = FindVariable(script_timeArrayHandle, script_currentTimeKey);
    if (timeSlot == 0) {
        return;
    }

    uint16_t threadList = FindObject(timeSlot);
    VM_Resume(threadList);
    SafeRemoveVariable(script_timeArrayHandle, script_currentTimeKey);
}

/* Source: CoDUOMP.exe 0x0048e370..0x0048e5c2.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0048e370_0048e5c3.mcode. */
void Scr_NotifyId(uint16_t objectHandle, uint16_t notifyName, uint32_t paramCount)
{
    while (script_parameterCount != 0) {
        RemoveRefToValue(script_valueStackTop);
        script_valueStackTop--;
        script_parameterCount--;
    }

    VariableValue *stackBase = &script_valueStackTop[-(int32_t)paramCount];
    uint32_t savedDepth = script_valueStackDepth - paramCount;
    script_variable_type_t savedType = stackBase->type;

    stackBase->type = SCRIPT_VAR_CODEPOS;
    script_valueStackDepth = 0;
    VM_Notify(objectHandle, notifyName, script_valueStackTop);
    stackBase->type = savedType;

    while (script_valueStackTop != stackBase) {
        RemoveRefToValue(script_valueStackTop);
        script_valueStackTop--;
    }

    script_valueStackDepth = savedDepth;
}

/* Source: CoDUOMP.exe 0x0048e600..0x0048e862.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0048e600_0048e863.mcode. */
void Scr_NotifyNum(int32_t entityNum, int32_t classNum, uint16_t notifyName, uint32_t paramCount)
{
    while (script_parameterCount != 0) {
        RemoveRefToValue(script_valueStackTop);
        script_valueStackTop--;
        script_parameterCount--;
    }

    VariableValue *stackBase = &script_valueStackTop[-(int32_t)paramCount];
    uint32_t savedDepth = script_valueStackDepth - paramCount;
    uint16_t objectHandle = FindEntityId(entityNum, classNum);

    if (objectHandle != 0) {
        script_variable_type_t savedType = stackBase->type;

        stackBase->type = SCRIPT_VAR_CODEPOS;
        script_valueStackDepth = 0;
        VM_Notify(objectHandle, notifyName, script_valueStackTop);
        stackBase->type = savedType;
    }

    while (script_valueStackTop != stackBase) {
        RemoveRefToValue(script_valueStackTop);
        script_valueStackTop--;
    }

    script_valueStackDepth = savedDepth;
}
