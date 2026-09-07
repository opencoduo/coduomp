#if defined(__APPLE__) && !defined(_DARWIN_C_SOURCE)
#define _DARWIN_C_SOURCE
#endif
#if !defined(_WIN32) && !defined(_DEFAULT_SOURCE)
#define _DEFAULT_SOURCE
#endif
#if !defined(_WIN32) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif

#include "config_store.h"

#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdatomic.h>

#if defined(_WIN32)
#include <windows.h>
#include <process.h>
#else
#include <dirent.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>
#if defined(__APPLE__)
#include <copyfile.h>
#else
#include <sys/xattr.h>
#endif
#endif

struct coduomp_config_job_s {
    char path[CODUOMP_CONFIG_PATH];
    char *data;
    size_t size;
    coduomp_config_revision_t expected;
    qboolean preserveOriginal, history;
    coduomp_config_result_t result;
#if defined(_WIN32)
    HANDLE thread;
#else
    pthread_t thread;
    pthread_mutex_t mutex;
    pthread_cond_t condition;
    qboolean done;
#endif
};

/* NOT_FROM_ORIGINAL_SOURCE: noncryptographic content identity for accidental
 * damage and external-edit detection. Ordinary editable configs carry no
 * required checksum. Managed snapshots include this FNV-1a-64 digest. */
uint64_t coduomp_config_digest(const void *data, size_t size)
{
    const unsigned char *bytes = data;
    uint64_t hash = UINT64_C(14695981039346656037);
    for (size_t i = 0; i < size; ++i)
        hash = (hash ^ bytes[i]) * UINT64_C(1099511628211);
    return hash;
}

/* NOT_FROM_ORIGINAL_SOURCE: retain the failing platform operation and code. */
static qboolean coduomp_store_error(char error[256], const char *operation)
{
#if defined(_WIN32)
    snprintf(error, 256, "%s (Windows error %lu)", operation, (unsigned long)GetLastError());
#else
    snprintf(error, 256, "%s: %s", operation, strerror(errno));
#endif
    return qfalse;
}

#if defined(_WIN32)
/* NOT_FROM_ORIGINAL_SOURCE: filesystem paths follow the engine's Windows CRT
 * code page; wide APIs preserve those names through replacement operations. */
static qboolean coduomp_store_wide(const char *path, wchar_t wide[CODUOMP_CONFIG_PATH])
{
    return MultiByteToWideChar(CP_ACP, MB_ERR_INVALID_CHARS, path, -1, wide, CODUOMP_CONFIG_PATH) != 0;
}
#endif

/* NOT_FROM_ORIGINAL_SOURCE: canonicalize the existing prefix, reject links
 * below it, and retain one absolute identity even before first creation. */
qboolean coduomp_config_path(const char *input, char output[CODUOMP_CONFIG_PATH], char error[256])
{
    char path[CODUOMP_CONFIG_PATH];
#if defined(_WIN32)
    DWORD fullLength = GetFullPathNameA(input, sizeof(path), path, NULL);
    if (fullLength == 0 || fullLength >= sizeof(path))
        return coduomp_store_error(error, "resolve config path");
    for (char *p = path; *p; ++p) {
        if (*p == '/')
            *p = '\\';
    }
    const size_t first = strlen(path) > 2 && path[1] == ':' ? 3 : 2;
    for (size_t i = first; ; ++i) {
        if (path[i] != '\\' && path[i] != '\0')
            continue;
        char saved = path[i];
        path[i] = '\0';
        wchar_t wide[CODUOMP_CONFIG_PATH];
        if (!coduomp_store_wide(path, wide))
            return coduomp_store_error(error, "decode config path");
        DWORD attributes = GetFileAttributesW(wide);
        DWORD code = GetLastError();
        path[i] = saved;
        if (attributes == INVALID_FILE_ATTRIBUTES && code != ERROR_FILE_NOT_FOUND && code != ERROR_PATH_NOT_FOUND) {
            SetLastError(code);
            return coduomp_store_error(error, "inspect config path");
        }
        if (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_REPARSE_POINT)) {
            snprintf(error, 256, "config paths through reparse points are unsupported");
            return qfalse;
        }
        if (!saved)
            break;
    }
