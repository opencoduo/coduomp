#ifndef SHARED_SCRIPT_COMPILE_LOAD_H
#define SHARED_SCRIPT_COMPILE_LOAD_H

#include "script_compile_types.h"
#include "qcommon/script_runtime_types.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

extern uint16_t script_currentFunctionRoot;
extern int32_t script_pendingScriptLoadCount;
extern scr_script_load_record_t *script_pendingScriptLoadCursor;
extern int32_t script_parseErrorCount;

qboolean Scr_IsIdentifier(const char *text);
uint32_t Scr_GetFunctionHandle(const char *scriptName, const char *functionName);
void Scr_BeginLoadScripts(void);
void Scr_BeginLoadAnimTrees(void);
void ScriptImport_ParseSource(char *source, scr_ast_node_t **out);
qboolean Scr_LoadScript(const char *filename);
void Scr_EndLoadScripts(void);
void Scr_PrecacheAnimTrees(script_anim_tree_alloc_t allocCallback);
void Scr_EndLoadAnimTrees(void);
void Scr_FreeScripts(void);

void EmitThreadInternal(scr_ast_node_t *node, uint32_t sourcePos);
void EmitNormalThread(scr_ast_node_t *node, uint32_t sourcePos);
void EmitDeveloperThread(scr_ast_node_t *node, uint32_t sourcePos);
void EmitThread(scr_ast_node_t *node, uint32_t sourcePos);
void EmitThreadList(scr_ast_script_entry_block_t *block);
void ScriptCompile(scr_ast_node_t *script, uint16_t currentFunctionRoot);

int32_t yyparse(void);

#ifdef __cplusplus
}
#endif

#endif
