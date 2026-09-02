#include "q_math.h"

#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#if defined(WINDOWS_BEHAVIOR) && defined(LINUX_BEHAVIOR)
#error "Select only one of WINDOWS_BEHAVIOR or LINUX_BEHAVIOR"
#elif !defined(WINDOWS_BEHAVIOR) && !defined(LINUX_BEHAVIOR)
#error "Select WINDOWS_BEHAVIOR or LINUX_BEHAVIOR"
#endif

#include "compat/coduo_x87emu.h"

/*
 * The original table is mutable data, matching the core Quake 3 declaration.
 * All six authoritative copies contain the same 1,944 bytes (SHA-256
 * db1cf609aca1c6ea814184177998a887768136e9f082406f11aeec61e08efe9c):
 *
 *   CoDUOMP.exe                 0x005c49b8
 *   uo_cgame_mp_x86.dll        0x30085f20
 *   uo_ui_mp_x86.dll           0x4003b040
 *   uo_game_mp_x86.dll         0x20086170
 *   coduo_lnxded               0x080ef540
 *   game.mp.uo.i386.so         0x000ac340
 *
 * Decimal literals below round to those exact binary32 words.
 */
vec3_t bytedirs[NUMVERTEXNORMALS] = {{-0.525731027f, 0.0f, 0.850651026f},
                                     {-0.442862988f, 0.238856003f, 0.864188015f},
                                     {-0.295242012f, 0.0f, 0.955422997f},
                                     {-0.309017003f, 0.5f, 0.809017003f},
                                     {-0.162459999f, 0.26286599f, 0.951056004f},
                                     {0.0f, 0.0f, 1.0f},
                                     {0.0f, 0.850651026f, 0.525731027f},
                                     {-0.147621006f, 0.71656698f, 0.681717992f},
                                     {0.147621006f, 0.71656698f, 0.681717992f},
                                     {0.0f, 0.525731027f, 0.850651026f},
                                     {0.309017003f, 0.5f, 0.809017003f},
                                     {0.525731027f, 0.0f, 0.850651026f},
                                     {0.295242012f, 0.0f, 0.955422997f},
                                     {0.442862988f, 0.238856003f, 0.864188015f},
                                     {0.162459999f, 0.26286599f, 0.951056004f},
                                     {-0.681717992f, 0.147621006f, 0.71656698f},
                                     {-0.809017003f, 0.309017003f, 0.5f},
                                     {-0.587785006f, 0.425325006f, 0.688190997f},
                                     {-0.850651026f, 0.525731027f, 0.0f},
                                     {-0.864188015f, 0.442862988f, 0.238856003f},
                                     {-0.71656698f, 0.681717992f, 0.147621006f},
                                     {-0.688190997f, 0.587785006f, 0.425325006f},
                                     {-0.5f, 0.809017003f, 0.309017003f},
                                     {-0.238856003f, 0.864188015f, 0.442862988f},
                                     {-0.425325006f, 0.688190997f, 0.587785006f},
                                     {-0.71656698f, 0.681717992f, -0.147621006f},
                                     {-0.5f, 0.809017003f, -0.309017003f},
                                     {-0.525731027f, 0.850651026f, 0.0f},
                                     {0.0f, 0.850651026f, -0.525731027f},
                                     {-0.238856003f, 0.864188015f, -0.442862988f},
                                     {0.0f, 0.955422997f, -0.295242012f},
                                     {-0.26286599f, 0.951056004f, -0.162459999f},
                                     {0.0f, 1.0f, 0.0f},
                                     {0.0f, 0.955422997f, 0.295242012f},
                                     {-0.26286599f, 0.951056004f, 0.162459999f},
                                     {0.238856003f, 0.864188015f, 0.442862988f},
                                     {0.26286599f, 0.951056004f, 0.162459999f},
                                     {0.5f, 0.809017003f, 0.309017003f},
                                     {0.238856003f, 0.864188015f, -0.442862988f},
                                     {0.26286599f, 0.951056004f, -0.162459999f},
                                     {0.5f, 0.809017003f, -0.309017003f},
                                     {0.850651026f, 0.525731027f, 0.0f},
                                     {0.71656698f, 0.681717992f, 0.147621006f},
                                     {0.71656698f, 0.681717992f, -0.147621006f},
                                     {0.525731027f, 0.850651026f, 0.0f},
                                     {0.425325006f, 0.688190997f, 0.587785006f},
                                     {0.864188015f, 0.442862988f, 0.238856003f},
                                     {0.688190997f, 0.587785006f, 0.425325006f},
                                     {0.809017003f, 0.309017003f, 0.5f},
                                     {0.681717992f, 0.147621006f, 0.71656698f},
                                     {0.587785006f, 0.425325006f, 0.688190997f},
                                     {0.955422997f, 0.295242012f, 0.0f},
                                     {1.0f, 0.0f, 0.0f},
                                     {0.951056004f, 0.162459999f, 0.26286599f},
                                     {0.850651026f, -0.525731027f, 0.0f},
                                     {0.955422997f, -0.295242012f, 0.0f},
                                     {0.864188015f, -0.442862988f, 0.238856003f},
                                     {0.951056004f, -0.162459999f, 0.26286599f},
                                     {0.809017003f, -0.309017003f, 0.5f},
                                     {0.681717992f, -0.147621006f, 0.71656698f},
                                     {0.850651026f, 0.0f, 0.525731027f},
                                     {0.864188015f, 0.442862988f, -0.238856003f},
                                     {0.809017003f, 0.309017003f, -0.5f},
                                     {0.951056004f, 0.162459999f, -0.26286599f},
                                     {0.525731027f, 0.0f, -0.850651026f},
                                     {0.681717992f, 0.147621006f, -0.71656698f},
                                     {0.681717992f, -0.147621006f, -0.71656698f},
                                     {0.850651026f, 0.0f, -0.525731027f},
                                     {0.809017003f, -0.309017003f, -0.5f},
                                     {0.864188015f, -0.442862988f, -0.238856003f},
                                     {0.951056004f, -0.162459999f, -0.26286599f},
                                     {0.147621006f, 0.71656698f, -0.681717992f},
                                     {0.309017003f, 0.5f, -0.809017003f},
                                     {0.425325006f, 0.688190997f, -0.587785006f},
                                     {0.442862988f, 0.238856003f, -0.864188015f},
                                     {0.587785006f, 0.425325006f, -0.688190997f},
                                     {0.688190997f, 0.587785006f, -0.425325006f},
                                     {-0.147621006f, 0.71656698f, -0.681717992f},
                                     {-0.309017003f, 0.5f, -0.809017003f},
                                     {0.0f, 0.525731027f, -0.850651026f},
                                     {-0.525731027f, 0.0f, -0.850651026f},
                                     {-0.442862988f, 0.238856003f, -0.864188015f},
                                     {-0.295242012f, 0.0f, -0.955422997f},
                                     {-0.162459999f, 0.26286599f, -0.951056004f},
                                     {0.0f, 0.0f, -1.0f},
                                     {0.295242012f, 0.0f, -0.955422997f},
                                     {0.162459999f, 0.26286599f, -0.951056004f},
                                     {-0.442862988f, -0.238856003f, -0.864188015f},
                                     {-0.309017003f, -0.5f, -0.809017003f},
                                     {-0.162459999f, -0.26286599f, -0.951056004f},
                                     {0.0f, -0.850651026f, -0.525731027f},
                                     {-0.147621006f, -0.71656698f, -0.681717992f},
                                     {0.147621006f, -0.71656698f, -0.681717992f},
                                     {0.0f, -0.525731027f, -0.850651026f},
                                     {0.309017003f, -0.5f, -0.809017003f},
                                     {0.442862988f, -0.238856003f, -0.864188015f},
                                     {0.162459999f, -0.26286599f, -0.951056004f},
                                     {0.238856003f, -0.864188015f, -0.442862988f},
                                     {0.5f, -0.809017003f, -0.309017003f},
                                     {0.425325006f, -0.688190997f, -0.587785006f},
                                     {0.71656698f, -0.681717992f, -0.147621006f},
                                     {0.688190997f, -0.587785006f, -0.425325006f},
                                     {0.587785006f, -0.425325006f, -0.688190997f},
                                     {0.0f, -0.955422997f, -0.295242012f},
                                     {0.0f, -1.0f, 0.0f},
                                     {0.26286599f, -0.951056004f, -0.162459999f},
                                     {0.0f, -0.850651026f, 0.525731027f},
                                     {0.0f, -0.955422997f, 0.295242012f},
                                     {0.238856003f, -0.864188015f, 0.442862988f},
                                     {0.26286599f, -0.951056004f, 0.162459999f},
                                     {0.5f, -0.809017003f, 0.309017003f},
                                     {0.71656698f, -0.681717992f, 0.147621006f},
                                     {0.525731027f, -0.850651026f, 0.0f},
                                     {-0.238856003f, -0.864188015f, -0.442862988f},
                                     {-0.5f, -0.809017003f, -0.309017003f},
                                     {-0.26286599f, -0.951056004f, -0.162459999f},
                                     {-0.850651026f, -0.525731027f, 0.0f},
                                     {-0.71656698f, -0.681717992f, -0.147621006f},
                                     {-0.71656698f, -0.681717992f, 0.147621006f},
                                     {-0.525731027f, -0.850651026f, 0.0f},
                                     {-0.5f, -0.809017003f, 0.309017003f},
                                     {-0.238856003f, -0.864188015f, 0.442862988f},
                                     {-0.26286599f, -0.951056004f, 0.162459999f},
                                     {-0.864188015f, -0.442862988f, 0.238856003f},
                                     {-0.809017003f, -0.309017003f, 0.5f},
                                     {-0.688190997f, -0.587785006f, 0.425325006f},
                                     {-0.681717992f, -0.147621006f, 0.71656698f},
                                     {-0.442862988f, -0.238856003f, 0.864188015f},
                                     {-0.587785006f, -0.425325006f, 0.688190997f},
                                     {-0.309017003f, -0.5f, 0.809017003f},
                                     {-0.147621006f, -0.71656698f, 0.681717992f},
                                     {-0.425325006f, -0.688190997f, 0.587785006f},
                                     {-0.162459999f, -0.26286599f, 0.951056004f},
                                     {0.442862988f, -0.238856003f, 0.864188015f},
                                     {0.162459999f, -0.26286599f, 0.951056004f},
                                     {0.309017003f, -0.5f, 0.809017003f},
                                     {0.147621006f, -0.71656698f, 0.681717992f},
                                     {0.0f, -0.525731027f, 0.850651026f},
                                     {0.425325006f, -0.688190997f, 0.587785006f},
                                     {0.587785006f, -0.425325006f, 0.688190997f},
                                     {0.688190997f, -0.587785006f, 0.425325006f},
                                     {-0.955422997f, 0.295242012f, 0.0f},
                                     {-0.951056004f, 0.162459999f, 0.26286599f},
                                     {-1.0f, 0.0f, 0.0f},
                                     {-0.850651026f, 0.0f, 0.525731027f},
                                     {-0.955422997f, -0.295242012f, 0.0f},
                                     {-0.951056004f, -0.162459999f, 0.26286599f},
                                     {-0.864188015f, 0.442862988f, -0.238856003f},
                                     {-0.951056004f, 0.162459999f, -0.26286599f},
                                     {-0.809017003f, 0.309017003f, -0.5f},
                                     {-0.864188015f, -0.442862988f, -0.238856003f},
                                     {-0.951056004f, -0.162459999f, -0.26286599f},
                                     {-0.809017003f, -0.309017003f, -0.5f},
                                     {-0.681717992f, 0.147621006f, -0.71656698f},
                                     {-0.681717992f, -0.147621006f, -0.71656698f},
                                     {-0.850651026f, 0.0f, -0.525731027f},
                                     {-0.688190997f, 0.587785006f, -0.425325006f},
                                     {-0.587785006f, 0.425325006f, -0.688190997f},
                                     {-0.425325006f, 0.688190997f, -0.587785006f},
                                     {-0.425325006f, -0.688190997f, -0.587785006f},
                                     {-0.587785006f, -0.425325006f, -0.688190997f},
                                     {-0.688190997f, -0.587785006f, -0.425325006f}};

