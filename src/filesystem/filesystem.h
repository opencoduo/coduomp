#ifndef Q_FILESYSTEM_H
#define Q_FILESYSTEM_H

#include "qcommon/filesystem_types.h"
#include "qcommon/q_cvar.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#ifdef __cplusplus
#ifndef _Static_assert
#define _Static_assert static_assert
#define FS_UNDEFINE_STATIC_ASSERT
#endif
#ifndef _Alignof
#define _Alignof alignof
#define FS_UNDEFINE_ALIGNOF
#endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

#if defined(_WIN32)
#define FS_HOST_PATH_SEPARATOR '\\'
#else
#define FS_HOST_PATH_SEPARATOR '/'
#endif

extern fileHandleData_t fs_handleFiles[FS_HANDLE_COUNT];
extern fs_dir_file_list_t *fs_dirFileLists;
extern fs_dir_file_list_t *fs_lookupDirFileLists;
extern searchpath_t *fs_lookupSearchpaths;
extern searchpath_t *fs_searchpaths;
extern int32_t fs_fakeChkSum;
extern int32_t fs_packFiles;
extern char fs_languageNameBuffers[2][64];
extern int32_t fs_languageNameBufferIndex;
extern qboolean fs_printedSupportedLanguages;
extern char fs_currentGameDir[FS_PACK_NAME_SIZE];
extern char fs_gameDirVar[FS_PACK_NAME_SIZE];
extern cvar_t *fs_basepath;
extern cvar_t *fs_basegame;
extern cvar_t *fs_cdpath;
extern cvar_t *fs_copyfiles;
extern cvar_t *fs_debug;
extern cvar_t *fs_game;
extern cvar_t *fs_homepath;
extern cvar_t *fs_ignoreLocalized;
extern cvar_t *fs_restrict;
extern cvar_t *cl_language;
extern cvar_t *sv_running;
extern int32_t fs_numServerPaks;
extern int32_t fs_serverPaks[FS_MAX_SERVER_PAKS];
extern char *fs_serverPakNames[FS_MAX_SERVER_PAKS];
extern int32_t fs_numServerReferencedPaks;
extern int32_t fs_serverReferencedPaks[FS_MAX_SERVER_PAKS];
extern char *fs_serverReferencedPakNames[FS_MAX_SERVER_PAKS];
extern char fs_savedBasePath[FS_PACK_NAME_SIZE];
extern char fs_savedGame[FS_PACK_NAME_SIZE];
extern int32_t fs_checksumFeed;
extern qboolean fs_fileAccessed;
extern int32_t fs_loadStack;

void FS_BuildOSPath_Internal(const char *base, const char *game, const char *qpath, char *osPath, qboolean quiet);
void FS_ReplaceSeparators(char *path);
void FS_ConvertPath(char *path);
void FS_BuildOSPath(const char *base, const char *game, const char *qpath, char *osPath);
qboolean FS_DirectoryHasNonDotEntries(const char *path);
FILE *FS_FileForHandle(int32_t handle);
qboolean FS_CreatePath(char *osPath);
void FS_Copyfiles(const char *sourceOSPath, char *destOSPath);
void FS_CopyFile(const char *sourceQPath, const char *destQPath);
void FS_Remove(const char *osPath);
qboolean FS_FileExists(const char *qpath);
qboolean FS_SV_FileExists(const char *qpath);
void FS_SV_Rename(const char *sourceQPath, const char *destQPath);
/* NOT_FROM_ORIGINAL_SOURCE: explicit-root variants used by the client
 * server-namespace adapter. The original server-facing wrappers below retain
 * fs_homepath as their root. */
