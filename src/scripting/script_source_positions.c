#include <string.h>

#include "script_runtime_host.h"
#include "script_source_positions.h"

enum {
    SCRIPT_SOURCE_POS_INITIAL_CAPACITY = 65536,
    SCRIPT_SOURCE_FILE_INITIAL_CAPACITY = 16,
    /* Original Win32 { char *source; int32_t sourceLen; } load-row stride.
     * Scr_LoadSource performs count * stride as a target dword before the
     * native pointer-bearing allocation is formed. */
    SCRIPT_SAVED_SOURCE_FILE_TARGET_STRIDE = 8,
    SCRIPT_CODEGEN_MODE_RELOCATED = 1,
    SCRIPT_CODEGEN_MODE_RELEASE_STRINGS = 2
};

/* NOT_FROM_ORIGINAL_SOURCE: preserve target dword arithmetic while retaining
 * a defined native C representation of every resulting bit pattern. */
static int32_t coduomp_script_source_int32_from_bits(uint32_t bits)
{
    int32_t value;

    memcpy(&value, &bits, sizeof(value));
    return value;
}

/* Source: CoDUOMP.exe 0x0047fdb0..0x0047fdce.
 * The Windows executable retains this loaded-code predicate as a separate
 * out-of-line copy even though its recovered callers inline the same test. */
qboolean ScriptCode_IsLoadedCodePos(const uint8_t *codePos)
{
    const uintptr_t address = (uintptr_t)codePos;
    const uintptr_t begin = (uintptr_t)script_codeBase;
    return begin <= address && address < begin + script_codeSize
               ? qtrue
               : qfalse;
}

/* Source: CoDUOMP.exe 0x0047fdd0..0x0047fde8.
 * Name: exact same-module Mac symbol Scr_IsInDeveloperOpcodeMemory. */
qboolean Scr_IsInDeveloperOpcodeMemory(const uint8_t *codePos)
{
    const uintptr_t address = (uintptr_t)codePos;
    return (uintptr_t)script_developerOpBuffer <= address &&
                   address < (uintptr_t)script_codeRelocationEnd
               ? qtrue
               : qfalse;
}

/* Source: CoDUOMP.exe 0x0047fdf0..0x0047fe08.
 * Name: exact same-module Mac symbol Scr_IsInOpcodeMemory. Unlike the loaded
 * predicate above, this covers the complete committed script-code hunk. */
qboolean Scr_IsInOpcodeMemory(const uint8_t *codePos)
{
    const uintptr_t address = (uintptr_t)codePos;
    return (uintptr_t)script_codeBase <= address &&
                   address < (uintptr_t)script_codeEnd
               ? qtrue
               : qfalse;
}

/* Source: CoDUOMP.exe 0x00481080..0x00481183.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00481080_00481184.mcode. */
void InitOpcodeLookup(void)
{
    if (script_runtimeDebugReportFlag == qfalse) {
        return;
    }

    for (int32_t tableIndex = 0;
         tableIndex < SCRIPT_SOURCE_POS_TABLE_COUNT; ++tableIndex) {
        script_sourcePosTableCapacity[tableIndex] =
            SCRIPT_SOURCE_POS_INITIAL_CAPACITY;
        script_sourcePosTableCount[tableIndex] = 0;
        script_sourcePosTables[tableIndex] =
            Z_MallocInternal(script_sourcePosTableCapacity[tableIndex] *
                     sizeof(script_sourcePosTables[tableIndex][0]));
        memset(script_sourcePosTables[tableIndex], 0,
               script_sourcePosTableCapacity[tableIndex] *
                   sizeof(script_sourcePosTables[tableIndex][0]));
    }

    script_sourcePosPoolCapacity = SCRIPT_SOURCE_POS_INITIAL_CAPACITY;
    script_sourcePosPoolCount = 0;
    script_sourcePosPool =
        Z_MallocInternal(script_sourcePosPoolCapacity * sizeof(script_sourcePosPool[0]));
    memset(script_sourcePosPool, 0,
           script_sourcePosPoolCapacity * sizeof(script_sourcePosPool[0]));
    script_sourcePosLastCodePos = NULL;
    script_sourcePosCountForLastCodePos = 0;

    script_sourceFileCapacity = SCRIPT_SOURCE_FILE_INITIAL_CAPACITY;
    script_sourceFileCount = 0;
    script_sourceFiles =
        Z_MallocInternal(script_sourceFileCapacity * sizeof(script_sourceFiles[0]));
    memset(script_sourceFiles, 0,
           script_sourceFileCapacity * sizeof(script_sourceFiles[0]));
}

