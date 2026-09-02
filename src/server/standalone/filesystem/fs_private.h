#ifndef CODUO_FS_PRIVATE_H
#define CODUO_FS_PRIVATE_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <zlib.h>

#include "server/standalone/bindings/coduo_engine_structs.h"
#include "qcommon/com_lifecycle.h"
#include "qcommon/com_startup_commands.h"
#include "qcommon/com_sprintf.h"
#include "filesystem/filesystem.h"
#include "qcommon/q_memory.h"
#include "qcommon/q_string.h"
#include "../core_math/core_math_private.h"

typedef struct coduo_unz_read_info_s coduo_unz_read_info_t;
struct coduo_unz_s;

typedef struct coduo_zip_global_info_s {
    uint32_t number_entry;
    uint32_t size_comment;
} coduo_zip_global_info_t;

typedef struct coduo_zip_file_info_s {
    uint32_t version;
    uint32_t version_needed;
    uint32_t flag;
    uint32_t compression_method;
    uint32_t dos_date;
    uint32_t crc;
    uint32_t compressed_size;
    uint32_t uncompressed_size;
    uint32_t size_filename;
    uint32_t size_file_extra;
    uint32_t size_file_comment;
    uint32_t disk_num_start;
    uint32_t internal_fa;
    uint32_t external_fa;
    uint32_t tmu_date[6];
} coduo_zip_file_info_t;

typedef struct coduo_unz_s {
    FILE *file;
    uint32_t number_entry;
    uint32_t size_comment;
    uint32_t byte_before_the_zipfile;
    uint32_t num_file;
    uint32_t pos_in_central_dir;
    qboolean current_file_ok;
    uint32_t central_pos;
    uint32_t size_central_dir;
    uint32_t offset_central_dir;
    coduo_zip_file_info_t cur_file_info;
    uint32_t cur_file_info_internal_offset_curfile;
    coduo_unz_read_info_t *pfile_in_zip_read;
} coduo_unz_t;

typedef coduo_unz_t coduo_fs_zip_io_file_t;

#if UINTPTR_MAX == UINT32_MAX
_Static_assert(sizeof(coduo_zip_global_info_t) == 0x08,
               "coduo_zip_global_info_t size mismatch");
_Static_assert(sizeof(coduo_zip_file_info_t) == 0x50,
               "coduo_zip_file_info_t size mismatch");
_Static_assert(offsetof(coduo_zip_file_info_t, version) == 0x00,
               "coduo_zip_file_info_t.version offset mismatch");
_Static_assert(offsetof(coduo_zip_file_info_t, version_needed) == 0x04,
               "coduo_zip_file_info_t.version_needed offset mismatch");
_Static_assert(offsetof(coduo_zip_file_info_t, flag) == 0x08,
               "coduo_zip_file_info_t.flag offset mismatch");
_Static_assert(offsetof(coduo_zip_file_info_t, compression_method) == 0x0c,
               "coduo_zip_file_info_t.compression_method offset mismatch");
_Static_assert(offsetof(coduo_zip_file_info_t, dos_date) == 0x10,
               "coduo_zip_file_info_t.dos_date offset mismatch");
_Static_assert(offsetof(coduo_zip_file_info_t, crc) == 0x14,
               "coduo_zip_file_info_t.crc offset mismatch");
_Static_assert(offsetof(coduo_zip_file_info_t, compressed_size) == 0x18,
               "coduo_zip_file_info_t.compressed_size offset mismatch");
_Static_assert(offsetof(coduo_zip_file_info_t, uncompressed_size) == 0x1c,
               "coduo_zip_file_info_t.uncompressed_size offset mismatch");
_Static_assert(offsetof(coduo_zip_file_info_t, size_filename) == 0x20,
               "coduo_zip_file_info_t.size_filename offset mismatch");
_Static_assert(offsetof(coduo_zip_file_info_t, size_file_extra) == 0x24,
               "coduo_zip_file_info_t.size_file_extra offset mismatch");
_Static_assert(offsetof(coduo_zip_file_info_t, size_file_comment) == 0x28,
               "coduo_zip_file_info_t.size_file_comment offset mismatch");
_Static_assert(offsetof(coduo_zip_file_info_t, disk_num_start) == 0x2c,
               "coduo_zip_file_info_t.disk_num_start offset mismatch");
_Static_assert(offsetof(coduo_zip_file_info_t, internal_fa) == 0x30,
               "coduo_zip_file_info_t.internal_fa offset mismatch");
_Static_assert(offsetof(coduo_zip_file_info_t, external_fa) == 0x34,
               "coduo_zip_file_info_t.external_fa offset mismatch");
