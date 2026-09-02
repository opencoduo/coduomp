#include "script_runtime.h"

/*
 * NOT_FROM_ORIGINAL_SOURCE_STORAGE_FILE
 *
 * This is a portable source-level storage aggregation, not a claim that these
 * independently linked C objects reproduce the original executable's physical
 * global layout. The recovered script compiler, VM, string pool, and variable
 * allocator prove the state roles through their original machine-code
 * consumers; keeping their definitions together makes missing storage a link
 * error instead of allowing translation units to invent private copies. Exact
 * original addresses remain a separate recovery audit where they affect
 * behavior or ownership.
 */

uint8_t script_codeNeedsDeferredCheck;
int32_t script_codegenMode;
int32_t script_codeStackDepth;
int32_t script_codeMaxStackDepth;
int32_t script_codeMaxLocalDepth;
size_t script_codeTempSize;
uint8_t *script_codeEmitCursor;
uint8_t *script_codeLastOpcodePos;
uint32_t script_codeChecksum;
uint8_t script_codeOwnsStrings;
uint16_t script_currentFunctionRoot;
script_code_string_fixup_t *script_codeStringFixups;

int32_t script_runtimeDebugReportFlag;
uint8_t script_runtimeActive;  /* original 0x0389fe4e */
uint32_t script_sourcePosTableCapacity[2];
uint32_t script_sourcePosTableCount[2];
script_source_pos_record_t *script_sourcePosTables[2];
uint32_t script_sourcePosPoolCapacity;
uint32_t script_sourcePosPoolCount;
uint32_t *script_sourcePosPool;
uint8_t *script_sourcePosLastCodePos;
uint32_t script_sourcePosCountForLastCodePos;
uint32_t script_sourceFileCount;
uint32_t script_sourceFileCapacity;
script_source_file_record_t *script_sourceFiles;
/*
 * Original initialized-data value: no serialized source-file set is active.
 * ShutdownOpcodeLookup restores this sentinel after freeing saved sources.
 */
int32_t script_savedSourceFileCount = SCRIPT_SAVED_SOURCE_FILE_COUNT_NONE;
script_saved_source_file_t *script_savedSourceFiles;
uint8_t *script_codeRelocationStart;
uint8_t *script_codeRelocationEnd;
uint8_t *script_codeBase;
uint8_t *script_developerOpBuffer;
int32_t script_developerOpcodePatchCapacity;
uint8_t **script_developerOpcodePatchTable;
qboolean script_runtimeDeveloperScriptFlag;
uint8_t *script_frameBackupCodepos[32];
uint8_t script_frameBackupOpcode[32];
int32_t script_pendingScriptLoadCount;
scr_script_load_record_t *script_pendingScriptLoadCursor;
uint16_t script_loadScriptHandleRoot;
uint16_t script_loadScriptCodeRoot;
uint8_t script_loadScriptsActive;
uint8_t script_loadAnimTreesActive;
uint8_t *script_codeEnd;
int32_t script_parseErrorCount;
int32_t xanim_activePoolPayloadSlot;
int32_t script_activeAnimTreeHandle; /* original 0x009d65c8 */
int32_t script_animCheckEnabled;
uint16_t script_animCurrentTreeRoot; /* original 0x009d65cc */
uint16_t script_animCurrentUsingTree; /* original 0x009d67a0 */
const char *script_animParseStart; /* original 0x009d5fb4 */
char *script_animParseState; /* original 0x009d5fb8 */
uint32_t script_animTreeChecksum; /* original 0x0389fe44 */
int32_t script_animTreeCounts[SCRIPT_ANIM_SLOT_COUNT];
XAnim *script_animTrees[SCRIPT_ANIM_SLOT_COUNT][SCRIPT_ANIM_TREE_SLOT_COUNT];
uint16_t script_animTreeHandles[SCRIPT_ANIM_SLOT_COUNT][SCRIPT_ANIM_TREE_SLOT_COUNT];
int32_t script_animCommentDepth;
uint16_t script_animTreeRoot;
const char *script_sourcePos;
const char *script_sourceFilename;
uint32_t script_sourceBufferOffset;
const uint8_t *script_sourceBufferEnd;
const uint8_t *script_sourceBufferStart;
uint32_t script_sourceChecksum;
int32_t script_callStackDepth;
uint8_t *script_callStackCodepos[32];
int32_t script_runtimeDeveloperFlag;
uint8_t script_forceErrorReport;
script_vm_callback_slot_t script_importCallbacks[SCRIPT_IMPORT_CALLBACK_COUNT];
script_vm_callback_slot_t script_exportCallbacks[SCRIPT_EXPORT_CALLBACK_COUNT];
script_variable_node_t script_variableNodes[SCRIPT_VARIABLE_NODE_COUNT];
Variable script_variableIndirections[SCRIPT_VARIABLE_NODE_COUNT];
uint16_t script_entityTypeClassMapRoot;
uint16_t script_classMapRoot;
script_class_map_entry_t *script_entityTypeUsageRecords;
uint32_t script_entityTypeUsageCount;
uint16_t script_timeArrayHandle;
uint16_t script_pauseArrayHandle;
uint16_t script_levelHandle;
uint16_t script_gameHandle;
uint32_t script_currentTimeKey;
int32_t script_loopWatchdogWarningFlag;
uint16_t script_animArrayHandle;
uint16_t script_tempValueHandle;
uint32_t script_parameterCount;
const char *script_errorMessage;
const char *script_errorSource;
int32_t script_errorParameterIndex;
const char *script_variableTypeNames[] = {
    "undefined", "string",    "localized string", "vector", "float",  "int",   "codepos",     "object",      "key/value",   "function",
    "stack",     "animation", "thread",           "entity", "struct", "array", "dead thread", "dead entity", "dead object",
};
VariableValue *script_valueStackTop;
VariableValue *script_valueStackLimit;
uint32_t script_valueStackDepth;
VariableValue script_valueStack[SCRIPT_VALUE_STACK_COUNT];
uint32_t script_loopWatchdogTick;
script_variable_type_t script_coerceLeftType;
script_variable_type_t script_coerceRightType;
uint16_t *script_stringCanonicalMap;
uint16_t script_stringCanonicalCount;
uint8_t *script_stringPoolBase;
uint8_t *script_vectorLocalPoolBase;
size_t script_vectorLocalPoolByteCount;
uint8_t *script_memoryArenaEnd;
uint8_t *script_importFieldBuffer;
uint16_t *script_variableToObjectId;
uint16_t *script_objectIdToVariable;
uint16_t script_savedObjectCount;
uint8_t *script_serializationCursor;
script_memory_block_t script_memoryBlocks[SCRIPT_MEMORY_BLOCK_COUNT];
uint8_t script_memoryBitWidthTable[256];
uint8_t script_memoryPopcountTable[256];
uint8_t script_memoryTrailingZeroBits[256];
uint16_t script_memoryFreeRoots[17];
int32_t script_memoryAllocatedBucketCount;
int32_t script_memoryAllocatedInstanceCount;
script_string_hash_slot_t script_stringHashSlots[16384];
script_string_hash_slot_t *script_stringFreedHashSlot;
size_t script_codeSize;
script_code_offset_patch_t *script_breakPatchList;
script_code_offset_patch_t *script_continuePatchList;
uint8_t script_caseAllowed;
uint8_t script_caseAllowedInDeveloperBlock;
script_switch_case_record_t *script_caseRecordList;
uint8_t script_breakAllowed;
uint8_t script_breakAllowedInDeveloperBlock;
uint8_t script_continueAllowed;
uint8_t script_continueAllowedInDeveloperBlock;
