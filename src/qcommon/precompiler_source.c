#include "precompiler.h"

#include "precompiler_float.h"
#include "precompiler_services.h"

#include <string.h>

#if defined(WINDOWS_BEHAVIOR)
void *GetMemory(size_t size);
void *GetClearedMemory(size_t size);
void FreeMemory(void *memory);
#elif defined(LINUX_BEHAVIOR)
void *Com_ZoneDebugAlloc(size_t size);
void *Com_ZoneDebugAllocClear(size_t size);
void Com_DebugFree(void *pointer);
#else
#error "precompiler_source.c requires WINDOWS_BEHAVIOR or LINUX_BEHAVIOR"
#endif

char pc_baseFolder[MAX_QPATH];

/* The original 64-entry parser-source table reserves handle zero; every
 * public handle operation accepts only indices 1..63.  The Windows table is
 * at 0x04927d80. */
source_t *pc_sourceFiles[PC_SOURCE_HANDLE_COUNT];

/* Source: CoDUOMP.exe 0x00446c00..0x00446caa.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00446c00_00446caa.mcode.
 * Name: same-module Mac symbol LoadSourceFile. */
source_t *LoadSourceFile(const char *filename)
{
    PC_InitTokenHeap();

    script_t *script = LoadScriptFile(filename);
    if (script == NULL)
        return NULL;

    script->next = NULL;

    /* Windows GetMemory adds its four-byte marker, producing the PE's total
     * 0x65c-byte allocation for the 0x658-byte payload.  Linux allocates its
     * ABI-specific 0x4c8-byte payload directly. */
    source_t *source;
#if defined(WINDOWS_BEHAVIOR)
    source = GetMemory(sizeof(*source));
#else
    source = Com_ZoneDebugAlloc(sizeof(*source));
#endif
    memset(source, 0, sizeof(*source));
    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    strncpy(source->filename, filename, sizeof(source->filename) - 1u);
    source->filename[sizeof(source->filename) - 1u] = '\0';
    source->scriptStack = script;
#if defined(WINDOWS_BEHAVIOR)
    source->defineHash = GetClearedMemory(PC_DEFINE_HASH_BUCKET_COUNT * sizeof(*source->defineHash));
#else
    source->defineHash = Com_ZoneDebugAllocClear(PC_DEFINE_HASH_BUCKET_COUNT * sizeof(*source->defineHash));
#endif
    PC_AddGlobalDefinesToSource(source);
    return source;
}

/* Source: CoDUOMP.exe 0x00446cb0..0x00446d5f.
 * Evidence: repaired executable-gap record
 * coduomp/mcode/CoDUOMP/FUN_00446cb0_00446d5f.mcode.
 * Name: same-module family name LoadSourceMemory. */
source_t *LoadSourceMemory(const char *buffer, size_t length, const char *name)
{
    PC_InitTokenHeap();

    script_t *script = LoadScriptMemory(buffer, length, name);
    if (script == NULL)
        return NULL;

    script->next = NULL;

    /* The file and memory loaders use the same platform-sized source
     * allocation described above. */
    source_t *source;
#if defined(WINDOWS_BEHAVIOR)
    source = GetMemory(sizeof(*source));
#else
    source = Com_ZoneDebugAlloc(sizeof(*source));
#endif
    memset(source, 0, sizeof(*source));
    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    strncpy(source->filename, name, sizeof(source->filename) - 1u);
    source->filename[sizeof(source->filename) - 1u] = '\0';
    source->scriptStack = script;
#if defined(WINDOWS_BEHAVIOR)
    source->defineHash = GetClearedMemory(PC_DEFINE_HASH_BUCKET_COUNT * sizeof(*source->defineHash));
#else
    source->defineHash = Com_ZoneDebugAllocClear(PC_DEFINE_HASH_BUCKET_COUNT * sizeof(*source->defineHash));
#endif
    PC_AddGlobalDefinesToSource(source);
    return source;
}

/* Source: CoDUOMP.exe 0x00446d60..0x00446ec4.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00446d60_00446ec4.mcode.
 * Name: same-module Mac symbol FreeSource. */
void FreeSource(source_t *source)
{
    while (source->scriptStack != NULL) {
        script_t *script = source->scriptStack;
        source->scriptStack = script->next;
        FreeScript(script);
    }

    while (source->tokens != NULL) {
        token_t *token = source->tokens;
        source->tokens = token->next;
        PC_FreeToken(token);
    }

    for (int32_t bucket = 0; bucket < PC_DEFINE_HASH_BUCKET_COUNT; ++bucket) {
        while (source->defineHash[bucket] != NULL) {
            define_t *define = source->defineHash[bucket];
            source->defineHash[bucket] = define->hashNext;
            PC_FreeDefine(define);
        }
    }

    while (source->indentStack != NULL) {
        indent_t *indent = source->indentStack;
        source->indentStack = indent->next;
#if defined(WINDOWS_BEHAVIOR)
        FreeMemory(indent);
#else
        Com_DebugFree(indent);
#endif
    }

    if (source->defineHash != NULL) {
#if defined(WINDOWS_BEHAVIOR)
        FreeMemory(source->defineHash);
#else
        Com_DebugFree(source->defineHash);
#endif
    }
#if defined(WINDOWS_BEHAVIOR)
    FreeMemory(source);
#else
    Com_DebugFree(source);
#endif
}

