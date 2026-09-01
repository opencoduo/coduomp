#include "filesystem.h"
#include "filesystem_path_security.h"
#include "filesystem_services.h"

#include "qcommon/q_string.h"

#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#if defined(_WIN32)
#include <windows.h>
#endif

enum {
    FS_SAVE_PREFIX_LENGTH = 4
};

/* Source: CoDUOMP.exe 0x0042ccb0..0x0042cda7. The binary's own diagnostics
 * identify this low-level host-path helper as FS_Copyfiles. The Linux body
 * agrees on journal exclusions, allocation, transfer sizes, error behavior,
 * destination creation, and ownership. */
void FS_Copyfiles(const char *sourceOSPath, char *destOSPath)
{
    if (strstr(sourceOSPath, "journal.dat") != NULL ||
        strstr(sourceOSPath, "journaldata.dat") != NULL) {
        return;
    }

    FILE *file = fopen(sourceOSPath, "rb");
    if (file == NULL)
        return;

    (void)fseek(file, 0, SEEK_END);
    const long hostFileLength = ftell(file);
    if (hostFileLength < 0 || hostFileLength > INT32_MAX) {
        /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
        (void)fclose(file);
        Com_Error(ERR_FATAL, "\x15" "Invalid file length in FS_Copyfiles()\n");
        return;
    }
    const int32_t fileLength = (int32_t)hostFileLength;
    const size_t transferSize = (size_t)fileLength;
    (void)fseek(file, 0, SEEK_SET);

    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    void *buffer = NULL;
    if (transferSize != 0) {
        buffer = malloc(transferSize);
        if (buffer == NULL) {
            (void)fclose(file);
            Com_Error(ERR_FATAL, "\x15" "Out of memory in FS_Copyfiles()\n");
            return;
        }
    }
    if (transferSize != 0 &&
        fread(buffer, 1, transferSize, file) != transferSize) {
        Com_Error(ERR_FATAL, "\x15" "Short read in FS_Copyfiles()\n");
    }
    (void)fclose(file);

    if (FS_CreatePath(destOSPath) != qfalse) {
        free(buffer);
        return;
    }

    file = fopen(destOSPath, "wb");
    if (file == NULL) {
        free(buffer);
        return;
    }

    if (transferSize != 0 &&
        fwrite(buffer, 1, transferSize, file) != transferSize) {
        Com_Error(ERR_FATAL, "\x15" "Short write in FS_Copyfiles()\n");
    }

    (void)fclose(file);
    free(buffer);
}

/* Source: CoDUOMP.exe 0x0042cdb0..0x0042cf4a. Name: exact same-module Mac
 * symbol FS_CopyFile. Both original bodies perform the same low-level copy;
 * compiler factoring differs, so the maintained common body calls the proven
 * FS_Copyfiles operation. */
void FS_CopyFile(const char *sourceQPath, const char *destQPath)
{
    /* NOT_FROM_ORIGINAL_SOURCE: both qpaths must remain below the selected
     * game root. */
    if (coduo_compat_path_is_safe_relative(sourceQPath) == qfalse ||
        coduo_compat_path_is_safe_relative(destQPath) == qfalse) {
        Com_Printf("WARNING: refusing unsafe copy path\n");
        return;
    }

    char sourceOSPath[MAX_OSPATH];
    char destOSPath[MAX_OSPATH];
    const char *const stateRoot =
        filesystem_compat_state_root(fs_homepath->string);

    /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
    FS_BuildOSPath(stateRoot, fs_currentGameDir,
                   sourceQPath, sourceOSPath);
    FS_BuildOSPath(stateRoot, fs_currentGameDir,
                   destQPath, destOSPath);

    char resolvedPath[MAX_OSPATH];
    if (filesystem_compat_resolve_case_path(
            stateRoot, sourceOSPath,
            resolvedPath, sizeof(resolvedPath)) != qfalse) {
        Q_strncpyz(sourceOSPath, resolvedPath, sizeof(sourceOSPath));
    }

    if (strstr(sourceOSPath, "journal.dat") != NULL ||
        strstr(sourceOSPath, "journaldata.dat") != NULL) {
        Com_Printf("Ignoring journal files\n");
        return;
    }

    FS_Copyfiles(sourceOSPath, destOSPath);
}