#else
    if (input[0] != '/') {
        if (!getcwd(path, sizeof(path)))
            return coduomp_store_error(error, "resolve current directory");
        size_t length = strlen(path);
        if (snprintf(path + length, sizeof(path) - length, "/%s", input) >= (int)(sizeof(path) - length))
            goto too_long;
    } else {
        if (strlen(input) >= sizeof(path))
            goto too_long;
        strcpy(path, input);
    }
    /* /tmp and /var are system aliases on macOS. Resolve the selected home
     * before calling here; child links are intentionally unsupported. */
    for (size_t i = 1; ; ++i) {
        if (path[i] != '/' && path[i] != '\0')
            continue;
        char saved = path[i];
        path[i] = '\0';
        struct stat st;
        int status = lstat(path, &st);
        path[i] = saved;
        if (status == 0 && S_ISLNK(st.st_mode)) {
            snprintf(error, 256, "config paths through symbolic links are unsupported");
            return qfalse;
        }
        if (status != 0 && errno != ENOENT)
            return coduomp_store_error(error, "inspect config path");
        if (!saved)
            break;
    }
#endif
    strcpy(output, path);
    return qtrue;
#if !defined(_WIN32)
too_long:
    snprintf(error, 256, "config path is too long");
    return qfalse;
#endif
}

/* NOT_FROM_ORIGINAL_SOURCE: distinguish absence from unreadable, incomplete,
 * special, and oversized input. A read owns the exact bytes it fingerprints. */
int coduomp_config_read_disk(const char *path, char **data, size_t *size, coduomp_config_revision_t *revision, char error[256])
{
    *data = NULL;
    *size = 0;
    memset(revision, 0, sizeof(*revision));
#if defined(_WIN32)
    wchar_t wide[CODUOMP_CONFIG_PATH];
    if (!coduomp_store_wide(path, wide))
        goto failed;
    HANDLE file = CreateFileW(wide, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        DWORD code = GetLastError();
        if (code == ERROR_FILE_NOT_FOUND || code == ERROR_PATH_NOT_FOUND)
            return 0;
        goto failed;
    }
    BY_HANDLE_FILE_INFORMATION st;
    if (!GetFileInformationByHandle(file, &st) ||
        (st.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)) || st.nFileSizeHigh ||
        st.nFileSizeLow > CODUOMP_CONFIG_MAX_BYTES + 512) {
        CloseHandle(file);
        snprintf(error, 256, "config is not a supported regular text file");
        return -1;
    }
    *size = st.nFileSizeLow;
    *data = malloc(*size + 1);
    DWORD count = 0;
    BOOL ok = *data != NULL && ReadFile(file, *data, (DWORD)*size, &count, NULL) && count == *size;
    char extra;
    DWORD extraCount = 0;
    ok = ok && ReadFile(file, &extra, 1, &extraCount, NULL) && extraCount == 0;
    if (!CloseHandle(file))
        ok = FALSE;
    if (!ok)
        goto failed;
    revision->device = st.dwVolumeSerialNumber;
    revision->file = ((uint64_t)st.nFileIndexHigh << 32) | st.nFileIndexLow;
    revision->modified = ((int64_t)st.ftLastWriteTime.dwHighDateTime << 32) | st.ftLastWriteTime.dwLowDateTime;
#else
    int file = open(path, O_RDONLY | O_NOFOLLOW | O_NONBLOCK);
    if (file < 0) {
        if (errno == ENOENT)
            return 0;
        goto failed;
    }
    struct stat before, after;
    if (fstat(file, &before) != 0 || !S_ISREG(before.st_mode) || before.st_size < 0 ||
        before.st_size > CODUOMP_CONFIG_MAX_BYTES + 512) {
        close(file);
        snprintf(error, 256, "config is not a supported regular text file");
        return -1;
    }
    *size = (size_t)before.st_size;
    *data = malloc(*size + 1);
    size_t total = 0;
    if (*data != NULL) {
        while (total < *size) {
            ssize_t count = read(file, *data + total, *size - total);
            if (count < 0 && errno == EINTR)
                continue;
            if (count <= 0)
                break;
            total += (size_t)count;
        }
    }
    char extra;
    qboolean ok = *data != NULL && total == *size && read(file, &extra, 1) == 0 && fstat(file, &after) == 0 &&
        before.st_size == after.st_size && before.st_mtime == after.st_mtime;