/* Source: CoDUOMP.exe 0x00481190..0x0048127e.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00481190_0048127f.mcode. */
void ShutdownOpcodeLookup(void)
{
    for (int32_t tableIndex = 0;
         tableIndex < SCRIPT_SOURCE_POS_TABLE_COUNT; ++tableIndex) {
        if (script_sourcePosTables[tableIndex] != NULL) {
            Z_FreeInternal(script_sourcePosTables[tableIndex]);
            script_sourcePosTables[tableIndex] = NULL;
        }
    }

    if (script_sourcePosPool != NULL) {
        Z_FreeInternal(script_sourcePosPool);
        script_sourcePosPool = NULL;
    }

    if (script_sourceFiles != NULL) {
        for (uint32_t index = 0; index < script_sourceFileCount; ++index) {
            Z_FreeInternal(script_sourceFiles[index].filename);
        }
        Z_FreeInternal(script_sourceFiles);
        script_sourceFiles = NULL;
    }

    if (script_savedSourceFiles != NULL) {
        for (int32_t index = 0; index < script_savedSourceFileCount; ++index) {
            if (script_savedSourceFiles[index].source != NULL) {
                Z_FreeInternal(script_savedSourceFiles[index].source);
            }
        }
        Z_FreeInternal(script_savedSourceFiles);
        script_savedSourceFileCount = SCRIPT_SAVED_SOURCE_FILE_COUNT_NONE;
        script_savedSourceFiles = NULL;
    }
}

/* Source: CoDUOMP.exe 0x004814b0..0x004814b5.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004814b0_004814b6.mcode. */
qboolean Scr_HasSourceFiles(void)
{
    return script_runtimeDebugReportFlag;
}

/* Source: CoDUOMP.exe 0x004814c0..0x00481521.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004814c0_00481522.mcode. */
void Scr_SaveSource(script_source_io_fn_t writeData)
{
    writeData(&script_sourceFileCount, sizeof(script_sourceFileCount));

    for (uint32_t index = 0; index < script_sourceFileCount; ++index) {
        writeData(&script_sourceFiles[index].sourceLen,
                  sizeof(script_sourceFiles[index].sourceLen));
        if (script_sourceFiles[index].sourceLen > 0) {
            writeData(script_sourceFiles[index].source,
                      script_sourceFiles[index].sourceLen);
        }
    }
}

/* Source: CoDUOMP.exe 0x00481530..0x004815de.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00481530_004815df.mcode. */
void Scr_LoadSource(script_source_io_fn_t readData)
{
    readData(&script_savedSourceFileCount,
             sizeof(script_savedSourceFileCount));

    /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
    uint32_t targetArrayBytes =
        (uint32_t)script_savedSourceFileCount *
        SCRIPT_SAVED_SOURCE_FILE_TARGET_STRIDE;
    size_t nativeArrayBytes =
        (size_t)(targetArrayBytes / SCRIPT_SAVED_SOURCE_FILE_TARGET_STRIDE) *
        sizeof(script_savedSourceFiles[0]);
    script_savedSourceFiles =
        Z_MallocInternal(nativeArrayBytes);
    memset(script_savedSourceFiles, 0, nativeArrayBytes);

    for (int32_t index = coduomp_script_source_int32_from_bits(
             (uint32_t)script_savedSourceFileCount - 1u);
         index >= 0;
         --index) {
        readData(&script_savedSourceFiles[index].sourceLen,
                 sizeof(script_savedSourceFiles[index].sourceLen));
        if (script_savedSourceFiles[index].sourceLen < 1) {
            script_savedSourceFiles[index].source = NULL;
            continue;
        }

        script_savedSourceFiles[index].source =
            Z_MallocInternal((size_t)script_savedSourceFiles[index].sourceLen);
        memset(script_savedSourceFiles[index].source, 0,
               (size_t)script_savedSourceFiles[index].sourceLen);
        /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
        readData(script_savedSourceFiles[index].source,
                 script_savedSourceFiles[index].sourceLen);
    }
}

