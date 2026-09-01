#include "hunk.h"

#include "com_sprintf.h"
#include "filesystem/filesystem.h"
#include "q_command.h"
#include "q_cvar.h"
#include "q_string.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

enum {
    HUNK_LOG_BUFFER_SIZE = 4096,
    HUNK_LOG_COMPARE_LIMIT = 99999,
    HUNK_MINIMUM_MEGABYTES_DEDICATED = 1,
    HUNK_MINIMUM_MEGABYTES_CLIENT = 80,
    HUNK_MEGABYTE_SHIFT = 20
};

#if UINTPTR_MAX > UINT32_MAX
/* COMPATIBILITY_PATCH (NOT_FROM_ORIGINAL_SOURCE): the native client retains
 * host pointers in renderer world, model, and cell records.  Large stock-era
 * maps can consequently consume more of the fixed hunk than their original
 * i386 equivalents and leave no room for ordinary renderer scratch.  Use a
 * wider default only when the client cvar does not already exist; explicit
 * com_hunkMegs values, dedicated servers, and original-width builds retain
 * their existing meaning and defaults. */
#define HUNK_DEFAULT_CLIENT_MEGABYTES "256"
#else
#define HUNK_DEFAULT_CLIENT_MEGABYTES "128"
#endif
#define HUNK_DEFAULT_DEDICATED_MEGABYTES "128"

void Com_Error(errorParm_t code, const char *format, ...);
void Com_Printf(const char *format, ...);
void Sys_OutOfMemory(void);
extern cvar_t *dedicated;

/* The Linux port stores these dormant diagnostic values beside hunk_state_t;
 * Windows keeps them inside its larger hunk_state_t.  The functions and
 * behavior are common; only the proven target ABI storage owner differs. */
#if defined(LINUX_BEHAVIOR)
extern hunk_log_block_t *hunk_logBlocks;
extern int32_t hunk_logFile;
extern size_t hunk_totalZoneSize;
#define HUNK_LOG_BLOCKS hunk_logBlocks
#define HUNK_LOG_FILE hunk_logFile
#define HUNK_TOTAL_ZONE_SIZE hunk_totalZoneSize
#else
#define HUNK_LOG_BLOCKS hunk.logBlocks
#define HUNK_LOG_FILE hunk.logFile
#define HUNK_TOTAL_ZONE_SIZE hunk.totalZoneSize
#endif

#if UINTPTR_MAX > UINT32_MAX
#define HUNK_SIZE_FORMAT "%8zu"
#define HUNK_SIZE_ARGUMENT(value) (value)
#define HUNK_LOG_SIZE_FORMAT "%zu"
#define HUNK_LOG_SIZE_ARGUMENT(value) (value)
#else
#define HUNK_SIZE_FORMAT "%8i"
#define HUNK_SIZE_ARGUMENT(value) ((int32_t)(value))
#define HUNK_LOG_SIZE_FORMAT "%d"
#define HUNK_LOG_SIZE_ARGUMENT(value) ((int32_t)(value))
#endif

/*
 * Common hunk diagnostics and initialization:
 *
 *   Com_Meminfo_f      CoDUOMP.exe 0x004355b0; coduo_lnxded 0x0806bbaa
 *   Hunk_Log           CoDUOMP.exe 0x00435760; coduo_lnxded 0x0806bd84
 *   Hunk_SmallLog      CoDUOMP.exe 0x00435880; coduo_lnxded 0x0806bedd
 *   Com_InitHunkMemory CoDUOMP.exe 0x00435a00; coduo_lnxded 0x0806c0c7
 *
 * Hunk_SmallLog uses the Windows comparison form.  Linux's unbounded
 * Q_stricmp differs only for impossible null or 100000-byte source-file
 * names; neither binary has a producer for such log records, so that compiler
 * artifact does not justify a permanent platform behavior split.
 */

