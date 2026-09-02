#include "../client_recovered.h"
#include "../globals.h"
#include "compat/coduo_native_x87.h"


// Source: uo_cgame_mp_x86.dll 0x30019370..0x3001951c
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30019370_3001951c.mcode
//
// Naming: the .mcode "CG_ParseImpactEffects" tag is a pure win-size==0x1ac guess
// against a same-size symbol in a different bank and is REJECTED — this body has no
// token/string/CSV parsing, only float pool angle/projection math. It matches the
// pre-existing role-name decl CG_ProjectDamageDirToScreen (client_recovered.h),
// which was derived from the consumer CG_DrawDamageDirectionIndicators; that name is reused.
//
// Register ABI: the two output float addresses arrive in EDI (screen X) and ESI
// (screen Y). All three callers LEA two adjacent stack floats into ESI/EDI before
// the CALL (e.g. 0x3001a9cc: LEA ESI,[ESP+8] / LEA EDI,[ESP+0xc]). Expressed here
// as (float *outX, float *outY).
//
// What it computes:
//   1. Build a forward unit vector from the cached {pitch,yaw} angle pair
//      (cg_effectProjAnglePitch @0x3048b0b8, cg_effectProjAngleYaw @0x3048b0bc),
//      each * DEG2RAD, via two FSINCOS blocks (this is AngleVectors' forward with
//      roll unused):
//        fwd = { cos(pitch)*cos(yaw), cos(pitch)*sin(yaw), -sin(pitch) }.
//   2. Perspective-project fwd through the current view basis:
//        depth = dot(fwd, cg_refdef.viewaxis[0])      (view Z / forward)
//        sx    = dot(fwd, cg_refdef.viewaxis[1])         (view X)
//        sy    = dot(fwd, cg_refdef.viewaxis[2])         (view Y)
//      then map to a 640x480 virtual screen:
//        *outX = sx / (tan(fovX/2) * depth) * -320
//        *outY = sy / (tan(fovY/2) * depth) * -240
//      where fovX/2 = cg_viewProjScaleA * (DEG2RAD/2), fovY/2 = cg_viewProjScaleB *
//      (DEG2RAD/2). 320 = 640/2, 240 = 480/2 (both negated for screen orientation).
//   3. Gate: if depth <= 0 (point behind the view plane) OR either projection scale
//      (cg_refdef.fov_x / fov_y) <= 0, both outputs are set to 0.
//      The three FCOMP 0.0 / FNSTSW / TEST AH,0x41 / JNP guards fall through (keep
//      projecting) when the value is strictly positive OR unordered; NaNs therefore
//      follow the projection path, matching the target status-word tests.
//
// 0x3006bfa0 is the statically-linked MSVC CRT tan(double): SUB ESP,0xc; FSTP
// [ESP] (store double arg); CALL 0x3006c2b8 (inf/NaN classifier); CALL 0x3006bfbd
// (the FPTAN core: fldcw guard, FPTAN, FPREM1 range reduction). The established
// hardware-x87 tangent adapter models the raw ST0 argument/result on Intel/AMD;
// this does not add client ARM64 emulation.
//
// Float constants dumped exactly from .rdata (objdump -s -j .rdata):
//   0x3007bcec = 0x00000000 (0.0f, the comparison zero)
//   0x3007bd70 = 0x3c8efa35 (0.017453292f, DEG2RAD = pi/180)
//   0x3007be78 = 0x3c0efa35 (0.008726646f, DEG2RAD/2 = pi/360; converts a full-fov
//                            degrees value to a half-fov radians value for tan())
//   0x3007bf30 = 0xc3a00000 (-320.0f, -screenWidth/2)
//   0x3007bf2c = 0xc3700000 (-240.0f, -screenHeight/2)

/* DEG2RAD (pi/180), exact .rdata value @0x3007bd70. */
static const float CG_DEG2RAD = 0.017453292f;
/* DEG2RAD/2 (pi/360), exact .rdata value @0x3007be78; the projection scales hold a
 * full field-of-view in degrees, so a half is taken before converting to radians. */
static const float CG_DEG2RAD_HALF = 0.008726646f;
/* Virtual-screen half extents (negated), exact .rdata values @0x3007bf30 / 0x3007bf2c. */
enum {
    CG_PROJ_SCREEN_HALF_W = 320,
    CG_PROJ_SCREEN_HALF_H = 240
};

