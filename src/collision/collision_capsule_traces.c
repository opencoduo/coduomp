#include "collision_capsule_traces.h"

#include "collision_brush_traces.h"
#include "collision_geometry.h"
#include "collision_queries.h"
#include "compat/coduo_x87emu.h"
#include "math/q_math.h"

#include <math.h>
#include <stdint.h>

extern collisionModel_t cm_boxModel;
extern collisionBrush_t *cm_boxBrush;

/*
 * Complete temporary-capsule collision subsystem. The authoritative entry
 * points are:
 *
 *   CoDUOMP.exe  0x004262f0..0x0042657c, 0x00426580..0x004266ee,
 *                0x00427300..0x00427bca, 0x00429140..0x0042981d
 *   coduo_lnxded 0x0805848f..0x080589fd, 0x080599e6..0x0805a58e,
 *                0x0805bf30..0x0805c909
 *
 * The platform bodies retain their proven x87 operation graphs and spill
 * points. Linux's exported symbols provide the canonical operation names;
 * the former Windows-only Probe/TempBox reconstruction names described these
 * same functions and were normalized before extraction.
 */
#if defined(WINDOWS_BEHAVIOR)
/* Source: CoDUOMP.exe 0x004262f0..0x0042657c.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004262f0_0042657d.mcode.
 * Name: same-family Linux symbol CM_TestCapsuleInCapsule. The helper
 * checks the two capsule cap centers against the temporary box's upper and
 * lower rounded ends, then checks the intervening cylinder band. */
void CM_TestCapsuleInCapsule(
    traceWork_t *traceWork)
{
    vec3_t startOffsetMax;
    vec3_t startOffsetMin;
    vec3_t boxCenter;
    vec3_t boxMaxsFromCenter;

    for (int32_t axis = 0; axis < 3; ++axis) {
        startOffsetMax[axis] =
            traceWork->start[axis] +
            traceWork->sphere.offset[axis];
        startOffsetMin[axis] =
            traceWork->start[axis] -
            traceWork->sphere.offset[axis];
        boxCenter[axis] =
            (cm_boxModel.mins[axis] +
             cm_boxModel.maxs[axis]) *
            0.5f;
        boxMaxsFromCenter[axis] =
            cm_boxModel.maxs[axis] -
            boxCenter[axis];
    }

    const float lowerZFromCenter =
        boxMaxsFromCenter[0] >
                boxMaxsFromCenter[2]
            ? boxMaxsFromCenter[2]
            : boxMaxsFromCenter[0];
    const float verticalSpan =
        boxMaxsFromCenter[2] -
        lowerZFromCenter;
    const float expandedRadius =
        traceWork->sphere.radius +
        lowerZFromCenter;
    const float radiusSquared =
        expandedRadius * expandedRadius;

    vec3_t tempPoint = {
        boxCenter[0],
        boxCenter[1],
        boxCenter[2] + verticalSpan
    };

    for (int32_t cap = 0; cap < 2; ++cap) {
        const float *const startOffset =
            cap == 0 ? startOffsetMax : startOffsetMin;
        const long double distanceSquared =
            CM_DistanceSquared(
                tempPoint, startOffset);
        if (distanceSquared < radiusSquared) {
            traceWork->trace.allsolid = qtrue;
            traceWork->trace.startsolid = qtrue;
            traceWork->trace.fraction = 0.0f;
        }
    }

    tempPoint[2] =
        boxCenter[2] - verticalSpan;
    for (int32_t cap = 0; cap < 2; ++cap) {
        const float *const startOffset =
            cap == 0 ? startOffsetMax : startOffsetMin;
        const long double distanceSquared =
            CM_DistanceSquared(
                tempPoint, startOffset);
        if (distanceSquared < radiusSquared) {
            traceWork->trace.allsolid = qtrue;
            traceWork->trace.startsolid = qtrue;
            traceWork->trace.fraction = 0.0f;
        }
    }

    const float startZDelta =
        traceWork->start[2] - boxCenter[2];
    const float cylinderHalfHeight =
        verticalSpan +
        traceWork->sphere.halfheight -
        traceWork->sphere.radius;
    if (startZDelta <= cylinderHalfHeight &&
        -cylinderHalfHeight <= startZDelta) {
        tempPoint[2] = 0.0f;
        startOffsetMax[2] = 0.0f;

        const long double distanceSquared =
            CM_DistanceSquared(
                startOffsetMax, tempPoint);
        if (distanceSquared < radiusSquared) {
            traceWork->trace.allsolid = qtrue;
            traceWork->trace.startsolid = qtrue;
            traceWork->trace.fraction = 0.0f;
        }
    }
}

/* Source: CoDUOMP.exe 0x00426580..0x004266ee.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00426580_004266ef.mcode.
 * Name: same-family Linux symbol CM_TestBoundingBoxInCapsule. The
 * temporary model is treated as a capsule: the trace endpoints are recentered
 * on it, its x/z extents provide the capsule radius and half-height, and the
 * moving bounding box is installed as the brush tested for containment. */
void CM_TestBoundingBoxInCapsule(
    traceWork_t *traceWork)
{
    vec3_t boxCenter;
    vec3_t boxMaxsFromCenter;

    for (int32_t axis = 0; axis < 3; ++axis) {
        boxCenter[axis] =
            (cm_boxModel.mins[axis] +
             cm_boxModel.maxs[axis]) *
            0.5f;
        boxMaxsFromCenter[axis] =
            cm_boxModel.maxs[axis] -
            boxCenter[axis];
        traceWork->start[axis] -=
            boxCenter[axis];
        traceWork->end[axis] -=
            boxCenter[axis];
    }

    traceWork->sphere.use = qtrue;
    traceWork->sphere.radius =
        boxMaxsFromCenter[2] <
                boxMaxsFromCenter[0]
            ? boxMaxsFromCenter[2]
            : boxMaxsFromCenter[0];
    traceWork->sphere.halfheight =
        boxMaxsFromCenter[2];
    traceWork->sphere.offset[0] = 0.0f;
    traceWork->sphere.offset[1] = 0.0f;
    traceWork->sphere.offset[2] =
        boxMaxsFromCenter[2] -
        traceWork->sphere.radius;

    CM_TempBoxModel(
        traceWork->mins, traceWork->maxs,
        cm_boxBrush->contents, qfalse);
    CM_TestBoxInBrush(traceWork, cm_boxBrush);
}
/* Source: CoDUOMP.exe 0x00427300..0x004274d3.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00427300_004274d4.mcode.
 * Name: same-family Linux symbol CM_TraceSphereThroughSphere. Returns qfalse
 * when the probe installs a blocking result and qtrue when the swept capsule
 * misses the sphere or cannot improve the current fraction. */
qboolean CM_TraceSphereThroughSphere(
    traceWork_t *traceWork,
    const vec3_t start, const vec3_t end,
    const vec3_t sphereOrigin,
    float sphereRadius)
{
    vec3_t delta = {
        start[0] - sphereOrigin[0],
        start[1] - sphereOrigin[1],
        start[2] - sphereOrigin[2]
    };

    const long double combinedRadiusRaw =
        (long double)sphereRadius +
        traceWork->sphere.radius;
    const float radiusSquared = (float)(
        combinedRadiusRaw * combinedRadiusRaw);
    /* 0x0042733a..0x0042735e accumulates Z + Y + X, stores the distance as
     * float, and compares the retained x87 result against zero. */
    const long double startDistanceMinusRadiusRaw =
        (((long double)delta[2] * (long double)delta[2] +
          (long double)delta[1] * (long double)delta[1]) +
         (long double)delta[0] * (long double)delta[0]) -
        (long double)radiusSquared;
    const float startDistanceMinusRadius =
        (float)startDistanceMinusRadiusRaw;

    if (startDistanceMinusRadiusRaw <= (long double)0.0f) {
        traceWork->trace.fraction = 0.0f;
        traceWork->trace.startsolid = qtrue;
        VectorNormalize2(
            delta, traceWork->trace.normal);
        traceWork->trace.contents =
            cm_boxBrush->contents;

        for (int32_t axis = 0; axis < 3; ++axis) {
            delta[axis] =
                end[axis] -
                sphereOrigin[axis];
        }
        const long double endDistanceSquaredRaw =
            ((long double)delta[2] * delta[2] +
             (long double)delta[1] * delta[1]) +
            (long double)delta[0] * delta[0];
        if (endDistanceSquaredRaw <=
            (long double)radiusSquared) {
            traceWork->trace.allsolid = qtrue;
        }
        return qfalse;
    }

    /* 0x004273df..0x00427407 likewise accumulates Z + Y + X and tests the
     * retained dot product after saving its float copy. */
    const long double deltaDotRaw =
        ((long double)delta[2] * (long double)traceWork->delta[2] +
         (long double)delta[1] * (long double)traceWork->delta[1]) +
        (long double)delta[0] * (long double)traceWork->delta[0];
    const float deltaDot = (float)deltaDotRaw;
    if (deltaDotRaw >= (long double)0.0f)
        return qtrue;

    const long double discriminantRaw =
        (long double)deltaDot * (long double)deltaDot -
        (long double)traceWork->deltaLengthSquared *
            (long double)startDistanceMinusRadius;
    const float discriminant = (float)discriminantRaw;
    if (discriminantRaw < (long double)0.0f)
        return qtrue;

    vec3_t normal;
    const float startDistance =
        VectorNormalize2(delta, normal);
    /* 0x00427448..0x0042746a retains both quotients across the base-fraction
     * float store, then adds them and compares the wide enter fraction. */
    const long double epsilonFractionRaw =
        (long double)startDistance * (long double)0.125f /
        (long double)deltaDot;
    const long double baseFractionRaw =
        (-(long double)deltaDot -
         sqrt((double)discriminant)) /
        (long double)traceWork->deltaLengthSquared;
    const float baseFraction = (float)baseFractionRaw;
    const long double enterFractionRaw =
        baseFractionRaw + epsilonFractionRaw;
    const float enterFraction = (float)enterFractionRaw;
    (void)baseFraction;

    if (!(enterFractionRaw <
          (long double)traceWork->trace.fraction)) {
        return qtrue;
    }

    traceWork->trace.fraction =
        enterFractionRaw > (long double)0.0f
            ? enterFraction
            : 0.0f;
    for (int32_t axis = 0; axis < 3; ++axis) {
        traceWork->trace.normal[axis] =
            normal[axis];
    }
    traceWork->trace.contents =
        cm_boxBrush->contents;
    return qfalse;
}

