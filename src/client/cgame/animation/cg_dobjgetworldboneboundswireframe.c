// Source: uo_cgame_mp_x86.dll 0x30020190..0x30020538
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30020190_30020538.mcode
//
// This is the recovered parent of the former 0x300201b2 mid-function fragment.
// It first builds the 24 local bone-box edge endpoints, then rotates every point
// through the entity's negate-right axis and adds the entity origin.

#include "client/cgame/client_recovered.h"

qboolean CG_DObjGetWorldBoneBoundsWireframe(struct DObj_s *dobj, centity_t *cent, const char *tagName, vec3_t points[24])
{
    if (!CG_DObjGetBoneBoundsWireframe(dobj, tagName, points))
        return qfalse;

    vec3_t forward;
    vec3_t right;
    vec3_t up;
    AngleVectors(cent->lerpAngles, forward, right, up);

    /* 0x300201cc..0x300201f0 loads the three binary32 right components, applies
     * FCHS, and retains all three values in x87 across the complete point loop.
     * The X origin is likewise retained; Y/Z remain binary32 memory operands. */
    const long double negRightX = -(long double)right[0];
    const long double negRightY = -(long double)right[1];
    const long double negRightZ = -(long double)right[2];
    const long double originX = cent->lerpOrigin[0];
    const float originY = cent->lerpOrigin[1];
    const float originZ = cent->lerpOrigin[2];

    /* Every endpoint (unrolled 8x per loop pass, 0x30020210..0x30020528) sums
     * its terms as ((z*axis2 + x*axis0) + y*axis1) + origin -- the origin is
     * FADDed LAST (FADD ST0,ST1 / FADD [ESP+0x44] / FADD [ESP+0x48]), after
     * the two FADDP roundings, so the C term order must put lerpOrigin at
     * the end of the chain (each 80-bit add rounds; grouping is observable). */
    for (int32_t i = 0; i < 24; ++i) {
        const long double x = points[i][0];
        const long double y = points[i][1];
        const long double z = points[i][2];

        const long double outX = ((z * up[0] + x * forward[0]) + y * negRightX) + originX;
        const long double outY = ((x * forward[1] + z * up[1]) + y * negRightY) + originY;
        const long double outZ = ((x * forward[2] + z * up[2]) + y * negRightZ) + originZ;

        /* The unrolled body rounds Z to a stack m32 first and publishes its raw
         * dword, then stores X and Y from the two remaining x87 results. */
        points[i][2] = (float)outZ;
        points[i][0] = (float)outX;
        points[i][1] = (float)outY;
    }
    return qtrue;
}
