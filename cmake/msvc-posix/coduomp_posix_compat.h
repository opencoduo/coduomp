/*
 * NOT_FROM_ORIGINAL_SOURCE: MSVC POSIX compatibility layer for the recovered
 * standalone server.
 *
 * The dedicated server reproduces the Linux server and is built for Windows
 * with MinGW, whose C library supplies the POSIX names below. MSVC does not,
 * so this header maps them onto the Microsoft CRT and Win32 APIs. It is
 * force-included only by the coduo_dedicated_recovered target.
 */
#ifndef CODUOMP_MSVC_POSIX_COMPAT_H
#define CODUOMP_MSVC_POSIX_COMPAT_H

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <BaseTsd.h>
#include <direct.h>
#include <errno.h>
#include <fcntl.h>
#include <io.h>
#include <malloc.h>
#include <process.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#ifndef CODUOMP_MSVC_SSIZE_T
#define CODUOMP_MSVC_SSIZE_T
typedef SSIZE_T ssize_t;
#endif

#ifndef CODUOMP_MSVC_MODE_T
#define CODUOMP_MSVC_MODE_T
typedef unsigned short mode_t;
#endif

/* POSIX file descriptors map onto the CRT descriptors. */
#define STDIN_FILENO 0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2

#define read _read
#define write _write
#define close _close
#define dup _dup
#define dup2 _dup2
#define unlink _unlink
#define access _access
#define chdir _chdir
#define getcwd _getcwd
#define rmdir _rmdir
#define isatty _isatty
#define fileno _fileno
#define getpid _getpid
#define chmod _chmod
#define stat _stat
#define fstat _fstat

/*
 * The shared filesystem sources keep the original Linux libc comparison
 * boundary (see the standalone filesystem_services.h), which MinGW satisfies
 * through strings.h. The Microsoft CRT spells the same C-locale folding as
 * _stricmp/_strnicmp. The client compat layer carries the same mapping for the
 * sources it owns.
 */
#define strcasecmp _stricmp
#define strncasecmp _strnicmp

#define S_IFMT _S_IFMT
#define S_IFDIR _S_IFDIR
#define S_IFREG _S_IFREG

/* POSIX permission bits; _S_IREAD/_S_IWRITE use the same bit values. */
#define S_IRUSR 0000400
#define S_IWUSR 0000200
#define S_IXUSR 0000100
#define S_IRGRP 0000040
#define S_IWGRP 0000020
#define S_IXGRP 0000010
#define S_IROTH 0000004
#define S_IWOTH 0000002
#define S_IXOTH 0000001

#define sleep(seconds) Sleep((DWORD)(seconds) * 1000u)
#define usleep(microseconds) Sleep((DWORD)((microseconds) / 1000u))

/*
 * struct timeval comes from Winsock. The server only needs wall-clock
 * milliseconds; both members are long on Win32, so this is Y2038-bounded in
 * the same way as the CRT's time_t.
 */
static __inline int coduomp_msvc_gettimeofday(struct timeval *timeValue,
                                              void *timeZone)
{
    FILETIME fileTime;
    ULARGE_INTEGER ticks;

    (void)timeZone;
    GetSystemTimeAsFileTime(&fileTime);
    ticks.LowPart = fileTime.dwLowDateTime;
    ticks.HighPart = fileTime.dwHighDateTime;
    ticks.QuadPart -= 116444736000000000ULL; /* 1601-01-01 to 1970-01-01 */
    timeValue->tv_sec = (long)(ticks.QuadPart / 10000000ULL);
    timeValue->tv_usec = (long)((ticks.QuadPart % 10000000ULL) / 10ULL);
    return 0;
}

#define gettimeofday(timeValue, timeZone) \
    coduomp_msvc_gettimeofday((timeValue), (timeZone))

/* Directory iteration over the Win32 find API (MinGW provides dirent.h). */
struct dirent {
    char d_name[MAX_PATH];
};

typedef struct coduomp_msvc_dir_s {
    HANDLE handle;
    WIN32_FIND_DATAA data;
    struct dirent entry;
    int firstEntry;
} DIR;

static __inline DIR *coduomp_msvc_opendir(const char *path)
{
    DIR *directory;
    char pattern[MAX_PATH];
    size_t length;

    if (path == NULL) {
        return NULL;
    }
    length = strlen(path);
    if (length + 3u > sizeof(pattern)) {
        return NULL;
    }
    memcpy(pattern, path, length);
    pattern[length] = '\\';
    pattern[length + 1u] = '*';
    pattern[length + 2u] = '\0';

    directory = (DIR *)calloc(1, sizeof(*directory));
    if (directory == NULL) {
        return NULL;
    }
    directory->handle = FindFirstFileA(pattern, &directory->data);
    if (directory->handle == INVALID_HANDLE_VALUE) {
        free(directory);
        return NULL;
    }
    directory->firstEntry = 1;
    return directory;
}

static __inline struct dirent *coduomp_msvc_readdir(DIR *directory)
{
    size_t length;

    if (directory == NULL) {
        return NULL;
    }
    if (directory->firstEntry != 0) {
        directory->firstEntry = 0;
    } else if (FindNextFileA(directory->handle, &directory->data) == 0) {
        return NULL;
    }

    length = strlen(directory->data.cFileName);
    if (length >= sizeof(directory->entry.d_name)) {
        length = sizeof(directory->entry.d_name) - 1u;
    }
    memcpy(directory->entry.d_name, directory->data.cFileName, length);
    directory->entry.d_name[length] = '\0';
    return &directory->entry;
}

static __inline int coduomp_msvc_closedir(DIR *directory)
{
    if (directory == NULL) {
        return -1;
    }
    if (directory->handle != INVALID_HANDLE_VALUE) {
        FindClose(directory->handle);
    }
    free(directory);
    return 0;
}

#define opendir coduomp_msvc_opendir
#define readdir coduomp_msvc_readdir
#define closedir coduomp_msvc_closedir

#endif /* CODUOMP_MSVC_POSIX_COMPAT_H */