#if defined(__APPLE__)
    ok = ok && before.st_mtimespec.tv_nsec == after.st_mtimespec.tv_nsec &&
        before.st_ctimespec.tv_sec == after.st_ctimespec.tv_sec && before.st_ctimespec.tv_nsec == after.st_ctimespec.tv_nsec;
#else
    ok = ok && before.st_mtim.tv_nsec == after.st_mtim.tv_nsec &&
        before.st_ctim.tv_sec == after.st_ctim.tv_sec && before.st_ctim.tv_nsec == after.st_ctim.tv_nsec;
#endif
    if (close(file) != 0)
        ok = qfalse;
    if (!ok)
        goto failed;
    revision->device = before.st_dev;
    revision->file = before.st_ino;
#if defined(__APPLE__)
    revision->modified = (int64_t)before.st_mtimespec.tv_sec * INT64_C(1000000000) + before.st_mtimespec.tv_nsec;
#else
    revision->modified = (int64_t)before.st_mtim.tv_sec * INT64_C(1000000000) + before.st_mtim.tv_nsec;
#endif
#endif
    (*data)[*size] = '\0';
    revision->exists = qtrue;
    revision->size = *size;
    revision->digest = coduomp_config_digest(*data, *size);
    return 1;
failed:
    coduomp_store_error(error, "read complete config");
    free(*data);
    *data = NULL;
    return -1;
}

/* NOT_FROM_ORIGINAL_SOURCE: compare both host identity and observed bytes. */
qboolean coduomp_config_same_revision(const coduomp_config_revision_t *a, const coduomp_config_revision_t *b)
{
    return a->exists == b->exists && (!a->exists || (a->size == b->size && a->digest == b->digest &&
        a->device == b->device && a->file == b->file && a->modified == b->modified));
}

/* NOT_FROM_ORIGINAL_SOURCE: completion metadata belongs in recovery files,
 * never in a sidecar checksum used to reject a manually edited primary. */
char *coduomp_config_envelope(const char *data, size_t size, size_t *outputSize)
{
    char header[160], footer[100];
    const uint64_t digest = coduomp_config_digest(data, size);
    int head = snprintf(header, sizeof(header), "// OpenCoDUO snapshot v1 %zu %016" PRIx64 "\n", size, digest);
    int tail = snprintf(footer, sizeof(footer), "\n// OpenCoDUO snapshot end %016" PRIx64 "\n", digest);
    char *output = malloc((size_t)head + size + (size_t)tail + 1);
    if (!output)
        return NULL;
    memcpy(output, header, head);
    memcpy(output + head, data, size);
    memcpy(output + head + size, footer, (size_t)tail + 1);
    *outputSize = (size_t)head + size + (size_t)tail;
    return output;
}

/* NOT_FROM_ORIGINAL_SOURCE: verify the managed snapshot's complete envelope
 * before exposing its payload to the command parser. */
qboolean coduomp_config_read_snapshot(const char *path, char **data, size_t *size, char error[256])
{
    char *file;
    size_t length;
    coduomp_config_revision_t revision;
    if (coduomp_config_read_disk(path, &file, &length, &revision, error) != 1)
        return qfalse;
    size_t payload = 0;
    uint64_t digest = 0;
    int head = 0;
    char footer[100];
    if (sscanf(file, "// OpenCoDUO snapshot v1 %zu %16" SCNx64 "%n", &payload, &digest, &head) != 2 ||
        head <= 0 || file[head++] != '\n' || payload > CODUOMP_CONFIG_MAX_BYTES || (size_t)head + payload > length)
        goto invalid;
    int tail = snprintf(footer, sizeof(footer), "\n// OpenCoDUO snapshot end %016" PRIx64 "\n", digest);
    if ((size_t)head + payload + (size_t)tail != length || memcmp(file + head + payload, footer, (size_t)tail) ||
        coduomp_config_digest(file + head, payload) != digest)
        goto invalid;
    memmove(file, file + head, payload);
    file[payload] = '\0';
    *data = file;
    *size = payload;
    return qtrue;
invalid:
    free(file);
    snprintf(error, 256, "incomplete or damaged managed recovery snapshot");
    return qfalse;
}

