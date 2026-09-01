#ifndef ENGINE_FILESYSTEM_SERVICES_H
#define ENGINE_FILESYSTEM_SERVICES_H

#include "coduo_engine_structs.h"
#include "filesystem/filesystem_compat_types.h"
#include "filesystem/filesystem.h"
#include "filesystem/filesystem_path_security.h"
#include "qcommon/q_command.h"
#include "qcommon/q_cvar.h"
#include "qcommon/q_memory.h"
#include "qcommon/q_string.h"
#include "compat/coduo_ctype_compat.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdarg.h>

struct stat;

/* NOT_FROM_ORIGINAL_SOURCE: retain the Linux libc boundary used by the
 * original filesystem extension comparison. */
static inline int32_t fs_compat_stricmp(const char *left,
                                       const char *right)
{
    return strcasecmp(left, right);
}

/* NOT_FROM_ORIGINAL_SOURCE: preserve the original glibc signed-byte ctype
 * dependency used by PakFileLanguage. */
static inline int32_t fs_compat_isalpha(int32_t character)
{
    return isalpha(coduo_ctype_signed_byte_arg(character));
}

void Com_Error(errorParm_t code, const char *format, ...);
void Com_DPrintf(const char *format, ...);
void Com_Printf(const char *format, ...);
void *Hunk_AllocateTempMemoryInternal(size_t size);
void Hunk_FreeTempMemory(void *ptr);
void FS_CheckFileSystemStarted(void);
void Sys_LoadingKeepAlive(void);
void Sys_Mkdir(char *path);
int32_t Sys_Stat(const char *path, struct stat *statbuf);
const char *Sys_DefaultCDPath(void);
const char *Sys_DefaultBasePath(void);
const char *Sys_DefaultHomePath(void);

/* NOT_FROM_ORIGINAL_SOURCE: published pack names must satisfy the relative
 * virtual-path policy and exclude the surrounding stream delimiter. */
static inline qboolean filesystem_compat_accept_server_pak_name(
    const char *path)
{
    return coduo_compat_path_is_safe_relative(path) != qfalse &&
                   strchr(path, '@') == NULL
               ? qtrue
               : qfalse;
}

static inline qboolean filesystem_compat_resolve_case_path(
    const char *trustedRoot, const char *path,
    char *resolvedPath, size_t resolvedPathSize)
{
    (void)trustedRoot;
    (void)path;
    (void)resolvedPath;
    (void)resolvedPathSize;
    return qfalse;
}

/* NOT_FROM_ORIGINAL_SOURCE: retain the dedicated engine's public filesystem
 * initialized-state assertion at the common list entry points. */
static inline void filesystem_compat_check_started(void)
{
    FS_CheckFileSystemStarted();
}

/* NOT_FROM_ORIGINAL_SOURCE: preserve the dedicated engine's streamed-file
 * ownership edge before a shared handle is cleared. */
static inline void filesystem_compat_end_streamed_file(int32_t handle)
{
    if (fs_handleFiles[handle].seekCallbackGuard != 0)
        Sys_EndStreamedFile(handle);
}

/* NOT_FROM_ORIGINAL_SOURCE: retain the original dedicated server's direct
 * libc fopen boundary. */
static inline FILE *filesystem_compat_fopen_read(
    const char *trustedRoot, const char *path)
{
    (void)trustedRoot;
    return fopen(path, "rb");
}

/* NOT_FROM_ORIGINAL_SOURCE: retain the original dedicated engine's system
 * directory-create boundary used by FS_CreatePath. */
static inline void filesystem_compat_mkdir(char *path)
{
    Sys_Mkdir(path);
}

/* NOT_FROM_ORIGINAL_SOURCE: the original dedicated server has no native
 * case-recovery cache to invalidate. */
static inline void filesystem_compat_host_paths_changed(void)
{
}

/* NOT_FROM_ORIGINAL_SOURCE: the standalone target has no connection-owned
 * filesystem namespace; these adapters retain its original home-root policy. */
static inline const char *filesystem_compat_state_root(
    const char *ordinaryHomeRoot)
{
    return ordinaryHomeRoot;
}

static inline void filesystem_compat_add_server_game_directory(
    const char *gameName)
{
    (void)gameName;
}

/* NOT_FROM_ORIGINAL_SOURCE: the standalone server has no client mod menu or
 * per-server content cache. */
static inline int32_t filesystem_compat_append_cached_mods(
    char *listBuffer, int32_t bufferSize)
{
    (void)listBuffer;
    (void)bufferSize;
    return 0;
}

static inline qboolean filesystem_compat_server_scope_allows_searchpath(
    const searchpath_t *searchpath)
{
    (void)searchpath;
    return qtrue;
}

static inline qboolean filesystem_compat_download_file_exists(
    const char *qpath)
{
    return FS_SV_FileExists(qpath);
}