void Com_Meminfo_f(void)
{
    Com_Printf(HUNK_SIZE_FORMAT " bytes total hunk\n",
               HUNK_SIZE_ARGUMENT(hunk.totalSize));
    Com_Printf(HUNK_SIZE_FORMAT " bytes total zone\n",
               HUNK_SIZE_ARGUMENT(HUNK_TOTAL_ZONE_SIZE));
    Com_Printf("\n");

    Com_Printf(HUNK_SIZE_FORMAT " low mark\n",
               HUNK_SIZE_ARGUMENT(hunk.lowMark));
    Com_Printf(HUNK_SIZE_FORMAT " low permanent\n",
               HUNK_SIZE_ARGUMENT(hunk.lowUsed));
    if (hunk.lowTemp != hunk.lowUsed) {
        Com_Printf(HUNK_SIZE_FORMAT " low temp\n",
                   HUNK_SIZE_ARGUMENT(hunk.lowTemp));
    }
    Com_Printf("\n");

    Com_Printf(HUNK_SIZE_FORMAT " high mark\n",
               HUNK_SIZE_ARGUMENT(hunk.highMark));
    Com_Printf(HUNK_SIZE_FORMAT " high permanent\n",
               HUNK_SIZE_ARGUMENT(hunk.highUsed));
    if (hunk.highTemp != hunk.highUsed) {
        Com_Printf(HUNK_SIZE_FORMAT " high temp\n",
                   HUNK_SIZE_ARGUMENT(hunk.highTemp));
    }
    Com_Printf("\n");

    Com_Printf(HUNK_SIZE_FORMAT " total hunk in use\n",
               HUNK_SIZE_ARGUMENT(hunk.highUsed + hunk.lowUsed));
    Com_Printf("\n");
}

void Hunk_Log(void)
{
    char line[HUNK_LOG_BUFFER_SIZE];
    size_t totalBytes = 0;
    int32_t blockCount = 0;

    if (HUNK_LOG_FILE == 0 || FS_Initialized() == qfalse) {
        return;
    }

    Com_sprintf(
        line, sizeof(line),
        "\r\n================\r\nHunk log\r\n================\r\n");
    (void)FS_Write(line, (int32_t)strlen(line), HUNK_LOG_FILE);

    for (const hunk_log_block_t *block = HUNK_LOG_BLOCKS;
         block != NULL; block = block->next) {
        totalBytes += block->size;
        ++blockCount;
    }

    Com_sprintf(line, sizeof(line),
                HUNK_LOG_SIZE_FORMAT " Hunk memory\r\n",
                HUNK_LOG_SIZE_ARGUMENT(totalBytes));
    (void)FS_Write(line, (int32_t)strlen(line), HUNK_LOG_FILE);
    Com_sprintf(line, sizeof(line), "%d hunk blocks\r\n", blockCount);
    (void)FS_Write(line, (int32_t)strlen(line), HUNK_LOG_FILE);
}

void Hunk_SmallLog(void)
{
    char line[HUNK_LOG_BUFFER_SIZE];
    size_t totalBytes = 0;
    int32_t blockCount = 0;

    if (HUNK_LOG_FILE == 0 || FS_Initialized() == qfalse) {
        return;
    }

    for (hunk_log_block_t *block = HUNK_LOG_BLOCKS;
         block != NULL; block = block->next) {
        block->printed = 0;
    }

    Com_sprintf(
        line, sizeof(line),
        "\r\n================\r\nHunk Small log\r\n"
        "================\r\n");
    (void)FS_Write(line, (int32_t)strlen(line), HUNK_LOG_FILE);

    for (hunk_log_block_t *block = HUNK_LOG_BLOCKS;
         block != NULL; block = block->next) {
        if (block->printed != 0) {
            continue;
        }

        for (hunk_log_block_t *candidate = block->next;
             candidate != NULL; candidate = candidate->next) {
            if (block->sourceLine == candidate->sourceLine &&
                block->sourceFile != NULL &&
                candidate->sourceFile != NULL &&
                Q_stricmpn(block->sourceFile, candidate->sourceFile,
                           HUNK_LOG_COMPARE_LIMIT) == 0) {
                totalBytes += candidate->size;
                candidate->printed = 1;
            }
        }

        totalBytes += block->size;
        ++blockCount;
    }

    Com_sprintf(line, sizeof(line),
                HUNK_LOG_SIZE_FORMAT " Hunk memory\r\n",
                HUNK_LOG_SIZE_ARGUMENT(totalBytes));
    (void)FS_Write(line, (int32_t)strlen(line), HUNK_LOG_FILE);
    Com_sprintf(line, sizeof(line), "%d hunk blocks\r\n", blockCount);
    (void)FS_Write(line, (int32_t)strlen(line), HUNK_LOG_FILE);
}