/* NOT_FROM_ORIGINAL_SOURCE: exclusive adjacent names retain crash evidence
 * and never reuse a previous transaction's prepared file or rollback copy. */
static qboolean coduomp_store_name(const char *path, const char *kind, char output[CODUOMP_CONFIG_PATH])
{
    static atomic_uint serial;
    unsigned sequence = atomic_fetch_add(&serial, 1);
#if defined(_WIN32)
    unsigned long process = GetCurrentProcessId();
#else
    unsigned long process = (unsigned long)getpid();
#endif
    return snprintf(output, CODUOMP_CONFIG_PATH, "%s.%s-%lld-%lu-%u.cfg", path, kind,
        (long long)time(NULL), process, sequence) < CODUOMP_CONFIG_PATH;
}

#if !defined(_WIN32)
/* NOT_FROM_ORIGINAL_SOURCE: request the strongest supported local flush.
 * Unsupported full-sync is distinguished from an actual storage failure. */
static qboolean coduomp_store_sync(int file, qboolean *weaker)
{
#if defined(__APPLE__)
    if (fcntl(file, F_FULLFSYNC) == 0)
        return qtrue;
    if (errno != EINVAL && errno != ENOTSUP)
        return qfalse;
    *weaker = qtrue;
#else
    (void)weaker;
#endif
    return fsync(file) == 0;
}

/* NOT_FROM_ORIGINAL_SOURCE: retain ownership, permissions, ACLs and extended
 * attributes. Failure to preserve metadata prevents publication. */
static qboolean coduomp_store_metadata(int source, int destination)
{
#if defined(__APPLE__)
    return fcopyfile(source, destination, NULL, COPYFILE_METADATA) == 0;
#else
    struct stat st;
    if (fstat(source, &st) || fchown(destination, st.st_uid, st.st_gid) || fchmod(destination, st.st_mode & 07777))
        return qfalse;
    ssize_t count = flistxattr(source, NULL, 0);
    if (count < 0)
        return errno == ENOTSUP;
    char *names = malloc((size_t)count + 1);
    if (!names)
        return qfalse;
    qboolean ok = flistxattr(source, names, (size_t)count) == count;
    for (ssize_t i = 0; ok && i < count; i += (ssize_t)strlen(names + i) + 1) {
        ssize_t size = fgetxattr(source, names + i, NULL, 0);
        if (size < 0) {
            ok = qfalse;
            break;
        }
        void *value = malloc((size_t)size + 1);
        ok = value && fgetxattr(source, names + i, value, (size_t)size) == size &&
            fsetxattr(destination, names + i, value, (size_t)size, 0) == 0;
        free(value);
    }
    free(names);
    return ok;
#endif
}
#endif

/* NOT_FROM_ORIGINAL_SOURCE: write and flush a complete, exclusively created
 * file. Existing destinations are never opened for truncation. */
static qboolean coduomp_store_prepare(const char *path, const char *data, size_t size, const char *metadataSource, qboolean preserveTimes, qboolean *weaker, char error[256])
{
#if defined(_WIN32)
    (void)metadataSource;
    (void)preserveTimes;
    (void)weaker;
    wchar_t wide[CODUOMP_CONFIG_PATH];
    if (!coduomp_store_wide(path, wide))
        return coduomp_store_error(error, "decode prepared path");
    HANDLE file = CreateFileW(wide, GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE)
        return coduomp_store_error(error, "create adjacent prepared config");
    size_t total = 0;
    qboolean ok = qtrue;
    while (total < size) {
        DWORD written = 0;
        if (!WriteFile(file, data + total, (DWORD)(size - total), &written, NULL) || !written) {
            ok = qfalse;
            break;
        }
        total += written;
    }
    if (!FlushFileBuffers(file))
        ok = qfalse;
    if (!CloseHandle(file))
        ok = qfalse;
#else
    int file = open(path, O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW, 0600);
    if (file < 0)
        return coduomp_store_error(error, "create adjacent prepared config");
    qboolean ok = qtrue;
    size_t total = 0;
    while (total < size) {
        ssize_t written = write(file, data + total, size - total);
        if (written < 0 && errno == EINTR)
            continue;
        if (written <= 0) {
            ok = qfalse;
            break;
        }
        total += (size_t)written;
    }
    if (metadataSource) {
        int source = open(metadataSource, O_RDONLY | O_NOFOLLOW);
        if (source < 0)
            ok = qfalse;
        else {
            if (!coduomp_store_metadata(source, file))
                ok = qfalse;
            if (close(source) != 0)
                ok = qfalse;
        }
    }
    if (!preserveTimes && futimens(file, NULL) != 0)
        ok = qfalse;
    if (!coduomp_store_sync(file, weaker))
        ok = qfalse;
    if (close(file) != 0)
        ok = qfalse;
#endif
    if (!ok)
        return coduomp_store_error(error, "write/flush/close prepared config");
    return qtrue;
}

