#include "script_runtime_host.h"

#include "script_compile_developer.h"
#include "script_string.h"

#include <stdint.h>
#include <string.h>

enum {
    SCRIPT_CODEGEN_MODE_INTERN_STRINGS = 0,
    SCRIPT_CODEGEN_MODE_RECORD_STRING_FIXUPS = 1,
    SCRIPT_CODEGEN_MODE_RELEASE_STRINGS = 2,
    SCRIPT_OPCODE_DEVELOPER_COMMAND = 0x57,
    SCRIPT_OPCODE_DEFERRED_DEVELOPER_CHECK = 0x58,
    SCRIPT_DEVELOPER_OP_BUFFER_SIZE = 1048576,
    SCRIPT_DEVELOPER_OPCODE_PATCH_INITIAL_CAPACITY = 65536
};

/* CoDUOMP.exe 0x0047ee40..0x0047ee63 and coduo_lnxded
 * 0x080a1140..0x080a1177 perform the same list walk.  The name/signature is
 * also the exact same-module Mac symbol. */
void EmitStatementList(scr_ast_statement_block_t *block)
{
    for (scr_ast_statement_item_t *item = block->sentinel->head; item != NULL; item = item->next) {
        EmitStatement(item->node);
    }
}

/* CoDUOMP.exe 0x0047ee70..0x0047eedc and coduo_lnxded
 * 0x080a1178..0x080a11eb copy the same temporary byte range into the
 * developer buffer. */
void Scr_TransferToDeveloperBuffer(void)
{
    uint8_t *tempEnd = TempMalloc(0);
    int32_t byteCount = (int32_t)((uintptr_t)tempEnd - (uintptr_t)script_codeRelocationStart);
    int32_t usedBytes = (int32_t)((uintptr_t)script_codeRelocationEnd - (uintptr_t)script_developerOpBuffer);

    if (SCRIPT_DEVELOPER_OP_BUFFER_SIZE < usedBytes + byteCount) {
        Com_Error(ERR_DROP, "max developer script size exceeded - increase DEV_OP_BUF_SIZE");
    }

    Com_Memcpy(script_codeRelocationEnd, script_codeRelocationStart, (size_t)byteCount);
    script_codeRelocationEnd += byteCount;
}

/* CoDUOMP.exe 0x0047eee0..0x0047efcf and coduo_lnxded
 * 0x080a11ec..0x080a12fe emit the same deferred-check opcode, grow the same
 * patch table, transfer the pending statement bytes, and rewind temp memory.
 * The client reconstruction formerly had these two transfer names reversed;
 * the identities here follow the exact Mac symbols. */
void Scr_TransferStatementListToDeveloperBuffer(void)
{
    uint32_t savedChecksum = script_codeChecksum;

    EmitOpcode(SCRIPT_OPCODE_DEFERRED_DEVELOPER_CHECK, 0, 0);
    int32_t codeOffset = (int32_t)((uintptr_t)script_codeRelocationStart - (uintptr_t)script_codeBase);
    script_codeChecksum = savedChecksum;

    if (script_developerOpcodePatchCapacity <= codeOffset) {
        int32_t oldCapacity = script_developerOpcodePatchCapacity;
        int32_t newCapacity = (int32_t)((uint32_t)oldCapacity * 2u);
        if (newCapacity <= codeOffset) {
            newCapacity = (int32_t)((uint32_t)codeOffset * 2u);
        }

        uint8_t **newTable = Z_MallocInternal((size_t)newCapacity * sizeof(newTable[0]));
        Com_Memcpy(newTable, script_developerOpcodePatchTable, (size_t)oldCapacity * sizeof(newTable[0]));
        Com_Memset(&newTable[oldCapacity], 0, (size_t)(newCapacity - oldCapacity) * sizeof(newTable[0]));

        script_developerOpcodePatchCapacity = newCapacity;
        Z_FreeInternal(script_developerOpcodePatchTable);
        script_developerOpcodePatchTable = newTable;
    }

    script_developerOpcodePatchTable[codeOffset] = script_codeRelocationEnd;
    Scr_TransferToDeveloperBuffer();
    TempMemorySetPos(script_codeRelocationStart);
}

/* CoDUOMP.exe 0x0047efd0..0x0047f0fd and coduo_lnxded
 * 0x080a12fe..0x080a13c2 agree on both developer-enabled paths. */
