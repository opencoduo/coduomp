#ifndef SHARED_SCRIPT_COMPILE_DEVELOPER_H
#define SHARED_SCRIPT_COMPILE_DEVELOPER_H

#include "script_compile_types.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

extern int32_t script_developerOpcodePatchCapacity;
extern uint8_t **script_developerOpcodePatchTable;
extern script_codepos_t script_frameBackupCodepos[SCRIPT_CALL_STACK_COUNT];
extern uint8_t script_frameBackupOpcode[SCRIPT_CALL_STACK_COUNT];

void EmitStatementList(scr_ast_statement_block_t *block);
void Scr_TransferToDeveloperBuffer(void);
void Scr_TransferStatementListToDeveloperBuffer(void);
void EmitDeveloperStatementList(scr_ast_statement_block_t *block, uint32_t sourcePos);
void Scr_InitDeveloperOpcodes(void);
void Scr_InsertDeveloperOpcodes(void);
void Scr_ShutdownDeveloperOpcodes(void);

#ifdef __cplusplus
}
#endif

#endif
