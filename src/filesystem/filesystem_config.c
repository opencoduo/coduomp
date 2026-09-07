#if !defined(_WIN32) && !defined(_XOPEN_SOURCE)
#define _XOPEN_SOURCE 700
#endif

#include "filesystem_config.h"
#include "filesystem.h"
#include "filesystem_services.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#if !defined(_WIN32)
#include <unistd.h>
#endif

static char *coduomp_config_readSource;
static qboolean coduomp_config_readFailed;
extern cvar_t *com_journal;

/* NOT_FROM_ORIGINAL_SOURCE: retain the exact selected VFS origin while the
 * handle is open; a pak or installation fallback is not the writable file. */
void coduomp_fs_config_read_origin(const char *root, const char *path, const char *entry)
{
    if (!coduomp_config_readSource)
        return;
    char resolved[CODUOMP_CONFIG_PATH];
    if (!entry && filesystem_compat_resolve_case_path(root, path, resolved, sizeof(resolved)))
        path = resolved;
    snprintf(coduomp_config_readSource, CODUOMP_CONFIG_PATH, "%s%s%s", path, entry ? "::" : "", entry ? entry : "");
}

/* NOT_FROM_ORIGINAL_SOURCE: a higher-priority unreadable file must not silently
 * become a successful load from a lower-priority fallback. */
void coduomp_fs_config_read_error(void)
{
    if (coduomp_config_readSource && errno != ENOENT && errno != ENOTDIR)
        coduomp_config_readFailed = qtrue;
}

/* NOT_FROM_ORIGINAL_SOURCE: resolve the active mod/server destination once,
 * enforce the existing virtual-path limits, and reject child link aliases. */
qboolean coduomp_fs_config_destination(const char *root, const char *game, const char *name, char path[CODUOMP_CONFIG_PATH], char error[256])
{
    if (!fs_homepath || !fs_searchpaths || !coduo_compat_path_is_safe_relative(name)) {
        snprintf(error, 256, "config destination is unavailable or unsafe");
        return qfalse;
    }
    if (!root)
        root = filesystem_compat_state_root(fs_homepath->string);
    if (!game)
        game = fs_currentGameDir;
    if (!coduo_compat_path_is_safe_relative(game) || strlen(root) + strlen(game) + strlen(name) + 3 >= MAX_OSPATH) {
        snprintf(error, 256, "config path exceeds the filesystem's supported path length");
        return qfalse;
    }
    char canonicalRoot[CODUOMP_CONFIG_PATH];
#if !defined(_WIN32)
    /* The user-selected root may itself be a platform alias; child paths may
     * not escape it. Resolve its existing prefix before checking children. */
    char suffix[CODUOMP_CONFIG_PATH] = "";
    if (strlen(root) >= sizeof(canonicalRoot))
        return qfalse;
    strcpy(canonicalRoot, root);
    char resolvedRoot[CODUOMP_CONFIG_PATH];
    while (!realpath(canonicalRoot, resolvedRoot)) {
        if (errno != ENOENT)
            return qfalse;
        char *slash = strrchr(canonicalRoot, '/');
        if (!slash)
            return qfalse;
        char remainder[CODUOMP_CONFIG_PATH];
        if (snprintf(remainder, sizeof(remainder), "%s%s", slash, suffix) >= (int)sizeof(remainder))
            return qfalse;
        strcpy(suffix, remainder);
        if (slash == canonicalRoot)
            slash[1] = '\0';
        else
            *slash = '\0';
    }
    if (snprintf(canonicalRoot, sizeof(canonicalRoot), "%s%s", resolvedRoot, suffix) >= (int)sizeof(canonicalRoot))
        return qfalse;
    root = canonicalRoot;
#else
    (void)canonicalRoot;
#endif
    char requested[CODUOMP_CONFIG_PATH], resolved[CODUOMP_CONFIG_PATH];
    if (snprintf(requested, sizeof(requested), "%s/%s/%s", root, game, name) >= (int)sizeof(requested))
        return qfalse;
    for (char *p = requested; *p; ++p) {
        if (*p == '\\')
            *p = '/';
    }
    if (!coduomp_config_path(requested, path, error))
        return qfalse;
    if (filesystem_compat_resolve_case_path(root, path, resolved, sizeof(resolved))) {
        if (!coduomp_config_path(resolved, path, error))
            return qfalse;
    }
    return qtrue;
}

/* NOT_FROM_ORIGINAL_SOURCE: read one owned, length-delimited VFS snapshot and
 * check read/close results before publishing it to preflight or execution. */
int coduomp_fs_config_read(const char *name, char **data, size_t *size, char source[CODUOMP_CONFIG_PATH], char error[256])
{
    *data = NULL;
    *size = 0;
    snprintf(source, CODUOMP_CONFIG_PATH, "%s", name);
    coduomp_config_readSource = source;
    coduomp_config_readFailed = qfalse;
    /* Keep the existing journal stream aligned during recording and replay.
     * Its temporary allocation is copied before parsing or queuing commands. */
    if (com_journal && com_journal->integer && strstr(name, ".cfg")) {
        void *journalData = NULL;
        int32_t length = FS_ReadFile(name, &journalData);
        coduomp_config_readSource = NULL;
        if (com_journal->integer == 2)
            snprintf(source, CODUOMP_CONFIG_PATH, "journal::%s", name);
        if (length < 0 || !journalData)
            return coduomp_config_readFailed ? -1 : 0;
        qboolean accepted = !coduomp_config_readFailed && length <= CODUOMP_CONFIG_MAX_BYTES;
        char *bytes = accepted ? malloc((size_t)length + 1) : NULL;
        if (bytes)
            memcpy(bytes, journalData, (size_t)length + 1);
        FS_FreeFile(journalData);
        if (!bytes) {
            snprintf(error, 256, "journal config could not be read within execution capacity");
            return -1;
        }
        *data = bytes;
        *size = (size_t)length;
        return 1;
    }
    int32_t handle = 0;
    int32_t length = FS_FOpenFileRead(name, &handle, qtrue);
    coduomp_config_readSource = NULL;
    if (length < 0 || handle == 0) {
        if (coduomp_config_readFailed) {
            snprintf(error, 256, "config could not be opened for reading");
            return -1;
        }
        return 0;
    }
    if (coduomp_config_readFailed || length > CODUOMP_CONFIG_MAX_BYTES) {
        FS_FCloseFile(handle);
        snprintf(error, 256, "%s", coduomp_config_readFailed ? "a higher-priority config is unreadable" : "config exceeds command queue capacity");
        return -1;
    }
    char *bytes = malloc((size_t)length + 1);
    if (!bytes) {
        FS_FCloseFile(handle);
        snprintf(error, 256, "not enough memory to load config");
        return -1;
    }
    qboolean ok = FS_Read(bytes, length, handle) == length;
    if (fs_handleFiles[handle].zipArchive == NULL) {
        FILE *file = FS_FileForHandle(handle);
        if (fgetc(file) != EOF || ferror(file))
            ok = qfalse;
        if (fclose(file))
            ok = qfalse;
        memset(&fs_handleFiles[handle], 0, sizeof(fs_handleFiles[handle]));
    } else {
        FS_FCloseFile(handle);
    }
    if (!ok) {
        free(bytes);
        snprintf(error, 256, "config read/close failed or the file changed during reading");
        return -1;
    }
    bytes[length] = '\0';
    *data = bytes;
    *size = (size_t)length;
    return 1;
}

/* NOT_FROM_ORIGINAL_SOURCE: call only on the main thread after publication. */
void coduomp_fs_config_changed(void)
{
    filesystem_compat_host_paths_changed();
}
