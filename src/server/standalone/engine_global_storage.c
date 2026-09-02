#include <stdint.h>
#include <stdio.h>

#include "server/standalone/bindings/coduo_engine_structs.h"
#include "animation/xanim_private.h"
#include "core_memory/core_memory_private.h"
#include "core_runtime/core_runtime_private.h"
#include "filesystem/fs_private.h"
#include "physics_collision/cm_world_sector_private.h"
#include "scripting/script_compile_private.h"
#include "scripting/script_memory.h"
#include "scripting/script_source_positions.h"
#include "sound/alias/sound_alias.h"

_Static_assert(sizeof(script_string_hash_slot_t) == 4,
               "script string hash slot size mismatch");

cvar_t *sv_serverid;
cvar_t *sv_showloss;
cbuf_t cmd_text;
char cmd_textData[CBUF_TEXT_CAPACITY];
int32_t cmd_wait;
cvar_t *cl_running;
worldSector_t *cm_freeWorldSectors;
worldSector_t cm_nullWorldSector;
vec3_t cm_worldMaxs;
vec3_t cm_worldMins;
worldSector_t cm_worldSectorPool[SERVER_WORLD_SECTOR_POOL_COUNT];
worldSector_t cm_worldSectorRoot;
int32_t cmd_argc;
char cmd_args[CMD_ARGS_CAPACITY];
const char *cmd_argv[CMD_ARGUMENT_CAPACITY];
cmd_function_t *cmd_functions;
char cmd_tokenBuffer[CMD_TOKEN_BUFFER_CAPACITY];
int32_t com_frameNumber;
int32_t com_consoleLogFile;
qboolean com_printMessageOpeningLog;
int32_t com_timeBackend;
int32_t com_timeFrontend;
int32_t com_timeGame;
qboolean com_vprintfOpeningLog;
vm_t *currentVM;
cvar_t *fs_basegame;
cvar_t *fs_basepath;
cvar_t *fs_cdpath;
int32_t fs_checksumFeed;
cvar_t *fs_copyfiles;
char fs_currentGameDir[FS_PACK_NAME_SIZE];
cvar_t *fs_debug;
fs_dir_file_list_t *fs_dirFileLists;
int32_t fs_fakeChkSum;
cvar_t *fs_game;
/* Source: coduo_lnxded 0x084843a0. FS_FOpenFileRead_Internal writes the
 * selected BSP pack/directory game name here at 0x08061d81/0x08061daf; the
 * sound-loadspec game_ modifier reads the same object at 0x0806ce47 and
 * 0x0806cfcc. */
char fs_gameDirVar[FS_PACK_NAME_SIZE];
fileHandleData_t fs_handleFiles[FS_HANDLE_COUNT];
cvar_t *fs_homepath;
cvar_t *fs_ignoreLocalized;
int fs_languageNameBufferIndex;
char fs_languageNameBuffers[FS_LANGUAGE_NAME_BUFFER_COUNT]
                           [FS_LANGUAGE_NAME_BUFFER_SIZE];
/* Source: coduo_lnxded 0x08477a20. FS_ReadFile increments this count at
 * 0x08063004 and 0x08063132; FS_ResetFiles clears it at 0x080631fb and
 * FS_FreeFile decrements it at 0x0806322c.  The exact Mac symbol/API names
 * this outstanding temporary-allocation count fs_loadStack. */
int32_t fs_loadStack;
/* Source: coduo_lnxded 0x084885c0. FS_FOpenFileRead sets this one-shot file-
 * access latch at 0x0806255e. It is distinct from the allocation stack. */
