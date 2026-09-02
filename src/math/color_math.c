#include "q_math.h"

#include "compat/coduo_fp_conversion.h"
#include "compat/coduo_native_x87.h"

#include <limits.h>
#include <stdint.h>
#include <string.h>

#if defined(WINDOWS_BEHAVIOR) && defined(LINUX_BEHAVIOR)
#error "Select only one of WINDOWS_BEHAVIOR or LINUX_BEHAVIOR"
#elif !defined(WINDOWS_BEHAVIOR) && !defined(LINUX_BEHAVIOR)
#error "Select WINDOWS_BEHAVIOR or LINUX_BEHAVIOR"
#endif

#include "compat/coduo_x87emu.h"

enum {
    COLOR_BYTE_SCALE = 255,
    COLOR_GREEN_SHIFT = 8,
    COLOR_BLUE_SHIFT = 16,
    COLOR_ALPHA_SHIFT = 24,
    COLOR_FISTP_I16_MINIMUM_EXCLUSIVE = -32769,
    COLOR_FISTP_I16_MAXIMUM_EXCLUSIVE = 32768
};

#if defined(WINDOWS_BEHAVIOR) && EMULATE_X87
/* NOT_FROM_ORIGINAL_SOURCE: low-dword adapter for the original Windows
 * `_ftol2` call sites below.  The live x87 product must not pass through a
 * binary64 store before conversion. */
static uint32_t coduo_color_ftol2_low_u32(x87f value)
{
#if EMULATE_X87_BACKEND == EMU_X87_SOFTFLOAT
    const int64_t nearest = extF80_to_i64(value, softfloat_round_near_even, false);

    /* `_ftol2` first executes a nearest-mode signed-qword FISTP.  A masked
     * invalid conversion yields INT64_MIN, whose low dword is zero. */
    if (nearest == INT64_MIN) {
        return 0;
    }
    return (uint32_t)(uint64_t)extF80_to_i64(value, softfloat_round_minMag, false);
#else
    return (uint32_t)coduo_fp_to_i32_extended((long double)value);
#endif
}
#endif

/*
 * The four authoritative Windows bodies are instruction-identical apart from
 * constant and `_ftol2` addresses:
 *
 *   CoDUOMP.exe                 0x00433a50, 0x00433aa0
 *   uo_cgame_mp_x86.dll        0x3004bbb0, 0x3004bc00
 *   uo_ui_mp_x86.dll           0x40003b80, 0x40003bd0
 *   uo_game_mp_x86.dll         0x20018c00, 0x20018c50
 *
 * Each channel is multiplied by binary32 255 under Windows PC=53, converted
 * through `_ftol2`, and packed from the returned low byte.
 *
 * The two Linux bodies use the same channel and packing order, but truncate
 * each live PC=64 product with FISTP m16: coduo_lnxded 0x08069bf2 and
 * 0x08069c61; game.mp.uo.i386.so RVAs 0x0003d8e5 and 0x0003d964.  The m16
 * destination makes overflow/NaN produce integer-indefinite 0x8000, whose low
 * byte is zero, rather than the Windows `_ftol2` low-dword result.
 */
#if defined(WINDOWS_BEHAVIOR)
uint32_t ColorBytes3(float red, float green, float blue)
{
#if EMULATE_X87
    const x87f scale = x87f_load_f32((float)COLOR_BYTE_SCALE);
    const uint32_t redByte = (uint8_t)coduo_color_ftol2_low_u32(x87f_mul(x87f_load_f32(red), scale));
    const uint32_t greenByte = (uint8_t)coduo_color_ftol2_low_u32(x87f_mul(x87f_load_f32(green), scale));
    const uint32_t blueByte = (uint8_t)coduo_color_ftol2_low_u32(x87f_mul(x87f_load_f32(blue), scale));
#else
    const double scale = (double)(float)COLOR_BYTE_SCALE;
    const uint32_t redByte = (uint8_t)coduo_fp_to_i32_extended((long double)((double)red * scale));
    const uint32_t greenByte = (uint8_t)coduo_fp_to_i32_extended((long double)((double)green * scale));
    const uint32_t blueByte = (uint8_t)coduo_fp_to_i32_extended((long double)((double)blue * scale));
#endif

    return redByte | (greenByte << COLOR_GREEN_SHIFT) | (blueByte << COLOR_BLUE_SHIFT) | (UINT32_C(255) << COLOR_ALPHA_SHIFT);
}