void EmitDeveloperStatementList(scr_ast_statement_block_t *block, uint32_t sourcePos)
{
    if (script_codegenMode != SCRIPT_CODEGEN_MODE_INTERN_STRINGS) {
        CompileError(sourcePos, "cannot recurse /#");
    }

    uint32_t savedChecksum = script_codeChecksum;
    if (script_runtimeDeveloperScriptFlag == qfalse) {
        uint8_t *tempMark = TempMalloc(0);
        script_codegenMode = SCRIPT_CODEGEN_MODE_RELEASE_STRINGS;
        EmitStatementList(block);
        TempMemorySetPos(tempMark);
    } else {
        if (script_codeNeedsDeferredCheck == qfalse) {
            EmitOpcode(SCRIPT_OPCODE_DEVELOPER_COMMAND, 0, 0);
            script_codeRelocationStart = script_codeEmitCursor;
        }
        script_codegenMode = SCRIPT_CODEGEN_MODE_RECORD_STRING_FIXUPS;
        EmitStatementList(block);
        script_codeNeedsDeferredCheck = qtrue;
    }

    script_codegenMode = SCRIPT_CODEGEN_MODE_INTERN_STRINGS;
    script_codeChecksum = savedChecksum;
}

/* CoDUOMP.exe 0x0047f100..0x0047f1a5 and coduo_lnxded
 * 0x080a13c2..0x080a144e allocate and initialize identical storage. */
void Scr_InitDeveloperOpcodes(void)
{
    if (script_runtimeDeveloperScriptFlag == qfalse) {
        return;
    }

    script_developerOpBuffer = Z_MallocInternal(SCRIPT_DEVELOPER_OP_BUFFER_SIZE);
    script_developerOpcodePatchCapacity = SCRIPT_DEVELOPER_OPCODE_PATCH_INITIAL_CAPACITY;
    script_developerOpcodePatchTable =
        Z_MallocInternal((size_t)script_developerOpcodePatchCapacity * sizeof(script_developerOpcodePatchTable[0]));
    Com_Memset(script_developerOpcodePatchTable, 0,
               (size_t)script_developerOpcodePatchCapacity * sizeof(script_developerOpcodePatchTable[0]));
    script_codeRelocationEnd = script_developerOpBuffer;
    Com_Memset(script_frameBackupCodepos, 0, sizeof(script_frameBackupCodepos));
    script_codeStringFixups = NULL;
}

/* CoDUOMP.exe 0x0047f1b0..0x0047f26e and coduo_lnxded
 * 0x080a144e..0x080a1512 agree on opcode replacement and string fixups. */
void Scr_InsertDeveloperOpcodes(void)
{
    if (script_runtimeDeveloperScriptFlag == qfalse) {
        return;
    }

    for (int32_t codeOffset = 0; codeOffset < script_developerOpcodePatchCapacity; ++codeOffset) {
        uint8_t *patch = script_developerOpcodePatchTable[codeOffset];
        if (patch != NULL) {
            *patch = script_codeBase[codeOffset];
            script_codeBase[codeOffset] = SCRIPT_OPCODE_DEVELOPER_COMMAND;
        }
    }

    while (script_codeStringFixups != NULL) {
        script_code_string_fixup_t *fixup = script_codeStringFixups;
        uint16_t string;
        memcpy(&string, fixup->codePos, sizeof(string));
        if (string != 0) {
            string = SL_TransferToCanonicalString(string);
            memcpy(fixup->codePos, &string, sizeof(string));
        }
        script_codeStringFixups = fixup->next;
        Z_FreeInternal(fixup);
    }
}

/* CoDUOMP.exe 0x0047f270..0x0047f2b2 and coduo_lnxded
 * 0x080a1512..0x080a1566 have the same guarded frees and null stores. */
void Scr_ShutdownDeveloperOpcodes(void)
{
    if (script_runtimeDeveloperScriptFlag == qfalse) {
        return;
    }

    if (script_developerOpBuffer != NULL) {
        Z_FreeInternal(script_developerOpBuffer);
        script_developerOpBuffer = NULL;
    }
    if (script_developerOpcodePatchTable != NULL) {
        Z_FreeInternal(script_developerOpcodePatchTable);
        script_developerOpcodePatchTable = NULL;
    }
}