qboolean coduomp_fs_root_file_exists(const char *root, const char *qpath);
void coduomp_fs_root_rename(const char *root, const char *sourceQPath, const char *destQPath);
void coduomp_fs_root_remove(const char *root, const char *qpath);
void FS_Rename(const char *sourceQPath, const char *destQPath);
uint32_t FS_HashFileName(const char *name, int32_t hashSize);
qboolean FS_PakIsPure(const pack_t *pack);
int32_t FS_FileIsInPAK(const char *path, int32_t *checksumOut);
qboolean FS_serverPak(const char *pakName);
qboolean FS_idPak(const char *path, const char *mainGame, const char *baseGame);
qboolean FS_ComparePaks(char *neededPaks, int32_t neededPaksSize, qboolean includeAlternateNames);
const char *FS_ReferencedPakPureChecksums(void);
const char *FS_LoadedPakChecksums(void);
const char *FS_LoadedPakNames(void);
const char *FS_LoadedPakPureChecksums(void);
const char *FS_ReferencedPakChecksums(void);
const char *FS_ReferencedPakNames(void);
int32_t FS_HandleForFile(qboolean uniqueFile);
void FS_ForceFlush(int32_t handle);
void FS_Flush(int32_t handle);
int32_t FS_FTell(int32_t handle);
int32_t FS_filelength(int32_t handle);
void FS_FCloseFile(int32_t handle);
int32_t FS_FOpenFileWrite(const char *qpath);
int32_t FS_FOpenTextFileWrite(const char *qpath);
int32_t FS_FOpenFileAppend(const char *qpath);
void FS_Printf(int32_t fileHandle, const char *format, ...);
int32_t FS_SV_FOpenFileRead(const char *qpath, int32_t *handleOut);
int32_t FS_SV_FOpenFileWrite(const char *qpath);
int32_t coduomp_fs_root_fopen_file_write(const char *root, const char *qpath);
int32_t FS_FilenameCompare(const char *left, const char *right);
qboolean FS_FileCompare(const char *leftPath, const char *rightPath);
const char *FS_ShiftStr(const char *text, int32_t shift);
const char *FS_ShiftedStrStr(const char *haystack, const char *encodedNeedle, int8_t shift);
const char *FS_GetExtensionSubString(const char *path);
qboolean FS_PureIgnoresExtension(const char *extension);
char *FS_ShortOSFilePath(const char *qpath);
int32_t FS_ReturnPath(const char *path, char *directory, int32_t *depth);
int32_t FS_AddFileToList(const char *text, char **list, int32_t count);
pack_t *FS_LoadZipFile(const char *zipFile, const char *basename);
char **FS_ListFilteredFiles(const char *path, const char *extension, const char *filter, int32_t *fileCount);
char **FS_ListFiles(const char *path, const char *extension, int32_t *fileCount);
void FS_FreeFileList(char **files);
int32_t FS_GetFileList(const char *path, const char *extension, char *listBuffer, int32_t bufferSize);
int32_t FS_GetModList(char *listBuffer, int32_t bufferSize);
char **Sys_ConcatenateFileLists(char **firstList, char **secondList, char **thirdList);
int32_t FS_PathCmp(const char *left, const char *right);
void FS_SortFileList(char **list, int32_t count);
void FS_DisplayPath(qboolean localizedFilter);
void FS_Path_f(void);
void FS_FullPath_f(void);
char *PakFileLanguage(const char *text);
int32_t paksort(const void *leftEntry, const void *rightEntry);
void FS_AddSearchPath(searchpath_t *searchpath);
qboolean FS_UseSearchPath(const searchpath_t *searchpath);
qboolean FS_LanguageHasAssets(int32_t language);
void FS_AddPakFilesForGameDirectory(const char *base, const char *game);
void FS_AddGameDirectory(const char *base, const char *game, qboolean localized, int32_t language);
void FS_AddLocalizedGameDirectory(const char *base, const char *game);
void FS_AddNonPackFileDirectory_Internal(const char *base, const char *game, const char *path, const char *extension);
void FS_AddNonPackFileDirectory(const char *path, const char *extension);
fileData_t *FS_GetDataForFile(const char *base, const char *path, const char *extension);
void FS_ClearDataForFiles(const void *rangeStart, const void *rangeEnd);
void FS_ShutdownSearchPaths(searchpath_t *searchpath);
void FS_ShutdownFileLists(fs_dir_file_list_t *list);
void FS_RefreshLookupCache(void);
void FS_ShutdownServerPakNames(void);
void FS_ShutdownServerReferencedPaks(void);
void FS_Shutdown(qboolean clearLookupLists);
qboolean FS_Initialized(void);
void FS_Startup(const char *gameName);
void FS_InitFilesystem(void);
void FS_Restart(int32_t checksumFeed);
qboolean FS_ConditionalRestart(int32_t checksumFeed);
void FS_CheckRestrictedDemoPaks(void);
void FS_ClearPakReferences(qboolean preserveGeneralAndGameReferences);
void FS_PureServerSetLoadedPaks(const char *checksumText, const char *nameText);
void FS_PureServerSetReferencedPaks(const char *checksumText, const char *nameText);
void FS_Dir_f(void);
void FS_NewDir_f(void);
void FS_TouchFile_f(void);
void FS_AddCommands(void);
void FS_RemoveCommands(void);
qboolean FS_Delete(const char *qpath);
qboolean FS_MakeReadOnly(const char *qpath, qboolean makeReadOnly);
int32_t FS_Read(void *buffer, int32_t byteCount, int32_t handle);
int32_t FS_Read2(void *buffer, int32_t byteCount, int32_t handle);
int32_t FS_Write(const void *buffer, int32_t byteCount, int32_t handle);
int32_t FS_Seek(int32_t handle, int32_t offset, int32_t origin);
int32_t FS_ReadFile(const char *qpath, void **buffer);
void Sys_BeginStreamedFile(int32_t handle, int32_t readAhead);
void Sys_EndStreamedFile(int32_t handle);
int32_t Sys_StreamedRead(void *buffer, int32_t size, int32_t count, int32_t handle);
void Sys_StreamSeek(int32_t handle, int32_t offset, int32_t origin);
int32_t FS_LoadStack(void);
void FS_ResetFiles(void);
void FS_FreeFile(void *buffer);
void FS_WriteFile(const char *qpath, const void *buffer, int32_t byteCount);
int32_t FS_FOpenFileRead_Internal(const char *qpath, int32_t *handle, qboolean uniqueFile, qboolean quiet);
int32_t FS_FOpenFileReadStream(const char *qpath, int32_t *handle, qboolean uniqueFile);
int32_t FS_FOpenFileRead(const char *qpath, int32_t *handle, qboolean uniqueFile);
qboolean FS_TouchFile(const char *qpath);
int32_t FS_FOpenFileByMode(const char *qpath, int32_t *handle, fsMode_t mode);

