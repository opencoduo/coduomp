#ifndef SHARED_SCRIPT_NOTIFY_H
#define SHARED_SCRIPT_NOTIFY_H

#include "qcommon/script_runtime_types.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void KillThread(uint16_t objectHandle);
void VM_ArchiveStack(int32_t valueCount, script_codepos_t codePos,
                     uint16_t threadHandle, uint16_t objectHandle,
                     VariableValue *valueBeforeArgs,
                     uint32_t resumeTimeKey);
void VM_Notify(uint16_t objectHandle, uint16_t notifyName,
               VariableValue *argTop);
void VM_Resume(uint16_t listHandle);
void VM_CancelNotify(uint16_t objectHandle, uint16_t waitThread);
void VM_Terminate(uint16_t objectHandle);
void VM_TerminateStack(uint16_t objectHandle);
void VM_TerminateStackInternal(uint16_t objectHandle,
                               VariableStackBuffer *frame);
void VM_TerminateTime(uint16_t listHandle);
void ScriptNotify_StopAllWaiters(uint16_t objectHandle);

void Scr_NotifyId(uint16_t objectHandle, uint16_t notifyName,
                  uint32_t paramCount);
void Scr_NotifyNum(int32_t entityNum, int32_t classNum,
                   uint16_t notifyName, uint32_t paramCount);
void Scr_InitSystem(uint32_t unused, uint32_t time);
void Scr_ShutdownSystem(uint8_t unused);

#ifdef __cplusplus
}
#endif

#endif
