#ifndef CODUOMP_FILESYSTEM_SERVICES_H
#define CODUOMP_FILESYSTEM_SERVICES_H

#include "client/engine/q_shared.h"
#include "client/engine/filesystem/server_namespace.h"
#include "filesystem/filesystem_compat_types.h"
#include "filesystem/filesystem_path_security.h"
#include "client/engine/localization/string_ed_api.h"
#include "client/engine/platform/case_sensitive_fs.h"
#include "client/engine/platform/crt_boundary.h"
#include "client/engine/system_platform.h"

#include <string.h>
#include <stdarg.h>
#include <stdio.h>

/* NOT_FROM_ORIGINAL_SOURCE: keep the original engine's CRT case-folding
 * boundary available to common filesystem code. */
static inline int32_t fs_compat_stricmp(const char *left,
                                       const char *right)
{
    return coduo_crt_stricmp(left, right);
}

/* NOT_FROM_ORIGINAL_SOURCE: preserve the original Windows CRT classification
 * dependency used by PakFileLanguage. */
static inline int32_t fs_compat_isalpha(int32_t character)
{
    return coduo_crt_isalpha(character);
}

/* NOT_FROM_ORIGINAL_SOURCE: client-only policy and host-filesystem services
 * used at the common pak-purity boundary. */
static inline qboolean filesystem_compat_accept_server_pak_name(
    const char *path)
{
    if (path == NULL || path[0] == '\0')
        return qfalse;
    if (coduo_compat_path_is_safe_relative(path) == qfalse ||
        strchr(path, '@') != NULL) {
        Com_Printf(
            "WARNING: refusing unsafe server pak name '%s'\n",
            path);
        return qfalse;
    }
    return qtrue;
}

static inline qboolean filesystem_compat_resolve_case_path(
    const char *trustedRoot, const char *path,
    char *resolvedPath, size_t resolvedPathSize)
{
    return coduomp_resolve_case_path(
        trustedRoot, path, resolvedPath, resolvedPathSize);
}

/* NOT_FROM_ORIGINAL_SOURCE: the Windows engine's public list entry points do
 * not repeat the dedicated engine's initialized-state assertion. */
static inline void filesystem_compat_check_started(void)
{
}

/* NOT_FROM_ORIGINAL_SOURCE: the Windows handle table has no dedicated-engine
 * streamed-file teardown hook. */
static inline void filesystem_compat_end_streamed_file(int32_t handle)
{
    (void)handle;
}

/* NOT_FROM_ORIGINAL_SOURCE: retain the native client's case-recovery service
 * at host fopen boundaries without changing the recovered engine algorithm. */
static inline FILE *filesystem_compat_fopen_read(
    const char *trustedRoot, const char *path)
{
    return coduomp_fopen_case_read(trustedRoot, path);
}

/* NOT_FROM_ORIGINAL_SOURCE: retain the original Windows CRT directory-create
 * boundary used by FS_CreatePath. */
static inline void filesystem_compat_mkdir(char *path)
{
    (void)coduomp_crt_mkdir(path);
}

/* NOT_FROM_ORIGINAL_SOURCE: invalidate native case-recovery state after host
 * namespace mutations performed by common filesystem code. */
static inline void filesystem_compat_host_paths_changed(void)
{
    coduomp_case_path_cache_clear();
}

/* NOT_FROM_ORIGINAL_SOURCE: client filesystem policy is selected by the
 * server-namespace provider, keeping common filesystem code build-neutral. */
static inline const char *filesystem_compat_state_root(
    const char *ordinaryHomeRoot)
{
    return coduomp_server_namespace_state_root(ordinaryHomeRoot);
}

static inline void filesystem_compat_add_server_game_directory(
    const char *gameName)
{
    coduomp_server_namespace_add_game_directory(gameName);
}

/* NOT_FROM_ORIGINAL_SOURCE: client-only extension of the retail $modlist
 * stream with launchable per-server cache paths. */
static inline int32_t filesystem_compat_append_cached_mods(
    char *listBuffer, int32_t bufferSize)
{
    return coduomp_server_namespace_append_cached_mods(
        listBuffer, bufferSize);
}

static inline qboolean filesystem_compat_server_scope_allows_searchpath(
    const searchpath_t *searchpath)
{
    return coduomp_server_namespace_allows_searchpath(searchpath);
}

static inline qboolean filesystem_compat_download_file_exists(
    const char *qpath)
{
    return coduomp_server_namespace_download_file_exists(qpath);
}

