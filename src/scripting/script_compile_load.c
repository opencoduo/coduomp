#include "script_runtime_host.h"

#include "script_code_emit.h"
#include "script_anim.h"
#include "script_compile_developer.h"
#include "script_compile_expr.h"
#include "script_compile_load.h"
#include "script_compile_statements.h"
#include "script_error_reporting.h"
#include "script_memory.h"
#include "script_source_positions.h"
#include "script_string.h"
#include "script_temp_memory.h"
#include "script_variable.h"

#include <string.h>

enum {
    SCRIPT_CODEGEN_MODE_INTERN_STRINGS = 0,
    SCRIPT_CODEGEN_MODE_RECORD_STRING_FIXUPS = 1,
    SCRIPT_CODEGEN_MODE_RELEASE_STRINGS = 2,
    SCRIPT_OPCODE_END = 0,
    SCRIPT_STRING_USAGE_RUNTIME = 1,
    SCRIPT_STRING_USAGE_FUNCTION = 2,
    SCRIPT_FUNCTION_LOCAL_SLOT_WIDTH = 32,
    SCRIPT_FUNCTION_OPERAND_STACK_LIMIT = 2047,
    SCRIPT_PENDING_LOAD_COUNT_LIMIT = 2048,
    SCRIPT_PARSE_BUFFER_SIZE = 16384,
    SCRIPT_PARSE_INITIAL_START_STATE = 3
};

/* Source: CoDUOMP.exe 0x0047fe10..0x0047fe45.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0047fe10_0047fe45.mcode. */
qboolean Scr_IsIdentifier(const char *text)
{
    while (*text != '\0') {
        int32_t ch = (int32_t)(signed char)*text;

        if (SCRIPT_ISALNUM_SIGNED_BYTE(ch) == 0 && ch != '_') {
            return qfalse;
        }
        text++;
    }

    return qtrue;
}

/* Source: CoDUOMP.exe 0x0047fe50..0x0047ff24.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0047fe50_0047ff25.mcode. */
uint32_t Scr_GetFunctionHandle(const char *scriptName, const char *functionName)
{
    uint16_t canonicalScript = Scr_CreateCanonicalFilename(scriptName);
    uint16_t scriptHandle = FindVariable(script_loadScriptHandleRoot, canonicalScript);
    SL_RemoveRefToString(canonicalScript);

    if (scriptHandle == 0) {
        return 0;
    }

    uint16_t functionRoot = FindObject(scriptHandle);
    uint16_t functionNameHandle = SL_FindLowercaseString(functionName);
    if (functionNameHandle == 0) {
        return 0;
    }

    uint16_t functionHandle = FindVariable(functionRoot, functionNameHandle);
    if (functionHandle == 0) {
        return 0;
    }

    uint8_t *codePos = (uint8_t *)GetVariableValueAddress(functionHandle)->payload;
    uintptr_t codeAddress = (uintptr_t)codePos;
    uintptr_t codeBaseAddress = (uintptr_t)script_codeBase;
    if (codeAddress < codeBaseAddress || codeAddress >= codeBaseAddress + script_codeSize) {
        return 0;
    }

    return (uint32_t)(codeAddress - codeBaseAddress);
}

/* Source: CoDUOMP.exe 0x00480020..0x004800e4.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00480020_004800e5.mcode. */
void Scr_BeginLoadScripts(void)
{
    script_loadScriptsActive = qtrue;
    InitOpcodeLookup();
    Scr_InitDeveloperOpcodes();
    script_loadScriptHandleRoot = Scr_AllocArray();
    script_loadScriptCodeRoot = Scr_AllocArray();
    script_codeBase = SCRIPT_HUNK_ALLOC_LOW(0);
    script_codeSize = 0;
    script_codeEnd = NULL;
    SL_BeginLoadScripts();
    script_parseErrorCount = 0;
    Scr_BeginLoadAnimTrees();
}

/* Source: CoDUOMP.exe 0x004800f0..0x00480147.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004800f0_00480148.mcode. */
void CODUO_SCRIPT_CDECL Scr_BeginLoadAnimTrees(void)
{
    int32_t slot = xanim_activePoolPayloadSlot;

    coduo_script_compat_anim_release_unresolved_ref_sidecars();
    script_loadAnimTreesActive = qtrue;
    script_animTreeCounts[slot] = 0;
    script_animTrees[slot][0] = NULL;
    script_animTreeRoot = Scr_AllocArray();
    script_animCurrentUsingTree = 0;
}

/* Source: CoDUOMP.exe 0x00493760..0x00493802.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00493760_00493803.mcode.
 *
 * The original uses a non-owning Flex buffer state and a 16384-byte scanner
 * buffer on this stack frame. Pointer fields deliberately remain native-width:
 * the buffer never crosses a serialized or mixed-bitness boundary. */