/* Source: CoDUOMP.exe 0x004274e0..0x0042773e.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004274e0_0042773f.mcode.
 * Name: same-family Linux symbol CM_TraceCylinderThroughCylinder. The horizontal
 * quadratic finds the swept cylinder hit; the result is accepted only when
 * its z coordinate lies within the combined capsule/cylinder half-height. */
qboolean CM_TraceCylinderThroughCylinder(
    traceWork_t *traceWork,
    const vec3_t cylinderOrigin,
    float cylinderHalfHeight,
    float cylinderRadius)
{
    vec3_t startDelta = {
        traceWork->start[0] -
            cylinderOrigin[0],
        traceWork->start[1] -
            cylinderOrigin[1],
        0.0f
    };
    const long double startDeltaZRaw =
        (long double)traceWork->start[2] -
        cylinderOrigin[2];

    const long double combinedRadiusRaw =
        (long double)cylinderRadius +
        traceWork->sphere.radius;
    const float radialStartDistance = (float)(
        (long double)startDelta[1] * startDelta[1] +
        (long double)startDelta[0] * startDelta[0] -
        combinedRadiusRaw * combinedRadiusRaw);

    if (radialStartDistance <= 0.0f) {
        const float expandedHalfHeight =
            traceWork->sphere.halfheight -
            traceWork->sphere.radius +
            cylinderHalfHeight;
        if ((long double)expandedHalfHeight <
                startDeltaZRaw ||
            startDeltaZRaw <
                -expandedHalfHeight) {
            return qtrue;
        }

        traceWork->trace.fraction = 0.0f;
        traceWork->trace.startsolid = qtrue;
        VectorNormalize2(
            startDelta,
            traceWork->trace.normal);
        traceWork->trace.contents =
            cm_boxBrush->contents;

        startDelta[0] =
            traceWork->end[0] -
            cylinderOrigin[0];
        startDelta[1] =
            traceWork->end[1] -
            cylinderOrigin[1];
        const long double endDeltaZRaw =
            (long double)traceWork->end[2] -
            cylinderOrigin[2];
        /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
        if (endDeltaZRaw <=
                expandedHalfHeight &&
            -expandedHalfHeight <=
                endDeltaZRaw) {
            traceWork->trace.allsolid = qtrue;
        }
        return qfalse;
    }

    /* 0x004275ec..0x0042760b stores this dot product but compares the
     * retained x87 sum. */
    const long double radialDeltaDotRaw =
        (long double)startDelta[1] *
            (long double)traceWork->delta[1] +
        (long double)startDelta[0] *
            (long double)traceWork->delta[0];
    const float radialDeltaDot = (float)radialDeltaDotRaw;
    if (radialDeltaDotRaw >= (long double)0.0f)
        return qtrue;

    const float radialDeltaLengthSquared =
        traceWork->delta[0] *
            traceWork->delta[0] +
        traceWork->delta[1] *
            traceWork->delta[1];
    const long double discriminantRaw =
        (long double)radialDeltaDot *
            (long double)radialDeltaDot -
        (long double)radialDeltaLengthSquared *
            (long double)radialStartDistance;
    const float discriminant = (float)discriminantRaw;
    if (discriminantRaw < (long double)0.0f)
        return qtrue;

    startDelta[2] = 0.0f;
    vec3_t hitNormal;
    const float startLength =
        VectorNormalize2(startDelta, hitNormal);
    const float epsilonFraction =
        startLength * 0.125f /
        radialDeltaDot;
    const long double baseFractionRaw =
        (-(long double)radialDeltaDot -
         sqrt((double)discriminant)) /
        (long double)radialDeltaLengthSquared;
    const long double hitFractionRaw =
        baseFractionRaw + (long double)epsilonFraction;
    const float hitFraction = (float)hitFractionRaw;

    /* 0x00427689 stores hitFraction for the later Z calculation while the
     * retained sum controls the current-fraction rejection. */
    if (!((long double)traceWork->trace.fraction >
          hitFractionRaw)) {
        return qtrue;
    }

    /* 0x0042769e..0x004276d5 retains both values through the two height
     * comparisons; hitFraction itself is reloaded from its float copy. */
    const long double expandedHalfHeightRaw =
        ((long double)traceWork->sphere.halfheight -
         (long double)traceWork->sphere.radius) +
        (long double)cylinderHalfHeight;
    const long double zAtHitRaw =
        (((long double)hitFraction -
          (long double)epsilonFraction) *
             (long double)traceWork->delta[2] +
         (long double)traceWork->start[2]) -
        (long double)cylinderOrigin[2];
    if (expandedHalfHeightRaw < zAtHitRaw ||
        zAtHitRaw < -expandedHalfHeightRaw) {
        return qtrue;
    }

    traceWork->trace.fraction =
        hitFraction > 0.0f
            ? hitFraction
            : 0.0f;
    for (int32_t axis = 0; axis < 3; ++axis) {
        traceWork->trace.normal[axis] =
            hitNormal[axis];
    }
    traceWork->trace.contents =
        cm_boxBrush->contents;
    return qfalse;
}

/* Source: CoDUOMP.exe 0x00427740..0x00427a49.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00427740_00427a4a.mcode.
 * Name: same-family Linux symbol CM_TraceCapsuleThroughCapsule. The temporary
 * model is decomposed into two cap spheres and a cylinder; vertical ordering
 * selects the first cap probe and avoids redundant tests. */
void CM_TraceCapsuleThroughCapsule(
    traceWork_t *traceWork)
{
    for (int32_t axis = 0; axis < 3; ++axis) {
        if (traceWork->bounds[0][axis] >
            cm_boxModel.maxs[axis] + 1.0f) {
            return;
        }
    }
    for (int32_t axis = 0; axis < 3; ++axis) {
        if (cm_boxModel.mins[axis] - 1.0f >
            traceWork->bounds[1][axis]) {
            return;
        }
    }

    vec3_t startOffsetMax;
    vec3_t startOffsetMin;
    vec3_t endOffsetMax;
    vec3_t endOffsetMin;
    vec3_t capsuleCenter;
    vec3_t capsuleMaxsFromCenter;
    long double startOffsetMinZRaw = 0.0L;
    for (int32_t axis = 0; axis < 3; ++axis) {
        startOffsetMax[axis] =
            traceWork->start[axis] +
            traceWork->sphere.offset[axis];
        if (axis == 2) {
            /* 0x00427839 stores this subtraction but retains it until the
             * first cap-order comparison at 0x0042793b. */
            startOffsetMinZRaw =
                (long double)traceWork->start[axis] -
                (long double)traceWork->sphere.offset[axis];
            startOffsetMin[axis] = (float)startOffsetMinZRaw;
        } else {
            startOffsetMin[axis] =
                traceWork->start[axis] -
                traceWork->sphere.offset[axis];
        }
        endOffsetMax[axis] =
            traceWork->end[axis] +
            traceWork->sphere.offset[axis];
        endOffsetMin[axis] =
            traceWork->end[axis] -
            traceWork->sphere.offset[axis];
        if (axis != 0) {
            capsuleCenter[axis] =
                (cm_boxModel.mins[axis] +
                 cm_boxModel.maxs[axis]) *
                0.5f;
            capsuleMaxsFromCenter[axis] =
                cm_boxModel.maxs[axis] -
                capsuleCenter[axis];
        }
    }

    /* 0x0042788b..0x004278f9 stores center X but retains it through the X
     * extent and radius selection. Y and Z centers are explicitly rounded. */
    const long double capsuleCenterXRaw =
        ((long double)cm_boxModel.mins[0] +
         (long double)cm_boxModel.maxs[0]) *
        (long double)0.5f;
    capsuleCenter[0] = (float)capsuleCenterXRaw;
    const long double capsuleMaxXRaw =
        (long double)cm_boxModel.maxs[0] - capsuleCenterXRaw;
    capsuleMaxsFromCenter[0] = (float)capsuleMaxXRaw;
    const long double capsuleRadiusRaw =
        (long double)capsuleMaxsFromCenter[2] < capsuleMaxXRaw
            ? (long double)capsuleMaxsFromCenter[2]
            : capsuleMaxXRaw;
    const float capsuleRadius = (float)capsuleRadiusRaw;
    const float capsuleHalfHeight =
        capsuleMaxsFromCenter[2] -
        capsuleRadius;
    const vec3_t topSphereOrigin = {
        capsuleCenter[0],
        capsuleCenter[1],
        capsuleCenter[2] +
            capsuleHalfHeight
    };
    const vec3_t bottomSphereOrigin = {
        capsuleCenter[0],
        capsuleCenter[1],
        capsuleCenter[2] -
            capsuleHalfHeight
    };

    if ((long double)topSphereOrigin[2] <
        startOffsetMinZRaw) {
        if (CM_TraceSphereThroughSphere(
                traceWork, startOffsetMin,
                endOffsetMin, topSphereOrigin,
                capsuleRadius) == qfalse) {
            return;
        }
        if (traceWork->delta[2] >= 0.0f)
            return;
    } else if (startOffsetMax[2] <
               bottomSphereOrigin[2]) {
        if (CM_TraceSphereThroughSphere(
                traceWork, startOffsetMax,
                endOffsetMax,
                bottomSphereOrigin,
                capsuleRadius) == qfalse) {
            return;
        }
        if (traceWork->delta[2] <= 0.0f)
            return;
    }

    if (CM_TraceCylinderThroughCylinder(
            traceWork, capsuleCenter,
            capsuleHalfHeight,
            capsuleRadius) == qfalse) {
        return;
    }

    if (topSphereOrigin[2] <
        endOffsetMin[2]) {
        if (startOffsetMin[2] <=
            topSphereOrigin[2]) {
            CM_TraceSphereThroughSphere(
                traceWork, startOffsetMin,
                endOffsetMin, topSphereOrigin,
                capsuleRadius);
        }
    } else if (endOffsetMax[2] <
                   bottomSphereOrigin[2] &&
               bottomSphereOrigin[2] <=
                   startOffsetMax[2]) {
        CM_TraceSphereThroughSphere(
            traceWork, startOffsetMax,
            endOffsetMax, bottomSphereOrigin,
            capsuleRadius);
    }
}

