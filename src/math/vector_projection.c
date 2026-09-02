#include "q_math.h"

#include "compat/coduo_x87emu.h"

#include <stdint.h>
#include <string.h>

#if defined(WINDOWS_BEHAVIOR) && defined(LINUX_BEHAVIOR)
#error "Select only one of WINDOWS_BEHAVIOR or LINUX_BEHAVIOR"
#elif !defined(WINDOWS_BEHAVIOR) && !defined(LINUX_BEHAVIOR)
#error "Select WINDOWS_BEHAVIOR or LINUX_BEHAVIOR"
#endif

/*
 * The six CrossProductUp bodies implement the same ordered mapping:
 *
 *   CoDUOMP.exe                 0x00431580
 *   uo_cgame_mp_x86.dll        0x300496e0
 *   uo_ui_mp_x86.dll           0x400016b0
 *   uo_game_mp_x86.dll         0x20016730
 *   coduo_lnxded               0x08066725
 *   game.mp.uo.i386.so         0x0003a193
 *
 * The Linux game symbol supplies the canonical name.  The former
 * PerpendicularVector2D spelling described the same operation under an
 * unproved local name.  Dword copies preserve the first read before output
 * overlap and retain the input payload bits for the copied lane.
 */
void CrossProductUp(const vec3_t input, vec3_t output)
{
    uint32_t component;

    memcpy(&component, &input[1], sizeof(component));
    memcpy(&output[0], &component, sizeof(component));
    output[1] = -input[0];
    component = 0;
    memcpy(&output[2], &component, sizeof(component));
}

/*
 * Complete perpendicular-view/project-to-line cluster.  The Windows bodies
 * are byte-identical in all four authoritative images:
 *
 *   CoDUOMP.exe                 0x00432310, 0x004323c0
 *   uo_cgame_mp_x86.dll        0x3004a470, 0x3004a520
 *   uo_ui_mp_x86.dll           0x40002440, 0x400024f0
 *   uo_game_mp_x86.dll         0x200174c0, 0x20017570
 *
 * The Linux bodies agree between engine 0x08067758/0x08067806 and game RVAs
 * 0x0003b344/0x0003b402.  GetPerpendicularViewVector has the same value-level
 * graph on both platforms.  ProjectPointOntoVector differs: Windows stores
 * one z,y,x projection scalar as binary32; Linux recomputes an unspilled
 * x,y,z dot for each output lane.
 */
void GetPerpendicularViewVector(const vec3_t point, const vec3_t point1, const vec3_t point2, vec3_t perpendicular)
{
    vec3_t direction1 = {point[0] - point1[0], point[1] - point1[1], point[2] - point1[2]};
    vec3_t direction2 = {point[0] - point2[0], point[1] - point2[1], point[2] - point2[2]};

    (void)VectorNormalize(direction1);
    (void)VectorNormalize(direction2);
    CrossProduct(direction1, direction2, perpendicular);
    (void)VectorNormalize(perpendicular);
}

#if defined(WINDOWS_BEHAVIOR)

void ProjectPointOntoVector(const vec3_t point, const vec3_t lineStart, const vec3_t lineEnd, vec3_t projected)
{
    vec3_t pointDelta;
    vec3_t direction;
    float projection;

#if EMULATE_X87
    for (int32_t lane = 0; lane < 3; ++lane) {
        pointDelta[lane] = x87f_store_f32(x87f_sub(x87f_load_f32(point[lane]), x87f_load_f32(lineStart[lane])));
        direction[lane] = x87f_store_f32(x87f_sub(x87f_load_f32(lineEnd[lane]), x87f_load_f32(lineStart[lane])));
    }
#else
    for (int32_t lane = 0; lane < 3; ++lane) {
        pointDelta[lane] = (float)((long double)point[lane] - lineStart[lane]);
        direction[lane] = (float)((long double)lineEnd[lane] - lineStart[lane]);
    }
#endif

    (void)VectorNormalize(direction);

#if EMULATE_X87
    projection = x87f_store_f32(x87f_add(x87f_add(x87f_mul(x87f_load_f32(direction[2]), x87f_load_f32(pointDelta[2])),
                                                  x87f_mul(x87f_load_f32(direction[1]), x87f_load_f32(pointDelta[1]))),
                                         x87f_mul(x87f_load_f32(direction[0]), x87f_load_f32(pointDelta[0]))));
    for (int32_t lane = 0; lane < 3; ++lane) {
        projected[lane] =
            x87f_store_f32(x87f_add(x87f_load_f32(lineStart[lane]), x87f_mul(x87f_load_f32(projection), x87f_load_f32(direction[lane]))));
    }
#else
    projection = (float)(((long double)direction[2] * pointDelta[2] + (long double)direction[1] * pointDelta[1]) +
                         (long double)direction[0] * pointDelta[0]);
    for (int32_t lane = 0; lane < 3; ++lane) {
        projected[lane] = (float)((long double)lineStart[lane] + (long double)projection * direction[lane]);
    }
#endif
}

#else

void ProjectPointOntoVector(const vec3_t point, const vec3_t lineStart, const vec3_t lineEnd, vec3_t projected)
{
    vec3_t pointDelta;
    vec3_t direction;

#if EMULATE_X87
    for (int32_t lane = 0; lane < 3; ++lane) {
        pointDelta[lane] = x87f_store_f32(x87f_sub(x87f_load_f32(point[lane]), x87f_load_f32(lineStart[lane])));
        direction[lane] = x87f_store_f32(x87f_sub(x87f_load_f32(lineEnd[lane]), x87f_load_f32(lineStart[lane])));
    }
#else
    for (int32_t lane = 0; lane < 3; ++lane) {
        pointDelta[lane] = (float)((long double)point[lane] - lineStart[lane]);
        direction[lane] = (float)((long double)lineEnd[lane] - lineStart[lane]);
    }
#endif

    (void)VectorNormalize(direction);

    for (int32_t lane = 0; lane < 3; ++lane) {
#if EMULATE_X87
        const x87f projection = x87f_add(x87f_add(x87f_mul(x87f_load_f32(pointDelta[0]), x87f_load_f32(direction[0])),
                                                  x87f_mul(x87f_load_f32(pointDelta[1]), x87f_load_f32(direction[1]))),
                                         x87f_mul(x87f_load_f32(pointDelta[2]), x87f_load_f32(direction[2])));
        projected[lane] = x87f_store_f32(x87f_add(x87f_load_f32(lineStart[lane]), x87f_mul(projection, x87f_load_f32(direction[lane]))));
#else
        const long double projection = ((long double)pointDelta[0] * direction[0] + (long double)pointDelta[1] * direction[1]) +
                                       (long double)pointDelta[2] * direction[2];
        projected[lane] = (float)((long double)lineStart[lane] + projection * direction[lane]);
#endif
    }
}

#endif