void ScriptImport_ParseSource(char *source, scr_ast_node_t **out)
{
    script_yy_buffer_t buffer;
    char scannerBuffer[SCRIPT_PARSE_BUFFER_SIZE];

    script_yyInputCursor = source;
    script_yyCurrentSourcePos = 0;
    script_yyPreviousSourcePos = 0;
    script_yyInit = qtrue;

    buffer.bufSize = SCRIPT_PARSE_BUFFER_SIZE;
    buffer.chBuf = scannerBuffer;
    buffer.isOurBuffer = qfalse;
    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    buffer.inputFile = NULL;
    yy_init_buffer(&buffer, NULL);

    script_yyCurrentBuffer = &buffer;
    script_yyStart = SCRIPT_PARSE_INITIAL_START_STATE;
    yyparse();
    *out = script_parseRoot;
}

/* Source: CoDUOMP.exe 0x00480150..0x00480324.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00480150_00480325.mcode. */
qboolean Scr_LoadScript(const char *filename)
{
    char scriptPath[MAX_QPATH];
    scr_ast_node_t *scriptNode;

    uint16_t canonicalFilename = Scr_CreateCanonicalFilename(filename);
    uint16_t scriptHandle = FindVariable(script_loadScriptCodeRoot, canonicalFilename);
    if (scriptHandle != 0) {
        SL_RemoveRefToString(canonicalFilename);
        return FindVariable(script_loadScriptHandleRoot, canonicalFilename) != 0 ? qtrue : qfalse;
    }

    const char *canonicalName = SL_ConvertToString(canonicalFilename);

    /* NOT_FROM_ORIGINAL_SOURCE: the canonical name, ".gsc" suffix, and
     * terminator must fit before the script-load variable is published. */
    if (strlen(canonicalName) > sizeof(scriptPath) - sizeof(".gsc")) {
        SL_RemoveRefToString(canonicalFilename);
        return qfalse;
    }

    GetVariable(script_loadScriptCodeRoot, canonicalFilename);
    SL_RemoveRefToString(canonicalFilename);
    TempMemoryReset();
    Com_sprintf(scriptPath, sizeof(scriptPath), "%s.gsc", canonicalName);

    const char *savedSourcePos = script_sourcePos;
    char *source = Scr_AddSourceBuffer(scriptPath, SCRIPT_HUNK_ALLOC_LOW(0), script_codeRelocationEnd);
    const char *savedSourceFilename = script_sourceFilename;
    if (source == NULL) {
        script_sourceFilename = savedSourceFilename;
        return qfalse;
    }

    script_sourceBufferOffset = 0;
    /* Both original loaders reset the per-script using-tree handle here:
     * CoDUOMP.exe 0x00480271 and coduo_lnxded 0x080a23a8. Its omission from
     * the former Linux recovery was a transcription error. */
    script_animCurrentUsingTree = 0;
    script_pendingScriptLoadCount = 0;
    script_sourceFilename = scriptPath;
    ScriptImport_ParseSource(source, &scriptNode);

    uint16_t functionRoot = GetObject(GetVariable(script_loadScriptHandleRoot, canonicalFilename));
    ScriptCompile(scriptNode, functionRoot);

    script_sourceFilename = savedSourceFilename;
    script_sourcePos = savedSourcePos;
    return qtrue;
}

/* Source: CoDUOMP.exe 0x00480330..0x004804a2.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00480330_004804a3.mcode. */
void Scr_EndLoadScripts(void)
{
    script_loadScriptsActive = qfalse;
    SL_EndLoadScripts();
    ClearObject(script_loadScriptCodeRoot);
    RemoveRefToObject(script_loadScriptCodeRoot);
    script_loadScriptCodeRoot = 0;
    ClearObject(script_loadScriptHandleRoot);
    RemoveRefToObject(script_loadScriptHandleRoot);
    script_loadScriptHandleRoot = 0;
    Scr_InsertDeveloperOpcodes();
    SL_ShutdownSystem(SCRIPT_STRING_USAGE_FUNCTION);
}

/* Source: CoDUOMP.exe 0x004804b0..0x004804ee.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004804b0_004804ef.mcode. */
void CODUO_SCRIPT_CDECL Scr_PrecacheAnimTrees(script_anim_tree_alloc_t allocCallback)
{
    for (int32_t treeIndex = 1; treeIndex <= script_animTreeCounts[xanim_activePoolPayloadSlot]; ++treeIndex) {
        Scr_LoadAnimTreeAtIndex(treeIndex, allocCallback);
    }
}