/* Source: CoDUOMP.exe 0x00427a50..0x00427bca.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00427a50_00427bcb.mcode.
 * Name: same-family Linux symbol CM_TraceBoundingBoxThroughCapsule. Recenter
 * the temporary capsule, express it in the trace sphere/extents fields, then
 * trace the moving box as the temporary convex brush. */
void CM_TraceBoundingBoxThroughCapsule(
    traceWork_t *traceWork)
{
    vec3_t capsuleCenter;
    vec3_t capsuleMaxsFromCenter;
    /* 0x00427a53..0x00427aa3 retains the X/Y center sums through each extent
     * and endpoint subtraction. The Z center is explicitly float-rounded. */
    for (int32_t axis = 0; axis < 2; ++axis) {
        const long double centerRaw =
            ((long double)cm_boxModel.mins[axis] +
             (long double)cm_boxModel.maxs[axis]) *
            (long double)0.5f;
        capsuleCenter[axis] = (float)centerRaw;
        capsuleMaxsFromCenter[axis] = (float)(
            (long double)cm_boxModel.maxs[axis] - centerRaw);
        traceWork->start[axis] = (float)(
            (long double)traceWork->start[axis] - centerRaw);
        traceWork->end[axis] = (float)(
            (long double)traceWork->end[axis] - centerRaw);
    }
    capsuleCenter[2] =
        (cm_boxModel.mins[2] + cm_boxModel.maxs[2]) * 0.5f;
    const long double capsuleMaxZRaw =
        (long double)cm_boxModel.maxs[2] -
        (long double)capsuleCenter[2];
    capsuleMaxsFromCenter[2] = (float)capsuleMaxZRaw;
    traceWork->start[2] -= capsuleCenter[2];
    traceWork->end[2] -= capsuleCenter[2];

    traceWork->sphere.use = qtrue;
    const long double capsuleRadiusRaw =
        capsuleMaxZRaw < (long double)capsuleMaxsFromCenter[0]
            ? capsuleMaxZRaw
            : (long double)capsuleMaxsFromCenter[0];
    traceWork->sphere.radius = (float)capsuleRadiusRaw;
    traceWork->sphere.halfheight = (float)capsuleMaxZRaw;
    traceWork->sphere.offset[0] = 0.0f;
    traceWork->sphere.offset[1] = 0.0f;
    traceWork->sphere.offset[2] = (float)(
        capsuleMaxZRaw - capsuleRadiusRaw);
    traceWork->sphereExtents[0] = (float)capsuleRadiusRaw;
    traceWork->sphereExtents[1] = (float)capsuleRadiusRaw;
    traceWork->sphereExtents[2] = (float)capsuleMaxZRaw;

    CM_TempBoxModel(
        traceWork->mins, traceWork->maxs,
        cm_boxBrush->contents, qfalse);
    CM_TraceThroughBrush(
        traceWork, cm_boxBrush);
}
/* Source: CoDUOMP.exe 0x00429140..0x0042924d.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00429140_0042924e.mcode.
 * Name: exact same-module Mac symbol CM_SightTraceSphereThroughSphere.
 * Returns qfalse when the swept sphere starts intersecting or reaches the
 * expanded sphere before the trace's current fraction; this sight-only test
 * deliberately does not write a collision result. */
qboolean CM_SightTraceSphereThroughSphere(
    const traceWork_t *traceWork,
    const vec3_t start, const vec3_t end,
    const vec3_t sphereOrigin,
    float sphereRadius)
{
    (void)end;

    vec3_t delta = {
        start[0] - sphereOrigin[0],
        start[1] - sphereOrigin[1],
        start[2] - sphereOrigin[2]
    };
    const long double combinedRadiusRaw =
        (long double)sphereRadius +
        traceWork->sphere.radius;
    const float startDistanceMinusRadius = (float)(
        (((long double)delta[2] * delta[2] +
          (long double)delta[1] * delta[1]) +
         (long double)delta[0] * delta[0]) -
        combinedRadiusRaw * combinedRadiusRaw);

    if (startDistanceMinusRadius <= 0.0f)
        return qfalse;

    /* 0x4291a4..0x4291cc stores the Z + Y + X dot product as float but
     * tests the retained x87 sum. */
    const long double deltaDotRaw =
        ((long double)traceWork->delta[2] * (long double)delta[2] +
         (long double)traceWork->delta[1] * (long double)delta[1]) +
        (long double)traceWork->delta[0] * (long double)delta[0];
    const float deltaDot = (float)deltaDotRaw;
    if (deltaDotRaw >= 0.0L)
        return qtrue;

    const float deltaLengthSquared =
        traceWork->deltaLengthSquared;
    const long double discriminantRaw =
        (long double)deltaDot * (long double)deltaDot -
        (long double)deltaLengthSquared *
            (long double)startDistanceMinusRadius;
    const float discriminant = (float)discriminantRaw;
    if (discriminantRaw < 0.0L)
        return qtrue;

    vec3_t normal;
    const float startDistance =
        VectorNormalize2(delta, normal);
    const float epsilonNumerator =
        deltaDot * 0.125f;
    const long double epsilonFractionRaw =
        (long double)epsilonNumerator /
        (long double)startDistance;
    const long double baseFractionRaw =
        (-(long double)deltaDot -
         sqrt((double)discriminant)) /
        (long double)deltaLengthSquared;
    const float baseFraction = (float)baseFractionRaw;
    const long double enterFractionRaw =
        baseFractionRaw + epsilonFractionRaw;
    (void)baseFraction;

    return enterFractionRaw <
                   traceWork->trace.fraction
               ? qfalse
               : qtrue;
}

/* Source: CoDUOMP.exe 0x00429250..0x004293c5.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00429250_004293c6.mcode.
 * Name: exact same-module Mac symbol CM_SightTraceCylinderThroughCylinder.
 * The radial quadratic uses the trace's full deltaLengthSquared, as the
 * original does, and accepts a hit only when its z coordinate lies within
 * the combined capsule/cylinder half-height. */
qboolean CM_SightTraceCylinderThroughCylinder(
    const traceWork_t *traceWork,
    const vec3_t cylinderOrigin,
    float cylinderHalfHeight,
    float cylinderRadius)
{
    vec3_t startDelta = {
        traceWork->start[0] -
            cylinderOrigin[0],
        traceWork->start[1] -
            cylinderOrigin[1],
        0.0f
    };
    const long double startDeltaZRaw =
        (long double)traceWork->start[2] -
        cylinderOrigin[2];
    const long double combinedRadiusRaw =
        (long double)cylinderRadius +
        traceWork->sphere.radius;
    const float radialStartDistance = (float)(
        (long double)startDelta[1] * startDelta[1] +
        (long double)startDelta[0] * startDelta[0] -
        combinedRadiusRaw * combinedRadiusRaw);

    if (radialStartDistance <= 0.0f) {
        const long double expandedHalfHeightRaw =
            ((long double)traceWork->sphere.halfheight -
             traceWork->sphere.radius) +
            cylinderHalfHeight;
        if (expandedHalfHeightRaw <
                startDeltaZRaw ||
            startDeltaZRaw <
                -expandedHalfHeightRaw) {
            return qtrue;
        }
        return qfalse;
    }

    const long double radialDeltaDotRaw =
        (long double)traceWork->delta[1] *
            (long double)startDelta[1] +
        (long double)traceWork->delta[0] *
            (long double)startDelta[0];
    const float radialDeltaDot = (float)radialDeltaDotRaw;
    if (radialDeltaDotRaw >= 0.0L)
        return qtrue;

    const float deltaLengthSquared =
        traceWork->deltaLengthSquared;
    const long double discriminantRaw =
        (long double)radialDeltaDot *
            (long double)radialDeltaDot -
        (long double)deltaLengthSquared *
            (long double)radialStartDistance;
    const float discriminant = (float)discriminantRaw;
    if (discriminantRaw < 0.0L)
        return qtrue;

    startDelta[2] = 0.0f;
    vec3_t hitNormal;
    const float radialStartLength =
        VectorNormalize2(
            startDelta, hitNormal);
    const float epsilonNumerator =
        radialDeltaDot * 0.125f;
    const float epsilonFraction = (float)(
        (long double)epsilonNumerator /
        (long double)radialStartLength);
    const long double baseFractionRaw =
        (-(long double)radialDeltaDot -
         sqrt((double)discriminant)) /
        (long double)deltaLengthSquared;
    const long double hitFractionRaw =
        baseFractionRaw +
        (long double)epsilonFraction;
    const float hitFraction = (float)hitFractionRaw;

    if (!(hitFractionRaw <
          traceWork->trace.fraction)) {
        return qtrue;
    }

    const long double expandedHalfHeightRaw =
        ((long double)traceWork->sphere.halfheight -
         (long double)traceWork->sphere.radius) +
        (long double)cylinderHalfHeight;
    const long double zAtHitRaw =
        (((long double)hitFraction -
          (long double)epsilonFraction) *
             (long double)traceWork->delta[2] +
         (long double)traceWork->start[2]) -
        (long double)cylinderOrigin[2];
    if (expandedHalfHeightRaw < zAtHitRaw ||
        zAtHitRaw < -expandedHalfHeightRaw) {
        return qtrue;
    }

    return qfalse;
}