/* Source: CoDUOMP.exe 0x00446ed0..0x00446f50.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00446ed0_00446f50.mcode.
 * Name: exact same-module Mac symbol PC_LoadSourceHandle. */
int32_t PC_LoadSourceHandle(const char *filename)
{
    int32_t handle;
    for (handle = 1; handle < PC_SOURCE_HANDLE_COUNT; ++handle) {
        if (pc_sourceFiles[handle] == NULL)
            break;
    }

    if (handle == PC_SOURCE_HANDLE_COUNT)
        return 0;

    PS_SetBaseFolder("");

    source_t *source = LoadSourceFile(filename);
    if (source == NULL)
        return 0;

    pc_sourceFiles[handle] = source;
    return handle;
}

/* Source: CoDUOMP.exe 0x00446f50..0x00446f84.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00446f50_00446f84.mcode.
 * Name: same-module family name PC_FreeSourceHandle. */
qboolean PC_FreeSourceHandle(int32_t handle)
{
    if (handle < 1 || handle >= PC_SOURCE_HANDLE_COUNT)
        return qfalse;
    if (pc_sourceFiles[handle] == NULL)
        return qfalse;

    FreeSource(pc_sourceFiles[handle]);
    pc_sourceFiles[handle] = NULL;
    return qtrue;
}

/* Source: CoDUOMP.exe 0x00446f90..0x00447048.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00446f90_00447048.mcode.
 * Name: exact same-module Mac symbol PC_ReadTokenHandle. */
qboolean PC_ReadTokenHandle(int32_t handle, pc_token_t *token)
{
    if (handle < 1 || handle >= PC_SOURCE_HANDLE_COUNT)
        return qfalse;

    source_t *source = pc_sourceFiles[handle];
    if (source == NULL)
        return qfalse;

    token_t readToken;
    const qboolean readStatus = PC_ReadToken(source, &readToken);

    /* NOT_FROM_ORIGINAL_SOURCE: publish a public token only after the internal
     * parser reports a complete successful result. */
    if (readStatus == qfalse) {
        return qfalse;
    }

    strcpy(token->string, readToken.string);
    token->type = readToken.type;
    token->subtype = readToken.subtype;
    token->intValue = readToken.intValue;
#if defined(WINDOWS_BEHAVIOR)
    /* CoDUOMP.exe 0x00447026..0x0044702e: FLD QWORD, FSTP DWORD. */
    token->floatValue = (float)readToken.floatValue;
#elif EMULATE_X87
    /* coduo_lnxded loads the token's TBYTE directly and stores binary32. */
    token->floatValue = x87f_store_f32(coduo_pc_load_token_float80(readToken.floatValue));
#else
    long double floatValue = 0.0L;
    size_t valueSize = sizeof(floatValue);
    if (valueSize > PC_TOKEN_FLOAT_VALUE_SIZE) {
        valueSize = PC_TOKEN_FLOAT_VALUE_SIZE;
    }
    memcpy(&floatValue, readToken.floatValue, valueSize);
    token->floatValue = (float)floatValue;
#endif

    if (token->type == PC_TOKEN_TYPE_STRING)
        StripDoubleQuotes(token->string);

    return qtrue;
}

/* Source: CoDUOMP.exe 0x00447050..0x004470aa.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00447050_004470aa.mcode.
 * Name: exact same-module Mac symbol PC_SourceFileAndLine. */
qboolean PC_SourceFileAndLine(int32_t handle, char *filename, int32_t *line)
{
    if (handle < 1 || handle >= PC_SOURCE_HANDLE_COUNT)
        return qfalse;

    source_t *source = pc_sourceFiles[handle];
    if (source == NULL)
        return qfalse;

    script_t *script = source->scriptStack;
    if (script != NULL) {
        strcpy(filename, script->filename);
        *line = script->line;
    } else {
        strcpy(filename, source->filename);
        *line = 0;
    }
    return qtrue;
}

/* Source: CoDUOMP.exe 0x004470b0..0x004470c8.
 * Evidence: repaired executable-gap record
 * coduomp/mcode/CoDUOMP/FUN_004470b0_004470c8.mcode.
 * Name: same-module family name PC_SetBaseFolder. */
void PC_SetBaseFolder(const char *path)
{
    PS_SetBaseFolder(path);
}

/* Source: CoDUOMP.exe 0x004470d0..0x004470fd.
 * Evidence: repaired executable-gap record
 * coduomp/mcode/CoDUOMP/FUN_004470d0_004470fd.mcode.
 * Name: same-module family name PC_CheckOpenSourceHandles. */
void PC_CheckOpenSourceHandles(void)
{
    for (int32_t handle = 1; handle < PC_SOURCE_HANDLE_COUNT; ++handle) {
        source_t *source = pc_sourceFiles[handle];
        if (source != NULL) {
            Com_Printf("^1Error: file %s still open in precompiler\n", source->scriptStack->filename);
        }
    }
}