/* Source: CoDUOMP.exe 0x004804f0..0x004805bc.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004804f0_004805bd.mcode. */
void CODUO_SCRIPT_CDECL Scr_EndLoadAnimTrees(void)
{
    script_loadAnimTreesActive = qfalse;
    coduo_script_compat_anim_release_unresolved_ref_sidecars();
    ClearObject(script_animTreeRoot);
    RemoveRefToObject(script_animTreeRoot);
    script_animTreeRoot = 0;

    if (script_animCurrentUsingTree != 0) {
        RemoveRefToObject(script_animCurrentUsingTree);
    }

    SL_ShutdownSystem(SCRIPT_STRING_USAGE_FUNCTION);
    script_codeEnd = SCRIPT_HUNK_ALLOC_LOW(0);
}

/* Source: CoDUOMP.exe 0x004805c0..0x0048064a.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004805c0_0048064b.mcode. */
void Scr_FreeScripts(void)
{
    if (script_loadScriptsActive != qfalse) {
        Scr_EndLoadScripts();
    }
    if (script_loadAnimTreesActive != qfalse) {
        Scr_EndLoadAnimTrees();
    }

    script_codeBase = NULL;
    script_codeSize = 0;
    script_codeEnd = NULL;
    script_codeChecksum = 0;
    script_loadScriptsActive = qfalse;
    SL_ShutdownSystem(SCRIPT_STRING_USAGE_RUNTIME);
    ShutdownOpcodeLookup();
    Scr_ShutdownDeveloperOpcodes();
}

/* Source: CoDUOMP.exe 0x0047f3e0..0x0047f510.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0047f3e0_0047f510.mcode. */
void EmitThreadInternal(scr_ast_node_t *node, uint32_t sourcePos)
{
    AddOpcodePos(sourcePos);
    script_codeStackDepth = 0;
    script_codeMaxStackDepth = 0;
    script_codeMaxLocalDepth = 0;
    CompileTransferRefToString(SCR_AST_STRING_HANDLE(node->payload.functionDefinition.nameHandle), SCRIPT_STRING_USAGE_FUNCTION);
    EmitFormalParameterListRef(node->payload.functionDefinition.parameters, sourcePos);
    EmitStatementList(node->payload.functionDefinition.body);
#if defined(WINDOWS_BEHAVIOR)
    /* Windows flushes a pending deferred developer check before END
     * (CoDUOMP.exe 0x0047f482..0x0047f498). Linux coduo_lnxded
     * 0x080a163e..0x080a16f2 proceeds directly from the statement-list call to
     * END. This is an original behavior difference, not recovery latitude. */
    if (script_codeNeedsDeferredCheck != qfalse && script_codegenMode == SCRIPT_CODEGEN_MODE_INTERN_STRINGS) {
        script_codeNeedsDeferredCheck = qfalse;
        Scr_TransferStatementListToDeveloperBuffer();
    }
#endif
    EmitOpcode(SCRIPT_OPCODE_END, 0, 0);

    if (SCRIPT_FUNCTION_OPERAND_STACK_LIMIT < script_codeMaxLocalDepth * SCRIPT_FUNCTION_LOCAL_SLOT_WIDTH + script_codeMaxStackDepth) {
        CompileError(sourcePos, "function exceeds operand stack size");
    }
}

/* Source: CoDUOMP.exe 0x0047f510..0x0047f589.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0047f510_0047f589.mcode. */
void EmitNormalThread(scr_ast_node_t *node, uint32_t sourcePos)
{
    uint16_t functionName = SCR_AST_STRING_HANDLE(node->payload.functionDefinition.nameHandle);
    uint16_t functionObject = FindObject(FindVariable(script_currentFunctionRoot, functionName));

    SetThreadPosition(functionObject, node->payload.functionDefinition.sourcePos);
    EmitThreadInternal(node, sourcePos);
}

/* Source: CoDUOMP.exe 0x0047f590..0x0047f688.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0047f590_0047f688.mcode. */
void EmitDeveloperThread(scr_ast_node_t *node, uint32_t sourcePos)
{
    if (script_codegenMode != SCRIPT_CODEGEN_MODE_INTERN_STRINGS) {
        CompileError(sourcePos, "cannot recurse /#");
    }

    script_codeRelocationStart = TempMalloc(0);
    uint32_t savedChecksum = script_codeChecksum;

    if (script_runtimeDeveloperScriptFlag == qfalse) {
        script_codegenMode = SCRIPT_CODEGEN_MODE_RELEASE_STRINGS;
        EmitThreadInternal(node, sourcePos);
    } else {
        script_codegenMode = SCRIPT_CODEGEN_MODE_RECORD_STRING_FIXUPS;
        uint16_t functionName = SCR_AST_STRING_HANDLE(node->payload.functionDefinition.nameHandle);
        uint16_t functionObject = FindObject(FindVariable(script_currentFunctionRoot, functionName));
        SetDeveloperThreadPosition(functionObject, node->payload.functionDefinition.sourcePos);
        EmitThreadInternal(node, sourcePos);
        Scr_TransferToDeveloperBuffer();
    }

    script_codegenMode = SCRIPT_CODEGEN_MODE_INTERN_STRINGS;
    TempMemorySetPos(script_codeRelocationStart);
    script_codeChecksum = savedChecksum;
}