_Static_assert(offsetof(coduo_zip_file_info_t, tmu_date) == 0x38,
               "coduo_zip_file_info_t.tmu_date offset mismatch");
_Static_assert(sizeof(coduo_unz_t) == 0x80,
               "coduo_unz_t size mismatch");
_Static_assert(offsetof(coduo_unz_t, file) == 0x00,
               "coduo_unz_t.file offset mismatch");
_Static_assert(offsetof(coduo_unz_t, number_entry) == 0x04,
               "coduo_unz_t.number_entry offset mismatch");
_Static_assert(offsetof(coduo_unz_t, size_comment) == 0x08,
               "coduo_unz_t.size_comment offset mismatch");
_Static_assert(offsetof(coduo_unz_t, byte_before_the_zipfile) == 0x0c,
               "coduo_unz_t.byte_before_the_zipfile offset mismatch");
_Static_assert(offsetof(coduo_unz_t, num_file) == 0x10,
               "coduo_unz_t.num_file offset mismatch");
_Static_assert(offsetof(coduo_unz_t, pos_in_central_dir) == 0x14,
               "coduo_unz_t.pos_in_central_dir offset mismatch");
_Static_assert(offsetof(coduo_unz_t, current_file_ok) == 0x18,
               "coduo_unz_t.current_file_ok offset mismatch");
_Static_assert(offsetof(coduo_unz_t, central_pos) == 0x1c,
               "coduo_unz_t.central_pos offset mismatch");
_Static_assert(offsetof(coduo_unz_t, size_central_dir) == 0x20,
               "coduo_unz_t.size_central_dir offset mismatch");
_Static_assert(offsetof(coduo_unz_t, offset_central_dir) == 0x24,
               "coduo_unz_t.offset_central_dir offset mismatch");
_Static_assert(offsetof(coduo_unz_t, cur_file_info) == 0x28,
               "coduo_unz_t.cur_file_info offset mismatch");
_Static_assert(offsetof(coduo_unz_t,
                        cur_file_info_internal_offset_curfile) == 0x78,
               "coduo_unz_t.cur_file_info_internal_offset_curfile offset "
               "mismatch");
_Static_assert(offsetof(coduo_unz_t, pfile_in_zip_read) == 0x7c,
               "coduo_unz_t.pfile_in_zip_read offset mismatch");
_Static_assert(sizeof(fileInPack_t) == 0x18,
               "fileInPack_t size mismatch");
_Static_assert(offsetof(fileInPack_t, data) == 0x04,
               "fileInPack_t.data offset mismatch");
_Static_assert(offsetof(fileInPack_t, name) == 0x10,
               "fileInPack_t.name offset mismatch");
_Static_assert(offsetof(fileInPack_t, next) == 0x14,
               "fileInPack_t.next offset mismatch");
_Static_assert(sizeof(pack_t) == 0x320,
               "pack_t size mismatch");
_Static_assert(sizeof(fs_dir_file_t) == 0x10,
               "fs_dir_file_t size mismatch");
_Static_assert(sizeof(fs_dir_file_list_t) == 0x114,
               "fs_dir_file_list_t size mismatch");
_Static_assert(offsetof(pack_t, pakFilename) == 0x00,
               "pack_t.pakFilename offset mismatch");
_Static_assert(offsetof(pack_t, pakBasename) == 0x100,
               "pack_t.pakBasename offset mismatch");
_Static_assert(offsetof(pack_t, pakGamename) == 0x200,
               "pack_t.pakGamename offset mismatch");
_Static_assert(offsetof(pack_t, zipFile) == 0x300,
               "pack_t.zipFile offset mismatch");
_Static_assert(offsetof(pack_t, checksum) == 0x304,
               "pack_t.checksum offset mismatch");
_Static_assert(offsetof(pack_t, pureChecksum) == 0x308,
               "pack_t.pureChecksum offset mismatch");
_Static_assert(offsetof(pack_t, numFiles) == 0x30c,
               "pack_t.numFiles offset mismatch");
_Static_assert(offsetof(pack_t, generalReference) == 0x310,
               "pack_t.generalReference offset mismatch");
_Static_assert(offsetof(pack_t, uiModuleReference) == 0x311,
               "pack_t.uiModuleReference offset mismatch");
_Static_assert(offsetof(pack_t, cgameModuleReference) == 0x312,
               "pack_t.cgameModuleReference offset mismatch");
_Static_assert(offsetof(pack_t, gameModuleReference) == 0x313,
               "pack_t.gameModuleReference offset mismatch");
_Static_assert(offsetof(pack_t, hashSize) == 0x314,
               "pack_t.hashSize offset mismatch");
