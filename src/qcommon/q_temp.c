#include "q_string.h"

#include "compat/coduo_int32_bits.h"
#include "q_temp_error_binding.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#if defined(WINDOWS_BEHAVIOR) && defined(LINUX_BEHAVIOR)
#error "Select only one of WINDOWS_BEHAVIOR or LINUX_BEHAVIOR"
#elif !defined(WINDOWS_BEHAVIOR) && !defined(LINUX_BEHAVIOR)
#error "Select WINDOWS_BEHAVIOR or LINUX_BEHAVIOR"
#endif

enum {
    Q_VA_LAST_INDEX = MAX_VA_STRING - 1,
    Q_TEMP_VECTOR_COUNT = 8,
    Q_TEMP_VECTOR_INDEX_MASK = Q_TEMP_VECTOR_COUNT - 1
};

/*
 * Complete temporary formatter/vector subsystem.  The four Windows va bodies
 * are instruction-identical apart from storage, formatter, error, and call
 * relocations; the Windows game error edge alone calls G_Error rather than
 * Com_Error.  The Linux bodies retain the same capacities, wrap boundary,
 * copy length, and returned pointer.  tv uses the same eight 12-byte slots and
 * masked cursor in every binary.
 *
 *                              va            tv
 *   CoDUOMP.exe                0x0044fab0    0x0044fb40
 *   uo_cgame_mp_x86.dll       0x3004e8a0    0x3004e930
 *   uo_ui_mp_x86.dll          0x400068d0    0x40006960
 *   uo_game_mp_x86.dll        0x200580b0    0x20058140
 *   coduo_lnxded              0x08086d78    0x08086e2b
 *   game.mp.uo.i386.so        0x0004b96e    0x0004ba2f
 *
 * The cgame controller body at 0x30020bf8..0x30020c29 contains an inlined
 * tv(2.0f, 0.0f, 0.0f) using this same ring; the former vehicle-private names
 * for that storage were therefore rejected.
 */

char q_vaStringBuffer[MAX_VA_STRING];
char q_vaTempBuffer[MAX_VA_STRING];
int32_t q_vaStringOffset;

vec3_t q_tempVectors[Q_TEMP_VECTOR_COUNT];
int32_t q_tempVectorIndex;

char *va(const char *format, ...)
{
    va_list arguments;
    int32_t length;
    int32_t offset;
    char *result;

    va_start(arguments, format);
#if defined(WINDOWS_BEHAVIOR) && defined(_WIN32)
    length = _vsnprintf(q_vaTempBuffer, sizeof(q_vaTempBuffer), format, arguments);
#else
    length = vsnprintf(q_vaTempBuffer, sizeof(q_vaTempBuffer), format, arguments);
#endif
    va_end(arguments);

#if defined(WINDOWS_BEHAVIOR) && !defined(_WIN32)
    /* The retained Windows formatter returns -1 when the complete result does
     * not fit.  The distinction is observable only at the common fatal edge. */
    if (length >= MAX_VA_STRING) {
        length = -1;
    }
#endif

    q_vaTempBuffer[Q_VA_LAST_INDEX] = '\0';
    if (length < 0 || length >= MAX_VA_STRING) {
        Q_TEMP_ERROR("\x15"
                     "Attempted to overrun string in call to va()\n");
    }

    offset = q_vaStringOffset;
    if (coduo_int32_from_bits((uint32_t)offset + (uint32_t)length) >= Q_VA_LAST_INDEX) {
        offset = 0;
    }

    result = &q_vaStringBuffer[offset];
    memcpy(result, q_vaTempBuffer, (size_t)((uint32_t)length + 1u));
    q_vaStringOffset = coduo_int32_from_bits((uint32_t)offset + (uint32_t)length + 1u);
    return result;
}

float *tv(float x, float y, float z)
{
    vec3_t *result = &q_tempVectors[q_tempVectorIndex];

    q_tempVectorIndex = coduo_int32_from_bits(((uint32_t)q_tempVectorIndex + 1u) & Q_TEMP_VECTOR_INDEX_MASK);
    (*result)[0] = x;
    (*result)[1] = y;
    (*result)[2] = z;
    return *result;
}