static inline void filesystem_compat_remove_download(const char *qpath)
{
    coduomp_server_namespace_remove_download(qpath);
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
    return CL_WWWBadChecksum(pakName);
}

static inline void filesystem_compat_pure_set_changed(void)
{
    MSS_StopSounds(MSS_STOP_PRESERVE_2D_AND_3D);
}

/* NOT_FROM_ORIGINAL_SOURCE: expose the Windows client's complete localized-
 * asset policy to the common search-path algorithm. */
static inline int32_t filesystem_compat_language_count(void)
{
    return 14;
}

static inline const char *filesystem_compat_language_name(int32_t language)
{
    return SEH_GetLanguageName(language);
}

static inline void filesystem_compat_report_unsupported_pak_language(
    const char *base, const char *game, const char *pakName)
{
    Com_Printf(
        "WARNING: Localized assets pak file %s/%s/%s has invalid name "
        "(bad language name specified). Proper naming convention is: "
        "localized_[language]_pak#.pk3\n",
        base, game, pakName);

    if (fs_printedSupportedLanguages == qfalse) {
        Com_Printf("Supported languages are:\n");
        for (int32_t language = 0;
             language < filesystem_compat_language_count();
             ++language) {
            Com_Printf("    %s\n",
                       filesystem_compat_language_name(language));
        }
        fs_printedSupportedLanguages = qtrue;
    }
}

static inline void filesystem_compat_loading_keepalive(void)
{
    coduomp_loading_keepalive();
}

/* NOT_FROM_ORIGINAL_SOURCE: client localization ownership edge used by the
 * common Windows filesystem startup and restart bodies. */
static inline void filesystem_compat_init_language(void)
{
    SEH_InitLanguage();
}

/* NOT_FROM_ORIGINAL_SOURCE: client localization ownership edge used by the
 * common Windows filesystem restart body. */
static inline void filesystem_compat_clear_localized_strings(void)
{
    SEH_StringEd_Clear(0);
}

/* NOT_FROM_ORIGINAL_SOURCE: client localization ownership edge used by the
 * common Windows filesystem startup and restart bodies. */
static inline void filesystem_compat_update_language_info(void)
{
    SEH_UpdateLanguageInfo();
}

#if defined(_WIN32)
void Com_ReadCDKey(void);

/* NOT_FROM_ORIGINAL_SOURCE: target adapter for the Windows client's original
 * no-argument CD-key read boundary. */
static inline void filesystem_compat_read_cd_key(void)
{
    Com_ReadCDKey();
}

/* NOT_FROM_ORIGINAL_SOURCE: the original Windows startup body does not append
 * a separate fs_game key. */
static inline void filesystem_compat_append_cd_key(const char *gameDirectory)
{
    (void)gameDirectory;
}
#else
void Com_ReadCDKey(const char *gameDirectory);
void Com_AppendCDKey(const char *gameDirectory);

/* NOT_FROM_ORIGINAL_SOURCE: native-client adapter for the platform-neutral
 * primary-key location. */
static inline void filesystem_compat_read_cd_key(void)
{
    Com_ReadCDKey("");
}

/* NOT_FROM_ORIGINAL_SOURCE: native-client adapter for a mod-specific key. */
static inline void filesystem_compat_append_cd_key(const char *gameDirectory)
{
    Com_AppendCDKey(gameDirectory);
}
#endif

/* NOT_FROM_ORIGINAL_SOURCE: the Windows streamed-read wrapper is folded into
 * FS_Read2 as an ordinary read after the reentrancy guard is cleared. */
static inline int32_t filesystem_compat_streamed_read(
    void *buffer, int32_t byteCount, int32_t handle)
{
    return FS_Read(buffer, byteCount, handle);
}

/* NOT_FROM_ORIGINAL_SOURCE: the original Windows streamed-seek wrapper
 * recursively enters FS_Seek while the callback guard is clear. */
static inline void filesystem_compat_stream_seek(
    int32_t handle, int32_t offset, int32_t origin)
{
    (void)FS_Seek(handle, offset, origin);
}

/* NOT_FROM_ORIGINAL_SOURCE: retain client-only native case state, sound, and
 * localization teardown before common filesystem ownership is released. */
static inline void filesystem_compat_shutdown_begin(void)
{
    coduomp_case_path_cache_clear();
    MSS_StopSounds(MSS_STOP_PRESERVE_2D_AND_3D);
    SEH_StringEd_Clear(0);
}

#endif