/* Source: CoDUOMP.exe 0x004293d0..0x00429699.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004293d0_0042969a.mcode.
 * Name: exact same-module Mac symbol CM_SightTraceCapsuleThroughCapsule.
 * The stationary capsule is derived from the global temporary collision
 * model. A blocking cap or cylinder test returns the temporary-model hit
 * sentinel -1; a clear sweep returns zero. */
int32_t CM_SightTraceCapsuleThroughCapsule(
    const traceWork_t *traceWork)
{
    for (int32_t axis = 0; axis < 3;
         ++axis) {
        if (traceWork->bounds[0][axis] >
            cm_boxModel.maxs[axis] + 1.0f) {
            return 0;
        }
    }
    for (int32_t axis = 0; axis < 3;
         ++axis) {
        if (traceWork->bounds[1][axis] <
            cm_boxModel.mins[axis] - 1.0f) {
            return 0;
        }
    }

    vec3_t startOffsetMax;
    vec3_t startOffsetMin;
    vec3_t endOffsetMax;
    vec3_t endOffsetMin;
    long double startOffsetMinZRaw = 0.0L;
    for (int32_t axis = 0; axis < 3;
         ++axis) {
        startOffsetMax[axis] =
            traceWork->start[axis] +
            traceWork->sphere.offset[axis];
        if (axis == 2) {
            startOffsetMinZRaw =
                (long double)traceWork->start[axis] -
                (long double)traceWork->sphere.offset[axis];
            startOffsetMin[axis] =
                (float)startOffsetMinZRaw;
        } else {
            startOffsetMin[axis] =
                traceWork->start[axis] -
                traceWork->sphere.offset[axis];
        }
        endOffsetMax[axis] =
            traceWork->end[axis] +
            traceWork->sphere.offset[axis];
        endOffsetMin[axis] =
            traceWork->end[axis] -
            traceWork->sphere.offset[axis];
    }

    vec3_t capsuleCenter;
    vec3_t capsuleMaxsFromCenter;
    for (int32_t axis = 1; axis < 3;
         ++axis) {
        capsuleCenter[axis] =
            (cm_boxModel.mins[axis] +
             cm_boxModel.maxs[axis]) *
            0.5f;
        capsuleMaxsFromCenter[axis] =
            cm_boxModel.maxs[axis] -
            capsuleCenter[axis];
    }
    const long double capsuleCenterXRaw =
        ((long double)cm_boxModel.mins[0] +
         (long double)cm_boxModel.maxs[0]) *
        0.5L;
    capsuleCenter[0] = (float)capsuleCenterXRaw;
    const long double capsuleMaxXRaw =
        (long double)cm_boxModel.maxs[0] -
        capsuleCenterXRaw;
    capsuleMaxsFromCenter[0] =
        (float)capsuleMaxXRaw;
    const long double capsuleRadiusRaw =
        (long double)capsuleMaxsFromCenter[2] <
                capsuleMaxXRaw
            ? (long double)capsuleMaxsFromCenter[2]
            : capsuleMaxXRaw;
    const float capsuleRadius =
        (float)capsuleRadiusRaw;
    const float capsuleHalfHeight =
        capsuleMaxsFromCenter[2] -
        capsuleRadius;

    const vec3_t topSphereOrigin = {
        capsuleCenter[0],
        capsuleCenter[1],
        capsuleCenter[2] +
            capsuleHalfHeight
    };
    const vec3_t bottomSphereOrigin = {
        capsuleCenter[0],
        capsuleCenter[1],
        capsuleCenter[2] -
            capsuleHalfHeight
    };

    if ((long double)topSphereOrigin[2] <
        startOffsetMinZRaw) {
        if (CM_SightTraceSphereThroughSphere(
                traceWork, startOffsetMin,
                endOffsetMin, topSphereOrigin,
                capsuleRadius) == qfalse) {
            return -1;
        }
        if (traceWork->delta[2] >= 0.0f)
            return 0;
    } else if (startOffsetMax[2] <
               bottomSphereOrigin[2]) {
        if (CM_SightTraceSphereThroughSphere(
                traceWork, startOffsetMax,
                endOffsetMax,
                bottomSphereOrigin,
                capsuleRadius) == qfalse) {
            return -1;
        }
        if (traceWork->delta[2] <= 0.0f)
            return 0;
    }

    if (CM_SightTraceCylinderThroughCylinder(
            traceWork, capsuleCenter,
            capsuleHalfHeight,
            capsuleRadius) == qfalse) {
        return -1;
    }

    if (topSphereOrigin[2] <
        endOffsetMin[2]) {
        if (startOffsetMin[2] <=
                topSphereOrigin[2] &&
            CM_SightTraceSphereThroughSphere(
                traceWork, startOffsetMin,
                endOffsetMin, topSphereOrigin,
                capsuleRadius) == qfalse) {
            return -1;
        }
    } else if (
        endOffsetMax[2] <
            bottomSphereOrigin[2] &&
        bottomSphereOrigin[2] <=
            startOffsetMax[2] &&
        CM_SightTraceSphereThroughSphere(
            traceWork, startOffsetMax,
            endOffsetMax, bottomSphereOrigin,
            capsuleRadius) == qfalse) {
        return -1;
    }

    return 0;
}

/* Source: CoDUOMP.exe 0x004296a0..0x0042981a.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004296a0_0042981b.mcode.
 * Name: exact same-module Mac symbol
 * CM_SightTraceBoundingBoxThroughCapsule. Recenter the trace around the
 * temporary capsule, install its sphere description, rebuild the moving
 * temporary box, and delegate to the ordinary brush sight clipper. */
int32_t CM_SightTraceBoundingBoxThroughCapsule(
    traceWork_t *traceWork)
{
    vec3_t capsuleCenter;
    vec3_t capsuleMaxsFromCenter;
    for (int32_t axis = 0; axis < 2;
         ++axis) {
        const long double centerRaw =
            ((long double)cm_boxModel.mins[axis] +
             (long double)cm_boxModel.maxs[axis]) *
            0.5L;
        capsuleCenter[axis] = (float)centerRaw;
        if (axis == 0) {
            capsuleMaxsFromCenter[axis] = (float)(
                (long double)cm_boxModel.maxs[axis] -
                centerRaw);
        }
        traceWork->start[axis] = (float)(
            (long double)traceWork->start[axis] -
            centerRaw);
        traceWork->end[axis] = (float)(
            (long double)traceWork->end[axis] -
            centerRaw);
    }
    capsuleCenter[2] =
        (cm_boxModel.mins[2] +
         cm_boxModel.maxs[2]) * 0.5f;
    const long double capsuleMaxZRaw =
        (long double)cm_boxModel.maxs[2] -
        (long double)capsuleCenter[2];
    capsuleMaxsFromCenter[2] =
        (float)capsuleMaxZRaw;
    traceWork->start[2] -= capsuleCenter[2];
    traceWork->end[2] -= capsuleCenter[2];

    const long double capsuleRadiusRaw =
        capsuleMaxZRaw <
                (long double)capsuleMaxsFromCenter[0]
            ? capsuleMaxZRaw
            : (long double)capsuleMaxsFromCenter[0];

    traceWork->sphere.use = qtrue;
    traceWork->sphere.radius =
        (float)capsuleRadiusRaw;
    traceWork->sphere.halfheight =
        (float)capsuleMaxZRaw;
    traceWork->sphere.offset[0] = 0.0f;
    traceWork->sphere.offset[1] = 0.0f;
    traceWork->sphere.offset[2] =
        (float)(capsuleMaxZRaw -
                capsuleRadiusRaw);
    traceWork->sphereExtents[0] =
        (float)capsuleRadiusRaw;
    traceWork->sphereExtents[1] =
        (float)capsuleRadiusRaw;
    traceWork->sphereExtents[2] =
        (float)capsuleMaxZRaw;

    CM_TempBoxModel(
        traceWork->mins, traceWork->maxs,
        cm_boxBrush->contents, qfalse);
    return CM_SightTraceThroughBrush(
        traceWork, cm_boxBrush);
}
#else