/* NOT_FROM_ORIGINAL_SOURCE: replacement never falls back to copy or delete. */
static qboolean coduomp_store_move(const char *source, const char *destination)
{
#if defined(_WIN32)
    wchar_t from[CODUOMP_CONFIG_PATH], to[CODUOMP_CONFIG_PATH];
    return coduomp_store_wide(source, from) && coduomp_store_wide(destination, to) &&
        MoveFileExW(from, to, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
#else
    return rename(source, destination) == 0;
#endif
}

/* NOT_FROM_ORIGINAL_SOURCE: remove only a known transaction file after a
 * confirmed commit. Preserved originals and uncertain outcomes are retained. */
static qboolean coduomp_store_remove(const char *path)
{
#if defined(_WIN32)
    wchar_t wide[CODUOMP_CONFIG_PATH];
    return coduomp_store_wide(path, wide) && DeleteFileW(wide);
#else
    return unlink(path) == 0;
#endif
}

/* NOT_FROM_ORIGINAL_SOURCE: rotate recovery generations only after publication
 * has succeeded; an interrupted rotation retains the unique rollback copy. */
static qboolean coduomp_store_history(coduomp_config_job_t *job, const char *previous, size_t previousSize, qboolean *weaker)
{
    size_t size;
    char *snapshot = coduomp_config_envelope(previous, previousSize, &size);
    char prepared[CODUOMP_CONFIG_PATH], from[CODUOMP_CONFIG_PATH], to[CODUOMP_CONFIG_PATH];
    if (!snapshot || !coduomp_store_name(job->path, "pending-backup", prepared)) {
        free(snapshot);
        return qfalse;
    }
    qboolean ok = coduomp_store_prepare(prepared, snapshot, size, NULL, qfalse, weaker, job->result.detail);
    free(snapshot);
    for (int generation = 3; ok && generation >= 2; --generation) {
        if (snprintf(from, sizeof(from), "%s.backup%d.cfg", job->path, generation - 1) >= (int)sizeof(from) ||
            snprintf(to, sizeof(to), "%s.backup%d.cfg", job->path, generation) >= (int)sizeof(to))
            return qfalse;
        char *bytes;
        size_t length;
        coduomp_config_revision_t revision;
        int exists = coduomp_config_read_disk(from, &bytes, &length, &revision, job->result.detail);
        free(bytes);
        if (exists < 0 || (exists && !coduomp_store_move(from, to)))
            ok = qfalse;
    }
    if (snprintf(to, sizeof(to), "%s.backup1.cfg", job->path) >= (int)sizeof(to))
        return qfalse;
    return ok && coduomp_store_move(prepared, to);
}

/* NOT_FROM_ORIGINAL_SOURCE: serialize cooperating processes with an OS-owned
 * lock; preserve old bytes and metadata before replacing a checked revision. */
static void coduomp_store_commit(coduomp_config_job_t *job)
{
    char lockPath[CODUOMP_CONFIG_PATH], temporary[CODUOMP_CONFIG_PATH], checked[CODUOMP_CONFIG_PATH];
    char *previous = NULL, *verify = NULL;
    size_t previousSize = 0, verifySize = 0;
    coduomp_config_revision_t observed, verifyRevision;
    qboolean weaker = qfalse;
    job->result.status = CODUOMP_CONFIG_NOT_COMMITTED;
    if (strlen(job->path) + 100 >= sizeof(lockPath) || !coduomp_config_path(job->path, checked, job->result.detail))
        return;
    if (snprintf(lockPath, sizeof(lockPath), "%s.lock", job->path) >= (int)sizeof(lockPath))
        return;
#if defined(_WIN32)
    wchar_t lockWide[CODUOMP_CONFIG_PATH], pathWide[CODUOMP_CONFIG_PATH], tempWide[CODUOMP_CONFIG_PATH], rollbackWide[CODUOMP_CONFIG_PATH];
    if (!coduomp_store_wide(lockPath, lockWide) || !coduomp_store_wide(job->path, pathWide))
        return;
    HANDLE lock = CreateFileW(lockWide, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_ALWAYS, FILE_FLAG_OPEN_REPARSE_POINT, NULL);
    if (lock == INVALID_HANDLE_VALUE) {
        coduomp_store_error(job->result.detail, "config is locked by another writer");
        return;
    }
    BY_HANDLE_FILE_INFORMATION lockInfo;
    if (!GetFileInformationByHandle(lock, &lockInfo) || (lockInfo.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT))
        goto done;
#else
    int lock = open(lockPath, O_RDWR | O_CREAT | O_NOFOLLOW, 0600);
    if (lock < 0)
        goto lock_failed;
    if (flock(lock, LOCK_EX | LOCK_NB) != 0) {
        close(lock);
lock_failed:
        coduomp_store_error(job->result.detail, "config is locked or its lock is inaccessible");
        return;
    }
    char parent[CODUOMP_CONFIG_PATH];
    strcpy(parent, job->path);
    *strrchr(parent, '/') = '\0';
    int directory = open(parent, O_RDONLY | O_DIRECTORY | O_NOFOLLOW);
    if (directory < 0) {
        close(lock);
        coduomp_store_error(job->result.detail, "open config directory");
        return;
    }
#endif
    if (coduomp_config_read_disk(job->path, &previous, &previousSize, &observed, job->result.detail) < 0)
        goto done;
    if (!coduomp_config_same_revision(&observed, &job->expected)) {
        snprintf(job->result.detail, sizeof(job->result.detail), "file changed outside this session; reload it before saving");
        job->result.status = CODUOMP_CONFIG_RECOVERY_REQUIRED;
        goto done;
    }
    if (observed.exists) {
#if defined(_WIN32)
        if (GetFileAttributesW(pathWide) & FILE_ATTRIBUTE_READONLY) {
            snprintf(job->result.detail, sizeof(job->result.detail), "config is read-only");
            goto done;
        }
        HANDLE writable = CreateFileW(pathWide, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            NULL, OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT, NULL);
        if (writable == INVALID_HANDLE_VALUE) {
            coduomp_store_error(job->result.detail, "config is not writable");
            goto done;
        }
        if (!CloseHandle(writable))
            goto done;
#else
        struct stat st;
        int writable = open(job->path, O_WRONLY | O_NOFOLLOW);
        if (writable < 0) {
            coduomp_store_error(job->result.detail, "config is not writable");
            goto done;
        }
        qboolean writableFile = fstat(writable, &st) == 0 && S_ISREG(st.st_mode) && st.st_nlink == 1 && (st.st_mode & 0222) != 0;
        if (close(writable) != 0 || !writableFile) {
            snprintf(job->result.detail, sizeof(job->result.detail), "config is read-only, hard-linked, or not a regular file");
            goto done;
        }
#endif
    }
    if (!coduomp_store_name(job->path, "pending", temporary) ||
        !coduomp_store_prepare(temporary, job->data, job->size, observed.exists ? job->path : NULL, qfalse, &weaker, job->result.detail))
        goto done;
    if (observed.exists) {
        if (!coduomp_store_name(job->path, job->preserveOriginal ? "original" : "rollback", job->result.rollback))
            goto done;
#if !defined(_WIN32)
        if (!coduomp_store_prepare(job->result.rollback, previous, previousSize, job->path, qtrue, &weaker, job->result.detail) || fsync(directory))
            goto done;
#endif
    }
    if (coduomp_config_read_disk(job->path, &verify, &verifySize, &verifyRevision, job->result.detail) < 0 ||
        !coduomp_config_same_revision(&observed, &verifyRevision) ||
        !coduomp_config_path(job->path, checked, job->result.detail)) {
        snprintf(job->result.detail, sizeof(job->result.detail), "config changed during save; prepared and original files retained");
        job->result.status = CODUOMP_CONFIG_RECOVERY_REQUIRED;
        goto done;
    }
    free(verify);
    verify = NULL;
#if defined(_WIN32)
    if (!coduomp_store_wide(temporary, tempWide) || !coduomp_store_wide(job->result.rollback, rollbackWide))
        goto done;
    BOOL published = observed.exists ? ReplaceFileW(pathWide, tempWide, rollbackWide, 0, NULL, NULL) :
        MoveFileExW(tempWide, pathWide, MOVEFILE_WRITE_THROUGH);
    if (!published) {
        coduomp_store_error(job->result.detail, "replacement failed; inspect the original and retained transaction files");
        job->result.status = CODUOMP_CONFIG_RECOVERY_REQUIRED;
        goto done;
    }
#else
    /* First creation must not overwrite an external file that appeared
     * after our absence check. link publishes this prepared inode atomically. */
    if (observed.exists ? rename(temporary, job->path) != 0 : link(temporary, job->path) != 0) {
        coduomp_store_error(job->result.detail, "publish prepared config");
        goto done;
    }
    if (!observed.exists && !coduomp_store_remove(temporary)) {
        job->result.status = CODUOMP_CONFIG_DURABILITY_UNCONFIRMED;
        coduomp_store_error(job->result.detail, "config published but prepared name could not be retired");
        goto done;
    }
    if (fsync(directory) != 0) {
        coduomp_store_error(job->result.detail, "config replaced but directory synchronization failed");
        job->result.status = CODUOMP_CONFIG_DURABILITY_UNCONFIRMED;
        goto done;
    }
#endif
    job->result.status = CODUOMP_CONFIG_COMMITTED;
    if (coduomp_config_read_disk(job->path, &verify, &verifySize, &job->result.revision, job->result.detail) != 1 ||
        verifySize != job->size || memcmp(verify, job->data, job->size)) {
        job->result.status = CODUOMP_CONFIG_RECOVERY_REQUIRED;
        snprintf(job->result.detail, sizeof(job->result.detail), "published config changed before verification; recovery files retained");
        goto done;
    }
    if (job->history && observed.exists && !job->preserveOriginal && !coduomp_store_history(job, previous, previousSize, &weaker)) {
        snprintf(job->result.detail, sizeof(job->result.detail), "config saved; recovery rotation incomplete, rollback retained");
    } else if (observed.exists && !job->preserveOriginal) {
#if !defined(_WIN32)
        if (fsync(directory) != 0) {
            job->result.status = CODUOMP_CONFIG_DURABILITY_UNCONFIRMED;
            coduomp_store_error(job->result.detail, "recovery metadata synchronization failed; rollback retained");
            goto done;
        }
#endif
        if (coduomp_store_remove(job->result.rollback))
            job->result.rollback[0] = '\0';
    }
#if !defined(_WIN32)
    if (fsync(directory) != 0) {
        job->result.status = CODUOMP_CONFIG_DURABILITY_UNCONFIRMED;
        coduomp_store_error(job->result.detail, "config replaced but recovery metadata synchronization failed");
    }
#endif
    if (weaker && job->result.detail[0] == '\0')
        snprintf(job->result.detail, sizeof(job->result.detail), "saved using fsync; full device synchronization is unsupported");
done:
    free(previous);
    free(verify);
#if defined(_WIN32)
    CloseHandle(lock);
#else
    close(directory);
    close(lock);
#endif
}

/* NOT_FROM_ORIGINAL_SOURCE: the I/O worker receives no live engine objects. */
#if defined(_WIN32)
static unsigned __stdcall coduomp_store_worker(void *argument)
#else
static void *coduomp_store_worker(void *argument)
#endif
{
    coduomp_config_job_t *job = argument;
    coduomp_store_commit(job);
#if defined(_WIN32)
    return 0;
#else
    pthread_mutex_lock(&job->mutex);
    job->done = qtrue;
    pthread_cond_signal(&job->condition);
    pthread_mutex_unlock(&job->mutex);
    return NULL;
#endif
}

/* NOT_FROM_ORIGINAL_SOURCE: capture all transaction inputs before dispatch. */
coduomp_config_job_t *coduomp_config_store_start(const char *path, const char *data, size_t size, const coduomp_config_revision_t *expected, qboolean preserveOriginal, qboolean recoveryHistory)
{
    if (strlen(path) + 100 >= CODUOMP_CONFIG_PATH)
        return NULL;
    coduomp_config_job_t *job = calloc(1, sizeof(*job));
    if (!job)
        return NULL;
    job->data = malloc(size + 1);
    if (!job->data) {
        free(job);
        return NULL;
    }
    strcpy(job->path, path);
    memcpy(job->data, data, size);
    job->data[size] = '\0';
    job->size = size;
    job->expected = *expected;
    job->preserveOriginal = preserveOriginal;
    job->history = recoveryHistory;
#if defined(_WIN32)
    job->thread = (HANDLE)_beginthreadex(NULL, 0, coduomp_store_worker, job, 0, NULL);
    if (job->thread)
        return job;
#else
    if (pthread_mutex_init(&job->mutex, NULL) == 0) {
        if (pthread_cond_init(&job->condition, NULL) == 0) {
            if (pthread_create(&job->thread, NULL, coduomp_store_worker, job) == 0)
                return job;
            pthread_cond_destroy(&job->condition);
        }
        pthread_mutex_destroy(&job->mutex);
    }
#endif
    free(job->data);
    free(job);
    return NULL;
}

/* NOT_FROM_ORIGINAL_SOURCE: bounded main-thread waits keep ownership with the
 * worker after timeout. Polling later retrieves and frees a completed job. */
qboolean coduomp_config_store_finish(coduomp_config_job_t *job, unsigned waitMilliseconds, coduomp_config_result_t *result)
{
#if defined(_WIN32)
    if (WaitForSingleObject(job->thread, waitMilliseconds) != WAIT_OBJECT_0)
        return qfalse;
    CloseHandle(job->thread);
#else
    struct timespec deadline;
    clock_gettime(CLOCK_REALTIME, &deadline);
    deadline.tv_sec += waitMilliseconds / 1000;
    deadline.tv_nsec += (long)(waitMilliseconds % 1000) * 1000000;
    if (deadline.tv_nsec >= 1000000000) {
        ++deadline.tv_sec;
        deadline.tv_nsec -= 1000000000;
    }
    pthread_mutex_lock(&job->mutex);
    while (!job->done && waitMilliseconds) {
        if (pthread_cond_timedwait(&job->condition, &job->mutex, &deadline) != 0)
            break;
    }
    qboolean done = job->done;
    pthread_mutex_unlock(&job->mutex);
    if (!done)
        return qfalse;
    pthread_join(job->thread, NULL);
    pthread_cond_destroy(&job->condition);
    pthread_mutex_destroy(&job->mutex);
#endif
    *result = job->result;
    free(job->data);
    free(job);
    return qtrue;
}

/* NOT_FROM_ORIGINAL_SOURCE: any managed or interrupted transaction evidence
 * prevents a missing config from being treated as a fresh installation. */
qboolean coduomp_config_recovery_exists(const char *path)
{
    char pattern[CODUOMP_CONFIG_PATH];
    if (strlen(path) + 4 >= sizeof(pattern))
        return qtrue;
#if defined(_WIN32)
    snprintf(pattern, sizeof(pattern), "%s.*.cfg", path);
    wchar_t wide[CODUOMP_CONFIG_PATH];
    WIN32_FIND_DATAW entry;
    if (!coduomp_store_wide(pattern, wide))
        return qtrue;
    HANDLE search = FindFirstFileW(wide, &entry);
    if (search == INVALID_HANDLE_VALUE)
        return GetLastError() != ERROR_FILE_NOT_FOUND && GetLastError() != ERROR_PATH_NOT_FOUND;
    FindClose(search);
    return qtrue;
#else
    strcpy(pattern, path);
    char *name = strrchr(pattern, '/');
    if (!name)
        return qtrue;
    *name++ = '\0';
    DIR *directory = opendir(pattern);
    if (!directory)
        return errno != ENOENT;
    qboolean found = qfalse;
    const size_t length = strlen(name);
    struct dirent *entry;
    while ((entry = readdir(directory)) != NULL) {
        size_t entryLength = strlen(entry->d_name);
        if (entryLength > length + 4 && !strncmp(entry->d_name, name, length) && entry->d_name[length] == '.' &&
            !strcmp(entry->d_name + entryLength - 4, ".cfg")) {
            found = qtrue;
            break;
        }
    }
    closedir(directory);
    return found;
#endif
}
