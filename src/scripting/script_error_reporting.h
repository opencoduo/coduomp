#ifndef SCRIPT_ERROR_REPORTING_H
#define SCRIPT_ERROR_REPORTING_H

#include "qcommon/script_runtime_types.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

extern script_codepos_t script_callStackCodepos[SCRIPT_CALL_STACK_COUNT];
extern int32_t script_callStackDepth;

void PrintSourcePos(int32_t channel, const char *filename,
                    const char *sourceText, uint32_t sourcePos);
void Scr_PrintPrevCodePos(int32_t channel, script_codepos_t codePos,
                          int32_t sourcePosOffset);
void CompileError(uint32_t sourcePos, const char *format, ...);
void CompileError2(script_codepos_t codePos, const char *format, ...);

void RuntimeError(script_codepos_t codePos, int32_t sourcePosOffset,
                  const char *message, const char *detail);
void RuntimeErrorInternal(int32_t channel, script_codepos_t codePos,
                          int32_t sourcePosOffset, const char *message,
                          const char *detail);
void ScriptRuntime_RaiseError(void);

#ifdef __cplusplus
}
#endif

#endif
