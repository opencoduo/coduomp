#include "hardware_profile.h"

#if defined(__APPLE__)
#include <sys/types.h>
#include <sys/sysctl.h>
#endif

/* NOT_FROM_ORIGINAL_SOURCE: isolated improved hardware-policy interface. The
 * stock source does not reference it. It identifies the host CPU family instead of treating
 * every macOS renderer as equivalent.  The runtime sysctl also recognizes an
 * Apple-Silicon host when an x86_64 build is running through Rosetta. */
qboolean coduomp_is_apple_silicon(void)
{
#if defined(__APPLE__)
    int arm64Capable = 0;
    size_t valueSize = sizeof(arm64Capable);

    if (sysctlbyname("hw.optional.arm64", &arm64Capable, &valueSize,
                     NULL, 0) == 0) {
        return arm64Capable != 0 ? qtrue : qfalse;
    }
#if defined(__aarch64__) || defined(__arm64__)
    return qtrue;
#endif
#endif
    return qfalse;
}