qboolean fs_fileAccessed;
fs_dir_file_list_t *fs_lookupDirFileLists;
searchpath_t *fs_lookupSearchpaths;
int32_t fs_numServerReferencedPaks;
int32_t fs_numServerPaks;
int32_t fs_packFiles;
cvar_t *fs_restrict;
char fs_savedBasePath[FS_PACK_NAME_SIZE];
char fs_savedGame[FS_PACK_NAME_SIZE];
searchpath_t *fs_searchpaths;
cvar_t *g_gametype;
cvar_t *sv_mapname;
cvar_t *rconPassword;
cvar_t *scr_allow_jeeps;
cvar_t *scr_allow_tanks;
cvar_t *sv_allowAnonymous;
cvar_t *sv_allowDownload;
cvar_t *sv_disableClientConsole;
cvar_t *sv_floodProtect;
cvar_t *sv_hostname;
cvar_t *sv_kickBanTime;
cvar_t *sv_killserver;
cvar_t *sv_mapRotationCurrent;
cvar_t *sv_mapRotation;
cvar_t *sv_maxPing;
cvar_t *sv_minPing;
cvar_t *sv_onlyVisibleClients;
cvar_t *sv_packet_info;
cvar_t *sv_privateClients;
cvar_t *sv_privatePassword;
cvar_t *sv_punkbuster;
cvar_t *sv_pure;
cvar_t *sv_reconnectlimit;
cvar_t *sv_showCommands;
cvar_t *sv_timeout;
cvar_t *sv_wwwBaseURL;
cvar_t *sv_wwwDlDisconnected;
cvar_t *sv_wwwDownload;
cvar_t *sv_zombietime;
char *sv_configstrings[MAX_CONFIGSTRINGS];
hunk_state_t hunk;
uint8_t *hunk_allocData;
uint8_t *hunk_data;
hunk_log_block_t *hunk_logBlocks;
int32_t hunk_logFile;
size_t hunk_totalZoneSize;
cvar_t *net_lanauthorize;
loopback_t net_loopbacks[NET_LOOPBACK_QUEUE_COUNT];
cvar_t *net_profile;
netProfileMode_t net_profileActiveMode;
cvar_t *net_qport;
cvar_t *net_showprofile;
int32_t script_activeAnimTreeHandle;
int32_t script_animCheckEnabled;
int32_t script_animCommentDepth;
uint16_t script_animCurrentTreeRoot;
uint16_t script_animCurrentUsingTree;
const char *script_animParseStart;
char *script_animParseState;
uint32_t script_animTreeChecksum;
int32_t script_animTreeCounts[SCRIPT_ANIM_SLOT_COUNT];
uint16_t script_animTreeHandles[SCRIPT_ANIM_SLOT_COUNT]
                               [SCRIPT_ANIM_TREE_SLOT_COUNT];
uint16_t script_animTreeRoot;
XAnim *script_animTrees[SCRIPT_ANIM_SLOT_COUNT][SCRIPT_ANIM_TREE_SLOT_COUNT];
uint8_t script_breakAllowed;
uint8_t script_breakAllowedInDeveloperBlock;
script_code_offset_patch_t *script_breakPatchList;
script_codepos_t script_callStackCodepos[SCRIPT_CALL_STACK_COUNT];
int32_t script_callStackDepth;
uint8_t script_caseAllowed;
uint8_t script_caseAllowedInDeveloperBlock;
script_switch_case_record_t *script_caseRecordList;
uint16_t script_classMapRoot;
uint8_t *script_codeBase;
uint32_t script_codeChecksum;
uint8_t *script_codeEmitCursor;
uint8_t *script_codeEnd;
uint8_t *script_codeLastOpcodePos;
int32_t script_codeMaxLocalDepth;
int32_t script_codeMaxStackDepth;
uint8_t script_codeNeedsDeferredCheck;
uint8_t script_codeOwnsStrings;
uint8_t *script_codeRelocationEnd;
uint8_t *script_codeRelocationStart;
size_t script_codeSize;
int32_t script_codeStackDepth;
script_code_string_fixup_t *script_codeStringFixups;
size_t script_codeTempSize;
int32_t script_codegenMode;
script_variable_type_t script_coerceLeftType;
script_variable_type_t script_coerceRightType;
uint8_t script_continueAllowed;
uint8_t script_continueAllowedInDeveloperBlock;
script_code_offset_patch_t *script_continuePatchList;
uint16_t script_currentFunctionRoot;
uint32_t script_currentTimeKey;
uint8_t *script_developerOpBuffer;
int32_t script_developerOpcodePatchCapacity;
uint8_t **script_developerOpcodePatchTable;
uint16_t script_entityTypeClassMapRoot;
uint32_t script_entityTypeUsageCount;
script_class_map_entry_t *script_entityTypeUsageRecords;
int32_t script_errorParameterIndex;
const char *script_errorMessage;
const char *script_errorSource;
script_vm_callback_slot_t
    script_exportCallbacks[SCRIPT_EXPORT_CALLBACK_COUNT];
uint8_t script_forceErrorReport;
script_codepos_t script_frameBackupCodepos[SCRIPT_CALL_STACK_COUNT];
uint8_t script_frameBackupOpcode[SCRIPT_CALL_STACK_COUNT];
uint16_t script_gameHandle;
script_vm_callback_slot_t
    script_importCallbacks[SCRIPT_IMPORT_CALLBACK_COUNT];
