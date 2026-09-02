#include "q_math.h"

#include "compat/coduo_fp_conversion.h"
#include "compat/coduo_x87emu.h"

#include <math.h>
#include <stdint.h>

#if defined(WINDOWS_BEHAVIOR) && defined(LINUX_BEHAVIOR)
#error "Select only one of WINDOWS_BEHAVIOR or LINUX_BEHAVIOR"
#elif !defined(WINDOWS_BEHAVIOR) && !defined(LINUX_BEHAVIOR)
#error "Select WINDOWS_BEHAVIOR or LINUX_BEHAVIOR"
#endif

/*
 * NormalToLatLong is byte-identical within the four Windows targets at
 * CoDUOMP.exe 0x00434aa0, cgame 0x3004cc00, UI 0x40004c10, and game
 * 0x20019c50.  Linux coduo_lnxded 0x0806ac7e and game RVA 0x0003eb22 have the
 * same branches, operation graph, 16-bit truncating conversions, and byte
 * stores.  The sole source-level platform difference is the pi constant:
 * Windows widens binary32 pi to binary64, while Linux loads binary64 pi.
 */
void NormalToLatLong(const vec3_t normal, uint8_t encoded[2])
{
    const double degrees = 180.0;
#if defined(WINDOWS_BEHAVIOR)
    const double pi = 3.1415927410125732;
#else
    const double pi = 3.141592653589793;
#endif
    const double byteScale = 0.7083333134651184;
    if (normal[0] == 0.0f && normal[1] == 0.0f) {
        encoded[1] = 0;
        encoded[0] = normal[2] > 0.0f ? 0 : 128;
        return;
    }

#if EMULATE_X87
    const x87f longitude =
        x87f_mul(x87f_div(x87f_mul(x87f_load_f64(atan2((double)normal[1], (double)normal[0])), x87f_load_f64(degrees)), x87f_load_f64(pi)),
                 x87f_load_f64(byteScale));
    const x87f latitude = x87f_mul(x87f_div(x87f_mul(x87f_load_f64(acos((double)normal[2])), x87f_load_f64(degrees)), x87f_load_f64(pi)),
                                   x87f_load_f64(byteScale));

    /* Finite results are confined to [-127.5, 127.5].  For NaN, the Windows
     * FISTP-qword helper and Linux FISTP-word instruction both leave a zero
     * low byte, as does the emulated dword conversion used here. */
    encoded[1] = (uint8_t)x87f_store_i32_trunc(longitude);
    encoded[0] = (uint8_t)x87f_store_i32_trunc(latitude);
#else
    const long double longitude =
        (long double)atan2((double)normal[1], (double)normal[0]) * (long double)degrees / (long double)pi * (long double)byteScale;
    const long double latitude = (long double)acos((double)normal[2]) * (long double)degrees / (long double)pi * (long double)byteScale;
    encoded[1] = coduo_fp_to_u8_extended(longitude);
    encoded[0] = coduo_fp_to_u8_extended(latitude);
#endif
}