#if UINTPTR_MAX == UINT32_MAX
_Static_assert(_Alignof(fileInPack_t) == 0x04, "i386 pack-file alignment changed");
_Static_assert(offsetof(fileInPack_t, zipPosition) == 0x00, "i386 pack-file ZIP position offset changed");
_Static_assert(sizeof(((fileInPack_t *)0)->zipPosition) == 0x04, "i386 pack-file ZIP position extent changed");
_Static_assert(offsetof(fileInPack_t, data) == 0x04, "i386 pack-file data record moved");
_Static_assert(sizeof(((fileInPack_t *)0)->data) == 0x0c, "i386 pack-file data record extent changed");
_Static_assert(offsetof(fileInPack_t, name) == 0x10, "i386 pack-file name offset changed");
_Static_assert(sizeof(((fileInPack_t *)0)->name) == 0x04, "i386 pack-file name pointer extent changed");
_Static_assert(offsetof(fileInPack_t, next) == 0x14, "i386 pack-file next offset changed");
_Static_assert(sizeof(((fileInPack_t *)0)->next) == 0x04, "i386 pack-file next pointer extent changed");
_Static_assert(sizeof(fileInPack_t) == 0x18, "original i386 pack-file size changed");

_Static_assert(_Alignof(directory_t) == 0x01, "i386 directory alignment changed");
_Static_assert(offsetof(directory_t, path) == 0x000, "i386 directory path offset changed");
_Static_assert(sizeof(((directory_t *)0)->path) == 0x100, "i386 directory path extent changed");
_Static_assert(offsetof(directory_t, gamedir) == 0x100, "i386 directory game-name offset changed");
_Static_assert(sizeof(((directory_t *)0)->gamedir) == 0x100, "i386 directory game-name extent changed");
_Static_assert(sizeof(directory_t) == 0x200, "original i386 directory size changed");

_Static_assert(_Alignof(fs_dir_file_t) == 0x04, "i386 loose-file alignment changed");
_Static_assert(offsetof(fs_dir_file_t, data) == 0x00, "i386 loose-file data record moved");
_Static_assert(sizeof(((fs_dir_file_t *)0)->data) == 0x0c, "i386 loose-file data record extent changed");
_Static_assert(offsetof(fs_dir_file_t, next) == 0x0c, "i386 loose-file next offset changed");
_Static_assert(sizeof(((fs_dir_file_t *)0)->next) == 0x04, "i386 loose-file next pointer extent changed");
_Static_assert(sizeof(fs_dir_file_t) == 0x10, "original i386 loose-file size changed");