/*
 * The core Quake 3 name is retained.  CoD:UO narrowed the result ABI from the
 * Quake 3 int prototype: the Windows bodies define only AL, and both Windows
 * and Linux callers zero-extend AL.  The four Windows bodies are
 * instruction-identical apart from their embedded table address:
 *
 *   CoDUOMP.exe                 0x00431230
 *   uo_cgame_mp_x86.dll        0x30049390
 *   uo_ui_mp_x86.dll           0x40001360
 *   uo_game_mp_x86.dll         0x200163e0
 *
 * They evaluate and spill each candidate as (z*dz + x*dx) + y*dy.  Keep that
 * distinct from the Linux source below because the fold order can change the
 * stored candidate and selected index.  FCOMP versus FUCOMPP is only a
 * comparison-lowering distinction.
 */
#if defined(WINDOWS_BEHAVIOR)
uint8_t DirToByte(const vec3_t direction)
{
    float bestDot = 0.0f;
    uint8_t bestIndex = 0;

    if (direction == NULL) {
        return 0;
    }

    for (uint8_t index = 0; index < NUMVERTEXNORMALS; ++index) {
        float dot;

#if EMULATE_X87
        dot = x87f_store_f32(x87f_add(x87f_add(x87f_mul(x87f_load_f32(bytedirs[index][2]), x87f_load_f32(direction[2])),
                                               x87f_mul(x87f_load_f32(bytedirs[index][0]), x87f_load_f32(direction[0]))),
                                      x87f_mul(x87f_load_f32(bytedirs[index][1]), x87f_load_f32(direction[1]))));
        if (x87f_lt(x87f_load_f32(bestDot), x87f_load_f32(dot))) {
#elif (defined(__i386__) || defined(__x86_64__)) && (defined(__GNUC__) || defined(__clang__))
        __asm__ __volatile__("flds %1\n\t"
                             "fmuls %2\n\t"
                             "flds %3\n\t"
                             "fmuls %4\n\t"
                             "faddp %%st, %%st(1)\n\t"
                             "flds %5\n\t"
                             "fmuls %6\n\t"
                             "faddp %%st, %%st(1)\n\t"
                             "fstps %0"
                             : "=m"(dot)
                             : "m"(bytedirs[index][2]), "m"(direction[2]), "m"(bytedirs[index][0]), "m"(direction[0]),
                               "m"(bytedirs[index][1]), "m"(direction[1])
                             : "st", "st(1)", "memory");

        uint16_t status;
        __asm__ __volatile__("flds %1\n\t"
                             "fcomps %2\n\t"
                             "fnstsw %%ax"
                             : "=a"(status)
                             : "m"(dot), "m"(bestDot)
                             : "st", "memory");
        if ((status & UINT16_C(0x4100)) == 0) {
#else
        /* Windows runs x87 at PC=53.  Volatile double steps preserve that
         * normal-value graph until client EMULATE_X87 wiring is introduced. */
        volatile double productZ = (double)bytedirs[index][2] * (double)direction[2];
        volatile double productX = (double)bytedirs[index][0] * (double)direction[0];
        volatile double sumZX = productZ + productX;
        volatile double productY = (double)bytedirs[index][1] * (double)direction[1];
        volatile double sum = sumZX + productY;
        dot = (float)sum;
        if (dot > bestDot) {
#endif
            bestDot = dot;
            bestIndex = index;
        }
    }

    return bestIndex;
}
#else
/*
 * The Linux bodies at coduo_lnxded 0x080661f4 and game.mp.uo.i386.so RVA
 * 0x00039bd4 instead evaluate and spill (x*dx + y*dy) + z*dz.  Their running
 * maximum comparison is quiet FUCOMPP.  The supporting Mac game body named
 * DirToByte uses the same lane order with PowerPC fused multiply-adds; it is
 * useful naming support but does not override the Linux server authority.
 */
uint8_t DirToByte(const vec3_t direction)
{
    float bestDot = 0.0f;
    uint8_t bestIndex = 0;

    if (direction == NULL) {
        return 0;
    }

    for (uint8_t index = 0; index < NUMVERTEXNORMALS; ++index) {
        float dot;

#if EMULATE_X87
        dot = x87f_store_f32(x87f_add(x87f_add(x87f_mul(x87f_load_f32(direction[0]), x87f_load_f32(bytedirs[index][0])),
                                               x87f_mul(x87f_load_f32(direction[1]), x87f_load_f32(bytedirs[index][1]))),
                                      x87f_mul(x87f_load_f32(direction[2]), x87f_load_f32(bytedirs[index][2]))));
        if (x87f_lt(x87f_load_f32(bestDot), x87f_load_f32(dot))) {
#elif (defined(__i386__) || defined(__x86_64__)) && (defined(__GNUC__) || defined(__clang__))
        __asm__ __volatile__("flds %1\n\t"
                             "fmuls %2\n\t"
                             "flds %3\n\t"
                             "fmuls %4\n\t"
                             "faddp %%st, %%st(1)\n\t"
                             "flds %5\n\t"
                             "fmuls %6\n\t"
                             "faddp %%st, %%st(1)\n\t"
                             "fstps %0"
                             : "=m"(dot)
                             : "m"(direction[0]), "m"(bytedirs[index][0]), "m"(direction[1]), "m"(bytedirs[index][1]), "m"(direction[2]),
                               "m"(bytedirs[index][2])
                             : "st", "st(1)", "memory");

        uint16_t status;
        __asm__ __volatile__("flds %1\n\t"
                             "flds %2\n\t"
                             "fxch %%st(1)\n\t"
                             "fucompp\n\t"
                             "fnstsw %%ax"
                             : "=a"(status)
                             : "m"(dot), "m"(bestDot)
                             : "st", "st(1)", "memory");
        if ((status & UINT16_C(0x4100)) == 0) {
#else
        dot = (direction[0] * bytedirs[index][0] + direction[1] * bytedirs[index][1]) + direction[2] * bytedirs[index][2];
        if (isless(bestDot, dot)) {
#endif
            bestDot = dot;
            bestIndex = index;
        }
    }

    return bestIndex;
}
#endif

/*
 * All bodies use signed bounds 0 <= value < 162 and copy valid table entries
 * as three ascending raw words.  Windows writes three immediate positive-zero
 * words for an invalid value while Linux obtains the same three values from
 * vec3_origin.  This is not a distinct ByteToDir computation.
 *
 *   CoDUOMP.exe                 0x004312a0
 *   uo_cgame_mp_x86.dll        0x30049400
 *   uo_ui_mp_x86.dll           0x400013d0
 *   uo_game_mp_x86.dll         0x20016450
 *   coduo_lnxded               0x080662ac
 *   game.mp.uo.i386.so         0x00039cb2
 *
 * The supporting Mac cgame symbol retains the core Quake 3 ByteToDir name and
 * likewise copies vec3_origin for an invalid value.
 */
void ByteToDir(int32_t value, vec3_t direction)
{
    uint32_t component;

    if (value < 0 || value >= NUMVERTEXNORMALS) {
        component = UINT32_C(0);
        memcpy(&direction[0], &component, sizeof(component));
        memcpy(&direction[1], &component, sizeof(component));
        memcpy(&direction[2], &component, sizeof(component));
        return;
    }

    memcpy(&component, &bytedirs[value][0], sizeof(component));
    memcpy(&direction[0], &component, sizeof(component));
    memcpy(&component, &bytedirs[value][1], sizeof(component));
    memcpy(&direction[1], &component, sizeof(component));
    memcpy(&component, &bytedirs[value][2], sizeof(component));
    memcpy(&direction[2], &component, sizeof(component));
}