/* Source: CoDUOMP.exe 0x004815e0..0x00481622.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004815e0_00481623.mcode. */
void Scr_SkipSource(script_source_io_fn_t readData)
{
    int32_t sourceFileCount;

    /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
    readData(&sourceFileCount, sizeof(sourceFileCount));
    for (int32_t index = coduomp_script_source_int32_from_bits(
             (uint32_t)sourceFileCount - 1u);
         index >= 0; --index) {
        int32_t sourceLen;

        readData(&sourceLen, sizeof(sourceLen));
        if (sourceLen > 0) {
            readData(NULL, sourceLen);
        }
    }
}

/* Source: CoDUOMP.exe 0x00481630..0x004816ad.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00481630_004816ae.mcode. */
script_source_file_record_t *Scr_GetNewSourceBuffer(void)
{
    if (script_sourceFileCapacity <= script_sourceFileCount) {
        script_sourceFileCapacity *= 2;
        script_source_file_record_t *newFiles =
            Z_MallocInternal(script_sourceFileCapacity * sizeof(newFiles[0]));
        memset(newFiles, 0,
               script_sourceFileCapacity * sizeof(newFiles[0]));

        Com_Memcpy(newFiles, script_sourceFiles,
                   script_sourceFileCount * sizeof(newFiles[0]));
        Z_FreeInternal(script_sourceFiles);
        script_sourceFiles = newFiles;
    }

    return &script_sourceFiles[script_sourceFileCount++];
}

/* Source: CoDUOMP.exe 0x004816b0..0x00481858.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004816b0_00481859.mcode. */
char *Scr_AddSourceBuffer(const char *filename,
                          uint8_t *normalCodeStart,
                          uint8_t *relocatedCodeStart)
{
    int32_t sourceLen;
    char *loadedSource;

    if (script_savedSourceFiles == NULL) {
        int32_t handle;

        sourceLen = FS_FOpenFileByMode(filename, &handle, FS_READ);
        if (sourceLen < 0) {
            goto missing_source;
        }

        loadedSource = Hunk_AllocateTempMemoryHighInternal((size_t)sourceLen + 1);
        FS_Read(loadedSource, sourceLen, handle);
        loadedSource[sourceLen] = '\0';
        FS_FCloseFile(handle);
    } else {
        /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
        script_savedSourceFileCount =
            coduomp_script_source_int32_from_bits(
                (uint32_t)script_savedSourceFileCount - 1u);
        sourceLen =
            script_savedSourceFiles[script_savedSourceFileCount].sourceLen;
        if (sourceLen < 0) {
            goto missing_source;
        }

        loadedSource = Hunk_AllocateTempMemoryHighInternal((size_t)sourceLen + 1);
        const char *savedSource =
            script_savedSourceFiles[script_savedSourceFileCount].source;
        for (int32_t index = 0; index < sourceLen; ++index) {
            char ch = savedSource[index];
            loadedSource[index] = ch == '\0' ? '\n' : ch;
        }
        loadedSource[sourceLen] = '\0';

        if (script_savedSourceFiles[script_savedSourceFileCount].source !=
            NULL) {
            Z_FreeInternal(script_savedSourceFiles[script_savedSourceFileCount].source);
        }
    }

    if (script_sourceFiles == NULL) {
        script_sourcePos = NULL;
        return loadedSource;
    }

    uint32_t filenameLen = (uint32_t)strlen(filename) + 1u;
    uint32_t trackedLen =
        (uint32_t)sourceLen + filenameLen + 2u;
    char *tracked = Z_MallocInternal(trackedLen);
    /* 0x004817e3..0x004817e7: retail zeroes the whole combined allocation.
     * Besides initializing the record payload, this leaves a second NUL after
     * the transformed source so PrintSourcePos can select the post-final-line
     * sentinel without scanning uninitialized zone memory. */
    memset(tracked, 0, trackedLen);
    strcpy(tracked, filename);

    char *trackedSource = tracked + filenameLen;
    for (int32_t index = 0; index <= sourceLen; ++index) {
        char ch = loadedSource[index];
        trackedSource[index] = ch == '\n' ? '\0' : ch;
    }

    script_source_file_record_t *record =
        Scr_GetNewSourceBuffer();
    record->normalCodeStart = normalCodeStart;
    record->relocatedCodeStart = relocatedCodeStart;
    record->filename = tracked;
    record->source = trackedSource;
    record->sourceLen = sourceLen;
    script_sourcePos = trackedSource;
    return loadedSource;

missing_source:
    if (script_sourceFiles != NULL) {
        script_source_file_record_t *record =
            Scr_GetNewSourceBuffer();
        record->normalCodeStart = NULL;
        record->relocatedCodeStart = NULL;
        record->filename = NULL;
        record->source = NULL;
        record->sourceLen = -1;
    }
    return NULL;
}