void Com_InitHunkMemory(void)
{
    int32_t minimumMegabytes;
    const char *minimumMessage;
    cvar_t *comHunkMegs;
    int32_t hunkMegabytes;

    if (FS_LoadStack() != 0) {
        Com_Error(
            ERR_FATAL,
            "\x15" "Hunk initialization failed. File system load stack not zero");
    }

    const char *defaultMegabytes = HUNK_DEFAULT_DEDICATED_MEGABYTES;
#if UINTPTR_MAX > UINT32_MAX
    if (dedicated == NULL || dedicated->integer == 0)
        defaultMegabytes = HUNK_DEFAULT_CLIENT_MEGABYTES;
#endif
    comHunkMegs = Cvar_Get("com_hunkMegs", defaultMegabytes,
                           CVAR_ARCHIVE | CVAR_LATCH);
    if (dedicated != NULL && dedicated->integer != 0) {
        minimumMegabytes = HUNK_MINIMUM_MEGABYTES_DEDICATED;
        minimumMessage =
            "Minimum com_hunkMegs for a dedicated server is %i, "
            "allocating %i megs.\n";
    } else {
        minimumMegabytes = HUNK_MINIMUM_MEGABYTES_CLIENT;
        minimumMessage =
            "Minimum com_hunkMegs is %i, allocating %i megs.\n";
    }

    hunkMegabytes = comHunkMegs->integer;
    if (hunkMegabytes < minimumMegabytes) {
        hunkMegabytes = minimumMegabytes;
        Com_Printf(minimumMessage, minimumMegabytes, hunkMegabytes);
    }

    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if ((size_t)hunkMegabytes >
        (SIZE_MAX - (size_t)(HUNK_ALIGNMENT - 1)) >> HUNK_MEGABYTE_SHIFT) {
        Sys_OutOfMemory();
    }
    hunk.totalSize = (size_t)hunkMegabytes << HUNK_MEGABYTE_SHIFT;

    hunk_allocData = malloc(hunk.totalSize + HUNK_ALIGNMENT - 1);
    if (hunk_allocData == NULL) {
        Sys_OutOfMemory();
    }

    hunk_data = (uint8_t *)(
        ((uintptr_t)hunk_allocData + HUNK_ALIGNMENT - 1) &
        ~(uintptr_t)(HUNK_ALIGNMENT - 1));

    Hunk_ClearToStart();
    Cmd_AddCommand("meminfo", Com_Meminfo_f);
}

#undef HUNK_LOG_BLOCKS
#undef HUNK_LOG_FILE
#undef HUNK_TOTAL_ZONE_SIZE
#undef HUNK_SIZE_FORMAT
#undef HUNK_SIZE_ARGUMENT
#undef HUNK_LOG_SIZE_FORMAT
#undef HUNK_LOG_SIZE_ARGUMENT
#undef HUNK_DEFAULT_CLIENT_MEGABYTES
#undef HUNK_DEFAULT_DEDICATED_MEGABYTES
