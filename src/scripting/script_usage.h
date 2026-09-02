#ifndef SCRIPT_USAGE_H
#define SCRIPT_USAGE_H

#include "qcommon/script_runtime_types.h"
#include "qcommon/script_types.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

extern uint16_t script_pauseArrayHandle;

int32_t ThreadInfoCompare(const void *left, const void *right);
float Scr_GetEntryUsage(script_variable_type_t type, VariableUnion value);
float Scr_GetEndonUsage(uint16_t localVars);
float Scr_GetObjectUsage(uint16_t object);
float Scr_GetThreadUsage(const VariableStackBuffer *frame, float *endonUsage);
void Scr_DumpScriptThreads(void);
void Scr_DumpScriptVariables(void);
int32_t Scr_GetNumScriptVars(void);

#ifdef __cplusplus
}
#endif

#endif