_Static_assert(offsetof(pack_t, hashTable) == 0x318,
               "pack_t.hashTable offset mismatch");
_Static_assert(offsetof(pack_t, fileList) == 0x31c,
               "pack_t.fileList offset mismatch");
_Static_assert(offsetof(directory_t, gamedir) == 0x100,
               "directory_t.gamedir offset mismatch");
_Static_assert(offsetof(fs_dir_file_t, next) == 0x0c,
               "fs_dir_file_t.next offset mismatch");
_Static_assert(offsetof(fs_dir_file_list_t, numFiles) == 0x100,
               "fs_dir_file_list_t.numFiles offset mismatch");
_Static_assert(offsetof(fs_dir_file_list_t, hashSize) == 0x104,
               "fs_dir_file_list_t.hashSize offset mismatch");
_Static_assert(offsetof(fs_dir_file_list_t, hashTable) == 0x108,
               "fs_dir_file_list_t.hashTable offset mismatch");
_Static_assert(offsetof(fs_dir_file_list_t, fileList) == 0x10c,
               "fs_dir_file_list_t.fileList offset mismatch");
_Static_assert(offsetof(fs_dir_file_list_t, next) == 0x110,
               "fs_dir_file_list_t.next offset mismatch");
_Static_assert(offsetof(coduo_fs_zip_io_file_t, file) == 0x00,
               "coduo_fs_zip_io_file_t.file offset mismatch");
_Static_assert(offsetof(coduo_fs_zip_io_file_t, cur_file_info) == 0x28,
               "coduo_fs_zip_io_file_t.cur_file_info offset mismatch");
_Static_assert(offsetof(coduo_fs_zip_io_file_t,
                        cur_file_info.uncompressed_size) == 0x44,
               "coduo_fs_zip_io_file_t.cur_file_info.uncompressed_size offset "
               "mismatch");
_Static_assert(offsetof(searchpath_t, localized) == 0x0c,
               "searchpath_t.localized offset mismatch");
_Static_assert(offsetof(searchpath_t, language) == 0x10,
               "searchpath_t.language offset mismatch");
#endif

extern int32_t fs_checksumFeed;
extern cvar_t *fs_basegame;
extern cvar_t *fs_basepath;
extern cvar_t *fs_cdpath;
extern cvar_t *fs_copyfiles;
extern cvar_t *fs_debug;
extern fs_dir_file_list_t *fs_dirFileLists;
extern int32_t fs_fakeChkSum;
extern cvar_t *fs_game;
extern char fs_gameDirVar[FS_PACK_NAME_SIZE];
extern char fs_currentGameDir[FS_PACK_NAME_SIZE];
extern cvar_t *fs_homepath;
extern cvar_t *fs_ignoreLocalized;
extern int fs_languageNameBufferIndex;
extern char fs_languageNameBuffers[FS_LANGUAGE_NAME_BUFFER_COUNT]
                                   [FS_LANGUAGE_NAME_BUFFER_SIZE];
extern cvar_t *fs_restrict;
extern int32_t fs_numServerPaks;
extern int32_t fs_numServerReferencedPaks;
extern fs_dir_file_list_t *fs_lookupDirFileLists;
extern searchpath_t *fs_lookupSearchpaths;
extern char *fs_serverPakNames[FS_MAX_SERVER_PAKS];
extern char *fs_serverReferencedPakNames[FS_MAX_SERVER_PAKS];
extern int32_t fs_serverPaks[FS_MAX_SERVER_PAKS];
extern int32_t fs_serverReferencedPaks[FS_MAX_SERVER_PAKS];
extern char fs_savedBasePath[FS_PACK_NAME_SIZE];
extern char fs_savedGame[FS_PACK_NAME_SIZE];
extern searchpath_t *fs_searchpaths;
extern fileHandleData_t fs_handleFiles[FS_HANDLE_COUNT];
extern int32_t hunk_logFile;
extern cvar_t *sv_running;

void FS_CheckFileSystemStarted(void);
void FS_BuildOSPath(const char *base, const char *game, const char *qpath,
                    char *ospath);
void FS_Copyfiles(const char *sourceOSPath, char *destOSPath);
void FS_Remove(const char *osPath);
qboolean FS_FileExists(const char *qpath);
void FS_Rename(const char *from, const char *to);
int32_t FS_FOpenFileWrite(const char *qpath);
int32_t FS_FOpenTextFileWrite(const char *qpath);
int32_t FS_FOpenFileAppend(const char *qpath);
qboolean FS_PakIsPure(const pack_t *pack);
int32_t FS_Read(void *buffer, int32_t length, int32_t handle);
qboolean FS_SV_FileExists(const char *path);
int32_t FS_SV_FOpenFileWrite(const char *path);
int32_t FS_SV_FOpenFileRead(const char *path,
                                           int32_t *handle);