uint32_t ColorBytes4(float red, float green, float blue, float alpha)
{
#if EMULATE_X87
    const x87f scale = x87f_load_f32((float)COLOR_BYTE_SCALE);
    const uint32_t redByte = (uint8_t)coduo_color_ftol2_low_u32(x87f_mul(x87f_load_f32(red), scale));
    const uint32_t greenByte = (uint8_t)coduo_color_ftol2_low_u32(x87f_mul(x87f_load_f32(green), scale));
    const uint32_t blueByte = (uint8_t)coduo_color_ftol2_low_u32(x87f_mul(x87f_load_f32(blue), scale));
    const uint32_t alphaByte = (uint8_t)coduo_color_ftol2_low_u32(x87f_mul(x87f_load_f32(alpha), scale));
#else
    const double scale = (double)(float)COLOR_BYTE_SCALE;
    const uint32_t redByte = (uint8_t)coduo_fp_to_i32_extended((long double)((double)red * scale));
    const uint32_t greenByte = (uint8_t)coduo_fp_to_i32_extended((long double)((double)green * scale));
    const uint32_t blueByte = (uint8_t)coduo_fp_to_i32_extended((long double)((double)blue * scale));
    const uint32_t alphaByte = (uint8_t)coduo_fp_to_i32_extended((long double)((double)alpha * scale));
#endif

    return redByte | (greenByte << COLOR_GREEN_SHIFT) | (blueByte << COLOR_BLUE_SHIFT) | (alphaByte << COLOR_ALPHA_SHIFT);
}
#else
#if EMULATE_X87
/* NOT_FROM_ORIGINAL_SOURCE: portable result adapter for the Linux FISTP m16
 * sites above.  The original instruction returns 0x8000 for every invalid
 * conversion; the general x87 emulator otherwise converts to signed dword. */
static int16_t coduo_color_fistp_i16(x87f value)
{
    const x87f minimumExclusive = x87f_load_i32(COLOR_FISTP_I16_MINIMUM_EXCLUSIVE);
    const x87f maximumExclusive = x87f_load_i32(COLOR_FISTP_I16_MAXIMUM_EXCLUSIVE);

    /* RECONSTRUCTION_FIX: the former engine/game emulation converted to i32
     * and merely cast to int16_t.  The original FISTP m16 instead produces
     * signed-word integer-indefinite for every out-of-range result. */
    if (!x87f_lt(minimumExclusive, value) || !x87f_lt(value, maximumExclusive)) {
#if EMULATE_X87_BACKEND == EMU_X87_SOFTFLOAT
        softfloat_raiseFlags(softfloat_flag_invalid);
#endif
        return INT16_MIN;
    }
    return (int16_t)x87f_store_i32_trunc(value);
}
#endif