_Static_assert(_Alignof(fs_dir_file_list_t) == 0x04, "i386 loose-file-list alignment changed");
_Static_assert(offsetof(fs_dir_file_list_t, path) == 0x000, "i386 loose-file-list path offset changed");
_Static_assert(sizeof(((fs_dir_file_list_t *)0)->path) == 0x100, "i386 loose-file-list path extent changed");
_Static_assert(offsetof(fs_dir_file_list_t, numFiles) == 0x100, "i386 loose-file-list count offset changed");
_Static_assert(sizeof(((fs_dir_file_list_t *)0)->numFiles) == 0x04, "i386 loose-file-list count extent changed");
_Static_assert(offsetof(fs_dir_file_list_t, hashSize) == 0x104, "i386 loose-file-list hash-size offset changed");
_Static_assert(sizeof(((fs_dir_file_list_t *)0)->hashSize) == 0x04, "i386 loose-file-list hash-size extent changed");
_Static_assert(offsetof(fs_dir_file_list_t, hashTable) == 0x108, "i386 loose-file-list hash-table offset changed");
_Static_assert(sizeof(((fs_dir_file_list_t *)0)->hashTable) == 0x04, "i386 loose-file-list hash-table pointer extent changed");
_Static_assert(offsetof(fs_dir_file_list_t, fileList) == 0x10c, "i386 loose-file-list entries offset changed");
_Static_assert(sizeof(((fs_dir_file_list_t *)0)->fileList) == 0x04, "i386 loose-file-list entries pointer extent changed");
_Static_assert(offsetof(fs_dir_file_list_t, next) == 0x110, "i386 loose-file-list next offset changed");
_Static_assert(sizeof(((fs_dir_file_list_t *)0)->next) == 0x04, "i386 loose-file-list next pointer extent changed");
_Static_assert(sizeof(fs_dir_file_list_t) == 0x114, "original i386 loose-file-list size changed");

_Static_assert(_Alignof(pack_t) == 0x04, "i386 pack alignment changed");
_Static_assert(offsetof(pack_t, pakFilename) == 0x000, "i386 pack filename offset changed");
_Static_assert(sizeof(((pack_t *)0)->pakFilename) == 0x100, "i386 pack filename extent changed");
_Static_assert(offsetof(pack_t, pakBasename) == 0x100, "i386 pack basename offset changed");
_Static_assert(sizeof(((pack_t *)0)->pakBasename) == 0x100, "i386 pack basename extent changed");
_Static_assert(offsetof(pack_t, pakGamename) == 0x200, "i386 pack game-name offset changed");
_Static_assert(sizeof(((pack_t *)0)->pakGamename) == 0x100, "i386 pack game-name extent changed");
_Static_assert(offsetof(pack_t, zipFile) == 0x300, "i386 pack ZIP object offset changed");
_Static_assert(sizeof(((pack_t *)0)->zipFile) == 0x04, "i386 pack ZIP object pointer extent changed");
_Static_assert(offsetof(pack_t, checksum) == 0x304, "i386 pack checksum offset changed");
_Static_assert(sizeof(((pack_t *)0)->checksum) == 0x04, "i386 pack checksum extent changed");
_Static_assert(offsetof(pack_t, pureChecksum) == 0x308, "i386 pack pure-checksum offset changed");
_Static_assert(sizeof(((pack_t *)0)->pureChecksum) == 0x04, "i386 pack pure-checksum extent changed");
_Static_assert(offsetof(pack_t, numFiles) == 0x30c, "i386 pack file-count offset changed");
_Static_assert(sizeof(((pack_t *)0)->numFiles) == 0x04, "i386 pack file-count extent changed");
_Static_assert(offsetof(pack_t, generalReference) == 0x310, "i386 pack general-reference flag offset changed");
_Static_assert(sizeof(((pack_t *)0)->generalReference) == 0x01, "i386 pack general-reference flag extent changed");
_Static_assert(offsetof(pack_t, uiModuleReference) == 0x311, "i386 pack UI-module reference flag offset changed");
_Static_assert(sizeof(((pack_t *)0)->uiModuleReference) == 0x01, "i386 pack UI-module reference flag extent changed");
_Static_assert(offsetof(pack_t, cgameModuleReference) == 0x312, "i386 pack cgame-module reference flag offset changed");
_Static_assert(sizeof(((pack_t *)0)->cgameModuleReference) == 0x01, "i386 pack cgame-module reference flag extent changed");
_Static_assert(offsetof(pack_t, gameModuleReference) == 0x313, "i386 pack game-module reference flag offset changed");
_Static_assert(sizeof(((pack_t *)0)->gameModuleReference) == 0x01, "i386 pack game-module reference flag extent changed");
_Static_assert(offsetof(pack_t, hashSize) == 0x314, "i386 pack hash-size offset changed");
_Static_assert(sizeof(((pack_t *)0)->hashSize) == 0x04, "i386 pack hash-size extent changed");
_Static_assert(offsetof(pack_t, hashTable) == 0x318, "i386 pack hash-table offset changed");
_Static_assert(sizeof(((pack_t *)0)->hashTable) == 0x04, "i386 pack hash-table pointer extent changed");
_Static_assert(offsetof(pack_t, fileList) == 0x31c, "i386 pack file-list offset changed");
_Static_assert(sizeof(((pack_t *)0)->fileList) == 0x04, "i386 pack file-list pointer extent changed");
_Static_assert(sizeof(pack_t) == 0x320, "original i386 pack size changed");