uint8_t *script_importFieldBuffer;
uint16_t script_levelHandle;
uint8_t script_loadAnimTreesActive;
uint16_t script_loadScriptCodeRoot;
uint16_t script_loadScriptHandleRoot;
uint8_t script_loadScriptsActive;
uint32_t script_loopWatchdogTick;
int32_t script_loopWatchdogWarningFlag;
int32_t script_memoryAllocatedBucketCount;
int32_t script_memoryAllocatedInstanceCount;
uint8_t *script_stringPoolBase;
uint8_t *script_vectorLocalPoolBase;
uint8_t *script_memoryArenaEnd;
size_t script_vectorLocalPoolByteCount;
uint8_t script_memoryBitWidthTable[SCRIPT_MEMORY_LOOKUP_COUNT];
script_memory_block_t script_memoryBlocks[SCRIPT_MEMORY_BLOCK_COUNT];
uint16_t script_memoryFreeRoots[SCRIPT_MEMORY_BUCKET_COUNT];
uint8_t script_memoryPopcountTable[SCRIPT_MEMORY_LOOKUP_COUNT];
uint8_t script_memoryTrailingZeroBits[SCRIPT_MEMORY_LOOKUP_COUNT];
uint16_t script_animArrayHandle;
uint16_t *script_objectIdToVariable;
uint32_t script_parameterCount;
int32_t script_parseErrorCount;
scr_ast_node_t *script_parseRoot;
char *script_parseSource;
int32_t script_parseSourceDone;
uint16_t script_pauseArrayHandle;
int32_t script_pendingScriptLoadCount;
scr_script_load_record_t *script_pendingScriptLoadCursor;
uint8_t script_runtimeActive;
int32_t script_runtimeDebugReportFlag;
int32_t script_runtimeDeveloperFlag;
qboolean script_runtimeDeveloperScriptFlag;
uint16_t script_savedObjectCount;
/*
 * Original .data RVA 0x000ad184 / VA 0x080f5184 stores ff ff ff ff.
 * ShutdownOpcodeLookup restores this sentinel after freeing saved sources;
 * Scr_LoadSource replaces it from the serialized count before allocation.
 */
int32_t script_savedSourceFileCount =
    SCRIPT_SAVED_SOURCE_FILE_COUNT_NONE;
script_saved_source_file_t *script_savedSourceFiles;
uint8_t *script_serializationCursor;
uint32_t script_sourceBufferOffset;
const uint8_t *script_sourceBufferEnd;
const uint8_t *script_sourceBufferStart;
uint32_t script_sourceChecksum;
const char *script_sourceFilename;
uint32_t script_sourceFileCapacity;
uint32_t script_sourceFileCount;
script_source_file_record_t *script_sourceFiles;
const char *script_sourcePos;
uint32_t script_sourcePosCountForLastCodePos;
uint8_t *script_sourcePosLastCodePos;
uint32_t *script_sourcePosPool;
uint32_t script_sourcePosPoolCapacity;
uint32_t script_sourcePosPoolCount;
uint32_t
    script_sourcePosTableCapacity[SCRIPT_SOURCE_POS_TABLE_COUNT];
uint32_t script_sourcePosTableCount[SCRIPT_SOURCE_POS_TABLE_COUNT];
script_source_pos_record_t *
    script_sourcePosTables[SCRIPT_SOURCE_POS_TABLE_COUNT];
uint16_t script_stringCanonicalCount;
uint16_t *script_stringCanonicalMap;
script_string_hash_slot_t *script_stringFreedHashSlot;
script_string_hash_slot_t script_stringHashSlots[SCRIPT_STRING_HASH_SLOT_COUNT];
uint16_t script_tempValueHandle;
uint16_t script_timeArrayHandle;
VariableValue script_valueStack[SCRIPT_VALUE_STACK_COUNT];
uint32_t script_valueStackDepth;
VariableValue *script_valueStackLimit;
VariableValue *script_valueStackTop;
Variable script_variableIndirections[SCRIPT_VARIABLE_NODE_COUNT];
script_variable_node_t script_variableNodes[SCRIPT_VARIABLE_NODE_COUNT];
uint16_t *script_variableToObjectId;
const char *script_variableTypeNames[SCRIPT_VAR_COUNT] = {
    "undefined",
    "string",
    "localized string",
    "vector",
    "float",
    "int",
    "codepos",
    "object",
    "key/value",
    "function",
    "stack",
    "animation",
    "thread",
    "entity",
    "struct",
    "array",
    "dead thread",
    "dead entity",
    "dead object",
};
cvar_t *showdrop;
cvar_t *showpackets;
uint16_t sys_snapVectorSavedFpuControlWord;
uint16_t sys_snapVectorFpuControlWord = UINT16_C(0x037f);
serverHeader_t sv;
int32_t sv_serverId;
/*
 * Original .data VA 0x080f5110..0x080f5127.  SV_LinkEntity and
 * SV_PointTraceToEntity uses these bounds for DObjs carrying svFlags 0x2.
 */