void CM_TestCapsuleInCapsule(traceWork_t *traceWork)
{
    vec3_t startOffsetMax;
    vec3_t startOffsetMin;
    vec3_t boxCenter;
    vec3_t boxMinsFromCenter;
    vec3_t boxMaxsFromCenter;
    float lowerZFromCenter;
    float verticalSpan;
    float radiusSquared;
    vec3_t tempPoint;
    vec3_t delta;
    float startZDelta;
    float cylinderHalfHeight;

    startOffsetMax[0] = traceWork->start[0] + traceWork->sphere.offset[0];
    startOffsetMax[1] = traceWork->start[1] + traceWork->sphere.offset[1];
    startOffsetMax[2] = traceWork->start[2] + traceWork->sphere.offset[2];

    startOffsetMin[0] = traceWork->start[0] - traceWork->sphere.offset[0];
    startOffsetMin[1] = traceWork->start[1] - traceWork->sphere.offset[1];
    startOffsetMin[2] = traceWork->start[2] - traceWork->sphere.offset[2];

    for (int axis = 0; axis < 3; ++axis) {
        boxCenter[axis] =
            (cm_boxModel.mins[axis] + cm_boxModel.maxs[axis]) * 0.5f;
        boxMinsFromCenter[axis] = cm_boxModel.mins[axis] - boxCenter[axis];
        boxMaxsFromCenter[axis] = cm_boxModel.maxs[axis] - boxCenter[axis];
    }
    (void)boxMinsFromCenter;

    if (boxMaxsFromCenter[0] > boxMaxsFromCenter[2]) {
        lowerZFromCenter = boxMaxsFromCenter[2];
    } else {
        lowerZFromCenter = boxMaxsFromCenter[0];
    }

    verticalSpan = boxMaxsFromCenter[2] - lowerZFromCenter;
    radiusSquared = (traceWork->sphere.radius + lowerZFromCenter) *
                    (traceWork->sphere.radius + lowerZFromCenter);

    tempPoint[0] = boxCenter[0];
    tempPoint[1] = boxCenter[1];
    tempPoint[2] = boxCenter[2] + verticalSpan;

    delta[0] = tempPoint[0] - startOffsetMax[0];
    delta[1] = tempPoint[1] - startOffsetMax[1];
    delta[2] = tempPoint[2] - startOffsetMax[2];
    if ((delta[0] * delta[0]) + (delta[1] * delta[1]) +
            (delta[2] * delta[2]) <
        radiusSquared) {
        traceWork->trace.allsolid = 1;
        traceWork->trace.startsolid = 1;
        traceWork->trace.fraction = 0.0f;
    }

    delta[0] = tempPoint[0] - startOffsetMin[0];
    delta[1] = tempPoint[1] - startOffsetMin[1];
    delta[2] = tempPoint[2] - startOffsetMin[2];
    if ((delta[0] * delta[0]) + (delta[1] * delta[1]) +
            (delta[2] * delta[2]) <
        radiusSquared) {
        traceWork->trace.allsolid = 1;
        traceWork->trace.startsolid = 1;
        traceWork->trace.fraction = 0.0f;
    }

    tempPoint[0] = boxCenter[0];
    tempPoint[1] = boxCenter[1];
    tempPoint[2] = boxCenter[2] - verticalSpan;

    delta[0] = tempPoint[0] - startOffsetMax[0];
    delta[1] = tempPoint[1] - startOffsetMax[1];
    delta[2] = tempPoint[2] - startOffsetMax[2];
    if ((delta[0] * delta[0]) + (delta[1] * delta[1]) +
            (delta[2] * delta[2]) <
        radiusSquared) {
        traceWork->trace.allsolid = 1;
        traceWork->trace.startsolid = 1;
        traceWork->trace.fraction = 0.0f;
    }

    delta[0] = tempPoint[0] - startOffsetMin[0];
    delta[1] = tempPoint[1] - startOffsetMin[1];
    delta[2] = tempPoint[2] - startOffsetMin[2];
    if ((delta[0] * delta[0]) + (delta[1] * delta[1]) +
            (delta[2] * delta[2]) <
        radiusSquared) {
        traceWork->trace.allsolid = 1;
        traceWork->trace.startsolid = 1;
        traceWork->trace.fraction = 0.0f;
    }

    startZDelta = traceWork->start[2] - boxCenter[2];
    cylinderHalfHeight =
        verticalSpan + traceWork->sphere.halfheight - traceWork->sphere.radius;
    if ((startZDelta <= cylinderHalfHeight) &&
        (-cylinderHalfHeight <= startZDelta)) {
        tempPoint[2] = 0.0f;
        startOffsetMax[2] = 0.0f;

        delta[0] = startOffsetMax[0] - tempPoint[0];
        delta[1] = startOffsetMax[1] - tempPoint[1];
        delta[2] = startOffsetMax[2] - tempPoint[2];
        if ((delta[0] * delta[0]) + (delta[1] * delta[1]) +
                (delta[2] * delta[2]) <
            radiusSquared) {
            traceWork->trace.allsolid = 1;
            traceWork->trace.startsolid = 1;
            traceWork->trace.fraction = 0.0f;
        }
    }
}

void CM_TestBoundingBoxInCapsule(traceWork_t *traceWork)
{
    vec3_t boxCenter;
    vec3_t boxMinsFromCenter;
    vec3_t boxMaxsFromCenter;
    float radius;

    for (int axis = 0; axis < 3; ++axis) {
        boxCenter[axis] =
            (cm_boxModel.mins[axis] + cm_boxModel.maxs[axis]) * 0.5f;
        boxMinsFromCenter[axis] = cm_boxModel.mins[axis] - boxCenter[axis];
        boxMaxsFromCenter[axis] = cm_boxModel.maxs[axis] - boxCenter[axis];
        traceWork->start[axis] -= boxCenter[axis];
        traceWork->end[axis] -= boxCenter[axis];
    }
    (void)boxMinsFromCenter;

    traceWork->sphere.use = 1;
    if (boxMaxsFromCenter[2] < boxMaxsFromCenter[0]) {
        radius = boxMaxsFromCenter[2];
    } else {
        radius = boxMaxsFromCenter[0];
    }
    traceWork->sphere.radius = radius;
    traceWork->sphere.halfheight = boxMaxsFromCenter[2];
    traceWork->sphere.offset[0] = 0.0f;
    traceWork->sphere.offset[1] = 0.0f;
    traceWork->sphere.offset[2] =
        boxMaxsFromCenter[2] - traceWork->sphere.radius;

    CM_TempBoxModel(traceWork->mins, traceWork->maxs, cm_boxBrush->contents, 0);
    CM_TestBoxInBrush(traceWork, cm_boxBrush);
}


qboolean CM_TraceSphereThroughSphere(traceWork_t *traceWork,
                                     const vec3_t start,
                                     const vec3_t end,
                                     const vec3_t sphereOrigin,
                                     float sphereRadius)
{
    vec3_t delta;
    vec3_t normal;

    delta[0] = start[0] - sphereOrigin[0];
    delta[1] = start[1] - sphereOrigin[1];
    delta[2] = start[2] - sphereOrigin[2];

    const float radiusSquared =
        (sphereRadius + traceWork->sphere.radius) *
        (sphereRadius + traceWork->sphere.radius);
    const float startDistanceMinusRadius =
        (((delta[0] * delta[0]) + (delta[1] * delta[1])) +
         (delta[2] * delta[2])) -
        radiusSquared;

    if (startDistanceMinusRadius <= 0.0f) {
        traceWork->trace.fraction = 0.0f;
        traceWork->trace.startsolid = 1;
        VectorNormalize2(delta, traceWork->trace.normal);
        traceWork->trace.contents = CM_TempBoxModelContents();

        delta[0] = end[0] - sphereOrigin[0];
        delta[1] = end[1] - sphereOrigin[1];
        delta[2] = end[2] - sphereOrigin[2];
        /* delta.delta 3-mul/2-add dot kept 80-bit, full-width compare vs
         * radiusSquared (inline, not stored) -> shim. */
#if EMULATE_X87
        if (x87f_le(
                x87f_add(x87f_add(
                    x87f_mul(x87f_load_f32(delta[0]), x87f_load_f32(delta[0])),
                    x87f_mul(x87f_load_f32(delta[1]), x87f_load_f32(delta[1]))),
                    x87f_mul(x87f_load_f32(delta[2]), x87f_load_f32(delta[2]))),
                x87f_load_f32(radiusSquared))) {
            traceWork->trace.allsolid = 1;
        }
#else
        if ((((delta[0] * delta[0]) + (delta[1] * delta[1])) +
             (delta[2] * delta[2])) <= radiusSquared) {
            traceWork->trace.allsolid = 1;
        }
#endif

        return 0;
    }

    /* traceWork->delta . delta dot kept 80-bit, stored to float -> shim. */
#if EMULATE_X87
    const float deltaDot = x87f_store_f32(x87f_add(x87f_add(
        x87f_mul(x87f_load_f32(traceWork->delta[0]), x87f_load_f32(delta[0])),
        x87f_mul(x87f_load_f32(traceWork->delta[1]), x87f_load_f32(delta[1]))),
        x87f_mul(x87f_load_f32(traceWork->delta[2]), x87f_load_f32(delta[2]))));
#else
    const float deltaDot =
        ((traceWork->delta[0] * delta[0]) +
         (traceWork->delta[1] * delta[1])) +
        (traceWork->delta[2] * delta[2]);
#endif

    if (0.0f <= deltaDot) {
        return 1;
    }

    const float deltaLengthSquared = traceWork->deltaLengthSquared;
    /* deltaDot^2 - lenSq*startDist kept 80-bit, stored to float (0x805631d). */
#if EMULATE_X87
    const float discriminant = x87f_store_f32(x87f_sub(
        x87f_mul(x87f_load_f32(deltaDot), x87f_load_f32(deltaDot)),
        x87f_mul(x87f_load_f32(deltaLengthSquared),
                 x87f_load_f32(startDistanceMinusRadius))));
#else
    const float discriminant =
        (deltaDot * deltaDot) -
        (deltaLengthSquared * startDistanceMinusRadius);
#endif

    if (discriminant < 0.0f) {
        return 1;
    }

    const float startDistance = VectorNormalize2(delta, normal);
    /* (-deltaDot - sqrt((double)disc)) / lenSq kept 80-bit (0x8056349: fchs;
     * fsub deltaDot; fdiv lenSq); enterFraction = base + (startDist*0.125)/
     * deltaDot, both one store -> shim. */
#if EMULATE_X87
    const float baseFraction = x87f_store_f32(x87f_div(
        x87f_sub(x87f_neg(x87f_load_f32(deltaDot)),
                 x87f_load_f64(sqrt((double)discriminant))),
        x87f_load_f32(deltaLengthSquared)));
    const float enterFraction = x87f_store_f32(x87f_add(
        x87f_load_f32(baseFraction),
        x87f_div(x87f_mul(x87f_load_f32(startDistance), x87f_load_f32(0.125f)),
                 x87f_load_f32(deltaDot))));
#else
    const float baseFraction =
        (-deltaDot - sqrt((double)discriminant)) / deltaLengthSquared;
    const float enterFraction =
        baseFraction + ((startDistance * 0.125f) / deltaDot);
#endif

    if (enterFraction < traceWork->trace.fraction) {
        if (enterFraction > 0.0f) {
            traceWork->trace.fraction = enterFraction;
        } else {
            traceWork->trace.fraction = 0.0f;
        }

        traceWork->trace.normal[0] = normal[0];
        traceWork->trace.normal[1] = normal[1];
        traceWork->trace.normal[2] = normal[2];
        traceWork->trace.contents = CM_TempBoxModelContents();

        return 0;
    }

    return 1;
}


