/*
 * Source reconstruction for debug drawing helper functions.
 *
 * Binary SHA-256:
 * 4d2391c10e234559ace294fb69d96741cad522653c361f13f626f4dc1891acb7
 */

#include <math.h>
#include <stdint.h>

#include "game_functions.h"
#include "game_globals.h"
#include "level_locals.h"
#include "recovered_game.h"
#include "compat/coduo_x87emu.h" /* defines EMULATE_X87; x87 shim when it is 1 */
#include "compat/coduo_native_x87.h"

#define DEBUG_CIRCLE_SEGMENTS 16
#define DEBUG_CIRCLE_STEP_RADIANS 0.39269908169872414 /* original double 0x3fd921fb54442d18 */
#define DEBUG_PI 3.141592653589793 /* original double 0x400921fb54442d18 */
#define DEBUG_DEGREES_PER_HALF_CIRCLE 180.0 /* original double 0x4066800000000000 */

void trap_AddDebugLine(const float *start, const float *end, const float *color,
                              int depthTest, int duration);

static const int debugBoxEdges[12][2] = {
    {0, 1},
    {0, 2},
    {0, 4},
    {1, 3},
    {1, 5},
    {2, 3},
    {2, 6},
    {3, 7},
    {4, 5},
    {4, 6},
    {5, 7},
    {6, 7},
};

/* NOT_FROM_ORIGINAL_SOURCE: local extraction of G_DebugCircle view-origin setup. */
static void game_compat_debug_view_origin(vec3_t origin)
{
    const gclient_t *client = level.clients;

    origin[0] = client->ps.psOrigin[0];
    origin[1] = client->ps.psOrigin[1];
    origin[2] = client->ps.psOrigin[2] + client->ps.viewHeightCurrent;
}

/* VERIFIED_DECOMPILER(0x51258, 61258_G_DebugLine.c, VERIFY-DEBUG-DRAW-PACKET-2026-06-17): DATAFLOW_VERIFIED - trap_AddDebugLine wrapper forwards all five arguments in order. */
/* 0x51258 G_DebugLine */
void G_DebugLine(const float *start, const float *end, const float *color, int depthTest,
                 int duration)
{
    trap_AddDebugLine(start, end, color, depthTest, duration);
}

/* VERIFIED_DECOMPILER(0x51297, 61297_G_DebugBox.c, VERIFY-DEBUG-DRAW-PACKET-2026-06-17): DATAFLOW_VERIFIED - eight corner stores, mins/maxs bit selection, 12 edge-table lookups, and G_DebugLine arguments checked. */
/* 0x51297 G_DebugBox */
void G_DebugBox(const float *mins, const float *maxs, const float *color, int depthTest,
                int duration)
{
    vec3_t corners[8];

    for (int corner = 0; corner < 8; corner++) {
        for (int axis = 0; axis < 3; axis++) {
            corners[corner][axis] = ((corner >> axis) & 1) ? maxs[axis] : mins[axis];
        }
    }

    for (int edge = 0; edge < 12; edge++) {
        G_DebugLine(corners[debugBoxEdges[edge][0]], corners[debugBoxEdges[edge][1]],
                    color, depthTest, duration);
    }
}

/* VERIFIED_DECOMPILER(0x518ec, 618ec_FUN_000618ec.c, VERIFY-DEBUG-DRAW-PACKET-2026-06-17): DATAFLOW_VERIFIED - x87 cos/sin sequence and sine/cosine output pointer order checked. */
/* 0x518ec FUN_000618ec */
/* Stock 0x518f9 is `fld DWORD; fsincos; fstp(cos); fstp(sin)`.  Native x87
 * targets use that exact sequence; only non-x87 targets use the permitted libm
 * fallback. */
static void DebugSinCos(float radians, float *sineOut, float *cosineOut)
{
#if defined(__i386__) || defined(__x86_64__)
    coduo_x87_sincosf(radians, sineOut, cosineOut);
#else
    long double angle = (long double)radians;

    *cosineOut = (float)cosl(angle);
    *sineOut = (float)sinl(angle);
#endif
}

/* VERIFIED_DECOMPILER(0x51465, 61465_G_DebugCircleEx.c, VERIFY-DEBUG-DRAW-PACKET-2026-06-17): DATAFLOW_VERIFIED - normal basis setup, 16-point sin/cos loop, radius scaling, point stores, and closed line loop checked. */
/* 0x51465 G_DebugCircleEx */
void G_DebugCircleEx(const float *center, float radius, const float *normal,
                     const float *color, int depthTest, int duration)
{
    vec3_t normalizedNormal;
    vec3_t tangent;
    vec3_t bitangent;
    vec3_t points[DEBUG_CIRCLE_SEGMENTS];

    VectorNormalize2(normal, normalizedNormal);
    PerpendicularVector(tangent, normalizedNormal);
    CrossProduct(normalizedNormal, tangent, bitangent);

    for (int index = 0; index < DEBUG_CIRCLE_SEGMENTS; index++) {
        float sine;
        float cosine;
        /* 0x514e3: fild(index) * STEP_RADIANS(QWORD double), one float store. */
#if EMULATE_X87
        float radians = x87f_store_f32(x87f_mul(
            x87f_load_i32(index),
            x87f_load_f64(DEBUG_CIRCLE_STEP_RADIANS)));
#else
        float radians = (float)((long double)index *
                                (long double)DEBUG_CIRCLE_STEP_RADIANS);
#endif

        DebugSinCos(radians, &sine, &cosine);
        sine *= radius;
        cosine *= radius;

        /* 0x51554..0x51697: stock stores center+bitangent*sine to the point
         * (one float rounding per component), then adds tangent*cosine in a
         * second rounding (two VectorMA passes) -> MA shims. */
#if EMULATE_X87
        for (int c = 0; c < 3; c++) {
            points[index][c] = x87f_store_f32(x87f_add(
                x87f_mul(x87f_load_f32(bitangent[c]), x87f_load_f32(sine)),
                x87f_load_f32(center[c])));
            points[index][c] = x87f_store_f32(x87f_add(
                x87f_mul(x87f_load_f32(tangent[c]), x87f_load_f32(cosine)),
                x87f_load_f32(points[index][c])));
        }
#else
        points[index][0] = center[0] + bitangent[0] * sine;
        points[index][1] = center[1] + bitangent[1] * sine;
        points[index][2] = center[2] + bitangent[2] * sine;
        points[index][0] += tangent[0] * cosine;
        points[index][1] += tangent[1] * cosine;
        points[index][2] += tangent[2] * cosine;
#endif
    }

    for (int index = 0; index < DEBUG_CIRCLE_SEGMENTS; index++) {
        G_DebugLine(points[index], points[(index + 1) & 0xf], color, depthTest, duration);
    }
}

