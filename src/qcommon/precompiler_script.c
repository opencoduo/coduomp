#include "precompiler.h"

#include "com_parse.h"
#include "com_sprintf.h"
#include "filesystem/filesystem.h"
#include "precompiler_services.h"

#include <string.h>

#if defined(WINDOWS_BEHAVIOR)
void *GetClearedMemory(size_t size);
void FreeMemory(void *memory);
#elif defined(LINUX_BEHAVIOR)
void *Com_ZoneDebugAlloc(size_t size);
void *Com_ZoneDebugAllocClear(size_t size);
void Com_DebugFree(void *pointer);
#else
#error "precompiler_script.c requires WINDOWS_BEHAVIOR or LINUX_BEHAVIOR"
#endif

enum {
    PC_SCRIPT_INITIAL_LINE = 1
};

/* Sources: CoDUOMP.exe 0x00448950..0x00448aa7 and coduo_lnxded
 * 0x0807f353..0x0807f4f1.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00448950_00448aa7.mcode.
 * Name: exact same-module Mac symbol LoadScriptFile. */
script_t *LoadScriptFile(const char *filename)
{
    /* NOT_FROM_ORIGINAL_SOURCE: require the complete source name and NUL to
     * fit the platform-specific script record before opening or allocating. */
    if (strlen(filename) >= PC_SCRIPT_FILENAME_CAPACITY) {
        return NULL;
    }

    char path[MAX_QPATH];
    if (strlen(pc_baseFolder) != 0) {
        Com_sprintf(path, sizeof(path), "%s/%s", pc_baseFolder, filename);
    } else {
        Com_sprintf(path, sizeof(path), "%s", filename);
    }

    int32_t handle;
    int32_t fileLength = FS_FOpenFileByMode(
        path, &handle, FS_READ);
    if (handle == 0)
        return NULL;

#if defined(WINDOWS_BEHAVIOR)
    script_t *script = GetClearedMemory(
        sizeof(*script) + (size_t)fileLength + 1);
#else
    script_t *script = Com_ZoneDebugAlloc(
        sizeof(*script) + (size_t)fileLength + 1);
#endif
    memset(script, 0, sizeof(*script));
    strcpy(script->filename, filename);

    script->buffer = (char *)(script + 1);
    script->buffer[fileLength] = '\0';
    script->length = fileLength;
    script->scriptCursor = script->buffer;
    script->lastScriptCursor = script->buffer;
    script->endCursor = script->buffer + fileLength;
    script->tokenAvailable = qfalse;
    script->line = PC_SCRIPT_INITIAL_LINE;
    script->lastLine = PC_SCRIPT_INITIAL_LINE;
    PS_CreatePunctuationTable(script, NULL);

    FS_Read(script->buffer, fileLength, handle);
    FS_FCloseFile(handle);
    script->length = Com_Compress(script->buffer);
    return script;
}

/* Sources: CoDUOMP.exe 0x00448ab0..0x00448b9d and coduo_lnxded
 * 0x0807f4f1..0x0807f5d9.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00448ab0_00448b9d.mcode.
 * Name: exact same-module Mac symbol LoadScriptMemory. */
script_t *LoadScriptMemory(const char *buffer, size_t length,
                              const char *name)
{
    /* NOT_FROM_ORIGINAL_SOURCE: require the complete source name and NUL to
     * fit the platform-specific script record before allocating. */
    if (strlen(name) >= PC_SCRIPT_FILENAME_CAPACITY) {
        return NULL;
    }

#if defined(WINDOWS_BEHAVIOR)
    script_t *script = GetClearedMemory(sizeof(*script) + length + 1);
#else
    script_t *script = Com_ZoneDebugAllocClear(
        sizeof(*script) + length + 1);
#endif
    memset(script, 0, sizeof(*script));
    strcpy(script->filename, name);

    script->buffer = (char *)(script + 1);
    script->buffer[length] = '\0';
    script->length = (int32_t)length;
    script->scriptCursor = script->buffer;
    script->lastScriptCursor = script->buffer;
    script->endCursor = script->buffer + length;
    script->tokenAvailable = qfalse;
    script->line = PC_SCRIPT_INITIAL_LINE;
    script->lastLine = PC_SCRIPT_INITIAL_LINE;
    PS_CreatePunctuationTable(script, NULL);

    memcpy(script->buffer, buffer, length);
    return script;
}

/* Sources: CoDUOMP.exe 0x00448ba0..0x00448bd7 and coduo_lnxded
 * 0x0807f5d9..0x0807f603.
 * Evidence: repaired executable-gap record
 * coduomp/mcode/CoDUOMP/FUN_00448ba0_00448bd7.mcode.
 * Name: exact same-module Mac symbol FreeScript. */
void FreeScript(script_t *script)
{
    if (script->punctuationTable != NULL) {
#if defined(WINDOWS_BEHAVIOR)
        FreeMemory(script->punctuationTable);
#else
        Com_DebugFree(script->punctuationTable);
#endif
    }
#if defined(WINDOWS_BEHAVIOR)
    FreeMemory(script);
#else
    Com_DebugFree(script);
#endif
}

/* Sources: CoDUOMP.exe 0x00448be0..0x00448bf8 and coduo_lnxded
 * 0x0807f603..0x0807f626.
 * Evidence: repaired executable-gap record
 * coduomp/mcode/CoDUOMP/FUN_00448be0_00448bf8.mcode.
 * Name: exact same-module Mac symbol PS_SetBaseFolder. The path is the format
 * argument in the original call, not a separate value for a "%s" format. */
void PS_SetBaseFolder(const char *path)
{
    /* NOT_FROM_ORIGINAL_SOURCE: keep the supplied path as data through the
     * single formatting pass. */
    Com_sprintf(pc_baseFolder, sizeof(pc_baseFolder), "%s", path);
}