qboolean CM_TraceCylinderThroughCylinder(traceWork_t *traceWork,
                                         const vec3_t cylinderOrigin,
                                         float cylinderHalfHeight,
                                         float cylinderRadius)
{
    vec3_t startDelta;
    vec3_t hitNormal;

    startDelta[0] = traceWork->start[0] - cylinderOrigin[0];
    startDelta[1] = traceWork->start[1] - cylinderOrigin[1];
    startDelta[2] = traceWork->start[2] - cylinderOrigin[2];

    /* 0x8059cbe: the squared combined radius is rounded to float in its own
     * slot before the radial dot product subtracts it. */
    const float combinedRadiusSquared =
        (cylinderRadius + traceWork->sphere.radius) *
        (cylinderRadius + traceWork->sphere.radius);
    const float radialStartDistance =
        (startDelta[0] * startDelta[0] + startDelta[1] * startDelta[1]) -
        combinedRadiusSquared;

    if (radialStartDistance <= 0.0f) {
        const float expandedHalfHeight =
            traceWork->sphere.halfheight - traceWork->sphere.radius +
            cylinderHalfHeight;

        if (expandedHalfHeight < startDelta[2] ||
            startDelta[2] < -expandedHalfHeight) {
            return 1;
        }

        traceWork->trace.fraction = 0.0f;
        traceWork->trace.startsolid = 1;
        startDelta[2] = 0.0f;
        VectorNormalize2(startDelta, traceWork->trace.normal);
        traceWork->trace.contents = CM_TempBoxModelContents();

        startDelta[0] = traceWork->end[0] - cylinderOrigin[0];
        startDelta[1] = traceWork->end[1] - cylinderOrigin[1];
        startDelta[2] = traceWork->end[2] - cylinderOrigin[2];

        /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
        if (startDelta[2] <= expandedHalfHeight &&
            -expandedHalfHeight <= startDelta[2]) {
            traceWork->trace.allsolid = 1;
        }

        return 0;
    }

    /* 2-component radial dots kept 80-bit, stored to float; discriminant,
     * epsilonFraction, baseFraction each 80-bit one store -> shim.  hitFraction
     * is a single add of two stored floats (native). */
#if EMULATE_X87
    const float radialDeltaDot = x87f_store_f32(x87f_add(
        x87f_mul(x87f_load_f32(traceWork->delta[0]), x87f_load_f32(startDelta[0])),
        x87f_mul(x87f_load_f32(traceWork->delta[1]), x87f_load_f32(startDelta[1]))));
    if (0.0f <= radialDeltaDot) {
        return 1;
    }

    const float radialDeltaLengthSquared = x87f_store_f32(x87f_add(
        x87f_mul(x87f_load_f32(traceWork->delta[0]),
                 x87f_load_f32(traceWork->delta[0])),
        x87f_mul(x87f_load_f32(traceWork->delta[1]),
                 x87f_load_f32(traceWork->delta[1]))));
    const float discriminant = x87f_store_f32(x87f_sub(
        x87f_mul(x87f_load_f32(radialDeltaDot), x87f_load_f32(radialDeltaDot)),
        x87f_mul(x87f_load_f32(radialDeltaLengthSquared),
                 x87f_load_f32(radialStartDistance))));
    if (discriminant < 0.0f) {
        return 1;
    }

    startDelta[2] = 0.0f;
    const float startLength = VectorNormalize2(startDelta, hitNormal);
    const float epsilonFraction = x87f_store_f32(x87f_div(
        x87f_mul(x87f_load_f32(startLength), x87f_load_f32(0.125f)),
        x87f_load_f32(radialDeltaDot)));
    const float baseFraction = x87f_store_f32(x87f_div(
        x87f_sub(x87f_neg(x87f_load_f32(radialDeltaDot)),
                 x87f_load_f64(sqrt((double)discriminant))),
        x87f_load_f32(radialDeltaLengthSquared)));
    const float hitFraction = baseFraction + epsilonFraction;
#else
    const float radialDeltaDot =
        traceWork->delta[0] * startDelta[0] +
        traceWork->delta[1] * startDelta[1];
    if (0.0f <= radialDeltaDot) {
        return 1;
    }

    const float radialDeltaLengthSquared =
        traceWork->delta[0] * traceWork->delta[0] +
        traceWork->delta[1] * traceWork->delta[1];
    const float discriminant =
        radialDeltaDot * radialDeltaDot -
        radialDeltaLengthSquared * radialStartDistance;
    if (discriminant < 0.0f) {
        return 1;
    }

    startDelta[2] = 0.0f;
    const float startLength = VectorNormalize2(startDelta, hitNormal);
    const float epsilonFraction = (startLength * 0.125f) / radialDeltaDot;
    const float baseFraction =
        (-radialDeltaDot - sqrt(discriminant)) / radialDeltaLengthSquared;
    const float hitFraction = baseFraction + epsilonFraction;
#endif

    if (!(traceWork->trace.fraction > hitFraction)) {
        return 1;
    }

    const float expandedHalfHeight =
        traceWork->sphere.halfheight - traceWork->sphere.radius +
        cylinderHalfHeight;
    const float zAtHit =
        ((hitFraction - epsilonFraction) * traceWork->delta[2] +
         traceWork->start[2]) -
        cylinderOrigin[2];

    if (expandedHalfHeight < zAtHit || zAtHit < -expandedHalfHeight) {
        return 1;
    }

    if (0.0f < hitFraction) {
        traceWork->trace.fraction = hitFraction;
    } else {
        traceWork->trace.fraction = 0.0f;
    }

    traceWork->trace.normal[0] = hitNormal[0];
    traceWork->trace.normal[1] = hitNormal[1];
    traceWork->trace.normal[2] = hitNormal[2];
    traceWork->trace.contents = CM_TempBoxModelContents();

    return 0;
}