static inline void filesystem_compat_remove_download(const char *qpath)
{
    char osPath[MAX_OSPATH];

    FS_BuildOSPath(fs_homepath->string, qpath, "", osPath);
    osPath[strlen(osPath) - 1] = '\0';
    FS_Remove(osPath);
}

int32_t filesystem_compat_archive_tell(
    const fileHandleData_t *fileHandle);
int32_t filesystem_compat_archive_length(
    const fileHandleData_t *fileHandle);
void filesystem_compat_archive_close_current(
    fileHandleData_t *fileHandle);
void filesystem_compat_archive_close(fileHandleData_t *fileHandle);
int32_t filesystem_compat_archive_read(fileHandleData_t *fileHandle,
                                       void *buffer, uint32_t byteCount);
int32_t filesystem_compat_archive_rewind(fileHandleData_t *fileHandle);
void filesystem_compat_pack_close(pack_t *pack);
qboolean filesystem_compat_archive_open_entry(
    pack_t *pack, const fileInPack_t *packFile,
    fileHandleData_t *fileHandle, qboolean uniqueFile,
    qboolean quiet);
qboolean filesystem_compat_archive_open_catalog(
    const char *path, void **archiveOut, uint32_t *entryCountOut);
void filesystem_compat_archive_close_catalog(void *archive);
int32_t filesystem_compat_archive_go_to_first(void *archive);
int32_t filesystem_compat_archive_go_to_next(void *archive);
int32_t filesystem_compat_archive_get_current_info(
    void *archive, filesystem_compat_archive_file_info_t *fileInfo,
    char *filename, uint32_t filenameSize);
int32_t filesystem_compat_archive_get_current_position(void *archive);

static inline qboolean filesystem_compat_www_bad_checksum(
    const char *pakName)
{
    (void)pakName;
    return qfalse;
}

static inline void filesystem_compat_pure_set_changed(void)
{
}

/* NOT_FROM_ORIGINAL_SOURCE: expose the Linux dedicated engine's English-only
 * localized-asset policy to the common search-path algorithm. */
static inline int32_t filesystem_compat_language_count(void)
{
    return 1;
}

static inline const char *filesystem_compat_language_name(int32_t language)
{
    return language == 0 ? "english" : NULL;
}

static inline void filesystem_compat_report_unsupported_pak_language(
    const char *base, const char *game, const char *pakName)
{
    /* The original dedicated engine silently ignores localized paks for
     * languages other than English. */
    (void)base;
    (void)game;
    (void)pakName;
}

static inline void filesystem_compat_loading_keepalive(void)
{
    Sys_LoadingKeepAlive();
}

/* NOT_FROM_ORIGINAL_SOURCE: the standalone server has no client localization
 * package, but the Windows filesystem body reads cl_language during its first
 * archive lookup.  Establish that filesystem-visible cvar just as the client
 * language initializer does. */
static inline void filesystem_compat_init_language(void)
{
    cl_language = Cvar_Get("cl_language", "0", CVAR_ARCHIVE);
}

/* NOT_FROM_ORIGINAL_SOURCE: the standalone server has no client localization
 * strings to clear when using Windows engine behavior. */
static inline void filesystem_compat_clear_localized_strings(void)
{
}

/* NOT_FROM_ORIGINAL_SOURCE: the standalone server has no client localization
 * state to refresh when using Windows engine behavior. */
static inline void filesystem_compat_update_language_info(void)
{
}

void Com_ReadCDKey(void);

/* NOT_FROM_ORIGINAL_SOURCE: retain the standalone server's original
 * no-argument CD-key boundary in Windows-behavior mode. */
static inline void filesystem_compat_read_cd_key(void)
{
    Com_ReadCDKey();
}

/* NOT_FROM_ORIGINAL_SOURCE: a dedicated server does not own client mod-key
 * accumulation. */
static inline void filesystem_compat_append_cd_key(const char *gameDirectory)
{
    (void)gameDirectory;
}

/* NOT_FROM_ORIGINAL_SOURCE: retain the dedicated engine's asynchronous
 * streamed-file service after the common reentrancy guard is cleared. */
static inline int32_t filesystem_compat_streamed_read(
    void *buffer, int32_t byteCount, int32_t handle)
{
    return Sys_StreamedRead(buffer, byteCount, 1, handle);
}

/* NOT_FROM_ORIGINAL_SOURCE: retain the dedicated engine's stream-thread seek
 * dispatch while the common callback guard is clear. */
static inline void filesystem_compat_stream_seek(
    int32_t handle, int32_t offset, int32_t origin)
{
    Sys_StreamSeek(handle, offset, origin);
}

/* NOT_FROM_ORIGINAL_SOURCE: the dedicated engine has no client sound,
 * localization-package, or case-cache ownership at filesystem shutdown. */
static inline void filesystem_compat_shutdown_begin(void)
{
}

char **Sys_ListFiles(const char *directory, const char *extension,
                     const char *filter, int32_t *numFiles,
                     qboolean wantDirectories);
void Sys_FreeFileList(char **list);

#endif
