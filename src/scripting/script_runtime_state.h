#ifndef SCRIPT_RUNTIME_STATE_H
#define SCRIPT_RUNTIME_STATE_H

#include "qcommon/script_runtime_types.h"
#include "qcommon/script_types.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

extern uint8_t script_runtimeActive;
extern int32_t script_runtimeDebugReportFlag;
extern qboolean script_runtimeDeveloperScriptFlag;
extern int32_t script_runtimeDeveloperFlag;
extern uint8_t script_loadScriptsActive;
extern uint8_t script_loadAnimTreesActive;
extern uint16_t script_loadScriptHandleRoot;
extern uint16_t script_loadScriptCodeRoot;
extern uint16_t script_animTreeRoot;
extern uint32_t script_sourceChecksum;
extern uint32_t script_sourceBufferOffset;
extern const uint8_t *script_sourceBufferEnd;
extern const uint8_t *script_sourceBufferStart;
extern int32_t script_loopWatchdogWarningFlag;

void ScriptRuntime_ResetValueRuntime(void);
qboolean ScriptRuntime_PruneGameVariableArray(uint16_t handle);
void ScriptRuntime_SetObjectFieldValue(int32_t classNum,
                                       int32_t objectNum,
                                       int32_t fieldIndex,
                                       VariableValue *value);
void ScriptRuntime_GetObjectFieldValue(int32_t classNum,
                                       int32_t objectNum,
                                       int32_t fieldIndex,
                                       VariableValue *value);

void Scr_Init(int32_t debugReport, int32_t developerScript,
              int32_t developer);
void Scr_Shutdown(void);
void Scr_Abort(void);
void Scr_SetLoading(int32_t enabled);
void Scr_AllocGameVariable(void);
void Scr_FreeGameVariable(qboolean shutdown);
void Scr_GetChecksum(uint32_t checksum[3]);

#ifdef __cplusplus
}
#endif

#endif