void CM_TraceCapsuleThroughCapsule(traceWork_t *traceWork)
{
    vec3_t startOffsetMax;
    vec3_t startOffsetMin;
    vec3_t endOffsetMax;
    vec3_t endOffsetMin;
    vec3_t capsuleCenter;
    vec3_t capsuleMinsFromCenter;
    vec3_t capsuleMaxsFromCenter;
    float capsuleRadius;
    float capsuleHalfHeight;
    vec3_t topSphereOrigin;
    vec3_t bottomSphereOrigin;

    if (traceWork->bounds[0][0] > cm_boxModel.maxs[0] + 1.0f) {
        return;
    }
    if (traceWork->bounds[0][1] > cm_boxModel.maxs[1] + 1.0f) {
        return;
    }
    if (traceWork->bounds[0][2] > cm_boxModel.maxs[2] + 1.0f) {
        return;
    }
    if (cm_boxModel.mins[0] - 1.0f > traceWork->bounds[1][0]) {
        return;
    }
    if (cm_boxModel.mins[1] - 1.0f > traceWork->bounds[1][1]) {
        return;
    }
    if (cm_boxModel.mins[2] - 1.0f > traceWork->bounds[1][2]) {
        return;
    }

    startOffsetMax[0] = traceWork->start[0] + traceWork->sphere.offset[0];
    startOffsetMax[1] = traceWork->start[1] + traceWork->sphere.offset[1];
    startOffsetMax[2] = traceWork->start[2] + traceWork->sphere.offset[2];

    startOffsetMin[0] = traceWork->start[0] - traceWork->sphere.offset[0];
    startOffsetMin[1] = traceWork->start[1] - traceWork->sphere.offset[1];
    startOffsetMin[2] = traceWork->start[2] - traceWork->sphere.offset[2];

    endOffsetMax[0] = traceWork->end[0] + traceWork->sphere.offset[0];
    endOffsetMax[1] = traceWork->end[1] + traceWork->sphere.offset[1];
    endOffsetMax[2] = traceWork->end[2] + traceWork->sphere.offset[2];

    endOffsetMin[0] = traceWork->end[0] - traceWork->sphere.offset[0];
    endOffsetMin[1] = traceWork->end[1] - traceWork->sphere.offset[1];
    endOffsetMin[2] = traceWork->end[2] - traceWork->sphere.offset[2];

    for (int axis = 0; axis < 3; ++axis) {
        capsuleCenter[axis] =
            (cm_boxModel.mins[axis] + cm_boxModel.maxs[axis]) * 0.5f;
        capsuleMinsFromCenter[axis] =
            cm_boxModel.mins[axis] - capsuleCenter[axis];
        capsuleMaxsFromCenter[axis] =
            cm_boxModel.maxs[axis] - capsuleCenter[axis];
    }
    (void)capsuleMinsFromCenter;

    if (capsuleMaxsFromCenter[2] < capsuleMaxsFromCenter[0]) {
        capsuleRadius = capsuleMaxsFromCenter[2];
    } else {
        capsuleRadius = capsuleMaxsFromCenter[0];
    }

    capsuleHalfHeight = capsuleMaxsFromCenter[2] - capsuleRadius;

    topSphereOrigin[0] = capsuleCenter[0];
    topSphereOrigin[1] = capsuleCenter[1];
    topSphereOrigin[2] = capsuleCenter[2] + capsuleHalfHeight;

    bottomSphereOrigin[0] = capsuleCenter[0];
    bottomSphereOrigin[1] = capsuleCenter[1];
    bottomSphereOrigin[2] = capsuleCenter[2] - capsuleHalfHeight;

    if (topSphereOrigin[2] < startOffsetMin[2]) {
        if (CM_TraceSphereThroughSphere(traceWork, startOffsetMin, endOffsetMin,
                                       topSphereOrigin,
                                       capsuleRadius) == 0) {
            return;
        }
        if (0.0f <= traceWork->delta[2]) {
            return;
        }
    } else if (startOffsetMax[2] < bottomSphereOrigin[2]) {
        if (CM_TraceSphereThroughSphere(traceWork, startOffsetMax, endOffsetMax,
                                       bottomSphereOrigin,
                                       capsuleRadius) == 0) {
            return;
        }
        if (traceWork->delta[2] <= 0.0f) {
            return;
        }
    }

    if (CM_TraceCylinderThroughCylinder(traceWork, capsuleCenter,
                                     capsuleHalfHeight,
                                     capsuleRadius) != 0) {
        if (topSphereOrigin[2] < endOffsetMin[2]) {
            if (startOffsetMin[2] <= topSphereOrigin[2]) {
                CM_TraceSphereThroughSphere(traceWork, startOffsetMin,
                                           endOffsetMin, topSphereOrigin,
                                           capsuleRadius);
            }
        } else if ((endOffsetMax[2] < bottomSphereOrigin[2]) &&
                   (bottomSphereOrigin[2] <= startOffsetMax[2])) {
            CM_TraceSphereThroughSphere(traceWork, startOffsetMax,
                                       endOffsetMax, bottomSphereOrigin,
                                       capsuleRadius);
        }
    }
}

void CM_TraceBoundingBoxThroughCapsule(traceWork_t *traceWork)
{
    vec3_t capsuleCenter;
    vec3_t capsuleMinsFromCenter;
    vec3_t capsuleMaxsFromCenter;
    float capsuleRadius;

    for (int axis = 0; axis < 3; ++axis) {
        capsuleCenter[axis] =
            (cm_boxModel.mins[axis] + cm_boxModel.maxs[axis]) * 0.5f;
        capsuleMinsFromCenter[axis] =
            cm_boxModel.mins[axis] - capsuleCenter[axis];
        capsuleMaxsFromCenter[axis] =
            cm_boxModel.maxs[axis] - capsuleCenter[axis];
        traceWork->start[axis] -= capsuleCenter[axis];
        traceWork->end[axis] -= capsuleCenter[axis];
    }
    (void)capsuleMinsFromCenter;

    traceWork->sphere.use = 1;
    if (capsuleMaxsFromCenter[2] < capsuleMaxsFromCenter[0]) {
        capsuleRadius = capsuleMaxsFromCenter[2];
    } else {
        capsuleRadius = capsuleMaxsFromCenter[0];
    }
    traceWork->sphere.radius = capsuleRadius;
    traceWork->sphere.halfheight = capsuleMaxsFromCenter[2];
    traceWork->sphere.offset[0] = 0.0f;
    traceWork->sphere.offset[1] = 0.0f;
    traceWork->sphere.offset[2] =
        capsuleMaxsFromCenter[2] - traceWork->sphere.radius;
    traceWork->sphereExtents[0] = traceWork->sphere.radius;
    traceWork->sphereExtents[1] = traceWork->sphere.radius;
    traceWork->sphereExtents[2] = traceWork->sphere.halfheight;

    CM_TempBoxModel(traceWork->mins, traceWork->maxs, cm_boxBrush->contents, 0);
    CM_TraceThroughBrush(traceWork, cm_boxBrush);
}


qboolean CM_SightTraceSphereThroughSphere(
    const traceWork_t *traceWork,
    const vec3_t start,
    const vec3_t end,
    const vec3_t sphereOrigin,
    float sphereRadius)
{
    vec3_t delta;
    vec3_t normal;

    (void)end;

    delta[0] = start[0] - sphereOrigin[0];
    delta[1] = start[1] - sphereOrigin[1];
    delta[2] = start[2] - sphereOrigin[2];

    const float expandedRadiusSquared =
        (sphereRadius + traceWork->sphere.radius) *
        (sphereRadius + traceWork->sphere.radius);
    const float startDistanceMinusRadius =
        ((delta[0] * delta[0] + delta[1] * delta[1]) +
         delta[2] * delta[2]) -
        expandedRadiusSquared;

    if (startDistanceMinusRadius <= 0.0f) {
        return 0;
    }

#if EMULATE_X87
    /* 3-comp dot, discriminant, rootFraction, enterFraction each 80-bit one
     * store -> shim. */
    const float deltaDot = x87f_store_f32(x87f_add(
        x87f_add(x87f_mul(x87f_load_f32(traceWork->delta[0]), x87f_load_f32(delta[0])),
                 x87f_mul(x87f_load_f32(traceWork->delta[1]), x87f_load_f32(delta[1]))),
        x87f_mul(x87f_load_f32(traceWork->delta[2]), x87f_load_f32(delta[2]))));

    if (0.0f <= deltaDot) {
        return 1;
    }

    const float deltaLengthSquared = traceWork->deltaLengthSquared;
    const float discriminant = x87f_store_f32(x87f_sub(
        x87f_mul(x87f_load_f32(deltaDot), x87f_load_f32(deltaDot)),
        x87f_mul(x87f_load_f32(deltaLengthSquared),
                 x87f_load_f32(startDistanceMinusRadius))));

    if (discriminant < 0.0f) {
        return 1;
    }

    const float startDistance = VectorNormalize2(delta, normal);
    const float rootFraction = x87f_store_f32(x87f_div(
        x87f_sub(x87f_neg(x87f_load_f32(deltaDot)),
                 x87f_load_f64(sqrt((double)discriminant))),
        x87f_load_f32(deltaLengthSquared)));
    /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
    const float enterFraction = x87f_store_f32(x87f_add(
        x87f_load_f32(rootFraction),
        x87f_div(x87f_mul(x87f_load_f32(deltaDot), x87f_load_f32(0.125f)),
                 x87f_load_f32(startDistance))));
#else
    const float deltaDot =
        (traceWork->delta[0] * delta[0] +
         traceWork->delta[1] * delta[1]) +
        traceWork->delta[2] * delta[2];

    if (0.0f <= deltaDot) {
        return 1;
    }

    const float deltaLengthSquared = traceWork->deltaLengthSquared;
    const float discriminant =
        deltaDot * deltaDot -
        deltaLengthSquared * startDistanceMinusRadius;

    if (discriminant < 0.0f) {
        return 1;
    }

    const float startDistance = VectorNormalize2(delta, normal);
    const float rootFraction =
        (-deltaDot - sqrt((double)discriminant)) / deltaLengthSquared;
    /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
    const float enterFraction =
        rootFraction + (deltaDot * 0.125f) / startDistance;
#endif

    if (enterFraction < traceWork->trace.fraction) {
        return 0;
    }

    return 1;
}