/* VERIFIED_DECOMPILER(0x5139d, 6139d_G_DebugCircle.c, VERIFY-DEBUG-DRAW-PACKET-2026-06-17): DATAFLOW_VERIFIED - view-origin normal path, up-normal branch, level/client offsets, and G_DebugCircleEx argument order checked. */
/* 0x5139d G_DebugCircle */
void G_DebugCircle(const float *center, float radius, const float *color, int depthTest,
                   qboolean useUpNormal, int duration)
{
    vec3_t normal;

    if (useUpNormal == 0) {
        vec3_t viewOrigin;

        game_compat_debug_view_origin(viewOrigin);
        normal[0] = center[0] - viewOrigin[0];
        normal[1] = center[1] - viewOrigin[1];
        normal[2] = center[2] - viewOrigin[2];
    } else {
        normal[0] = 0.0f;
        normal[1] = 0.0f;
        normal[2] = 1.0f;
    }

    G_DebugCircleEx(center, radius, normal, color, depthTest, duration);
}

/* VERIFIED_DECOMPILER(0x51725, 61725_G_DebugArc.c, VERIFY-DEBUG-DRAW-PACKET-2026-06-17): DATAFLOW_VERIFIED - step calculation, negative-range adjustment, radian conversion constants, 16 point stores, and 15 segment draw loop checked. */
/* 0x51725 G_DebugArc */
void G_DebugArc(const float *center, float radius, float startAngle, float endAngle,
                const float *color, int depthTest, int duration)
{
    vec3_t points[DEBUG_CIRCLE_SEGMENTS];
    /* 0x5173a: (endAngle - startAngle) / 15.0f kept 80-bit, one store -> shim;
     * startAngle -= 360.0f is a single sub (native). */
#if EMULATE_X87
    float step = x87f_store_f32(x87f_div(
        x87f_sub(x87f_load_f32(endAngle), x87f_load_f32(startAngle)),
        x87f_load_f32(15.0f)));
#else
    float step = (endAngle - startAngle) / 15.0f;
#endif

    if (step < 0.0f) {
        startAngle -= 360.0f;
#if EMULATE_X87
        step = x87f_store_f32(x87f_div(
            x87f_sub(x87f_load_f32(endAngle), x87f_load_f32(startAngle)),
            x87f_load_f32(15.0f)));
#else
        step = (endAngle - startAngle) / 15.0f;
#endif
    }

    for (int index = 0; index < DEBUG_CIRCLE_SEGMENTS; index++) {
        float sine;
        float cosine;
        /* 0x5179d..0x517bc: (fild(index)*step + startAngle) * PI(QWORD double)
         * / DEGREES(QWORD double) kept 80-bit, one float store. */
#if EMULATE_X87
        float radians = x87f_store_f32(x87f_div(
            x87f_mul(
                x87f_add(x87f_mul(x87f_load_i32(index), x87f_load_f32(step)),
                         x87f_load_f32(startAngle)),
                x87f_load_f64(DEBUG_PI)),
            x87f_load_f64(DEBUG_DEGREES_PER_HALF_CIRCLE)));
#else
        float radians = (float)((((long double)index * (long double)step +
                                  (long double)startAngle) *
                                 (long double)DEBUG_PI) /
                                (long double)DEBUG_DEGREES_PER_HALF_CIRCLE);
#endif

        DebugSinCos(radians, &sine, &cosine);
        /* center[k] + trig*radius MA, one store -> shim. */
#if EMULATE_X87
        points[index][0] = x87f_store_f32(x87f_add(
            x87f_mul(x87f_load_f32(cosine), x87f_load_f32(radius)),
            x87f_load_f32(center[0])));
        points[index][1] = x87f_store_f32(x87f_add(
            x87f_mul(x87f_load_f32(sine), x87f_load_f32(radius)),
            x87f_load_f32(center[1])));
#else
        points[index][0] = center[0] + cosine * radius;
        points[index][1] = center[1] + sine * radius;
#endif
        points[index][2] = center[2];
    }

    for (int index = 0; index < DEBUG_CIRCLE_SEGMENTS - 1; index++) {
        G_DebugLine(points[index], points[index + 1], color, depthTest, duration);
    }
}