void CG_ProjectDamageDirToScreen(float *outX, float *outY)
{
    /* 0x30019373 loads yaw first and 0x30019379 loads pitch before either
     * conversion or FSINCOS block starts. */
    float yawDegrees = cg_effectProjAngleYaw;
    float pitchDegrees = cg_effectProjAnglePitch;
    float yawRad = (float)((long double)yawDegrees * (long double)CG_DEG2RAD);

    /* 0x30019382..0x300193eb: yaw FSINCOS first, then pitch FSINCOS. Each
     * stores cosine before sine. */
    float sinYaw;
    float cosYaw;
    float sinPitch;
    float cosPitch;
    coduo_x87_sincosf(yawRad, &sinYaw, &cosYaw);
    float pitchRad = (float)((long double)pitchDegrees * (long double)CG_DEG2RAD);
    coduo_x87_sincosf(pitchRad, &sinPitch, &cosPitch);

    vec3_t fwd;
    fwd[0] = (float)((long double)cosPitch * (long double)cosYaw);
    fwd[1] = (float)((long double)cosPitch * (long double)sinYaw);
    fwd[2] = -sinPitch;           /* 0x30019403: FCHS(sinPitch) -> t2c */

    /* 0x3001940d..0x3001942f: depth = dot(fwd, cg_refdef.viewaxis[0]).
     * x87 association: the compiler sums the z and y terms FIRST (FADDP at
     * 0x30019421) and adds the x term to that (FADDP at 0x3001942d), then rounds
     * once (FSTP at 0x3001942f). FADD is not associative, so the (z+y)+x
     * grouping is reproduced literally rather than written left-to-right. */
    float depth = (float)(((long double)cg_refdef.viewaxis[0][2] * (long double)fwd[2] +
                           (long double)cg_refdef.viewaxis[0][1] * (long double)fwd[1]) +
                          (long double)cg_refdef.viewaxis[0][0] * (long double)fwd[0]);

    /* 0x30019433..0x30019470: gate — behind the view plane, or a non-positive
     * projection scale, zeroes both outputs. */
    if (!(depth <= 0.0f) && !(cg_refdef.fov_x <= 0.0f) && !(cg_refdef.fov_y <= 0.0f)) {
        /* 0x30019476..0x300194bd: screen X. Same (z+y)+x FADDP grouping as depth
         * (FADDP at 0x3001948a then 0x30019496). */
        /* sx is NOT stored: the dot-sum stays in an x87 register across the tan
         * call and is consumed directly by the FDIVP at 0x300194b5, so it is
         * never rounded to float (hence long double, not float). tanX by
         * contrast IS genuinely rounded -- FSTP DWORD at 0x300194a9 followed by
         * a reload of the same slot at 0x300194ad -- so it stays `float`. */
        long double sx =
            ((long double)cg_refdef.viewaxis[1][2] * (long double)fwd[2] + (long double)cg_refdef.viewaxis[1][1] * (long double)fwd[1]) +
            (long double)cg_refdef.viewaxis[1][0] * (long double)fwd[0];
        long double tanXRaw = coduo_x87_tanl((long double)cg_refdef.fov_x * (long double)CG_DEG2RAD_HALF);
        float tanX = (float)tanXRaw;
        long double denominatorX = (long double)tanX * (long double)depth;
        *outX = (float)((sx / denominatorX) * (long double)-(float)CG_PROJ_SCREEN_HALF_W);

        /* 0x300194bf..0x30019506: screen Y. Same (z+y)+x FADDP grouping as depth
         * (FADDP at 0x300194d3 then 0x300194df). */
        /* sy: register-held like sx (consumed by the FDIVP at 0x300194fe);
         * tanY genuinely rounded (FSTP 0x300194f2 + reload 0x300194f6). */
        long double sy =
            ((long double)cg_refdef.viewaxis[2][2] * (long double)fwd[2] + (long double)cg_refdef.viewaxis[2][1] * (long double)fwd[1]) +
            (long double)cg_refdef.viewaxis[2][0] * (long double)fwd[0];
        long double tanYRaw = coduo_x87_tanl((long double)cg_refdef.fov_y * (long double)CG_DEG2RAD_HALF);
        float tanY = (float)tanYRaw;
        long double denominatorY = (long double)tanY * (long double)depth;
        *outY = (float)((sy / denominatorY) * (long double)-(float)CG_PROJ_SCREEN_HALF_H);
    } else {
        /* 0x3001950c/0x30019512: MOV [EDI],0 / MOV [ESI],0. */
        *outX = 0.0f;
        *outY = 0.0f;
    }
}