void FS_SV_Rename(const char *from, const char *to);
int32_t FS_Read2(void *buffer, int32_t length, int32_t handle);
int32_t FS_PathCmp(const char *leftPath, const char *rightPath);
void FS_SortFileList(char **list, int count);
int32_t paksort(const void *leftEntry, const void *rightEntry);
void FS_AddPakFilesForGameDirectory(const char *base, const char *game);
void FS_AddGameDirectory(const char *base, const char *game, qboolean localized,
                  int32_t language);
void FS_AddLocalizedGameDirectory(const char *base, const char *game);
void FS_AddNonPackFileDirectory_Internal(const char *base, const char *game, const char *path,
                  const char *extension);
void FS_AddNonPackFileDirectory(const char *path, const char *extension);
void FS_ShutdownSearchPaths(searchpath_t *searchpath);
void FS_Shutdown(qboolean clearLookupLists);
const char *FS_LoadedPakChecksums(void);
const char *FS_LoadedPakNames(void);
const char *FS_ReferencedPakChecksums(void);
const char *FS_ReferencedPakNames(void);
uint32_t FS_LittleLong(uint32_t value);
char *Q_strlwr(char *string);
void Com_DPrintf(const char *format, ...);
void Com_Error(errorParm_t code, const char *format, ...);
void Com_Printf(const char *format, ...);
void *Hunk_AllocateTempMemoryInternal(size_t size);
void Hunk_FreeTempMemory(void *ptr);
cvar_t *Cvar_Get(const char *name, const char *value,
                              uint32_t flags);
void Cvar_Set(const char *name, const char *value);
char **Sys_ListFiles(const char *directory, const char *extension,
                     const char *filter, int *numfiles,
                     qboolean wantsubs);
void Sys_FreeFileList(char **list);
void Sys_Mkdir(char *path);
void Sys_LoadingKeepAlive(void);
void Sys_InitStreamThread(void);
void Sys_ShutdownStreamThread(void);
void Cmd_RemoveCommand(const char *name);
qboolean FS_serverPak(const char *pakName);
qboolean FS_ComparePaks(char *missingPaks, int32_t missingPaksSize,
                        qboolean includeAlternateNames);
const char *FS_ReferencedPakPureChecksums(void);
void FS_PureServerSetLoadedPaks(const char *checksumText,
                                const char *nameText);
void FS_PureServerSetReferencedPaks(const char *checksumText,
                                    const char *nameText);
int32_t Unzip_Close(struct coduo_unz_s *file);
int32_t Unzip_GetGlobalInfo(struct coduo_unz_s *file,
                            coduo_zip_global_info_t *globalInfo);
int32_t Unzip_GetCurrentFileInfo(struct coduo_unz_s *file,
                                 coduo_zip_file_info_t *fileInfo,
                                 char *filename, uint32_t filenameSize,
                                 void *extra, uint32_t extraSize,
                                 void *comment, uint32_t commentSize);
int32_t Unzip_GoToFirstFile(struct coduo_unz_s *file);
int32_t Unzip_GoToNextFile(struct coduo_unz_s *file);
int32_t Unzip_OpenCurrentFile(struct coduo_unz_s *zipStream);
int32_t Unzip_CloseCurrentFile(struct coduo_unz_s *file);
int32_t Unzip_TellCurrentFile(struct coduo_unz_s *zipStream);
int32_t Unzip_ReadCurrentFile(struct coduo_unz_s *file, void *buffer,
                              uint32_t length);
int32_t Unzip_GetCurrentFilePosition(struct coduo_unz_s *file,
                                     fileInPack_t *packFile);
int32_t Unzip_GoToFilePosition(struct coduo_unz_s *file, int32_t position);
struct coduo_unz_s *Unzip_CloneArchiveForPack(pack_t *pack,
                                              const struct coduo_unz_s *source);
struct coduo_unz_s *Unzip_Open(const char *path);
int32_t Sys_ReadLittleShort(FILE *file, uint32_t *value);
int32_t Sys_ReadLittleLong(FILE *file, uint32_t *value);
int32_t Sys_ZipStringCompare(const char *left, const char *right,
                             int32_t compareMode);
int32_t Sys_FindZipEndOfCentralDirectory(FILE *file);
int32_t coduomp_zlib_inflate_end(z_stream *stream);
int32_t coduomp_zlib_inflate_init2(z_stream *stream,
                                   int32_t windowBits, const char *version,
                                   int32_t streamSize);
int32_t coduomp_zlib_inflate(z_stream *stream, int32_t flush);
#endif
