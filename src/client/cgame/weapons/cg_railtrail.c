// Source: uo_cgame_mp_x86.dll 0x30043190..0x300435b6
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30043190_300435b6.mcode
//
// CG_RailTrail -- emit rail-core local entities through CG_SpawnRailCoreSegment.
// Identity is proven by its sole caller in CG_EntityEvent and by the callee at
// 0x300430a0, which creates RT_RAIL_CORE local entities using the railCore shader.
// The .mcode Item_Model_Paint label is rejected as a forbidden size match.
//
// colorIndex 0/1/69..72 select one-segment RGB colors. Index 5 builds a 16-edge
// circle in the XY plane centered at start with radius end[0]. Index 3 builds a red
// axis-aligned box; index 2 and all remaining values build the same box in cyan.

#include "../client_recovered.h"
#include "compat/coduo_native_x87.h"

#include <math.h>
#include <stdint.h>

enum {
    RAIL_COLOR_CYAN = 0,
    RAIL_COLOR_RED = 1,
    RAIL_BOX_CYAN = 2,
    RAIL_BOX_RED = 3,
    RAIL_RING = 5,
    RAIL_COLOR_YELLOW = 69,
    RAIL_COLOR_GREEN = 70,
    RAIL_COLOR_BLUE = 71,
    RAIL_COLOR_WHITE = 72,
    RAIL_RING_SEGMENTS = 16
};

void CG_RailTrail(int32_t colorIndex, const vec3_t start, const vec3_t end)
{
    vec3_t color = {0.0f, 1.0f, 1.0f};

    switch (colorIndex) {
    case RAIL_COLOR_CYAN:
        CG_SpawnRailCoreSegment(start, end, color);
        return;
    case RAIL_COLOR_RED:
        color[0] = 1.0f;
        color[1] = 0.0f;
        color[2] = 0.0f;
        CG_SpawnRailCoreSegment(start, end, color);
        return;
    case RAIL_COLOR_YELLOW:
        color[0] = 1.0f;
        color[1] = 1.0f;
        color[2] = 0.0f;
        CG_SpawnRailCoreSegment(start, end, color);
        return;
    case RAIL_COLOR_GREEN:
        color[0] = 0.0f;
        color[1] = 1.0f;
        color[2] = 0.0f;
        CG_SpawnRailCoreSegment(start, end, color);
        return;
    case RAIL_COLOR_BLUE:
        color[0] = 0.0f;
        color[1] = 0.0f;
        color[2] = 1.0f;
        CG_SpawnRailCoreSegment(start, end, color);
        return;
    case RAIL_COLOR_WHITE:
        color[0] = 1.0f;
        color[1] = 1.0f;
        color[2] = 1.0f;
        CG_SpawnRailCoreSegment(start, end, color);
        return;
    default:
        break;
    }

    if (colorIndex == RAIL_RING) {
        vec3_t axis = {0.0f, 0.0f, 1.0f};
        vec3_t radial;
        vec3_t tangent;
        vec3_t points[RAIL_RING_SEGMENTS];
        float radius = end[0];
        int32_t i;

        PerpendicularVector(radial, axis);
        /* 0x30043321..0x3004332b: ECX=radial, EAX=axis, EDX=tangent. */
        CrossProduct(radial, axis, tangent);

        for (i = 0; i < RAIL_RING_SEGMENTS; ++i) {
            /* i enters via a bare FILD fed straight into FMUL 0.3926991f (0x30043350
             * FILD; 0x30043354 FMUL) with no FSTP DWORD between, so drop the (float)
             * cast (Class 4). Constant 0x3007bedc = 0x3ec90fdb = 0.3926991f (2*pi/16). */
            float angle = (float)((long double)i * (long double)0.3926991f);
            float sine;
            float cosine;
            coduo_x87_sincosf(angle, &sine, &cosine);
            float s = (float)((long double)sine * (long double)radius);
            float c = (float)((long double)cosine * (long double)radius);
            /* 0x30043397..0x300433db keeps both products live through FADDP
             * and adds the start component before the sole float store. */
            points[i][0] =
                (float)((long double)radial[0] * (long double)c + (long double)tangent[0] * (long double)s + (long double)start[0]);
            points[i][1] =
                (float)((long double)radial[1] * (long double)c + (long double)tangent[1] * (long double)s + (long double)start[1]);
            points[i][2] =
                (float)((long double)radial[2] * (long double)c + (long double)tangent[2] * (long double)s + (long double)start[2]);
        }

        for (i = 0; i < RAIL_RING_SEGMENTS; ++i) {
            CG_SpawnRailCoreSegment(points[i], points[(i + 1) & (RAIL_RING_SEGMENTS - 1)], color);
        }
        return;
    }

    if (colorIndex == RAIL_BOX_RED) {
        color[0] = 1.0f;
        color[1] = 0.0f;
        color[2] = 0.0f;
    }

    /* The 12 calls at 0x30043493..0x300435a4 are the twelve edges of the box
     * spanned by the two opposite corners. The ordering below matches the machine:
     * three edges from end, three from start, then the six remaining connectors. */
    {
        vec3_t c001 = {start[0], start[1], end[2]};
        vec3_t c010 = {start[0], end[1], start[2]};
        vec3_t c011 = {start[0], end[1], end[2]};
        vec3_t c100 = {end[0], start[1], start[2]};
        vec3_t c101 = {end[0], start[1], end[2]};
        vec3_t c110 = {end[0], end[1], start[2]};

        CG_SpawnRailCoreSegment(end, c011, color);
        CG_SpawnRailCoreSegment(end, c101, color);
        CG_SpawnRailCoreSegment(end, c110, color);
        CG_SpawnRailCoreSegment(start, c100, color);
        CG_SpawnRailCoreSegment(start, c010, color);
        CG_SpawnRailCoreSegment(start, c001, color);
        CG_SpawnRailCoreSegment(c101, c001, color);
        CG_SpawnRailCoreSegment(c001, c011, color);
        CG_SpawnRailCoreSegment(c011, c010, color);
        CG_SpawnRailCoreSegment(c101, c100, color);
        CG_SpawnRailCoreSegment(c100, c110, color);
        CG_SpawnRailCoreSegment(c110, c010, color);
    }
}