_Static_assert(_Alignof(searchpath_t) == 0x04, "i386 search-path alignment changed");
_Static_assert(offsetof(searchpath_t, next) == 0x00, "i386 search-path next offset changed");
_Static_assert(sizeof(((searchpath_t *)0)->next) == 0x04, "i386 search-path next pointer extent changed");
_Static_assert(offsetof(searchpath_t, pack) == 0x04, "i386 search-path pack offset changed");
_Static_assert(sizeof(((searchpath_t *)0)->pack) == 0x04, "i386 search-path pack pointer extent changed");
_Static_assert(offsetof(searchpath_t, dir) == 0x08, "i386 search-path directory offset changed");
_Static_assert(sizeof(((searchpath_t *)0)->dir) == 0x04, "i386 search-path directory pointer extent changed");
_Static_assert(offsetof(searchpath_t, localized) == 0x0c, "i386 search-path localized offset changed");
_Static_assert(sizeof(((searchpath_t *)0)->localized) == 0x04, "i386 search-path localized extent changed");
_Static_assert(offsetof(searchpath_t, language) == 0x10, "i386 search-path language offset changed");
_Static_assert(sizeof(((searchpath_t *)0)->language) == 0x04, "i386 search-path language extent changed");
_Static_assert(sizeof(searchpath_t) == 0x14, "original i386 search-path size changed");

_Static_assert(_Alignof(fileHandleData_t) == 0x04, "i386 file-handle alignment changed");
_Static_assert(offsetof(fileHandleData_t, ioObject) == 0x00, "i386 file-handle IO object offset changed");
_Static_assert(sizeof(((fileHandleData_t *)0)->ioObject) == 0x04, "i386 file-handle IO object pointer extent changed");
_Static_assert(offsetof(fileHandleData_t, uniqueObject) == 0x04, "i386 file-handle unique-object offset changed");
_Static_assert(sizeof(((fileHandleData_t *)0)->uniqueObject) == 0x04, "i386 file-handle unique-object extent changed");
_Static_assert(offsetof(fileHandleData_t, sync) == 0x08, "i386 file-handle sync offset changed");
_Static_assert(sizeof(((fileHandleData_t *)0)->sync) == 0x04, "i386 file-handle sync extent changed");
_Static_assert(offsetof(fileHandleData_t, position) == 0x0c, "i386 file-handle position offset changed");
_Static_assert(sizeof(((fileHandleData_t *)0)->position) == 0x04, "i386 file-handle position extent changed");
_Static_assert(offsetof(fileHandleData_t, size) == 0x10, "i386 file-handle size offset changed");
_Static_assert(sizeof(((fileHandleData_t *)0)->size) == 0x04, "i386 file-handle size extent changed");
_Static_assert(offsetof(fileHandleData_t, zipRewindOffset) == 0x14, "i386 file-handle ZIP rewind offset changed");
_Static_assert(sizeof(((fileHandleData_t *)0)->zipRewindOffset) == 0x04, "i386 file-handle ZIP rewind extent changed");
_Static_assert(offsetof(fileHandleData_t, zipArchive) == 0x18, "i386 file-handle pack offset changed");
_Static_assert(sizeof(((fileHandleData_t *)0)->zipArchive) == 0x04, "i386 file-handle pack pointer extent changed");
_Static_assert(offsetof(fileHandleData_t, seekCallbackGuard) == 0x1c, "i386 file-handle seek guard offset changed");
_Static_assert(sizeof(((fileHandleData_t *)0)->seekCallbackGuard) == 0x04, "i386 file-handle seek guard extent changed");
_Static_assert(offsetof(fileHandleData_t, name) == 0x20, "i386 file-handle name offset changed");
_Static_assert(sizeof(((fileHandleData_t *)0)->name) == 0x100, "i386 file-handle name extent changed");
_Static_assert(sizeof(fileHandleData_t) == 0x120, "original i386 file-handle size changed");
#endif

#ifdef FS_UNDEFINE_ALIGNOF
#undef _Alignof
#undef FS_UNDEFINE_ALIGNOF
#endif
#ifdef FS_UNDEFINE_STATIC_ASSERT
#undef _Static_assert
#undef FS_UNDEFINE_STATIC_ASSERT
#endif

#ifdef __cplusplus
}
#endif

#endif
