#include "server_namespace.h"
#include "server_namespace_provider.h"

#include "filesystem/filesystem.h"
#include "filesystem/filesystem_path_security.h"
#include "qcommon/q_command.h"

#include <string.h>

/* NOT_FROM_ORIGINAL_SOURCE_STORAGE_FILE: archived user policy for the
 * compatibility server namespace. It defaults on for normal client builds. */
static cvar_t *coduomp_serverCache;

/* NOT_FROM_ORIGINAL_SOURCE: centralizes the policy gate so every cache entry
 * point agrees with the Advanced-menu setting. */
static qboolean coduomp_server_namespace_enabled(void)
{
    return coduomp_serverCache == NULL || coduomp_serverCache->integer != 0
               ? qtrue
               : qfalse;
}

/* NOT_FROM_ORIGINAL_SOURCE: bounds a server-selected qpath against both the
 * engine qpath policy and the longer namespace root before host construction. */
static qboolean coduomp_server_namespace_download_qpath_valid(
    const char *root, const char *qpath)
{
    if (root == NULL || qpath == NULL ||
        coduo_compat_path_is_safe_relative(qpath) == qfalse) {
        return qfalse;
    }
    return strlen(root) + strlen(qpath) + 3u <= MAX_OSPATH
               ? qtrue
               : qfalse;
}

void coduomp_server_namespace_reset_for_startup(void)
{
    coduomp_server_namespace_provider.resetForStartup();
}

/* NOT_FROM_ORIGINAL_SOURCE: explicitly promote the active isolated server
 * configuration into the user's ordinary configuration namespace. */
static void coduomp_server_namespace_promote_config_f(void)
{
    if (Cmd_Argc() != 1) {
        Com_Printf("Usage: promoteserverconfig\n");
        return;
    }
    coduomp_server_namespace_provider.promoteCurrentConfig();
}

/* NOT_FROM_ORIGINAL_SOURCE: remove only configuration files from every
 * isolated server namespace while retaining downloaded server content. */
static void coduomp_server_namespace_clear_configs_f(void)
{
    if (Cmd_Argc() != 1) {
        Com_Printf("Usage: clearserverconfigs\n");
        return;
    }
    coduomp_server_namespace_provider.clearConfigs();
}

/* NOT_FROM_ORIGINAL_SOURCE: expose the two user-controlled server namespace
 * maintenance operations through the client console. */
void coduomp_server_namespace_register_commands(void)
{
    coduomp_serverCache =
        Cvar_Get("cl_serverCache", "1", CVAR_ARCHIVE);
    Cmd_AddCommand("promoteserverconfig",
                   coduomp_server_namespace_promote_config_f);
    Cmd_AddCommand("clearserverconfigs",
                   coduomp_server_namespace_clear_configs_f);
}

qboolean coduomp_server_namespace_activate(
    const netadr_t *address, const char *serverName,
    qboolean eligibleRemoteServer)
{
    if (coduomp_server_namespace_enabled() == qfalse) {
        return coduomp_server_namespace_provider.isActive() != qfalse
                   ? coduomp_server_namespace_provider.deactivate()
                   : qfalse;
    }
    return coduomp_server_namespace_provider.activate(
        address, serverName, eligibleRemoteServer);
}

qboolean coduomp_server_namespace_deactivate(void)
{
    return coduomp_server_namespace_provider.deactivate();
}

qboolean coduomp_server_namespace_is_active(void)
{
    return coduomp_server_namespace_provider.isActive();
}

/* NOT_FROM_ORIGINAL_SOURCE: give the selected compatibility provider one
 * pre-restart opportunity to seed its cache from checksum-matched root paks. */
qboolean coduomp_server_namespace_cache_referenced_paks(void)
{
    if (coduomp_server_namespace_enabled() == qfalse)
        return qfalse;
    return coduomp_server_namespace_provider.cacheReferencedPaks();
}

/* NOT_FROM_ORIGINAL_SOURCE: append compatibility-owned cached mods to the
 * original paired-string $modlist result. */
int32_t coduomp_server_namespace_append_cached_mods(
    char *listBuffer, int32_t bufferSize)
{
    if (coduomp_server_namespace_enabled() == qfalse)
        return 0;
    return coduomp_server_namespace_provider.appendCachedMods(
        listBuffer, bufferSize);
}

const char *coduomp_server_namespace_state_root(
    const char *ordinaryHomeRoot)
{
    return coduomp_server_namespace_provider.stateRoot(
        ordinaryHomeRoot);
}

const char *coduomp_server_namespace_content_root(
    const char *ordinaryHomeRoot)
{
    return coduomp_server_namespace_provider.contentRoot(
        ordinaryHomeRoot);
}

void coduomp_server_namespace_add_game_directory(const char *gameName)
{
    if (gameName == NULL || gameName[0] == '\0' ||
        coduomp_server_namespace_is_active() == qfalse) {
        return;
    }

    const char *const contentRoot =
        coduomp_server_namespace_content_root(fs_homepath->string);
    const char *const stateRoot =
        coduomp_server_namespace_state_root(fs_homepath->string);

    /* Content is added first so the writable state directory is the final,
     * highest-priority loose-file search path for this game. */
    FS_AddLocalizedGameDirectory(contentRoot, gameName);
    FS_AddLocalizedGameDirectory(stateRoot, gameName);
}

qboolean coduomp_server_namespace_allows_searchpath(
    const searchpath_t *searchpath)
{
    return coduomp_server_namespace_provider.allowsSearchpath(searchpath);
}

qboolean coduomp_server_namespace_download_file_exists(
    const char *qpath)
{
    const char *const root =
        coduomp_server_namespace_content_root(fs_homepath->string);
    return coduomp_server_namespace_download_qpath_valid(root, qpath) !=
                   qfalse
               ? coduomp_fs_root_file_exists(root, qpath)
               : qfalse;
}

int32_t coduomp_server_namespace_open_download_write(
    const char *qpath)
{
    const char *const root =
        coduomp_server_namespace_content_root(fs_homepath->string);
    return coduomp_server_namespace_download_qpath_valid(root, qpath) !=
                   qfalse
               ? coduomp_fs_root_fopen_file_write(root, qpath)
               : 0;
}

void coduomp_server_namespace_rename_download(
    const char *sourceQPath, const char *destQPath)
{
    const char *const root =
        coduomp_server_namespace_content_root(fs_homepath->string);
    if (coduomp_server_namespace_download_qpath_valid(
            root, sourceQPath) != qfalse &&
        coduomp_server_namespace_download_qpath_valid(
            root, destQPath) != qfalse) {
        coduomp_fs_root_rename(root, sourceQPath, destQPath);
    }
}

qboolean coduomp_server_namespace_build_download_path(
    const char *qpath, char *osPath, size_t osPathSize)
{
    const char *const root =
        coduomp_server_namespace_content_root(fs_homepath->string);

    if (osPath == NULL || osPathSize == 0 ||
        coduomp_server_namespace_download_qpath_valid(root, qpath) ==
            qfalse ||
        strlen(root) + strlen(qpath) + 3u > osPathSize) {
        if (osPath != NULL && osPathSize > 0)
            osPath[0] = '\0';
        return qfalse;
    }

    FS_BuildOSPath(root, qpath, "", osPath);
    osPath[strlen(osPath) - 1] = '\0';
    return qtrue;
}

void coduomp_server_namespace_remove_download(const char *qpath)
{
    const char *const root =
        coduomp_server_namespace_content_root(fs_homepath->string);
    if (coduomp_server_namespace_download_qpath_valid(root, qpath) != qfalse)
        coduomp_fs_root_remove(root, qpath);
}