uint32_t ColorBytes3(float red, float green, float blue)
{
    uint32_t redByte;
    uint32_t greenByte;
    uint32_t blueByte;

#if EMULATE_X87
    const x87f scale = x87f_load_f32((float)COLOR_BYTE_SCALE);

    redByte = (uint8_t)coduo_color_fistp_i16(x87f_mul(x87f_load_f32(red), scale));
    greenByte = (uint8_t)coduo_color_fistp_i16(x87f_mul(x87f_load_f32(green), scale));
    blueByte = (uint8_t)coduo_color_fistp_i16(x87f_mul(x87f_load_f32(blue), scale));
#elif (defined(__i386__) || defined(__x86_64__)) && (defined(__GNUC__) || defined(__clang__))
    static const float scale = (float)COLOR_BYTE_SCALE;
    coduo_x87_truncation_control_t control;

    redByte = (uint8_t)CODUO_X87_TRUNCATE_I16_FIRST(&control, (long double)red * (long double)scale);
    greenByte = (uint8_t)CODUO_X87_TRUNCATE_I16_NEXT(&control, (long double)green * (long double)scale);
    blueByte = (uint8_t)CODUO_X87_TRUNCATE_I16_NEXT(&control, (long double)blue * (long double)scale);
#else
    /* This branch is available only when the caller deliberately disables
     * x87 emulation on a non-x86 host; ordinary supported builds do not use it. */
    redByte = (uint8_t)(int16_t)(red * (float)COLOR_BYTE_SCALE);
    greenByte = (uint8_t)(int16_t)(green * (float)COLOR_BYTE_SCALE);
    blueByte = (uint8_t)(int16_t)(blue * (float)COLOR_BYTE_SCALE);
#endif

    return redByte | (greenByte << COLOR_GREEN_SHIFT) | (blueByte << COLOR_BLUE_SHIFT) | (UINT32_C(255) << COLOR_ALPHA_SHIFT);
}

uint32_t ColorBytes4(float red, float green, float blue, float alpha)
{
    uint32_t redByte;
    uint32_t greenByte;
    uint32_t blueByte;
    uint32_t alphaByte;

#if EMULATE_X87
    const x87f scale = x87f_load_f32((float)COLOR_BYTE_SCALE);

    redByte = (uint8_t)coduo_color_fistp_i16(x87f_mul(x87f_load_f32(red), scale));
    greenByte = (uint8_t)coduo_color_fistp_i16(x87f_mul(x87f_load_f32(green), scale));
    blueByte = (uint8_t)coduo_color_fistp_i16(x87f_mul(x87f_load_f32(blue), scale));
    alphaByte = (uint8_t)coduo_color_fistp_i16(x87f_mul(x87f_load_f32(alpha), scale));
#elif (defined(__i386__) || defined(__x86_64__)) && (defined(__GNUC__) || defined(__clang__))
    static const float scale = (float)COLOR_BYTE_SCALE;
    coduo_x87_truncation_control_t control;

    redByte = (uint8_t)CODUO_X87_TRUNCATE_I16_FIRST(&control, (long double)red * (long double)scale);
    greenByte = (uint8_t)CODUO_X87_TRUNCATE_I16_NEXT(&control, (long double)green * (long double)scale);
    blueByte = (uint8_t)CODUO_X87_TRUNCATE_I16_NEXT(&control, (long double)blue * (long double)scale);
    alphaByte = (uint8_t)CODUO_X87_TRUNCATE_I16_NEXT(&control, (long double)alpha * (long double)scale);
#else
    redByte = (uint8_t)(int16_t)(red * (float)COLOR_BYTE_SCALE);
    greenByte = (uint8_t)(int16_t)(green * (float)COLOR_BYTE_SCALE);
    blueByte = (uint8_t)(int16_t)(blue * (float)COLOR_BYTE_SCALE);
    alphaByte = (uint8_t)(int16_t)(alpha * (float)COLOR_BYTE_SCALE);
#endif

    return redByte | (greenByte << COLOR_GREEN_SHIFT) | (blueByte << COLOR_BLUE_SHIFT) | (alphaByte << COLOR_ALPHA_SHIFT);
}
#endif

/*
 * NormalizeColor is instruction-identical within the four Windows images at
 * 0x00433b00, 0x3004bc60, 0x40003c30, and 0x20018cb0.  The Linux bodies at
 * coduo_lnxded 0x08069ce7 and game RVA 0x0003d9fa have the same max scan,
 * exact-zero branch, black fallback, lane order, and binary32 return.  Windows
 * performs each divide under PC=53; Linux performs it under PC=64.  The x87
 * backend selects that precision without duplicating comparison source.
 */
