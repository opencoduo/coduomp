#ifndef CODUOMP_SERVER_NAMESPACE_H
#define CODUOMP_SERVER_NAMESPACE_H

#include "client/engine/q_shared.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* NOT_FROM_ORIGINAL_SOURCE: client policy interface for transient,
 * connection-owned filesystem namespaces. The selected provider is either
 * the behaviorally inert stock implementation or the compatibility
 * implementation; callers do not contain build-policy conditionals. */
void coduomp_server_namespace_reset_for_startup(void);
void coduomp_server_namespace_register_commands(void);
qboolean coduomp_server_namespace_activate(
    const char *serverName, qboolean eligibleRemoteServer);
qboolean coduomp_server_namespace_activate_cached(
    const char *serverName);
qboolean coduomp_server_namespace_deactivate(void);
qboolean coduomp_server_namespace_is_active(void);
qboolean coduomp_server_namespace_cache_referenced_paks(void);
int32_t coduomp_server_namespace_append_cached_mods(
    char *listBuffer, int32_t bufferSize);
int32_t coduomp_server_namespace_resolve_cached_demo(
    const char *modName, const char *demoFileName,
    const char *serverName, char resolvedServer[MAX_QPATH],
    char resolvedMod[FS_PACK_NAME_SIZE]);
int32_t coduomp_server_namespace_list_cached_demos(
    const char *modName);

const char *coduomp_server_namespace_root(
    const char *ordinaryHomeRoot);

void coduomp_server_namespace_add_game_directory(const char *gameName);
qboolean coduomp_server_namespace_allows_searchpath(
    const searchpath_t *searchpath);

qboolean coduomp_server_namespace_download_file_exists(
    const char *qpath);
int32_t coduomp_server_namespace_open_download_write(
    const char *qpath);
void coduomp_server_namespace_rename_download(
    const char *sourceQPath, const char *destQPath);
qboolean coduomp_server_namespace_build_download_path(
    const char *qpath, char *osPath, size_t osPathSize);
void coduomp_server_namespace_remove_download(const char *qpath);

#ifdef __cplusplus
}
#endif

#endif
