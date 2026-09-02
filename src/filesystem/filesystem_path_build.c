#include "filesystem.h"
#include "filesystem_services.h"

#include <stdint.h>
#include <string.h>

/* Source: CoDUOMP.exe 0x0042cb20..0x0042cc1b; coduo_lnxded corresponding
 * filesystem path builder. Name, signature, fallback directory, join order,
 * and separator-normalization span agree with exact same-module Mac symbol
 * FS_BuildOSPath_Internal. Retail uses a 256-byte host-path contract;
 * NOT_FROM_ORIGINAL_SOURCE: maintained targets use the shared MAX_OSPATH
 * capacity without changing the join or failure behavior at that capacity. */
void FS_BuildOSPath_Internal(const char *base, const char *game, const char *qpath, char *osPath, qboolean quiet)
{
    if (game == NULL || game[0] == '\0')
        game = fs_currentGameDir;

    const int32_t baseLength = (int32_t)strlen(base);
    const int32_t gameLength = (int32_t)strlen(game);
    const int32_t qpathLength = (int32_t)strlen(qpath);
    const int32_t pathLength = baseLength + gameLength + qpathLength + 2;

    if (pathLength >= MAX_OSPATH) {
        if (quiet != qfalse) {
            osPath[0] = '\0';
            return;
        }
        Com_Error(ERR_FATAL, "\x15"
                             "FS_BuildOSPath: os path length exceeded\n");
    }

    /* NOT_FROM_ORIGINAL_SOURCE: host-facing entry points apply the shared
     * relative-qpath policy before reaching this retained low-level join. */
    memcpy(osPath, base, (size_t)(uint32_t)baseLength);
    osPath[baseLength] = '/';
    memcpy(osPath + baseLength + 1, game, (size_t)(uint32_t)gameLength);
    osPath[baseLength + gameLength + 1] = '/';
    memcpy(osPath + baseLength + gameLength + 2, qpath, (size_t)(uint32_t)(qpathLength + 1));

    FS_ReplaceSeparators(osPath + baseLength);
}

/* Source: CoDUOMP.exe 0x0042cc20..0x0042cc31. Name and argument forwarding:
 * exact same-module Mac symbol FS_BuildOSPath; Linux agrees. */
void FS_BuildOSPath(const char *base, const char *game, const char *qpath, char *osPath)
{
    FS_BuildOSPath_Internal(base, game, qpath, osPath, qfalse);
}

/* Source: CoDUOMP.exe 0x0042cc40..0x0042cca3. Name and traversal rejection:
 * exact same-module Mac symbol FS_CreatePath. Linux performs the same prefix
 * walk; only the target directory-create boundary differs. */
qboolean FS_CreatePath(char *osPath)
{
    if (strstr(osPath, "..") != NULL || strstr(osPath, "::") != NULL) {
        Com_Printf("WARNING: refusing to create relative path \"%s\"\n", osPath);
        return qtrue;
    }

    for (char *cursor = osPath + 1; *cursor != '\0'; ++cursor) {
        if (*cursor == FS_HOST_PATH_SEPARATOR) {
            *cursor = '\0';
            filesystem_compat_mkdir(osPath);
            *cursor = FS_HOST_PATH_SEPARATOR;
        }
    }

    return qfalse;
}
