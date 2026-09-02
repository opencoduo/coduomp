// Complete math/transform leaves recovered from their exact x87 instruction flow.

#include "client/cgame/client_recovered.h"
#include "client/cgame/globals.h"

// Source RVA: 0x3003b4d0
long double CG_CubicInterpolate(float t, float p0, float p1, float p2, float p3)
{
    /* 0x3003b4d0..0x3003b4ea: a = p3-p2+p1-p0 stays in an st register (no
     * float store) before both uses; long double keeps it unrounded. */
    const long double tWide = t;
    const long double p0Wide = p0;
    const long double p1Wide = p1;
    const long double p2Wide = p2;
    const long double p3Wide = p3;
    const long double a = p3Wide - p2Wide + p1Wide - p0Wide;

    return p1Wide + tWide * ((p2Wide - p0Wide) + tWide * ((p0Wide - p1Wide) - a + tWide * a));
}

// Source RVA: 0x3001ffd0
void TransformPoint(const float matrix[16], const vec3_t point, vec3_t out)
{
    /* FADDP chain order per component (0x3001ffd0/0x3001ffe9/0x30020004):
     * out[0] sums (m8*p2 + m4*p1) + m0*p0; out[1]/out[2] sum
     * (m1*p0 + m9*p2) + m5*p1 shape.  Association changes the rounding. */
    const long double outX =
        (((long double)matrix[8] * point[2] + (long double)matrix[4] * point[1]) + (long double)matrix[0] * point[0]) + matrix[12];
    out[0] = (float)outX;

    const long double outY =
        (((long double)matrix[1] * point[0] + (long double)matrix[9] * point[2]) + (long double)matrix[5] * point[1]) + matrix[13];
    out[1] = (float)outY;

    const long double outZ =
        (((long double)matrix[2] * point[0] + (long double)matrix[10] * point[2]) + (long double)matrix[6] * point[1]) + matrix[14];
    out[2] = (float)outZ;
}

// Source RVA: 0x30044cc0
void ApplyViewKickNudge(vec3_t angles)
{
    angles[1] -= cg_weaponSwayOffset[1];
    angles[2] += cg_weaponSwayOffset[2];
}

// Source RVA: 0x3003c7a0
void SetShellshockState(shellshock_t *params, int32_t startTime, int32_t duration)
{
    cg_shellShockSwayParams = params;
    cg_shellShockSwayStartTime = startTime;
    cg_shellShockSwayDuration = duration;
}