float NormalizeColor(const vec3_t input, vec3_t output)
{
    float maximum;
    memcpy(&maximum, &input[0], sizeof(maximum));

#if EMULATE_X87
    if (x87f_lt(x87f_load_f32(maximum), x87f_load_f32(input[1]))) {
#else
    if (maximum < input[1]) {
#endif
        memcpy(&maximum, &input[1], sizeof(maximum));
    }
#if EMULATE_X87
    if (x87f_lt(x87f_load_f32(maximum), x87f_load_f32(input[2]))) {
#else
    if (maximum < input[2]) {
#endif
        memcpy(&maximum, &input[2], sizeof(maximum));
    }

#if EMULATE_X87
    if (x87f_eq(x87f_load_f32(maximum), x87f_load_f32(0.0f))) {
#else
    if (maximum == 0.0f) {
#endif
        output[2] = 0.0f;
        output[1] = 0.0f;
        output[0] = 0.0f;
    } else {
#if EMULATE_X87
        output[0] = x87f_store_f32(x87f_div(x87f_load_f32(input[0]), x87f_load_f32(maximum)));
        output[1] = x87f_store_f32(x87f_div(x87f_load_f32(input[1]), x87f_load_f32(maximum)));
        output[2] = x87f_store_f32(x87f_div(x87f_load_f32(input[2]), x87f_load_f32(maximum)));
#else
        output[0] = (float)((long double)input[0] / (long double)maximum);
        output[1] = (float)((long double)input[1] / (long double)maximum);
        output[2] = (float)((long double)input[2] / (long double)maximum);
#endif
    }
    return maximum;
}

/*
 * ColorNormalize is likewise instruction-identical within the four Windows
 * images at 0x00434ba0, 0x3004cd00, 0x40004d10, and 0x20019d50.  The Linux
 * bodies at coduo_lnxded 0x0806ae56 and game RVA 0x0003ed26 agree on the same
 * behavior: zero produces white and returns +0.0f; otherwise a 1/max
 * reciprocal is narrowed once to binary32 and reused for three products.
 * Windows evaluates the reciprocal/products under PC=53 and loads scale first
 * for each multiply; Linux uses PC=64 and loads the input lane first.  The
 * load-order commutation of each single product is not a distinct computation.
 */
float ColorNormalize(const vec3_t input, vec3_t output)
{
    float maximum;
    memcpy(&maximum, &input[0], sizeof(maximum));

#if EMULATE_X87
    if (x87f_lt(x87f_load_f32(maximum), x87f_load_f32(input[1]))) {
#else
    if (maximum < input[1]) {
#endif
        memcpy(&maximum, &input[1], sizeof(maximum));
    }
#if EMULATE_X87
    if (x87f_lt(x87f_load_f32(maximum), x87f_load_f32(input[2]))) {
#else
    if (maximum < input[2]) {
#endif
        memcpy(&maximum, &input[2], sizeof(maximum));
    }

#if EMULATE_X87
    if (x87f_eq(x87f_load_f32(maximum), x87f_load_f32(0.0f))) {
#else
    if (maximum == 0.0f) {
#endif
        output[2] = 1.0f;
        output[1] = 1.0f;
        output[0] = 1.0f;
        return 0.0f;
    }

#if EMULATE_X87
    {
        const float scale = x87f_store_f32(x87f_div(x87f_load_f32(1.0f), x87f_load_f32(maximum)));
        output[0] = x87f_store_f32(x87f_mul(x87f_load_f32(input[0]), x87f_load_f32(scale)));
        output[1] = x87f_store_f32(x87f_mul(x87f_load_f32(input[1]), x87f_load_f32(scale)));
        output[2] = x87f_store_f32(x87f_mul(x87f_load_f32(input[2]), x87f_load_f32(scale)));
    }
#else
    {
        const float scale = (float)(1.0L / (long double)maximum);
        output[0] = (float)((long double)input[0] * (long double)scale);
        output[1] = (float)((long double)input[1] * (long double)scale);
        output[2] = (float)((long double)input[2] * (long double)scale);
    }
#endif
    return maximum;
}