/* Source: CoDUOMP.exe 0x0042cf50..0x0042cf57. Name and direct CRT operation:
 * exact same-module Mac symbol FS_Remove; Linux agrees. */
void FS_Remove(const char *osPath)
{
    (void)remove(osPath);
}

/* Source: CoDUOMP.exe 0x0042cf60..0x0042cfdf. Name and root-path behavior:
 * exact same-module Mac symbol FS_FileExists; Linux agrees. */
qboolean FS_FileExists(const char *qpath)
{
    if (coduo_compat_path_is_safe_relative(qpath) == qfalse)
        return qfalse;


    char osPath[MAX_OSPATH];
    const char *const stateRoot =
        filesystem_compat_state_root(fs_homepath->string);
    FS_BuildOSPath(stateRoot, fs_currentGameDir, qpath, osPath);
    FILE *const file = filesystem_compat_fopen_read(
        stateRoot, osPath);
    if (file == NULL)
        return qfalse;

    (void)fclose(file);
    return qtrue;
}

/* Source: CoDUOMP.exe 0x0043f3e0..0x0043f46e. Name and root-path behavior:
 * exact same-module Mac symbol FS_SV_FileExists. Both original bodies remove
 * the empty leaf's trailing separator before opening the host path. */
qboolean FS_SV_FileExists(const char *qpath)
{
    return coduomp_fs_root_file_exists(fs_homepath->string, qpath);
}

/* NOT_FROM_ORIGINAL_SOURCE: explicit-root form used by the client namespace
 * while FS_SV_FileExists retains the original fs_homepath root. */
qboolean coduomp_fs_root_file_exists(const char *root, const char *qpath)
{
    if (root == NULL || root[0] == '\0' ||
        coduo_compat_path_is_safe_relative(qpath) == qfalse) {
        return qfalse;
    }

    char osPath[MAX_OSPATH];
    FS_BuildOSPath(root, qpath, "", osPath);
    osPath[strlen(osPath) - 1] = '\0';

    FILE *const file = filesystem_compat_fopen_read(
        root, osPath);
    if (file == NULL)
        return qfalse;

    (void)fclose(file);
    return qtrue;
}

/* NOT_FROM_ORIGINAL_SOURCE: explicit-root removal used by the client
 * namespace. Case recovery preserves the existing native-client behavior. */
void coduomp_fs_root_remove(const char *root, const char *qpath)
{
    if (root == NULL || root[0] == '\0' ||
        coduo_compat_path_is_safe_relative(qpath) == qfalse) {
        return;
    }

    char osPath[MAX_OSPATH];
    FS_BuildOSPath(root, qpath, "", osPath);
    osPath[strlen(osPath) - 1] = '\0';

    char resolvedPath[MAX_OSPATH];
    if (filesystem_compat_resolve_case_path(
            root, osPath, resolvedPath, sizeof(resolvedPath)) != qfalse) {
        Q_strncpyz(osPath, resolvedPath, sizeof(osPath));
    }
    FS_Remove(osPath);
}

/* Source: CoDUOMP.exe 0x0043f7d0..0x0043f8c3. Name and path-root behavior:
 * exact same-module Mac symbol FS_SV_Rename. Linux agrees on the rename then
 * copy/remove fallback. */
void FS_SV_Rename(const char *sourceQPath, const char *destQPath)
{
    coduomp_fs_root_rename(
        fs_homepath->string, sourceQPath, destQPath);
}

/* NOT_FROM_ORIGINAL_SOURCE: explicit-root form used by the client namespace
 * while FS_SV_Rename retains the original fs_homepath root. */