vec3_t sv_defaultEntityClipMaxs = {64.0f, 64.0f, 72.0f};
vec3_t sv_defaultEntityClipMins = {-64.0f, -64.0f, -32.0f};
svEntity_t sv_entities[MAX_GENTITIES];
char *sv_entityParsePoint;
cvar_t *sv_fps;
sharedEntity_t *sv_gentities;
int32_t sv_gentitySize;
int32_t sv_numGentities;
playerState_t *sv_gameClients;
int32_t sv_gameClientSize;
vm_t *sv_gameVM;
char sv_gametypeNormalizeBuffer[MAX_QPATH];
cvar_t *sv_maxRate;
cvar_t *sv_maxclients;
cvar_t *sv_padPackets;
int sv_reconnectSequence;
cvar_t *sv_running;
cvar_t *sv_showAverageBPS;
serverStatic_t svs;
char sys_delayedProcessCommand[SYS_DELAYED_PROCESS_COMMAND_SIZE];
/*
 * Original .data RVA 0x000af340 / VA 0x080f7340 stores 01 00 00 00.
 * Sys_ConsoleInput reads it before the only runtime write, which disables
 * subsequent dedicated-console polling after stdin reaches EOF.
 */
int32_t sys_stdinActive = SYS_STDIN_ACTIVE;
int32_t sys_ttyConsoleActive;
console_input_field_t sys_ttyCurrentLine;
int32_t sys_ttyEofChar;
int32_t sys_ttyEraseChar;
console_input_field_t sys_ttyHistory[CON_HISTORY_FIELD_COUNT];
int32_t sys_ttyHistoryCount;
/*
 * Original .data RVA 0x000af358 / VA 0x080f7358 stores ff ff ff ff.
 * The previous/next history readers consume this no-selection sentinel before
 * Sys_TTYStoreHistoryLine can restore it after the first submitted line.
 */
int32_t sys_ttyHistoryCursor =
    SYS_TTY_HISTORY_RESET_CURSOR;
char sys_ttyInputReturnBuffer[SYS_TTY_INPUT_RETURN_BUFFER_SIZE];
coduo_terminal_state_t sys_ttyOriginalTermios;
int32_t sys_ttyOutputSuppressionDepth;
cvar_t *ttycon;
vm_t vmTable[VM_COUNT];
uint8_t *weaponInfo_memory;
int32_t weaponInfo_memoryState;
int32_t xanim_activePoolPayloadSlot;
DObj *xanim_currentEvalState;
XAnimTree *xanim_currentTree;
int32_t xanim_deferredNotifyCount;
uint16_t xanim_endNotifyHandle;
int32_t xanim_evalChildCount;
XModel **xanim_evalChildRefs;
int16_t xanim_evalCurrentFrame;
float xanim_evalCurrentTime;
uint8_t xanim_evalLeafOutputMode;
uint32_t xanim_evalPartBits[DOBJ_PART_BITSET_WORD_COUNT];
size_t xanim_evalPartBytes;
int32_t xanim_evalPartCount;
int32_t xanim_evalPoolWeightSelector;
uint16_t xanim_evalRootHandle;
uint32_t xanim_evalSkipBits[DOBJ_PART_BITSET_WORD_COUNT];
int16_t xanim_evalStartFrame;
float xanim_evalStartTime;
float xanim_evalTime;
float xanim_evalTimeStep;
int16_t xanim_evalWindowFrame;
float xanim_evalWindowTime;
XAnimInfo xanim_pool[XANIM_POOL_NODE_COUNT];
int32_t xanim_poolHighWaterCount;
int32_t xanim_poolUsedCount;
uint16_t xanim_rootTreeHandle;
char *fs_serverReferencedPakNames[FS_MAX_SERVER_PAKS];
char *fs_serverPakNames[FS_MAX_SERVER_PAKS];
int32_t fs_serverReferencedPaks[FS_MAX_SERVER_PAKS];
int32_t fs_serverPaks[FS_MAX_SERVER_PAKS];
xanim_deferred_notify_t
    xanim_deferredNotifies[XANIM_DEFERRED_NOTIFY_CAPACITY];
