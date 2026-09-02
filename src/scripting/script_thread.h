#ifndef SHARED_SCRIPT_THREAD_H
#define SHARED_SCRIPT_THREAD_H

#include "qcommon/script_runtime_types.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

uint16_t VM_ExecuteThread(uint16_t parent, script_codepos_t codePos,
                          uint32_t paramCount);
void Scr_AddExecThread(uint32_t codeOffset, uint32_t paramCount);
void Scr_AddExecEntThreadNum(int32_t entityNum, int32_t classNum,
                             uint32_t codeOffset, uint32_t paramCount);
uint16_t Scr_ExecThread(uint32_t codeOffset, uint32_t paramCount);
uint16_t Scr_ExecEntThreadNum(int32_t entityNum, int32_t classNum,
                              uint32_t codeOffset, uint32_t paramCount);
void Scr_FreeThread(uint16_t thread);
void VM_SetTime(void);

#ifdef __cplusplus
}
#endif

#endif
