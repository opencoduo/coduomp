#include "com_sprintf.h"

#include <stdarg.h>
#include <stdio.h>

#if defined(WINDOWS_BEHAVIOR) && defined(LINUX_BEHAVIOR)
#error "Select only one of WINDOWS_BEHAVIOR or LINUX_BEHAVIOR"
#elif !defined(WINDOWS_BEHAVIOR) && !defined(LINUX_BEHAVIOR)
#error "Select WINDOWS_BEHAVIOR or LINUX_BEHAVIOR"
#endif

/* Source: CoDUOMP.exe 0x0044fa30..0x0044fa49.
 * Source: uo_cgame_mp_x86.dll 0x3004e820..0x3004e839.
 * Source: uo_ui_mp_x86.dll 0x40006850..0x40006869.
 * Source: uo_game_mp_x86.dll 0x20058040..0x20058059.
 * Source: coduo_lnxded 0x08086c8e..0x08086ccb.
 * Source: game.mp.uo.i386.so 0x000937e5..0x00093832.
 * Name: exact same-module Mac client/game symbol Com_sprintf.
 *
 * The four Windows wrapper bodies are instruction-identical after their call
 * displacements are normalized. Their embedded legacy _vsnprintf bodies are
 * likewise instruction-identical. Both Linux bodies call vsnprintf@plt and
 * then perform the same forced final-byte store and return forwarding. */
int32_t Com_sprintf(char *destination, size_t destinationSize,
                    const char *format, ...)
{
    int32_t result;
    va_list arguments;

    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if (destinationSize == 0) {
        return -1;
    }

    va_start(arguments, format);
#if defined(WINDOWS_BEHAVIOR) && defined(_WIN32)
    result = _vsnprintf(destination, destinationSize, format, arguments);
#else
    result = vsnprintf(destination, destinationSize, format, arguments);
#endif
    va_end(arguments);

#if defined(WINDOWS_BEHAVIOR) && !defined(_WIN32)
    /* ORIGINAL_PLATFORM_DIFFERENCE: the embedded Windows formatter returns
     * the size for an exact fit and -1 only when output exceeds the size.
     * C99 returns the required length, so supporting non-Windows client builds
     * reproduce the Windows return without changing the final buffer bytes. */
    if (result >= 0 && (size_t)result > destinationSize) {
        result = -1;
    }
#endif

    destination[destinationSize - 1] = '\0';
    return result;
}
