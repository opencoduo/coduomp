#include <stdlib.h>
#if !defined(_WIN32)
#include <pwd.h>
#include <unistd.h>
#endif

#include "core_runtime_private.h"

#define SYS_CONFIGURE_PLATFORM_STUB_RESULT ((int32_t)0)
#define SYS_DEFAULT_USERNAME "player"

void Sys_LoadingKeepAlive(void)
{
}

qboolean Sys_InfoChanged(void)
{
    return SYS_CONFIGURE_PLATFORM_STUB_RESULT;
}

qboolean
Sys_ConfigureChecksumChanged(int32_t checksum)
{
    (void)checksum;
    return SYS_CONFIGURE_PLATFORM_STUB_RESULT;
}

const char *Sys_GetCurrentUser(void)
{
#if defined(_WIN32)
    const char *username = getenv("USERNAME");

    if (username == NULL || username[0] == '\0') {
        return SYS_DEFAULT_USERNAME;
    }

    return username;
#else
    struct passwd *entry;

    entry = getpwuid(getuid());
    if (entry == NULL) {
        return SYS_DEFAULT_USERNAME;
    }

    return entry->pw_name;
#endif
}
