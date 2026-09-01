#include "bg_pmove.h"

#include "compat/coduo_fp_conversion.h"

/*
 * Complete pmove water-state subsystem.  The Windows cgame/game bodies are
 * instruction-identical apart from relocations and dependency addresses:
 *
 *   uo_cgame_mp_x86.dll  PM_SetWaterLevel 0x3000a7a0
 *   uo_game_mp_x86.dll   PM_SetWaterLevel 0x2000a560
 *   uo_cgame_mp_x86.dll  PM_WaterEvents   0x3000c290
 *   uo_game_mp_x86.dll   PM_WaterEvents   0x2000c050
 *
 * Linux game retains the same three point-contents probes and transition
 * predicates at RVAs 0x00026cf1 and 0x0002a7b9.  Its unoptimized body chooses
 * the other operand as the initial FLD for commutative additions; that is a
 * compiler realization difference, not a different source computation.  All
 * arithmetic inputs are binary32 values or int32_t values and each probe Z is
 * stored as binary32 before the callback, so a long-double expression is
 * sufficient without an emulated-x87 operation graph.
 */

#define PM_WATER_BOTTOM_EPSILON 1.0L

void PM_SetWaterLevel(void)
{
    vec3_t point;
    int32_t contents;
    int32_t height;

    pm->waterlevel = 0;
    pm->watertype = 0;

    point[0] = pm->ps->psOrigin[0];
    point[1] = pm->ps->psOrigin[1];
    point[2] = (float)((long double)pm->ps->psOrigin[2] +
                       (long double)pm->ps->playerMins[2] +
                       PM_WATER_BOTTOM_EPSILON);

    contents = pm->pointContents(point, pm->ps->psClientNum,
                                 CONTENTS_WATER);
    if (contents == 0) {
        return;
    }

    /* The subtraction remains live through the original truncating x87
     * conversion: Windows consumes _ftol2's low dword, while Linux uses a
     * signed-dword FISTP.  The shared compatibility conversion preserves that
     * already-adjudicated platform ABI without changing the arithmetic. */
    height = coduo_fp_to_i32_extended(
        (long double)pm->ps->viewHeightCurrent -
        (long double)pm->ps->playerMins[2]);

    pm->watertype = (uint8_t)contents;
    pm->waterlevel = 1;

    point[2] = (float)((long double)pm->ps->psOrigin[2] +
                       (long double)pm->ps->playerMins[2] +
                       (long double)(height / 2));
    contents = pm->pointContents(point, pm->ps->psClientNum,
                                 CONTENTS_WATER);
    if (contents == 0) {
        return;
    }

    pm->waterlevel = 2;
    point[2] = (float)((long double)pm->ps->psOrigin[2] +
                       (long double)pm->ps->playerMins[2] +
                       (long double)height);
    contents = pm->pointContents(point, pm->ps->psClientNum,
                                 CONTENTS_WATER);
    if (contents != 0) {
        pm->waterlevel = 3;
    }
}

void PM_WaterEvents(void)
{
    if (pml.previousWaterLevel == 0 && pm->waterlevel != 0) {
        PM_AddEvent(EV_WATER_TOUCH);
    }
    if (pml.previousWaterLevel != 0 && pm->waterlevel == 0) {
        PM_AddEvent(EV_WATER_LEAVE);
    }
}

#undef PM_WATER_BOTTOM_EPSILON
