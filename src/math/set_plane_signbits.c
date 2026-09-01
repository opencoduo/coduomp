#include "q_math.h"

#include <stdint.h>

#if defined(WINDOWS_BEHAVIOR) && defined(LINUX_BEHAVIOR)
#error "Select only one of WINDOWS_BEHAVIOR or LINUX_BEHAVIOR"
#elif !defined(WINDOWS_BEHAVIOR) && !defined(LINUX_BEHAVIOR)
#error "Select WINDOWS_BEHAVIOR or LINUX_BEHAVIOR"
#endif

#include "compat/coduo_x87emu.h"

/*
 * The retained original bodies all test normal[0..2] in ascending order, set
 * bits 0..2 only for an ordered value below +0.0f, and finally store one byte
 * at cplane_t.signbits (+0x11):
 *
 *   CoDUOMP.exe                 0x004346a0
 *   uo_cgame_mp_x86.dll        0x3004c800
 *   uo_ui_mp_x86.dll           0x40004810
 *   uo_game_mp_x86.dll         0x20019850
 *   coduo_lnxded               0x0806ab2c
 *   game.mp.uo.i386.so         0x0003e9a8
 *
 * Windows uses FCOMP and Linux uses FUCOMPP, but both comparisons produce the
 * same sign-bit decision for every input.  That compiler-lowering difference
 * does not justify separate recovered source bodies.
 */
void SetPlaneSignbits(cplane_t *plane)
{
    uint8_t signbits = 0;

    for (int32_t axis = 0; axis < 3; ++axis) {
#if EMULATE_X87
        if (x87f_lt(x87f_load_f32(plane->normal[axis]),
                    x87f_load_f32(0.0f))) {
#else
        if (plane->normal[axis] < 0.0f) {
#endif
            signbits |= (uint8_t)(UINT8_C(1) << axis);
        }
    }

    plane->signbits = signbits;
}
