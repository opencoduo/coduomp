#ifndef SCRIPT_SOURCE_POSITIONS_H
#define SCRIPT_SOURCE_POSITIONS_H

#include "qcommon/script_runtime_types.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
    SCRIPT_SAVED_SOURCE_FILE_COUNT_NONE = -1
};

extern int32_t script_runtimeDebugReportFlag;
extern int32_t script_codegenMode;
extern uint8_t *script_codeLastOpcodePos;
extern uint8_t *script_codeBase;
extern size_t script_codeSize;
extern uint8_t *script_codeEnd;
extern uint8_t *script_developerOpBuffer;
extern uint8_t *script_codeRelocationStart;
extern uint8_t *script_codeRelocationEnd;

extern const char *script_sourcePos;
extern const char *script_sourceFilename;
extern uint32_t script_sourceFileCount;
extern uint32_t script_sourceFileCapacity;
extern script_source_file_record_t *script_sourceFiles;
extern int32_t script_savedSourceFileCount;
extern script_saved_source_file_t *script_savedSourceFiles;
extern uint32_t script_sourcePosCountForLastCodePos;
extern uint8_t *script_sourcePosLastCodePos;
extern uint32_t *script_sourcePosPool;
extern uint32_t script_sourcePosPoolCapacity;
extern uint32_t script_sourcePosPoolCount;
extern uint32_t script_sourcePosTableCapacity[SCRIPT_SOURCE_POS_TABLE_COUNT];
extern uint32_t script_sourcePosTableCount[SCRIPT_SOURCE_POS_TABLE_COUNT];
extern script_source_pos_record_t *script_sourcePosTables[SCRIPT_SOURCE_POS_TABLE_COUNT];

qboolean ScriptCode_IsLoadedCodePos(const uint8_t *codePos);
qboolean Scr_IsInDeveloperOpcodeMemory(const uint8_t *codePos);
qboolean Scr_IsInOpcodeMemory(const uint8_t *codePos);
void InitOpcodeLookup(void);
void ShutdownOpcodeLookup(void);
void AddOpcodePos(uint32_t sourcePos);
uint32_t GetPrevSourcePos(script_codepos_t codePos, int32_t sourcePosOffset);
qboolean Scr_HasSourceFiles(void);
void Scr_SaveSource(script_source_io_fn_t writeData);
void Scr_LoadSource(script_source_io_fn_t readData);
void Scr_SkipSource(script_source_io_fn_t readData);
script_source_file_record_t *Scr_GetNewSourceBuffer(void);
char *Scr_AddSourceBuffer(const char *filename, uint8_t *normalCodeStart, uint8_t *relocatedCodeStart);

#ifdef __cplusplus
}
#endif

#endif