void coduomp_fs_root_rename(const char *root,
                            const char *sourceQPath,
                            const char *destQPath)
{
    filesystem_compat_check_started();

    if (root == NULL || root[0] == '\0' ||
        coduo_compat_path_is_safe_relative(sourceQPath) == qfalse ||
        coduo_compat_path_is_safe_relative(destQPath) == qfalse) {
        Com_Printf("WARNING: refusing unsafe rename path\n");
        return;
    }

    char sourceOSPath[MAX_OSPATH];
    char destOSPath[MAX_OSPATH];
    FS_BuildOSPath(root, sourceQPath, "", sourceOSPath);
    FS_BuildOSPath(root, destQPath, "", destOSPath);
    sourceOSPath[strlen(sourceOSPath) - 1] = '\0';
    destOSPath[strlen(destOSPath) - 1] = '\0';

    char resolvedPath[MAX_OSPATH];
    if (filesystem_compat_resolve_case_path(
            root, sourceOSPath,
            resolvedPath, sizeof(resolvedPath)) != qfalse) {
        Q_strncpyz(sourceOSPath, resolvedPath, sizeof(sourceOSPath));
    }
    if (filesystem_compat_resolve_case_path(
            root, destOSPath,
            resolvedPath, sizeof(resolvedPath)) != qfalse) {
        Q_strncpyz(destOSPath, resolvedPath, sizeof(destOSPath));
    }

    if (fs_debug->integer != 0) {
        Com_Printf("FS_SV_Rename: %s --> %s\n",
                   sourceOSPath, destOSPath);
    }

    if (rename(sourceOSPath, destOSPath) != 0) {
        FS_Copyfiles(sourceOSPath, destOSPath);
        FS_Remove(sourceOSPath);
    }
    filesystem_compat_host_paths_changed();
}

/* Source: CoDUOMP.exe 0x0042cfe0..0x0042d0c5. Name and fallback sequence:
 * exact same-module Mac symbol FS_Rename. Linux agrees on rename, remove
 * destination, retry rename, then copy/remove. */
void FS_Rename(const char *sourceQPath, const char *destQPath)
{
    filesystem_compat_check_started();

    if (coduo_compat_path_is_safe_relative(sourceQPath) == qfalse ||
        coduo_compat_path_is_safe_relative(destQPath) == qfalse) {
        Com_Printf("WARNING: refusing unsafe rename path\n");
        return;
    }

    char sourceOSPath[MAX_OSPATH];
    char destOSPath[MAX_OSPATH];
    const char *const stateRoot =
        filesystem_compat_state_root(fs_homepath->string);
    FS_BuildOSPath(stateRoot, fs_currentGameDir,
                   sourceQPath, sourceOSPath);
    FS_BuildOSPath(stateRoot, fs_currentGameDir,
                   destQPath, destOSPath);

    char resolvedPath[MAX_OSPATH];
    if (filesystem_compat_resolve_case_path(
            stateRoot, sourceOSPath,
            resolvedPath, sizeof(resolvedPath)) != qfalse) {
        Q_strncpyz(sourceOSPath, resolvedPath, sizeof(sourceOSPath));
    }
    if (filesystem_compat_resolve_case_path(
            stateRoot, destOSPath,
            resolvedPath, sizeof(resolvedPath)) != qfalse) {
        Q_strncpyz(destOSPath, resolvedPath, sizeof(destOSPath));
    }

    if (fs_debug->integer != 0) {
        Com_Printf("FS_Rename: %s --> %s\n",
                   sourceOSPath, destOSPath);
    }

    if (rename(sourceOSPath, destOSPath) == 0) {
        filesystem_compat_host_paths_changed();
        return;
    }

    FS_Remove(destOSPath);
    if (rename(sourceOSPath, destOSPath) == 0) {
        filesystem_compat_host_paths_changed();
        return;
    }

    FS_Copyfiles(sourceOSPath, destOSPath);
    FS_Remove(sourceOSPath);
    filesystem_compat_host_paths_changed();
}

