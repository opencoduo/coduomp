#ifndef SHARED_SOUND_ALIAS_PRIVATE_H
#define SHARED_SOUND_ALIAS_PRIVATE_H

#include "qcommon/com_parse.h"
#include "qcommon/com_sprintf.h"
#include "qcommon/filesystem_types.h"
#include "qcommon/q_memory.h"
#include "qcommon/q_string.h"
#include "qcommon/qcommon_runtime_types.h"
#include "sound_alias.h"

#include <stddef.h>
#include <stdint.h>

extern char fs_gameDirVar[FS_PACK_NAME_SIZE];
extern cvar_t *fs_basepath;
extern char fs_currentGameDir[FS_PACK_NAME_SIZE];

cvar_t *Cvar_Get(const char *name, const char *value, uint32_t flags);
void Com_Error(errorParm_t code, const char *format, ...);
void Com_Printf(const char *format, ...);
void Com_DPrintf(const char *format, ...);
char *va(const char *format, ...);

int32_t FS_ReadFile(const char *path, void **buffer);
void FS_FreeFile(void *buffer);
char **FS_ListFiles(const char *path, const char *extension, int32_t *fileCount);
void FS_FreeFileList(char **files);
int32_t FS_Write(const void *buffer, int32_t byteCount, int32_t fileHandle);
int32_t FS_FOpenFileRead(const char *path, int32_t *fileHandle, qboolean uniqueFile);
void FS_FCloseFile(int32_t fileHandle);
int32_t FS_FOpenFileWrite(const char *path);
int32_t FS_FOpenTextFileWrite(const char *path);
void FS_Rename(const char *sourcePath, const char *destinationPath);
void FS_Remove(const char *path);
void FS_BuildOSPath(const char *base, const char *game, const char *qpath, char *osPath);
void FS_Copyfiles(const char *sourceOSPath, char *destinationOSPath);

void *Hunk_AllocateTempMemoryInternal(size_t size);
void Hunk_SetMarkTemp(void);
void Hunk_ClearToMarkTemp(void);

void Cmd_AddCommand(const char *name, xcommand_t function);
void Cmd_RemoveCommand(const char *name);

#endif
