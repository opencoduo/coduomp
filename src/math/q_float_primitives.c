#include "q_math.h"

#include "compat/coduo_native_x87.h"

#include <math.h>
#include <stdint.h>
#include <string.h>

#if defined(WINDOWS_BEHAVIOR) && defined(LINUX_BEHAVIOR)
#error "Select only one of WINDOWS_BEHAVIOR or LINUX_BEHAVIOR"
#elif !defined(WINDOWS_BEHAVIOR) && !defined(LINUX_BEHAVIOR)
#error "Select WINDOWS_BEHAVIOR or LINUX_BEHAVIOR"
#endif

/*
 * The retained Windows Q_fabs bodies are instruction-identical apart from
 * their image addresses: CoDUOMP.exe 0x00425d50, uo_cgame_mp_x86.dll
 * 0x30008230, and uo_ui_mp_x86.dll 0x400060d0.  Clearing the binary32 sign bit
 * preserves every other bit, including a NaN payload.
 */
float Q_fabs(float value)
{
    uint32_t bits;

    memcpy(&bits, &value, sizeof(bits));
    bits &= UINT32_C(0x7fffffff);
    memcpy(&value, &bits, sizeof(value));
    return value;
}

/*
 * All four Windows BG_SinCos bodies are instruction-identical:
 *
 *   CoDUOMP.exe             0x00431060
 *   uo_cgame_mp_x86.dll    0x30001190
 *   uo_ui_mp_x86.dll       0x40001190
 *   uo_game_mp_x86.dll     0x20001190
 *
 * The Linux engine and game helpers at 0x0806b4de and RVA 0x0003f464 retain
 * the same one-FSINCOS operation and cosine-then-sine binary32 stores.
 */
void BG_SinCos(float angle, float *sinOut, float *cosOut)
{
    coduo_x87_sincosf(angle, sinOut, cosOut);
}

/*
 * The corresponding Windows binary64 bodies also agree instruction for
 * instruction: CoDUOMP.exe 0x00431080, cgame 0x300491e0, UI 0x400011b0, and
 * game 0x20016230.  Linux instead calls sin and cos separately, in that order,
 * at coduo_lnxded 0x0806b4f6 and game RVA 0x0003f47c.  Keep the complete
 * platform bodies because one FSINCOS reduction is observably different from
 * two libm calls for exceptional and very large inputs.
 */
#if defined(WINDOWS_BEHAVIOR)
void CG_SinCos(double angle, double *sinOut, double *cosOut)
{
    coduo_x87_sincos(angle, sinOut, cosOut);
}
#else
void CG_SinCos(double angle, double *sinOut, double *cosOut)
{
    *sinOut = sin(angle);
    *cosOut = cos(angle);
}
#endif