/* Source: CoDUOMP.exe 0x0047f690..0x0047f728.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0047f690_0047f728.mcode. */
void EmitThread(scr_ast_node_t *node, uint32_t sourcePos)
{
    if (node->kind == SCR_AST_KIND_DEVELOPER_FUNCTION_DEFINITION) {
        EmitDeveloperThread(node, sourcePos);
        return;
    }

    if (node->kind == SCR_AST_KIND_FUNCTION_DEFINITION) {
        EmitNormalThread(node, sourcePos);
        return;
    }

    if (node->kind == SCR_AST_KIND_USING_ANIMTREE) {
        uint16_t animTreeNameHandle = SCR_AST_STRING_HANDLE(node->payload.usingAnimTree.nameHandle);
        const char *animTreeName = SL_ConvertToString(animTreeNameHandle);
        Scr_UsingTree(animTreeName, node->payload.usingAnimTree.sourcePos);
        CompileRemoveRefToString(animTreeNameHandle);
    }
}

/* Source: CoDUOMP.exe 0x0047f730..0x0047f77b.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0047f730_0047f77b.mcode. */
void EmitThreadList(scr_ast_script_entry_block_t *block)
{
    for (scr_ast_list_item_t *item = block->sentinel->head; item != NULL; item = item->next) {
        SpecifyThread(item->entry->node);
    }

    for (scr_ast_list_item_t *item = block->sentinel->head; item != NULL; item = item->next) {
        EmitThread(item->entry->node, item->entry->sourcePos);
    }
}

/* Source: CoDUOMP.exe 0x0047f780..0x0047f94c.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0047f780_0047f94c.mcode. */
void ScriptCompile(scr_ast_node_t *script, uint16_t currentFunctionRoot)
{
    script_currentFunctionRoot = currentFunctionRoot;
    script_codeEmitCursor = TempMalloc(0);
    script_codeOwnsStrings = qfalse;
    script_activeAnimTreeHandle = 0;
    script_codegenMode = SCRIPT_CODEGEN_MODE_INTERN_STRINGS;
    script_codeNeedsDeferredCheck = qfalse;

    int32_t pendingLoadCapacity = script_pendingScriptLoadCount;
    scr_script_load_record_t *pendingLoads = NULL;
    if (pendingLoadCapacity != 0) {
        /* NOT_FROM_ORIGINAL_SOURCE: recursive compilation keeps pending-load
         * records on this stack frame. Enforce the per-source record capacity
         * before reserving that storage. */
        if (pendingLoadCapacity < 0 || pendingLoadCapacity > SCRIPT_PENDING_LOAD_COUNT_LIMIT) {
            CompileError(0, "script has too many cross-script references (maximum %i)", SCRIPT_PENDING_LOAD_COUNT_LIMIT);
            return;
        }

        uint32_t pendingLoadBytes = (uint32_t)pendingLoadCapacity * (uint32_t)sizeof(*pendingLoads);
        pendingLoads = SCRIPT_HOST_ALLOCA((size_t)pendingLoadBytes);
    }
    script_pendingScriptLoadCursor = pendingLoads;

    EmitOpcode(SCRIPT_OPCODE_END, 0, 0);
    EmitThreadList(script->payload.scriptRoot.entries);
    script_codeSize = (size_t)(TempMalloc(0) - script_codeBase);
    AdjustFunctionAddresses();
    SCRIPT_HUNK_COMMIT_TEMP();
    SCRIPT_HUNK_CLEAR_TEMP_HIGH();

    int32_t pendingLoadCount = script_pendingScriptLoadCount;
    for (int32_t index = 0; index < pendingLoadCount; ++index) {
        const char *filename = SL_ConvertToString(pendingLoads[index].filenameHandle);

        if (Scr_LoadScript(filename) == qfalse) {
            const char *errorFilename = SL_ConvertToString(pendingLoads[index].filenameHandle);
            CompileError(pendingLoads[index].sourcePos, "Could not find script '%s'", errorFilename);
        }

        SL_RemoveRefToString(pendingLoads[index].filenameHandle);
    }
}