/* Source: CoDUOMP.exe 0x00481280..0x00481418.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00481280_00481418.mcode. */
void AddOpcodePos(uint32_t sourcePos)
{
    if (script_runtimeDebugReportFlag == qfalse ||
        script_codegenMode == SCRIPT_CODEGEN_MODE_RELEASE_STRINGS) {
        return;
    }

    uint32_t tableIndex =
        script_codegenMode == SCRIPT_CODEGEN_MODE_RELOCATED ? 1U : 0U;

    if (script_sourcePosTableCapacity[tableIndex] <=
        script_sourcePosTableCount[tableIndex]) {
        script_sourcePosTableCapacity[tableIndex] *= 2;
        script_source_pos_record_t *newTable =
            Z_MallocInternal(script_sourcePosTableCapacity[tableIndex] *
                     sizeof(newTable[0]));
        memset(newTable, 0,
               script_sourcePosTableCapacity[tableIndex] *
                   sizeof(newTable[0]));
        Com_Memcpy(newTable, script_sourcePosTables[tableIndex],
                   script_sourcePosTableCount[tableIndex] *
                       sizeof(newTable[0]));
        Z_FreeInternal(script_sourcePosTables[tableIndex]);
        script_sourcePosTables[tableIndex] = newTable;
    }

    if (script_sourcePosPoolCapacity <= script_sourcePosPoolCount) {
        script_sourcePosPoolCapacity *= 2;
        uint32_t *newPool =
            Z_MallocInternal(script_sourcePosPoolCapacity * sizeof(newPool[0]));
        Com_Memcpy(newPool, script_sourcePosPool,
                   script_sourcePosPoolCount * sizeof(newPool[0]));
        Z_FreeInternal(script_sourcePosPool);
        script_sourcePosPool = newPool;
    }

    if (script_codeLastOpcodePos == script_sourcePosLastCodePos) {
        script_sourcePosTableCount[tableIndex]--;
    } else {
        script_sourcePosCountForLastCodePos = 0;
        script_sourcePosLastCodePos = script_codeLastOpcodePos;
        script_sourcePosTables[tableIndex]
                              [script_sourcePosTableCount[tableIndex]]
                                  .sourcePosIndex = script_sourcePosPoolCount;
    }

    script_source_pos_record_t *record =
        &script_sourcePosTables[tableIndex]
                               [script_sourcePosTableCount[tableIndex]];
    if (script_codegenMode == SCRIPT_CODEGEN_MODE_RELOCATED) {
        /* The original performs two target-dword address operations. Integer
         * addresses retain that relocation graph when the buffers are
         * separate native allocations. */
        record->codePos = (uint8_t *)(
            (uintptr_t)script_codeLastOpcodePos +
            ((uintptr_t)script_codeRelocationEnd -
             (uintptr_t)script_codeRelocationStart));
    } else {
        record->codePos = script_codeLastOpcodePos;
    }

    script_sourcePosPool[record->sourcePosIndex +
                         script_sourcePosCountForLastCodePos] = sourcePos;
    script_sourcePosTableCount[tableIndex]++;
    script_sourcePosCountForLastCodePos++;
    script_sourcePosPoolCount++;
}

/* Source: CoDUOMP.exe 0x00481420..0x004814aa.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00481420_004814ab.mcode. */
uint32_t GetPrevSourcePos(uint8_t *codePos, int32_t sourcePosOffset)
{
    const uintptr_t address = (uintptr_t)codePos;
    const uintptr_t codeBegin = (uintptr_t)script_codeBase;
    uint32_t tableIndex =
        codeBegin <= address && address < codeBegin + script_codeSize
            ? 0U
            : 1U;
    int32_t low = 0;
    int32_t high = (int32_t)script_sourcePosTableCount[tableIndex] - 1;

    while (low <= high) {
        int32_t middle = (low + high) / 2;
        if ((uintptr_t)script_sourcePosTables[tableIndex][middle].codePos <
            address) {
            low = middle + 1;
            if (low == (int32_t)script_sourcePosTableCount[tableIndex] ||
                address <= (uintptr_t)script_sourcePosTables[tableIndex][low]
                               .codePos) {
                return script_sourcePosPool
                    [script_sourcePosTables[tableIndex][middle]
                         .sourcePosIndex +
                     sourcePosOffset];
            }
        } else {
            high = middle - 1;
        }
    }

    return 0;
}