/* Source: CoDUOMP.exe 0x0042e050..0x0042e12b. Name and restricted save-path
 * role: exact same-module Mac symbol FS_Delete. Linux agrees on the four-byte
 * case-sensitive prefix gate and following separator test. */
qboolean FS_Delete(const char *qpath)
{
    filesystem_compat_check_started();

    if (qpath == NULL || qpath[0] == '\0')
        return qfalse;
    if (strncmp(qpath, "save", FS_SAVE_PREFIX_LENGTH) != 0)
        return qfalse;
    if (qpath[FS_SAVE_PREFIX_LENGTH] != '/' &&
        qpath[FS_SAVE_PREFIX_LENGTH] != '\\') {
        return qfalse;
    }

    /* NOT_FROM_ORIGINAL_SOURCE: a save path must remain relative and below the
     * selected game root before host-path construction. */
    if (coduo_compat_path_is_safe_relative(qpath) == qfalse) {
        Com_Printf("WARNING: refusing unsafe delete path\n");
        return qfalse;
    }

    char osPath[MAX_OSPATH];
    FS_BuildOSPath(
        filesystem_compat_state_root(fs_homepath->string),
        fs_currentGameDir, qpath, osPath);
    return remove(osPath) != -1 ? qtrue : qfalse;
}

#if defined(WINDOWS_BEHAVIOR)
/* Source: CoDUOMP.exe 0x0042e130..0x0042e1bb.
 * Evidence: repaired executable-gap boundary and exact same-module Mac symbol
 * FS_MakeReadOnly. Win32 toggles FILE_ATTRIBUTE_READONLY and deliberately
 * returns false when the bit already has the requested value; when it differs,
 * it ignores SetFileAttributesA's result and returns true.
 * The POSIX branch maps that semantic flag to the owner's write permission;
 * it is a platform replacement, not an inferred second original path. */
qboolean FS_MakeReadOnly(const char *qpath, qboolean makeReadOnly)
{
    char osPath[MAX_OSPATH];
    FS_BuildOSPath(
        filesystem_compat_state_root(fs_homepath->string),
        fs_currentGameDir, qpath, osPath);

#if defined(_WIN32)
    DWORD attributes = GetFileAttributesA(osPath);
    DWORD updatedAttributes = attributes;
    if (makeReadOnly == qfalse)
        updatedAttributes &= ~FILE_ATTRIBUTE_READONLY;
    else
        updatedAttributes |= FILE_ATTRIBUTE_READONLY;
    if (updatedAttributes == attributes)
        return qfalse;
    (void)SetFileAttributesA(osPath, updatedAttributes);
#else
    struct stat status;
    if (stat(osPath, &status) == -1)
        return qfalse;

    const mode_t originalMode = status.st_mode;
    if (makeReadOnly == qfalse)
        status.st_mode |= S_IWUSR;
    else
        status.st_mode &= ~S_IWUSR;
    if (status.st_mode == originalMode)
        return qfalse;
    (void)chmod(osPath, status.st_mode);
#endif

    return qtrue;
}
#else
/* coduo_lnxded 0x08062782..0x08062852 uses the owner-read bit rather than
 * the owner-write bit.  This is a retained original Linux-port discrepancy. */
qboolean FS_MakeReadOnly(const char *qpath, qboolean makeReadOnly)
{
    struct stat fileStat;
    mode_t mode;
    char osPath[MAX_OSPATH];

    filesystem_compat_check_started();
    FS_BuildOSPath(filesystem_compat_state_root(fs_homepath->string),
                   fs_currentGameDir, qpath, osPath);

    if (Sys_Stat(osPath, &fileStat) == -1) {
        return qfalse;
    }

    if (makeReadOnly == qfalse) {
        mode = fileStat.st_mode | S_IRUSR;
    } else {
        mode = fileStat.st_mode & ~S_IRUSR;
    }

    fileStat.st_mode = mode;
    if (chmod(osPath, fileStat.st_mode) == -1) {
        return qfalse;
    }

    return qtrue;
}
#endif
