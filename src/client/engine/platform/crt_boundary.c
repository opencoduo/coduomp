#if !defined(_WIN32)
#define _POSIX_C_SOURCE 200809L
#endif

#include "crt_boundary.h"

#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <direct.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

/* NOT_FROM_ORIGINAL_SOURCE: portable linkage boundary for the statically
 * linked MSVC _mkdir entry at CoDUOMP.exe 0x0056ec9b. POSIX mkdir needs an
 * explicit mode; 0777 retains the original engine convention and lets the
 * process umask apply the user's platform policy. */
int coduomp_crt_mkdir(const char *path)
{
#if defined(_WIN32)
    return _mkdir(path);
#else
    return mkdir(path, 0777);
#endif
}

/* NOT_FROM_ORIGINAL_SOURCE: portable linkage boundary for the statically
 * linked MSVC _getcwd entry and helpers at CoDUOMP.exe
 * 0x0056ecc7..0x0056ee2e. */
char *coduomp_crt_getcwd(char *buffer, size_t size)
{
#if defined(_WIN32)
    return _getcwd(buffer, (int)size);
#else
    return getcwd(buffer, size);
#endif
}

/* NOT_FROM_ORIGINAL_SOURCE: ownership adapter for the statically linked MSVC
 * _putenv entry at CoDUOMP.exe 0x0057f691. MSVC copies the assignment, while
 * POSIX putenv retains the caller's pointer; CL_Setenv_f supplies stack
 * storage, so use setenv/unsetenv on POSIX to preserve the original lifetime
 * contract. */
int coduomp_crt_putenv_copy(const char *assignment)
{
#if defined(_WIN32)
    return _putenv(assignment);
#else
    const char *const separator = strchr(assignment, '=');
    if (separator == NULL || separator == assignment)
        return -1;

    const size_t nameLength = (size_t)(separator - assignment);
    char *const name = malloc(nameLength + 1);
    if (name == NULL)
        return -1;

    memcpy(name, assignment, nameLength);
    name[nameLength] = '\0';

    const int result =
        separator[1] != '\0'
            ? setenv(name, separator + 1, 1)
            : unsetenv(name);
    free(name);
    return result;
#endif
}