qboolean CM_SightTraceCylinderThroughCylinder(
    const traceWork_t *traceWork,
    const vec3_t planeOrigin,
    float planeHalfHeight,
    float planeRadius)
{
    vec3_t startDelta;
    vec3_t hitNormal;

    startDelta[0] = traceWork->start[0] - planeOrigin[0];
    startDelta[1] = traceWork->start[1] - planeOrigin[1];
    startDelta[2] = traceWork->start[2] - planeOrigin[2];

    /* 0x805c0f8: the squared combined radius is rounded to its own float
     * slot before the radial dot subtracts it (as in the trace-side
     * cylinder probe). */
    const float combinedRadiusSquared =
        (planeRadius + traceWork->sphere.radius) *
        (planeRadius + traceWork->sphere.radius);
    const float radialStartDistance =
        (startDelta[0] * startDelta[0] + startDelta[1] * startDelta[1]) -
        combinedRadiusSquared;

    if (radialStartDistance <= 0.0f) {
        const float expandedHalfHeight =
            traceWork->sphere.halfheight - traceWork->sphere.radius +
            planeHalfHeight;

        if (expandedHalfHeight < startDelta[2] ||
            startDelta[2] < -expandedHalfHeight) {
            return 1;
        }

        return 0;
    }

#if EMULATE_X87
    /* 2-comp radial dot, discriminant, epsilon/base fractions each 80-bit one
     * store -> shim; hitFraction single add (native). */
    const float radialDeltaDot = x87f_store_f32(x87f_add(
        x87f_mul(x87f_load_f32(traceWork->delta[0]), x87f_load_f32(startDelta[0])),
        x87f_mul(x87f_load_f32(traceWork->delta[1]), x87f_load_f32(startDelta[1]))));
    if (0.0f <= radialDeltaDot) {
        return 1;
    }

    /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
    const float deltaLengthSquared = traceWork->deltaLengthSquared;
    const float discriminant = x87f_store_f32(x87f_sub(
        x87f_mul(x87f_load_f32(radialDeltaDot), x87f_load_f32(radialDeltaDot)),
        x87f_mul(x87f_load_f32(deltaLengthSquared),
                 x87f_load_f32(radialStartDistance))));
    if (discriminant < 0.0f) {
        return 1;
    }

    startDelta[2] = 0.0f;
    const float radialStartLength = VectorNormalize2(startDelta, hitNormal);
    /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
    const float epsilonFraction = x87f_store_f32(x87f_div(
        x87f_mul(x87f_load_f32(radialDeltaDot), x87f_load_f32(0.125f)),
        x87f_load_f32(radialStartLength)));
    const float baseFraction = x87f_store_f32(x87f_div(
        x87f_sub(x87f_neg(x87f_load_f32(radialDeltaDot)),
                 x87f_load_f64(sqrt((double)discriminant))),
        x87f_load_f32(deltaLengthSquared)));
    const float hitFraction = baseFraction + epsilonFraction;
#else
    const float radialDeltaDot =
        traceWork->delta[0] * startDelta[0] +
        traceWork->delta[1] * startDelta[1];
    if (0.0f <= radialDeltaDot) {
        return 1;
    }

    const float deltaLengthSquared = traceWork->deltaLengthSquared;
    const float discriminant =
        radialDeltaDot * radialDeltaDot -
        deltaLengthSquared * radialStartDistance;
    if (discriminant < 0.0f) {
        return 1;
    }

    startDelta[2] = 0.0f;
    const float radialStartLength = VectorNormalize2(startDelta, hitNormal);
    /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
    const float epsilonFraction =
        (radialDeltaDot * 0.125f) / radialStartLength;
    const float baseFraction =
        (-radialDeltaDot - sqrt((double)discriminant)) / deltaLengthSquared;
    const float hitFraction = baseFraction + epsilonFraction;
#endif

    if (!(traceWork->trace.fraction > hitFraction)) {
        return 1;
    }

    const float expandedHalfHeight =
        traceWork->sphere.halfheight - traceWork->sphere.radius +
        planeHalfHeight;
    const float zAtHit =
        ((hitFraction - epsilonFraction) * traceWork->delta[2] +
         traceWork->start[2]) -
        planeOrigin[2];

    if (expandedHalfHeight < zAtHit || zAtHit < -expandedHalfHeight) {
        return 1;
    }

    return 0;
}

int32_t
CM_SightTraceCapsuleThroughCapsule(const traceWork_t *traceWork)
{
    vec3_t startOffsetMax;
    vec3_t startOffsetMin;
    vec3_t endOffsetMax;
    vec3_t endOffsetMin;
    vec3_t capsuleCenter;
    vec3_t capsuleMinsFromCenter;
    vec3_t capsuleMaxsFromCenter;
    float capsuleRadius;
    float capsuleHalfHeight;
    vec3_t topSphereOrigin;
    vec3_t bottomSphereOrigin;

    if (traceWork->bounds[0][0] > cm_boxModel.maxs[0] + 1.0f) {
        return 0;
    }
    if (traceWork->bounds[0][1] > cm_boxModel.maxs[1] + 1.0f) {
        return 0;
    }
    if (traceWork->bounds[0][2] > cm_boxModel.maxs[2] + 1.0f) {
        return 0;
    }
    if (traceWork->bounds[1][0] < cm_boxModel.mins[0] - 1.0f) {
        return 0;
    }
    if (traceWork->bounds[1][1] < cm_boxModel.mins[1] - 1.0f) {
        return 0;
    }
    if (traceWork->bounds[1][2] < cm_boxModel.mins[2] - 1.0f) {
        return 0;
    }

    startOffsetMax[0] = traceWork->start[0] + traceWork->sphere.offset[0];
    startOffsetMax[1] = traceWork->start[1] + traceWork->sphere.offset[1];
    startOffsetMax[2] = traceWork->start[2] + traceWork->sphere.offset[2];

    startOffsetMin[0] = traceWork->start[0] - traceWork->sphere.offset[0];
    startOffsetMin[1] = traceWork->start[1] - traceWork->sphere.offset[1];
    startOffsetMin[2] = traceWork->start[2] - traceWork->sphere.offset[2];

    endOffsetMax[0] = traceWork->end[0] + traceWork->sphere.offset[0];
    endOffsetMax[1] = traceWork->end[1] + traceWork->sphere.offset[1];
    endOffsetMax[2] = traceWork->end[2] + traceWork->sphere.offset[2];

    endOffsetMin[0] = traceWork->end[0] - traceWork->sphere.offset[0];
    endOffsetMin[1] = traceWork->end[1] - traceWork->sphere.offset[1];
    endOffsetMin[2] = traceWork->end[2] - traceWork->sphere.offset[2];

    for (int axis = 0; axis < 3; ++axis) {
        capsuleCenter[axis] =
            (cm_boxModel.mins[axis] + cm_boxModel.maxs[axis]) * 0.5f;
        capsuleMinsFromCenter[axis] =
            cm_boxModel.mins[axis] - capsuleCenter[axis];
        capsuleMaxsFromCenter[axis] =
            cm_boxModel.maxs[axis] - capsuleCenter[axis];
    }
    (void)capsuleMinsFromCenter;

    if (capsuleMaxsFromCenter[2] < capsuleMaxsFromCenter[0]) {
        capsuleRadius = capsuleMaxsFromCenter[2];
    } else {
        capsuleRadius = capsuleMaxsFromCenter[0];
    }

    capsuleHalfHeight = capsuleMaxsFromCenter[2] - capsuleRadius;

    topSphereOrigin[0] = capsuleCenter[0];
    topSphereOrigin[1] = capsuleCenter[1];
    topSphereOrigin[2] = capsuleCenter[2] + capsuleHalfHeight;

    bottomSphereOrigin[0] = capsuleCenter[0];
    bottomSphereOrigin[1] = capsuleCenter[1];
    bottomSphereOrigin[2] = capsuleCenter[2] - capsuleHalfHeight;

    if (topSphereOrigin[2] < startOffsetMin[2]) {
        if (CM_SightTraceSphereThroughSphere(traceWork, startOffsetMin,
                                          endOffsetMin, topSphereOrigin,
                                          capsuleRadius) == 0) {
            return -1;
        }
        if (0.0f <= traceWork->delta[2]) {
            return 0;
        }
    } else if (startOffsetMax[2] < bottomSphereOrigin[2]) {
        if (CM_SightTraceSphereThroughSphere(traceWork, startOffsetMax,
                                          endOffsetMax, bottomSphereOrigin,
                                          capsuleRadius) == 0) {
            return -1;
        }
        if (traceWork->delta[2] <= 0.0f) {
            return 0;
        }
    }

    if (CM_SightTraceCylinderThroughCylinder(traceWork, capsuleCenter,
                                       capsuleHalfHeight,
                                       capsuleRadius) == 0) {
        return -1;
    }

    if (topSphereOrigin[2] < endOffsetMin[2]) {
        if (startOffsetMin[2] <= topSphereOrigin[2] &&
            CM_SightTraceSphereThroughSphere(traceWork, startOffsetMin,
                                          endOffsetMin, topSphereOrigin,
                                          capsuleRadius) == 0) {
            return -1;
        }
    } else if (endOffsetMax[2] < bottomSphereOrigin[2] &&
               bottomSphereOrigin[2] <= startOffsetMax[2] &&
               CM_SightTraceSphereThroughSphere(traceWork, startOffsetMax,
                                             endOffsetMax, bottomSphereOrigin,
                                             capsuleRadius) == 0) {
        return -1;
    }

    return 0;
}

int32_t
CM_SightTraceBoundingBoxThroughCapsule(traceWork_t *traceWork)
{
    vec3_t boxCenter;
    vec3_t boxMinsFromCenter;
    vec3_t boxMaxsFromCenter;
    float radius;

    for (int32_t axis = 0; axis < 3; ++axis) {
        boxCenter[axis] =
            (cm_boxModel.mins[axis] + cm_boxModel.maxs[axis]) * 0.5f;
        boxMinsFromCenter[axis] = cm_boxModel.mins[axis] - boxCenter[axis];
        boxMaxsFromCenter[axis] = cm_boxModel.maxs[axis] - boxCenter[axis];
        traceWork->start[axis] -= boxCenter[axis];
        traceWork->end[axis] -= boxCenter[axis];
    }
    (void)boxMinsFromCenter;

    traceWork->sphere.use = 1;
    if (boxMaxsFromCenter[2] < boxMaxsFromCenter[0]) {
        radius = boxMaxsFromCenter[2];
    } else {
        radius = boxMaxsFromCenter[0];
    }
    traceWork->sphere.radius = radius;
    traceWork->sphere.halfheight = boxMaxsFromCenter[2];
    traceWork->sphere.offset[0] = 0.0f;
    traceWork->sphere.offset[1] = 0.0f;
    traceWork->sphere.offset[2] =
        boxMaxsFromCenter[2] - traceWork->sphere.radius;
    traceWork->sphereExtents[0] = traceWork->sphere.radius;
    traceWork->sphereExtents[1] = traceWork->sphere.radius;
    traceWork->sphereExtents[2] = traceWork->sphere.halfheight;

    CM_TempBoxModel(traceWork->mins, traceWork->maxs, cm_boxBrush->contents, 0);
    return CM_SightTraceThroughBrush(traceWork, cm_boxBrush);
}
#endif
