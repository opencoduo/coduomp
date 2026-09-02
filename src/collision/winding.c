#include "winding.h"

#include "compat/coduo_x87emu.h"
#include "math/q_math.h"
#include "qcommon/q_memory.h"

#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#if defined(WINDOWS_BEHAVIOR) && defined(LINUX_BEHAVIOR)
#error "Select only one of WINDOWS_BEHAVIOR or LINUX_BEHAVIOR"
#elif !defined(WINDOWS_BEHAVIOR) && !defined(LINUX_BEHAVIOR)
#error "winding.c requires WINDOWS_BEHAVIOR or LINUX_BEHAVIOR"
#endif

void Com_Error(errorParm_t code, const char *format, ...);
/*
 * Complete collision-winding subsystem.  The authoritative bodies occupy
 * CoDUOMP.exe 0x004217a0..0x004230d6 and coduo_lnxded
 * 0x08051408..0x08053668.  The supporting Mac client exports the same
 * canonical names.  Whole-function platform bodies are retained where the
 * instruction streams use different x87 operation graphs or spill points.
 */

/* CoDUOMP.exe 0x004217a0; coduo_lnxded 0x08051408. */
void PrintWinding(const winding_t *winding)
{
    for (int32_t pointIndex = 0;
         pointIndex < winding->numpoints;
         ++pointIndex) {
        printf("(%5.1f, %5.1f, %5.1f)\n",
               (double)winding->p[pointIndex][0],
               (double)winding->p[pointIndex][1],
               (double)winding->p[pointIndex][2]);
    }
}

/* CoDUOMP.exe 0x004217e0; coduo_lnxded 0x08051484. */
winding_t *AllocWinding(int32_t pointCapacity)
{
    cm_windingActiveCount++;
    if (cm_windingPeakActiveCount < cm_windingActiveCount) {
        cm_windingPeakActiveCount = cm_windingActiveCount;
    }

    const size_t byteCount =
        sizeof(winding_t) +
        (size_t)pointCapacity * sizeof(((winding_t *)0)->p[0]);

    /* Windows inlines the malloc/null-check/zero-fill body of
     * Z_MallocInternal here (compare 0x004217fd..0x00421826 with
     * 0x00435530..0x00435553).  Linux calls that same zero-filling allocator
     * at 0x080514bf.  The former explicit Com_Memset therefore modeled the
     * Windows allocator's inlined clear a second time. */
    return Z_MallocInternal(byteCount);
}

/* CoDUOMP.exe 0x00421830; coduo_lnxded 0x080514cc. */
void FreeWinding(winding_t *winding)
{
    if (winding->numpoints == CM_WINDING_FREED_SENTINEL) {
        Com_Error(ERR_FATAL,
                  "\x15" "FreeWinding: freed a freed winding");
    }

    winding->numpoints = CM_WINDING_FREED_SENTINEL;
    cm_windingActiveCount--;
    Z_FreeInternal(winding);
}

/* CoDUOMP.exe 0x00421870; coduo_lnxded 0x0805150d. */
#if defined(WINDOWS_BEHAVIOR)
void RemoveColinearPoints(winding_t *winding)
{
    vec3_t keptPoints[CM_WINDING_TEMP_POINT_LIMIT];
    int32_t keptCount = 0;

    for (int32_t pointIndex = 0;
         pointIndex < winding->numpoints;
         ++pointIndex) {
        const int32_t nextIndex =
            (pointIndex + 1) % winding->numpoints;
        const int32_t previousIndex =
            (winding->numpoints + pointIndex - 1) % winding->numpoints;
        vec3_t nextEdge;
        vec3_t previousEdge;

        for (int32_t axis = 0;
             axis < CM_PATCH_VECTOR_AXIS_COUNT;
             ++axis) {
            nextEdge[axis] = (float)(
                (long double)winding->p[nextIndex][axis] -
                (long double)winding->p[pointIndex][axis]);
            previousEdge[axis] = (float)(
                (long double)winding->p[pointIndex][axis] -
                (long double)winding->p[previousIndex][axis]);
        }

        (void)VectorNormalize2(nextEdge, nextEdge);
        (void)VectorNormalize2(previousEdge, previousEdge);

        const long double dot =
            ((long double)previousEdge[2] *
                 (long double)nextEdge[2] +
             (long double)previousEdge[1] *
                 (long double)nextEdge[1]) +
            (long double)previousEdge[0] *
                (long double)nextEdge[0];
        if (dot < (long double)CM_WINDING_COLINEAR_DOT_EPSILON) {
            keptPoints[keptCount][0] = winding->p[pointIndex][0];
            keptPoints[keptCount][1] = winding->p[pointIndex][1];
            keptPoints[keptCount][2] = winding->p[pointIndex][2];
            keptCount++;
        }
    }

    if (keptCount != winding->numpoints) {
        winding->numpoints = keptCount;
        Com_Memcpy(winding->p, keptPoints,
                   (size_t)keptCount * sizeof(keptPoints[0]));
    }
}
#else
void RemoveColinearPoints(winding_t *winding)
{
    vec3_t keptPoints[CM_WINDING_TEMP_POINT_LIMIT];
    int32_t keptCount = 0;

    for (int32_t pointIndex = 0;
         pointIndex < winding->numpoints;
         ++pointIndex) {
        const int32_t nextIndex =
            (pointIndex + 1) % winding->numpoints;
        const int32_t previousIndex =
            (winding->numpoints + pointIndex - 1) % winding->numpoints;
        vec3_t nextEdge = {
            winding->p[nextIndex][0] - winding->p[pointIndex][0],
            winding->p[nextIndex][1] - winding->p[pointIndex][1],
            winding->p[nextIndex][2] - winding->p[pointIndex][2]
        };
        vec3_t previousEdge = {
            winding->p[pointIndex][0] - winding->p[previousIndex][0],
            winding->p[pointIndex][1] - winding->p[previousIndex][1],
            winding->p[pointIndex][2] - winding->p[previousIndex][2]
        };

        (void)VectorNormalize2(nextEdge, nextEdge);
        (void)VectorNormalize2(previousEdge, previousEdge);

        const long double dot =
            (long double)nextEdge[0] * (long double)previousEdge[0] +
            (long double)nextEdge[1] * (long double)previousEdge[1] +
            (long double)nextEdge[2] * (long double)previousEdge[2];
        if (dot < (long double)CM_WINDING_COLINEAR_DOT_EPSILON) {
            keptPoints[keptCount][0] = winding->p[pointIndex][0];
            keptPoints[keptCount][1] = winding->p[pointIndex][1];
            keptPoints[keptCount][2] = winding->p[pointIndex][2];
            keptCount++;
        }
    }

    if (keptCount != winding->numpoints) {
        winding->numpoints = keptCount;
        const uint32_t copyBytes =
            (uint32_t)keptCount * (uint32_t)sizeof(keptPoints[0]);
        memcpy(winding->p, keptPoints, (size_t)copyBytes);
    }
}
#endif

/* CoDUOMP.exe 0x004219c0; coduo_lnxded 0x080517a4. */
#if defined(WINDOWS_BEHAVIOR)
void WindingPlane(const winding_t *winding, vec3_t normal,
                  float *distance)
{
    vec3_t edge0;
    vec3_t edge1;
    for (int32_t axis = 0;
         axis < CM_PATCH_VECTOR_AXIS_COUNT;
         ++axis) {
        edge0[axis] = (float)(
            (long double)winding->p[1][axis] -
            (long double)winding->p[0][axis]);
        edge1[axis] = (float)(
            (long double)winding->p[2][axis] -
            (long double)winding->p[0][axis]);
    }

    normal[0] = (float)(
        (long double)edge1[1] * (long double)edge0[2] -
        (long double)edge1[2] * (long double)edge0[1]);
    normal[1] = (float)(
        (long double)edge1[2] * (long double)edge0[0] -
        (long double)edge1[0] * (long double)edge0[2]);
    normal[2] = (float)(
        (long double)edge1[0] * (long double)edge0[1] -
        (long double)edge1[1] * (long double)edge0[0]);
    (void)VectorNormalize2(normal, normal);

    *distance = (float)(
        ((long double)winding->p[0][0] * (long double)normal[0] +
         (long double)winding->p[0][2] * (long double)normal[2]) +
        (long double)winding->p[0][1] * (long double)normal[1]);
}
#else
void WindingPlane(const winding_t *winding, vec3_t normal,
                  float *distance)
{
    const vec3_t edge0 = {
        winding->p[1][0] - winding->p[0][0],
        winding->p[1][1] - winding->p[0][1],
        winding->p[1][2] - winding->p[0][2]
    };
    const vec3_t edge1 = {
        winding->p[2][0] - winding->p[0][0],
        winding->p[2][1] - winding->p[0][1],
        winding->p[2][2] - winding->p[0][2]
    };

    CrossProduct(edge1, edge0, normal);
    (void)VectorNormalize2(normal, normal);
    *distance =
        winding->p[0][0] * normal[0] +
        winding->p[0][1] * normal[1] +
        winding->p[0][2] * normal[2];
}
#endif

/* CoDUOMP.exe 0x00421a70; coduo_lnxded 0x08051863. */
#if defined(WINDOWS_BEHAVIOR)
float WindingArea(const winding_t *winding)
{
    float area = 0.0f;
    for (int32_t pointIndex = 2;
         pointIndex < winding->numpoints;
         ++pointIndex) {
        vec3_t edge0;
        vec3_t edge1;
        for (int32_t axis = 0;
             axis < CM_PATCH_VECTOR_AXIS_COUNT;
             ++axis) {
            edge0[axis] = (float)(
                (long double)winding->p[pointIndex - 1][axis] -
                (long double)winding->p[0][axis]);
            edge1[axis] = (float)(
                (long double)winding->p[pointIndex][axis] -
                (long double)winding->p[0][axis]);
        }

        vec3_t cross;
        cross[0] = (float)(
            (long double)edge1[1] * (long double)edge0[2] -
            (long double)edge1[2] * (long double)edge0[1]);
        cross[1] = (float)(
            (long double)edge1[2] * (long double)edge0[0] -
            (long double)edge1[0] * (long double)edge0[2]);
        cross[2] = (float)(
            (long double)edge1[0] * (long double)edge0[1] -
            (long double)edge1[1] * (long double)edge0[0]);

        const long double lengthSquared =
            ((long double)cross[2] * (long double)cross[2] +
             (long double)cross[1] * (long double)cross[1]) +
            (long double)cross[0] * (long double)cross[0];
        const float length = (float)sqrt((double)lengthSquared);
        area = (float)((long double)length * 0.5L +
                       (long double)area);
    }
    return area;
}
#else
float WindingArea(const winding_t *winding)
{
    float area = 0.0f;
    for (int32_t pointIndex = 2;
         pointIndex < winding->numpoints;
         ++pointIndex) {
        const vec3_t edge0 = {
            winding->p[pointIndex - 1][0] - winding->p[0][0],
            winding->p[pointIndex - 1][1] - winding->p[0][1],
            winding->p[pointIndex - 1][2] - winding->p[0][2]
        };
        const vec3_t edge1 = {
            winding->p[pointIndex][0] - winding->p[0][0],
            winding->p[pointIndex][1] - winding->p[0][1],
            winding->p[pointIndex][2] - winding->p[0][2]
        };
        vec3_t cross;
        CrossProduct(edge0, edge1, cross);
        area += (float)sqrt((double)(
                    cross[0] * cross[0] +
                    cross[1] * cross[1] +
                    cross[2] * cross[2])) * 0.5f;
    }
    return area;
}
#endif

/* CoDUOMP.exe 0x00421b60; coduo_lnxded 0x0805199e. */
void WindingBounds(const winding_t *winding, vec3_t mins, vec3_t maxs)
{
    for (int32_t axis = 0;
         axis < CM_PATCH_VECTOR_AXIS_COUNT;
         ++axis) {
        mins[axis] = (float)CM_WINDING_WORLD_COORD_LIMIT;
        maxs[axis] = (float)-CM_WINDING_WORLD_COORD_LIMIT;
    }

    for (int32_t pointIndex = 0;
         pointIndex < winding->numpoints;
         ++pointIndex) {
        for (int32_t axis = 0;
             axis < CM_PATCH_VECTOR_AXIS_COUNT;
             ++axis) {
            const float coordinate = winding->p[pointIndex][axis];
            if (coordinate < mins[axis]) {
                mins[axis] = coordinate;
            }
            if (coordinate > maxs[axis]) {
                maxs[axis] = coordinate;
            }
        }
    }
}

/* CoDUOMP.exe 0x00421c00; coduo_lnxded 0x08051a91. */
#if defined(WINDOWS_BEHAVIOR)
void WindingCenter(const winding_t *winding, vec3_t center)
{
    center[0] = 0.0f;
    center[1] = 0.0f;
    center[2] = 0.0f;

    for (int32_t pointIndex = 0;
         pointIndex < winding->numpoints;
         ++pointIndex) {
        center[0] = (float)((long double)center[0] +
                            (long double)winding->p[pointIndex][0]);
        center[1] = (float)((long double)center[1] +
                            (long double)winding->p[pointIndex][1]);
        center[2] = (float)((long double)center[2] +
                            (long double)winding->p[pointIndex][2]);
    }

    const long double scale = 1.0L / (long double)winding->numpoints;
    center[0] = (float)(scale * (long double)center[0]);
    center[1] = (float)(scale * (long double)center[1]);
    center[2] = (float)(scale * (long double)center[2]);
}
#else
void WindingCenter(const winding_t *winding, vec3_t center)
{
    center[0] = 0.0f;
    center[1] = 0.0f;
    center[2] = 0.0f;

    for (int32_t pointIndex = 0;
         pointIndex < winding->numpoints;
         ++pointIndex) {
        center[0] += winding->p[pointIndex][0];
        center[1] += winding->p[pointIndex][1];
        center[2] += winding->p[pointIndex][2];
    }

    /* The original FILD feeds the divide without a binary32 spill. */
    const long double scale = 1.0L / (long double)winding->numpoints;
    center[0] = (float)((long double)center[0] * scale);
    center[1] = (float)((long double)center[1] * scale);
    center[2] = (float)((long double)center[2] * scale);
}
#endif

/* CoDUOMP.exe 0x00421c60; coduo_lnxded 0x08051b87. */
#if defined(WINDOWS_BEHAVIOR)
winding_t *BaseWindingForPlane(const vec3_t normal, float distance)
{
    int32_t majorAxis = -1;
    float majorMagnitude = (float)-CM_WINDING_WORLD_COORD_LIMIT;
    for (int32_t axis = 0;
         axis < CM_PATCH_VECTOR_AXIS_COUNT;
         ++axis) {
        const float magnitude = fabsf(normal[axis]);
        if (majorMagnitude < magnitude) {
            majorAxis = axis;
            majorMagnitude = magnitude;
        }
    }

    if (majorAxis == -1) {
        Com_Error(ERR_DROP,
                  "\x15" "BaseWindingForPlane: no axis found");
    }

    vec3_t up = {0.0f, 0.0f, 0.0f};
    if (majorAxis < 2) {
        up[2] = 1.0f;
    } else {
        up[0] = 1.0f;
    }

#if EMULATE_X87
    const float projection = x87f_store_f32(x87f_add(
        x87f_add(
            x87f_mul(x87f_load_f32(up[0]), x87f_load_f32(normal[0])),
            x87f_mul(x87f_load_f32(up[2]), x87f_load_f32(normal[2]))),
        x87f_mul(x87f_load_f32(up[1]), x87f_load_f32(normal[1]))));
    for (int32_t axis = 0;
         axis < CM_PATCH_VECTOR_AXIS_COUNT;
         ++axis) {
        up[axis] = x87f_store_f32(x87f_add(
            x87f_load_f32(up[axis]),
            x87f_mul(x87f_neg(x87f_load_f32(projection)),
                     x87f_load_f32(normal[axis]))));
    }
    (void)VectorNormalize2(up, up);

    vec3_t origin;
    for (int32_t axis = 0;
         axis < CM_PATCH_VECTOR_AXIS_COUNT;
         ++axis) {
        origin[axis] = x87f_store_f32(x87f_mul(
            x87f_load_f32(distance), x87f_load_f32(normal[axis])));
    }

    vec3_t right;
    CrossProduct(up, normal, right);
    for (int32_t axis = 0;
         axis < CM_PATCH_VECTOR_AXIS_COUNT;
         ++axis) {
        up[axis] = x87f_store_f32(x87f_mul(
            x87f_load_f32(up[axis]),
            x87f_load_f32((float)CM_WINDING_WORLD_COORD_LIMIT)));
        right[axis] = x87f_store_f32(x87f_mul(
            x87f_load_f32(right[axis]),
            x87f_load_f32((float)CM_WINDING_WORLD_COORD_LIMIT)));
    }

    winding_t *const winding = AllocWinding(4);
    for (int32_t axis = 0;
         axis < CM_PATCH_VECTOR_AXIS_COUNT;
         ++axis) {
        const float originMinusRight = x87f_store_f32(x87f_sub(
            x87f_load_f32(origin[axis]), x87f_load_f32(right[axis])));
        const float originPlusRight = x87f_store_f32(x87f_add(
            x87f_load_f32(origin[axis]), x87f_load_f32(right[axis])));
        winding->p[0][axis] = x87f_store_f32(x87f_add(
            x87f_load_f32(originMinusRight), x87f_load_f32(up[axis])));
        winding->p[1][axis] = x87f_store_f32(x87f_add(
            x87f_load_f32(originPlusRight), x87f_load_f32(up[axis])));
        winding->p[2][axis] = x87f_store_f32(x87f_sub(
            x87f_load_f32(originPlusRight), x87f_load_f32(up[axis])));
        winding->p[3][axis] = x87f_store_f32(x87f_sub(
            x87f_load_f32(originMinusRight), x87f_load_f32(up[axis])));
    }
#else
    const float projection = (float)(
        ((long double)up[0] * (long double)normal[0] +
         (long double)up[2] * (long double)normal[2]) +
        (long double)up[1] * (long double)normal[1]);
    for (int32_t axis = 0;
         axis < CM_PATCH_VECTOR_AXIS_COUNT;
         ++axis) {
        up[axis] = (float)(
            (long double)up[axis] +
            (long double)-projection * (long double)normal[axis]);
    }
    (void)VectorNormalize2(up, up);

    vec3_t origin;
    for (int32_t axis = 0;
         axis < CM_PATCH_VECTOR_AXIS_COUNT;
         ++axis) {
        origin[axis] = (float)(
            (long double)distance * (long double)normal[axis]);
    }

    vec3_t right;
    right[0] = (float)(
        (long double)up[1] * (long double)normal[2] -
        (long double)up[2] * (long double)normal[1]);
    right[1] = (float)(
        (long double)up[2] * (long double)normal[0] -
        (long double)up[0] * (long double)normal[2]);
    right[2] = (float)(
        (long double)up[0] * (long double)normal[1] -
        (long double)up[1] * (long double)normal[0]);

    for (int32_t axis = 0;
         axis < CM_PATCH_VECTOR_AXIS_COUNT;
         ++axis) {
        up[axis] = (float)(
            (long double)up[axis] *
            (long double)CM_WINDING_WORLD_COORD_LIMIT);
        right[axis] = (float)(
            (long double)right[axis] *
            (long double)CM_WINDING_WORLD_COORD_LIMIT);
    }

    winding_t *const winding = AllocWinding(4);
    for (int32_t axis = 0;
         axis < CM_PATCH_VECTOR_AXIS_COUNT;
         ++axis) {
        const float originMinusRight = (float)(
            (long double)origin[axis] - (long double)right[axis]);
        const float originPlusRight = (float)(
            (long double)origin[axis] + (long double)right[axis]);
        winding->p[0][axis] = (float)(
            (long double)originMinusRight + (long double)up[axis]);
        winding->p[1][axis] = (float)(
            (long double)originPlusRight + (long double)up[axis]);
        winding->p[2][axis] = (float)(
            (long double)originPlusRight - (long double)up[axis]);
        winding->p[3][axis] = (float)(
            (long double)originMinusRight - (long double)up[axis]);
    }
#endif
    winding->numpoints = 4;
    return winding;
}
#else
winding_t *BaseWindingForPlane(const vec3_t normal, float distance)
{
    int32_t majorAxis = -1;
    float majorMagnitude = (float)-CM_WINDING_WORLD_COORD_LIMIT;
    for (int32_t axis = 0;
         axis < CM_PATCH_VECTOR_AXIS_COUNT;
         ++axis) {
        const float magnitude = fabsf(normal[axis]);
        if (majorMagnitude < magnitude) {
            majorAxis = axis;
            majorMagnitude = magnitude;
        }
    }

    if (majorAxis == -1) {
        Com_Error(ERR_DROP,
                  "\x15" "BaseWindingForPlane: no axis found");
    }

    vec3_t up = {0.0f, 0.0f, 0.0f};
    if (majorAxis < 2) {
        up[2] = 1.0f;
    } else {
        up[0] = 1.0f;
    }

#if EMULATE_X87
    const float projection = x87f_store_f32(x87f_add(
        x87f_add(
            x87f_mul(x87f_load_f32(up[0]), x87f_load_f32(normal[0])),
            x87f_mul(x87f_load_f32(up[1]), x87f_load_f32(normal[1]))),
        x87f_mul(x87f_load_f32(up[2]), x87f_load_f32(normal[2]))));
    for (int32_t axis = 0;
         axis < CM_PATCH_VECTOR_AXIS_COUNT;
         ++axis) {
        up[axis] = x87f_store_f32(x87f_add(
            x87f_load_f32(up[axis]),
            x87f_mul(x87f_neg(x87f_load_f32(projection)),
                     x87f_load_f32(normal[axis]))));
    }
    (void)VectorNormalize2(up, up);

    vec3_t origin;
    for (int32_t axis = 0;
         axis < CM_PATCH_VECTOR_AXIS_COUNT;
         ++axis) {
        origin[axis] = x87f_store_f32(x87f_mul(
            x87f_load_f32(normal[axis]), x87f_load_f32(distance)));
    }
    vec3_t right;
    CrossProduct(up, normal, right);

    for (int32_t axis = 0;
         axis < CM_PATCH_VECTOR_AXIS_COUNT;
         ++axis) {
        up[axis] = x87f_store_f32(x87f_mul(
            x87f_load_f32(up[axis]),
            x87f_load_f32((float)CM_WINDING_WORLD_COORD_LIMIT)));
        right[axis] = x87f_store_f32(x87f_mul(
            x87f_load_f32(right[axis]),
            x87f_load_f32((float)CM_WINDING_WORLD_COORD_LIMIT)));
    }

    winding_t *const winding = AllocWinding(4);
    for (int32_t axis = 0;
         axis < CM_PATCH_VECTOR_AXIS_COUNT;
         ++axis) {
        const float originMinusRight = x87f_store_f32(x87f_sub(
            x87f_load_f32(origin[axis]), x87f_load_f32(right[axis])));
        const float originPlusRight = x87f_store_f32(x87f_add(
            x87f_load_f32(origin[axis]), x87f_load_f32(right[axis])));
        winding->p[0][axis] = x87f_store_f32(x87f_add(
            x87f_load_f32(originMinusRight), x87f_load_f32(up[axis])));
        winding->p[1][axis] = x87f_store_f32(x87f_add(
            x87f_load_f32(originPlusRight), x87f_load_f32(up[axis])));
        winding->p[2][axis] = x87f_store_f32(x87f_sub(
            x87f_load_f32(originPlusRight), x87f_load_f32(up[axis])));
        winding->p[3][axis] = x87f_store_f32(x87f_sub(
            x87f_load_f32(originMinusRight), x87f_load_f32(up[axis])));
    }
#else
    const float projection =
        up[0] * normal[0] +
        up[1] * normal[1] +
        up[2] * normal[2];
    up[0] -= projection * normal[0];
    up[1] -= projection * normal[1];
    up[2] -= projection * normal[2];
    (void)VectorNormalize2(up, up);

    const vec3_t origin = {
        normal[0] * distance,
        normal[1] * distance,
        normal[2] * distance
    };
    vec3_t right;
    CrossProduct(up, normal, right);

    for (int32_t axis = 0;
         axis < CM_PATCH_VECTOR_AXIS_COUNT;
         ++axis) {
        up[axis] *= (float)CM_WINDING_WORLD_COORD_LIMIT;
        right[axis] *= (float)CM_WINDING_WORLD_COORD_LIMIT;
    }

    winding_t *const winding = AllocWinding(4);
    for (int32_t axis = 0;
         axis < CM_PATCH_VECTOR_AXIS_COUNT;
         ++axis) {
        const float originMinusRight = origin[axis] - right[axis];
        const float originPlusRight = origin[axis] + right[axis];
        winding->p[0][axis] = originMinusRight + up[axis];
        winding->p[1][axis] = originPlusRight + up[axis];
        winding->p[2][axis] = originPlusRight - up[axis];
        winding->p[3][axis] = originMinusRight - up[axis];
    }
#endif
    winding->numpoints = 4;
    return winding;
}
#endif

/* CoDUOMP.exe 0x00421f50; coduo_lnxded 0x08051ebb. */
#if defined(WINDOWS_BEHAVIOR)
winding_t *CopyWinding(const winding_t *winding)
{
    const size_t byteCount =
        sizeof(winding_t) +
        (size_t)winding->numpoints * sizeof(winding->p[0]);
    winding_t *const copy = AllocWinding(winding->numpoints);
    Com_Memcpy(copy, winding, byteCount);
    return copy;
}
#else
winding_t *CopyWinding(const winding_t *winding)
{
    winding_t *const copy = AllocWinding(winding->numpoints);
    const uint32_t byteCount =
        (uint32_t)sizeof(*winding) +
        (uint32_t)winding->numpoints *
            (uint32_t)sizeof(winding->p[0]);
    memcpy(copy, winding, (size_t)byteCount);
    return copy;
}
#endif

/* CoDUOMP.exe 0x00421f80; coduo_lnxded 0x08051f03. */
winding_t *ReverseWinding(const winding_t *winding)
{
    winding_t *const reverse = AllocWinding(winding->numpoints);
    for (int32_t pointIndex = 0;
         pointIndex < winding->numpoints;
         ++pointIndex) {
        const int32_t sourceIndex = winding->numpoints - pointIndex - 1;
        reverse->p[pointIndex][0] = winding->p[sourceIndex][0];
        reverse->p[pointIndex][1] = winding->p[sourceIndex][1];
        reverse->p[pointIndex][2] = winding->p[sourceIndex][2];
    }
    reverse->numpoints = winding->numpoints;
    return reverse;
}
/* CoDUOMP.exe 0x00421fd0; coduo_lnxded 0x08051fe8. */
#if defined(WINDOWS_BEHAVIOR)
void ClipWindingEpsilon(const winding_t *winding,
                        const vec3_t normal, float distance,
                        float epsilon, winding_t **front,
                        winding_t **back)
{
    int32_t sideCounts[CM_WINDING_SIDE_COUNT] = {0, 0, 0};
    int32_t sides[CM_WINDING_TEMP_POINT_LIMIT];
    float distances[CM_WINDING_TEMP_POINT_LIMIT];
    const int32_t sourcePointCount = winding->numpoints;

    for (int32_t pointIndex = 0;
         pointIndex < sourcePointCount;
         ++pointIndex) {
#if EMULATE_X87
        x87f pointDistance = x87f_add(
            x87f_add(
                x87f_mul(x87f_load_f32(winding->p[pointIndex][2]),
                         x87f_load_f32(normal[2])),
                x87f_mul(x87f_load_f32(winding->p[pointIndex][0]),
                         x87f_load_f32(normal[0]))),
            x87f_mul(x87f_load_f32(winding->p[pointIndex][1]),
                     x87f_load_f32(normal[1])));
        cm_windingSplitDist = x87f_store_f32(x87f_sub(
            pointDistance, x87f_load_f32(distance)));
#else
        cm_windingSplitDist = (float)(
            (((long double)winding->p[pointIndex][2] *
                  (long double)normal[2] +
              (long double)winding->p[pointIndex][0] *
                  (long double)normal[0]) +
             (long double)winding->p[pointIndex][1] *
                 (long double)normal[1]) -
            (long double)distance);
#endif
        distances[pointIndex] = cm_windingSplitDist;

        if (epsilon < cm_windingSplitDist) {
            sides[pointIndex] = CM_WINDING_SIDE_FRONT;
        } else if (cm_windingSplitDist < -epsilon) {
            sides[pointIndex] = CM_WINDING_SIDE_BACK;
        } else {
            sides[pointIndex] = CM_WINDING_SIDE_ON;
        }
        sideCounts[sides[pointIndex]]++;
    }

    sides[sourcePointCount] = sides[0];
    distances[sourcePointCount] = distances[0];
    *front = NULL;
    *back = NULL;

    if (sideCounts[CM_WINDING_SIDE_FRONT] == 0) {
        *back = CopyWinding(winding);
        return;
    }
    if (sideCounts[CM_WINDING_SIDE_BACK] == 0) {
        *front = CopyWinding(winding);
        return;
    }

    const int32_t estimatedPointCount =
        sourcePointCount + CM_WINDING_SPLIT_EXTRA_POINTS;
    winding_t *const frontWinding = AllocWinding(estimatedPointCount);
    winding_t *const backWinding = AllocWinding(estimatedPointCount);
    *front = frontWinding;
    *back = backWinding;

    for (int32_t pointIndex = 0;
         pointIndex < sourcePointCount;
         ++pointIndex) {
        const int32_t side = sides[pointIndex];
        const float *const point = winding->p[pointIndex];

        if (side == CM_WINDING_SIDE_ON) {
            for (int32_t axis = 0;
                 axis < CM_PATCH_VECTOR_AXIS_COUNT;
                 ++axis) {
                frontWinding->p[frontWinding->numpoints][axis] =
                    point[axis];
                backWinding->p[backWinding->numpoints][axis] =
                    point[axis];
            }
            frontWinding->numpoints++;
            backWinding->numpoints++;
            continue;
        }

        if (side == CM_WINDING_SIDE_FRONT) {
            for (int32_t axis = 0;
                 axis < CM_PATCH_VECTOR_AXIS_COUNT;
                 ++axis) {
                frontWinding->p[frontWinding->numpoints][axis] =
                    point[axis];
            }
            frontWinding->numpoints++;
        }
        if (side == CM_WINDING_SIDE_BACK) {
            for (int32_t axis = 0;
                 axis < CM_PATCH_VECTOR_AXIS_COUNT;
                 ++axis) {
                backWinding->p[backWinding->numpoints][axis] =
                    point[axis];
            }
            backWinding->numpoints++;
        }

        const int32_t nextSide = sides[pointIndex + 1];
        if (nextSide == CM_WINDING_SIDE_ON || nextSide == side) {
            continue;
        }

#if EMULATE_X87
        cm_windingSplitDist = x87f_store_f32(x87f_div(
            x87f_load_f32(distances[pointIndex]),
            x87f_sub(x87f_load_f32(distances[pointIndex]),
                     x87f_load_f32(distances[pointIndex + 1]))));
#else
        cm_windingSplitDist = (float)(
            (long double)distances[pointIndex] /
            ((long double)distances[pointIndex] -
             (long double)distances[pointIndex + 1]));
#endif
        const int32_t nextPointIndex =
            (pointIndex + 1) % sourcePointCount;
        vec3_t splitPoint;
        for (int32_t axis = 0;
             axis < CM_PATCH_VECTOR_AXIS_COUNT;
             ++axis) {
            uint32_t normalBits;
            memcpy(&normalBits, &normal[axis], sizeof(normalBits));
            if (normalBits == UINT32_C(0x3f800000)) {
                splitPoint[axis] = distance;
            } else if (normalBits == UINT32_C(0xbf800000)) {
                splitPoint[axis] = -distance;
            } else {
#if EMULATE_X87
                splitPoint[axis] = x87f_store_f32(x87f_add(
                    x87f_load_f32(point[axis]),
                    x87f_mul(
                        x87f_sub(
                            x87f_load_f32(winding->p[nextPointIndex][axis]),
                            x87f_load_f32(point[axis])),
                        x87f_load_f32(cm_windingSplitDist))));
#else
                splitPoint[axis] = (float)(
                    (long double)point[axis] +
                    ((long double)winding->p[nextPointIndex][axis] -
                     (long double)point[axis]) *
                        (long double)cm_windingSplitDist);
#endif
            }
        }

        for (int32_t axis = 0;
             axis < CM_PATCH_VECTOR_AXIS_COUNT;
             ++axis) {
            frontWinding->p[frontWinding->numpoints][axis] =
                splitPoint[axis];
            backWinding->p[backWinding->numpoints][axis] =
                splitPoint[axis];
        }
        frontWinding->numpoints++;
        backWinding->numpoints++;
    }

    if (frontWinding->numpoints > estimatedPointCount ||
        backWinding->numpoints > estimatedPointCount) {
        Com_Error(ERR_DROP,
                  "\x15" "ClipWinding: points exceeded estimate");
    }
    if (frontWinding->numpoints > CM_WINDING_MAX_POINTS ||
        backWinding->numpoints > CM_WINDING_MAX_POINTS) {
        Com_Error(ERR_DROP,
                  "\x15" "ClipWinding: MAX_POINTS_ON_WINDING");
    }
}
#else
void ClipWindingEpsilon(const winding_t *winding,
                        const vec3_t normal, float distance,
                        float epsilon, winding_t **front,
                        winding_t **back)
{
    int32_t sideCounts[CM_WINDING_SIDE_COUNT] = {0, 0, 0};
    int32_t sides[CM_WINDING_TEMP_POINT_LIMIT];
    float distances[CM_WINDING_TEMP_POINT_LIMIT];

    for (int32_t pointIndex = 0;
         pointIndex < winding->numpoints;
         ++pointIndex) {
#if EMULATE_X87
        x87f pointDistance = x87f_mul(
            x87f_load_f32(winding->p[pointIndex][0]),
            x87f_load_f32(normal[0]));
        pointDistance = x87f_add(pointDistance, x87f_mul(
            x87f_load_f32(winding->p[pointIndex][1]),
            x87f_load_f32(normal[1])));
        pointDistance = x87f_add(pointDistance, x87f_mul(
            x87f_load_f32(winding->p[pointIndex][2]),
            x87f_load_f32(normal[2])));
        cm_windingSplitDist = x87f_store_f32(pointDistance);
        cm_windingSplitDist = x87f_store_f32(x87f_sub(
            x87f_load_f32(cm_windingSplitDist),
            x87f_load_f32(distance)));
#else
        cm_windingSplitDist =
            winding->p[pointIndex][0] * normal[0] +
            winding->p[pointIndex][1] * normal[1] +
            winding->p[pointIndex][2] * normal[2];
        cm_windingSplitDist -= distance;
#endif
        distances[pointIndex] = cm_windingSplitDist;

        if (epsilon < cm_windingSplitDist) {
            sides[pointIndex] = CM_WINDING_SIDE_FRONT;
        } else if (cm_windingSplitDist < -epsilon) {
            sides[pointIndex] = CM_WINDING_SIDE_BACK;
        } else {
            sides[pointIndex] = CM_WINDING_SIDE_ON;
        }
        sideCounts[sides[pointIndex]]++;
    }

    sides[winding->numpoints] = sides[0];
    distances[winding->numpoints] = distances[0];
    *back = NULL;
    *front = NULL;

    if (sideCounts[CM_WINDING_SIDE_FRONT] == 0) {
        *back = CopyWinding(winding);
        return;
    }
    if (sideCounts[CM_WINDING_SIDE_BACK] == 0) {
        *front = CopyWinding(winding);
        return;
    }

    const int32_t splitCapacity =
        winding->numpoints + CM_WINDING_SPLIT_EXTRA_POINTS;
    winding_t *const frontWinding = AllocWinding(splitCapacity);
    winding_t *const backWinding = AllocWinding(splitCapacity);
    *front = frontWinding;
    *back = backWinding;

    for (int32_t pointIndex = 0;
         pointIndex < winding->numpoints;
         ++pointIndex) {
        const float *const point = winding->p[pointIndex];
        const int32_t side = sides[pointIndex];

        if (side == CM_WINDING_SIDE_ON) {
            for (int32_t axis = 0;
                 axis < CM_PATCH_VECTOR_AXIS_COUNT;
                 ++axis) {
                frontWinding->p[frontWinding->numpoints][axis] = point[axis];
                backWinding->p[backWinding->numpoints][axis] = point[axis];
            }
            frontWinding->numpoints++;
            backWinding->numpoints++;
        } else {
            if (side == CM_WINDING_SIDE_FRONT) {
                for (int32_t axis = 0;
                     axis < CM_PATCH_VECTOR_AXIS_COUNT;
                     ++axis) {
                    frontWinding->p[frontWinding->numpoints][axis] =
                        point[axis];
                }
                frontWinding->numpoints++;
            }
            if (side == CM_WINDING_SIDE_BACK) {
                for (int32_t axis = 0;
                     axis < CM_PATCH_VECTOR_AXIS_COUNT;
                     ++axis) {
                    backWinding->p[backWinding->numpoints][axis] =
                        point[axis];
                }
                backWinding->numpoints++;
            }

            const int32_t nextSide = sides[pointIndex + 1];
            if (nextSide == CM_WINDING_SIDE_ON || nextSide == side) {
                continue;
            }

            const float *const nextPoint =
                winding->p[(pointIndex + 1) % winding->numpoints];
#if EMULATE_X87
            cm_windingSplitDist = x87f_store_f32(x87f_div(
                x87f_load_f32(distances[pointIndex]),
                x87f_sub(x87f_load_f32(distances[pointIndex]),
                         x87f_load_f32(distances[pointIndex + 1]))));
#else
            cm_windingSplitDist =
                distances[pointIndex] /
                (distances[pointIndex] - distances[pointIndex + 1]);
#endif

            vec3_t splitPoint;
            for (int32_t axis = 0;
                 axis < CM_PATCH_VECTOR_AXIS_COUNT;
                 ++axis) {
                if (normal[axis] == CM_WINDING_AXIS_NORMAL) {
                    splitPoint[axis] = distance;
                } else if (normal[axis] == -CM_WINDING_AXIS_NORMAL) {
                    splitPoint[axis] = -distance;
                } else {
#if EMULATE_X87
                    splitPoint[axis] = x87f_store_f32(x87f_add(
                        x87f_load_f32(point[axis]),
                        x87f_mul(
                            x87f_sub(x87f_load_f32(nextPoint[axis]),
                                     x87f_load_f32(point[axis])),
                            x87f_load_f32(cm_windingSplitDist))));
#else
                    splitPoint[axis] =
                        point[axis] +
                        (nextPoint[axis] - point[axis]) *
                            cm_windingSplitDist;
#endif
                }
            }

            for (int32_t axis = 0;
                 axis < CM_PATCH_VECTOR_AXIS_COUNT;
                 ++axis) {
                frontWinding->p[frontWinding->numpoints][axis] =
                    splitPoint[axis];
                backWinding->p[backWinding->numpoints][axis] =
                    splitPoint[axis];
            }
            frontWinding->numpoints++;
            backWinding->numpoints++;
        }
    }

    if (splitCapacity < frontWinding->numpoints ||
        splitCapacity < backWinding->numpoints) {
        Com_Error(ERR_DROP,
                  "\x15" "ClipWinding: points exceeded estimate");
    }
    if (CM_WINDING_MAX_POINTS < frontWinding->numpoints ||
        CM_WINDING_MAX_POINTS < backWinding->numpoints) {
        Com_Error(ERR_DROP,
                  "\x15" "ClipWinding: MAX_POINTS_ON_WINDING");
    }
}
#endif

/* CoDUOMP.exe 0x004227b0; coduo_lnxded 0x08052cfd. */
winding_t *ChopWinding(winding_t *winding,
                       const vec3_t normal, float distance)
{
    winding_t *front;
    winding_t *back;
    ClipWindingEpsilon(winding, normal, distance,
                       CM_WINDING_CLIP_EPSILON, &front, &back);
    FreeWinding(winding);
    if (back != NULL) {
        FreeWinding(back);
    }
    return front;
}

/* CoDUOMP.exe 0x00422450; coduo_lnxded 0x0805275d. */
#if defined(WINDOWS_BEHAVIOR)
void ChopWindingInPlace(winding_t **winding, const vec3_t normal,
                        float distance, float epsilon)
{
    int32_t sideCounts[CM_WINDING_SIDE_COUNT] = {0, 0, 0};
    int32_t sides[CM_WINDING_TEMP_POINT_LIMIT];
    float distances[CM_WINDING_TEMP_POINT_LIMIT];
    winding_t *const source = *winding;
    const int32_t sourcePointCount = source->numpoints;

    for (int32_t pointIndex = 0;
         pointIndex < sourcePointCount;
         ++pointIndex) {
#if EMULATE_X87
        x87f pointDistance = x87f_add(
            x87f_add(
                x87f_mul(x87f_load_f32(source->p[pointIndex][2]),
                         x87f_load_f32(normal[2])),
                x87f_mul(x87f_load_f32(source->p[pointIndex][0]),
                         x87f_load_f32(normal[0]))),
            x87f_mul(x87f_load_f32(source->p[pointIndex][1]),
                     x87f_load_f32(normal[1])));
        cm_windingChopDist = x87f_store_f32(x87f_sub(
            pointDistance, x87f_load_f32(distance)));
#else
        cm_windingChopDist = (float)(
            (((long double)source->p[pointIndex][2] *
                  (long double)normal[2] +
              (long double)source->p[pointIndex][0] *
                  (long double)normal[0]) +
             (long double)source->p[pointIndex][1] *
                 (long double)normal[1]) -
            (long double)distance);
#endif
        distances[pointIndex] = cm_windingChopDist;

        if (epsilon < cm_windingChopDist) {
            sides[pointIndex] = CM_WINDING_SIDE_FRONT;
        } else if (cm_windingChopDist < -epsilon) {
            sides[pointIndex] = CM_WINDING_SIDE_BACK;
        } else {
            sides[pointIndex] = CM_WINDING_SIDE_ON;
        }
        sideCounts[sides[pointIndex]]++;
    }

    sides[sourcePointCount] = sides[0];
    distances[sourcePointCount] = distances[0];

    if (sideCounts[CM_WINDING_SIDE_FRONT] == 0) {
        FreeWinding(source);
        *winding = NULL;
        return;
    }
    if (sideCounts[CM_WINDING_SIDE_BACK] == 0) {
        return;
    }

    const int32_t estimatedPointCount =
        sourcePointCount + CM_WINDING_SPLIT_EXTRA_POINTS;
    winding_t *const front = AllocWinding(estimatedPointCount);

    for (int32_t pointIndex = 0;
         pointIndex < sourcePointCount;
         ++pointIndex) {
        const int32_t side = sides[pointIndex];
        const float *const point = source->p[pointIndex];

        if (side == CM_WINDING_SIDE_ON ||
            side == CM_WINDING_SIDE_FRONT) {
            for (int32_t axis = 0;
                 axis < CM_PATCH_VECTOR_AXIS_COUNT;
                 ++axis) {
                front->p[front->numpoints][axis] = point[axis];
            }
            front->numpoints++;
        }

        const int32_t nextSide = sides[pointIndex + 1];
        if (nextSide == CM_WINDING_SIDE_ON || nextSide == side) {
            continue;
        }

#if EMULATE_X87
        cm_windingChopDist = x87f_store_f32(x87f_div(
            x87f_load_f32(distances[pointIndex]),
            x87f_sub(x87f_load_f32(distances[pointIndex]),
                     x87f_load_f32(distances[pointIndex + 1]))));
#else
        cm_windingChopDist = (float)(
            (long double)distances[pointIndex] /
            ((long double)distances[pointIndex] -
             (long double)distances[pointIndex + 1]));
#endif
        const int32_t nextPointIndex =
            (pointIndex + 1) % sourcePointCount;
        vec3_t splitPoint;
        for (int32_t axis = 0;
             axis < CM_PATCH_VECTOR_AXIS_COUNT;
             ++axis) {
            uint32_t normalBits;
            memcpy(&normalBits, &normal[axis], sizeof(normalBits));
            if (normalBits == UINT32_C(0x3f800000)) {
                splitPoint[axis] = distance;
            } else if (normalBits == UINT32_C(0xbf800000)) {
                splitPoint[axis] = -distance;
            } else {
#if EMULATE_X87
                splitPoint[axis] = x87f_store_f32(x87f_add(
                    x87f_load_f32(point[axis]),
                    x87f_mul(
                        x87f_sub(
                            x87f_load_f32(source->p[nextPointIndex][axis]),
                            x87f_load_f32(point[axis])),
                        x87f_load_f32(cm_windingChopDist))));
#else
                splitPoint[axis] = (float)(
                    (long double)point[axis] +
                    ((long double)source->p[nextPointIndex][axis] -
                     (long double)point[axis]) *
                        (long double)cm_windingChopDist);
#endif
            }
        }

        for (int32_t axis = 0;
             axis < CM_PATCH_VECTOR_AXIS_COUNT;
             ++axis) {
            front->p[front->numpoints][axis] = splitPoint[axis];
        }
        front->numpoints++;
    }

    if (front->numpoints > estimatedPointCount) {
        Com_Error(ERR_DROP,
                  "\x15" "ClipWinding: points exceeded estimate");
    }
    if (front->numpoints > CM_WINDING_MAX_POINTS) {
        Com_Error(ERR_DROP,
                  "\x15" "ClipWinding: MAX_POINTS_ON_WINDING");
    }

    FreeWinding(source);
    *winding = front;
}
#else
void ChopWindingInPlace(winding_t **winding, const vec3_t normal,
                        float distance, float epsilon)
{
    winding_t *const source = *winding;
    int32_t sideCounts[CM_WINDING_SIDE_COUNT] = {0, 0, 0};
    int32_t sides[CM_WINDING_TEMP_POINT_LIMIT];
    float distances[CM_WINDING_TEMP_POINT_LIMIT];

    for (int32_t pointIndex = 0;
         pointIndex < source->numpoints;
         ++pointIndex) {
#if EMULATE_X87
        x87f pointDistance = x87f_mul(
            x87f_load_f32(source->p[pointIndex][0]),
            x87f_load_f32(normal[0]));
        pointDistance = x87f_add(pointDistance, x87f_mul(
            x87f_load_f32(source->p[pointIndex][1]),
            x87f_load_f32(normal[1])));
        pointDistance = x87f_add(pointDistance, x87f_mul(
            x87f_load_f32(source->p[pointIndex][2]),
            x87f_load_f32(normal[2])));
        cm_windingChopDist = x87f_store_f32(pointDistance);
        cm_windingChopDist = x87f_store_f32(x87f_sub(
            x87f_load_f32(cm_windingChopDist),
            x87f_load_f32(distance)));
#else
        cm_windingChopDist =
            source->p[pointIndex][0] * normal[0] +
            source->p[pointIndex][1] * normal[1] +
            source->p[pointIndex][2] * normal[2];
        cm_windingChopDist -= distance;
#endif
        distances[pointIndex] = cm_windingChopDist;

        if (epsilon < cm_windingChopDist) {
            sides[pointIndex] = CM_WINDING_SIDE_FRONT;
        } else if (cm_windingChopDist < -epsilon) {
            sides[pointIndex] = CM_WINDING_SIDE_BACK;
        } else {
            sides[pointIndex] = CM_WINDING_SIDE_ON;
        }
        sideCounts[sides[pointIndex]]++;
    }

    sides[source->numpoints] = sides[0];
    distances[source->numpoints] = distances[0];

    if (sideCounts[CM_WINDING_SIDE_FRONT] == 0) {
        FreeWinding(source);
        *winding = NULL;
        return;
    }
    if (sideCounts[CM_WINDING_SIDE_BACK] == 0) {
        return;
    }

    const int32_t splitCapacity =
        source->numpoints + CM_WINDING_SPLIT_EXTRA_POINTS;
    winding_t *const front = AllocWinding(splitCapacity);

    for (int32_t pointIndex = 0;
         pointIndex < source->numpoints;
         ++pointIndex) {
        const float *const point = source->p[pointIndex];
        const int32_t side = sides[pointIndex];
        if (side == CM_WINDING_SIDE_ON ||
            side == CM_WINDING_SIDE_FRONT) {
            for (int32_t axis = 0;
                 axis < CM_PATCH_VECTOR_AXIS_COUNT;
                 ++axis) {
                front->p[front->numpoints][axis] = point[axis];
            }
            front->numpoints++;
        }

        const int32_t nextSide = sides[pointIndex + 1];
        if (nextSide == CM_WINDING_SIDE_ON || nextSide == side) {
            continue;
        }

        const float *const nextPoint =
            source->p[(pointIndex + 1) % source->numpoints];
#if EMULATE_X87
        cm_windingChopDist = x87f_store_f32(x87f_div(
            x87f_load_f32(distances[pointIndex]),
            x87f_sub(x87f_load_f32(distances[pointIndex]),
                     x87f_load_f32(distances[pointIndex + 1]))));
#else
        cm_windingChopDist =
            distances[pointIndex] /
            (distances[pointIndex] - distances[pointIndex + 1]);
#endif

        vec3_t splitPoint;
        for (int32_t axis = 0;
             axis < CM_PATCH_VECTOR_AXIS_COUNT;
             ++axis) {
            if (normal[axis] == CM_WINDING_AXIS_NORMAL) {
                splitPoint[axis] = distance;
            } else if (normal[axis] == -CM_WINDING_AXIS_NORMAL) {
                splitPoint[axis] = -distance;
            } else {
#if EMULATE_X87
                splitPoint[axis] = x87f_store_f32(x87f_add(
                    x87f_load_f32(point[axis]),
                    x87f_mul(
                        x87f_sub(x87f_load_f32(nextPoint[axis]),
                                 x87f_load_f32(point[axis])),
                        x87f_load_f32(cm_windingChopDist))));
#else
                splitPoint[axis] =
                    point[axis] +
                    (nextPoint[axis] - point[axis]) *
                        cm_windingChopDist;
#endif
            }
        }

        for (int32_t axis = 0;
             axis < CM_PATCH_VECTOR_AXIS_COUNT;
             ++axis) {
            front->p[front->numpoints][axis] = splitPoint[axis];
        }
        front->numpoints++;
    }

    if (splitCapacity < front->numpoints) {
        Com_Error(ERR_DROP,
                  "\x15" "ClipWinding: points exceeded estimate");
    }
    if (CM_WINDING_MAX_POINTS < front->numpoints) {
        Com_Error(ERR_DROP,
                  "\x15" "ClipWinding: MAX_POINTS_ON_WINDING");
    }

    FreeWinding(source);
    *winding = front;
}
#endif

/* CoDUOMP.exe 0x00422850; coduo_lnxded 0x08052d53. */
#if defined(WINDOWS_BEHAVIOR)
void CheckWinding(const winding_t *winding)
{
    if (winding->numpoints < CM_WINDING_MIN_POINT_COUNT) {
        Com_Error(ERR_DROP, "\x15" "CheckWinding: %i points",
                  winding->numpoints);
    }

    const float area = WindingArea(winding);
    if (area < CM_WINDING_MIN_AREA) {
        Com_Error(ERR_DROP, "\x15" "CheckWinding: %f area",
                  (double)area);
    }

    vec3_t normal;
    float planeDistance;
    WindingPlane(winding, normal, &planeDistance);

    for (int32_t pointIndex = 0;
         pointIndex < winding->numpoints;
         ++pointIndex) {
        const float *const point = winding->p[pointIndex];
        for (int32_t axis = 0;
             axis < CM_PATCH_VECTOR_AXIS_COUNT;
             ++axis) {
            if (point[axis] > (float)CM_WINDING_WORLD_COORD_LIMIT ||
                point[axis] < (float)-CM_WINDING_WORLD_COORD_LIMIT) {
                Com_Error(ERR_DROP,
                          "\x15" "CheckFace: BUGUS_RANGE: %f",
                          (double)point[axis]);
            }
        }

#if EMULATE_X87
        const float pointPlaneDistance = x87f_store_f32(x87f_sub(
            x87f_add(
                x87f_add(
                    x87f_mul(x87f_load_f32(point[0]),
                             x87f_load_f32(normal[0])),
                    x87f_mul(x87f_load_f32(point[2]),
                             x87f_load_f32(normal[2]))),
                x87f_mul(x87f_load_f32(point[1]),
                         x87f_load_f32(normal[1]))),
            x87f_load_f32(planeDistance)));
#else
        const float pointPlaneDistance = (float)(
            (((long double)point[0] * (long double)normal[0] +
              (long double)point[2] * (long double)normal[2]) +
             (long double)point[1] * (long double)normal[1]) -
            (long double)planeDistance);
#endif
        if (pointPlaneDistance < -CM_WINDING_CLIP_EPSILON ||
            pointPlaneDistance > CM_WINDING_CLIP_EPSILON) {
            Com_Error(ERR_DROP,
                      "\x15" "CheckWinding: point off plane");
        }

        const int32_t nextPointIndex =
            pointIndex + 1 == winding->numpoints ? 0 : pointIndex + 1;
        vec3_t edge;
        for (int32_t axis = 0;
             axis < CM_PATCH_VECTOR_AXIS_COUNT;
             ++axis) {
            edge[axis] = (float)(
                (long double)winding->p[nextPointIndex][axis] -
                (long double)point[axis]);
        }

        const long double edgeLengthSquared =
            ((long double)edge[2] * (long double)edge[2] +
             (long double)edge[1] * (long double)edge[1]) +
            (long double)edge[0] * (long double)edge[0];
        const float edgeLength =
            (float)sqrt((double)edgeLengthSquared);
        if (edgeLength < CM_WINDING_CLIP_EPSILON) {
            Com_Error(ERR_DROP,
                      "\x15" "CheckWinding: degenerate edge");
        }

        vec3_t edgeNormal;
        CrossProduct(normal, edge, edgeNormal);
        (void)VectorNormalize2(edgeNormal, edgeNormal);

        float edgeDistance = (float)(
            ((long double)edgeNormal[0] * (long double)point[0] +
             (long double)edgeNormal[2] * (long double)point[2]) +
            (long double)edgeNormal[1] * (long double)point[1]);
        edgeDistance = (float)(
            (long double)edgeDistance +
            (long double)CM_WINDING_CLIP_EPSILON);

        for (int32_t checkIndex = 0;
             checkIndex < winding->numpoints;
             ++checkIndex) {
            if (checkIndex == pointIndex) {
                continue;
            }
            const float *const checkPoint = winding->p[checkIndex];
            const float checkDistance = (float)(
                ((long double)edgeNormal[2] *
                     (long double)checkPoint[2] +
                 (long double)edgeNormal[0] *
                     (long double)checkPoint[0]) +
                (long double)edgeNormal[1] *
                    (long double)checkPoint[1]);
            if (edgeDistance < checkDistance) {
                Com_Error(ERR_DROP,
                          "\x15" "CheckWinding: non-convex");
            }
        }
    }
}
#else
void CheckWinding(const winding_t *winding)
{
    if (winding->numpoints < CM_WINDING_MIN_POINT_COUNT) {
        Com_Error(ERR_DROP, "\x15" "CheckWinding: %i points",
                  winding->numpoints);
    }

    const float area = WindingArea(winding);
    if (area < CM_WINDING_MIN_AREA) {
        Com_Error(ERR_DROP, "\x15" "CheckWinding: %f area",
                  (double)area);
    }

    vec3_t normal;
    float planeDistance;
    WindingPlane(winding, normal, &planeDistance);

    for (int32_t pointIndex = 0;
         pointIndex < winding->numpoints;
         ++pointIndex) {
        const float *const point = winding->p[pointIndex];
        for (int32_t axis = 0;
             axis < CM_PATCH_VECTOR_AXIS_COUNT;
             ++axis) {
            if ((float)CM_WINDING_WORLD_COORD_LIMIT < point[axis] ||
                point[axis] < (float)-CM_WINDING_WORLD_COORD_LIMIT) {
                Com_Error(ERR_DROP,
                          "\x15" "CheckFace: BUGUS_RANGE: %f",
                          (double)point[axis]);
            }
        }

        const float planeDistanceFromPoint =
            point[0] * normal[0] +
            point[1] * normal[1] +
            point[2] * normal[2] - planeDistance;
        if (planeDistanceFromPoint < -CM_WINDING_CLIP_EPSILON ||
            CM_WINDING_CLIP_EPSILON < planeDistanceFromPoint) {
            Com_Error(ERR_DROP,
                      "\x15" "CheckWinding: point off plane");
        }

        const int32_t nextIndex =
            pointIndex + 1 == winding->numpoints ? 0 : pointIndex + 1;
        const float *const nextPoint = winding->p[nextIndex];
        const vec3_t edge = {
            nextPoint[0] - point[0],
            nextPoint[1] - point[1],
            nextPoint[2] - point[2]
        };
        const float edgeLength = (float)sqrt((double)(
            edge[0] * edge[0] +
            edge[1] * edge[1] +
            edge[2] * edge[2]));
        if (edgeLength < CM_WINDING_CLIP_EPSILON) {
            Com_Error(ERR_DROP,
                      "\x15" "CheckWinding: degenerate edge");
        }

        vec3_t edgeNormal;
        CrossProduct(normal, edge, edgeNormal);
        (void)VectorNormalize2(edgeNormal, edgeNormal);
        float edgeDistance =
            point[0] * edgeNormal[0] +
            point[1] * edgeNormal[1] +
            point[2] * edgeNormal[2];
        edgeDistance += CM_WINDING_CLIP_EPSILON;

        for (int32_t checkIndex = 0;
             checkIndex < winding->numpoints;
             ++checkIndex) {
            if (checkIndex == pointIndex) {
                continue;
            }
            const float *const checkPoint = winding->p[checkIndex];
            const float checkDistance =
                checkPoint[0] * edgeNormal[0] +
                checkPoint[1] * edgeNormal[1] +
                checkPoint[2] * edgeNormal[2];
            if (edgeDistance < checkDistance) {
                Com_Error(ERR_DROP,
                          "\x15" "CheckWinding: non-convex");
            }
        }
    }
}
#endif

/* CoDUOMP.exe 0x00422af0; coduo_lnxded 0x08053092. */
#if defined(WINDOWS_BEHAVIOR)
int32_t WindingOnPlaneSide(const winding_t *winding,
                           const vec3_t normal, float distance)
{
    qboolean foundFront = qfalse;
    qboolean foundBack = qfalse;
    for (int32_t pointIndex = 0;
         pointIndex < winding->numpoints;
         ++pointIndex) {
#if EMULATE_X87
        const x87f pointDistance = x87f_sub(
            x87f_add(
                x87f_add(
                    x87f_mul(x87f_load_f32(winding->p[pointIndex][0]),
                             x87f_load_f32(normal[0])),
                    x87f_mul(x87f_load_f32(winding->p[pointIndex][2]),
                             x87f_load_f32(normal[2]))),
                x87f_mul(x87f_load_f32(winding->p[pointIndex][1]),
                         x87f_load_f32(normal[1]))),
            x87f_load_f32(distance));
        const qboolean behind = x87f_lt(
            pointDistance,
            x87f_load_f32(-CM_WINDING_CLIP_EPSILON)) ? qtrue : qfalse;
        const qboolean ahead = x87f_lt(
            x87f_load_f32(CM_WINDING_CLIP_EPSILON),
            pointDistance) ? qtrue : qfalse;
#else
        const long double pointDistance =
            ((long double)winding->p[pointIndex][0] *
                 (long double)normal[0] +
             (long double)winding->p[pointIndex][2] *
                 (long double)normal[2]) +
            (long double)winding->p[pointIndex][1] *
                (long double)normal[1] -
            (long double)distance;
        const qboolean behind =
            pointDistance < -(long double)CM_WINDING_CLIP_EPSILON
                ? qtrue : qfalse;
        const qboolean ahead =
            pointDistance > (long double)CM_WINDING_CLIP_EPSILON
                ? qtrue : qfalse;
#endif
        if (behind != qfalse) {
            if (foundFront != qfalse) {
                return CM_WINDING_SIDE_CROSS;
            }
            foundBack = qtrue;
        } else if (ahead != qfalse) {
            if (foundBack != qfalse) {
                return CM_WINDING_SIDE_CROSS;
            }
            foundFront = qtrue;
        }
    }

    if (foundBack != qfalse) {
        return CM_WINDING_SIDE_BACK;
    }
    if (foundFront != qfalse) {
        return CM_WINDING_SIDE_FRONT;
    }
    return CM_WINDING_SIDE_ON;
}
#else
int32_t WindingOnPlaneSide(const winding_t *winding,
                           const vec3_t normal, float distance)
{
    qboolean foundFront = qfalse;
    qboolean foundBack = qfalse;
    for (int32_t pointIndex = 0;
         pointIndex < winding->numpoints;
         ++pointIndex) {
        const float pointDistance =
            winding->p[pointIndex][0] * normal[0] +
            winding->p[pointIndex][1] * normal[1] +
            winding->p[pointIndex][2] * normal[2] - distance;
        if (pointDistance < -CM_WINDING_CLIP_EPSILON) {
            if (foundFront != qfalse) {
                return CM_WINDING_SIDE_CROSS;
            }
            foundBack = qtrue;
        } else if (CM_WINDING_CLIP_EPSILON < pointDistance) {
            if (foundBack != qfalse) {
                return CM_WINDING_SIDE_CROSS;
            }
            foundFront = qtrue;
        }
    }

    if (foundBack != qfalse) {
        return CM_WINDING_SIDE_BACK;
    }
    if (foundFront != qfalse) {
        return CM_WINDING_SIDE_FRONT;
    }
    return CM_WINDING_SIDE_ON;
}
#endif

/* CoDUOMP.exe 0x00422b90; coduo_lnxded 0x080531a5. */
#if defined(WINDOWS_BEHAVIOR)
void AddWindingToConvexHull(const winding_t *source,
                            winding_t **winding,
                            const vec3_t normal)
{
    if (*winding == NULL) {
        *winding = CopyWinding(source);
        return;
    }

    int32_t mergedPointCount = (*winding)->numpoints;
    vec3_t mergedPoints[CM_WINDING_MERGE_POINT_LIMIT];
    vec3_t newPoints[CM_WINDING_MERGE_POINT_LIMIT];
    vec3_t edgeNormals[CM_WINDING_MERGE_POINT_LIMIT];
    qboolean keepEdge[CM_WINDING_MERGE_POINT_LIMIT];

    Com_Memcpy(mergedPoints, (*winding)->p,
               (size_t)mergedPointCount * sizeof(mergedPoints[0]));

    for (int32_t sourcePointIndex = 0;
         sourcePointIndex < source->numpoints;
         ++sourcePointIndex) {
        const float *const sourcePoint =
            source->p[sourcePointIndex];

        for (int32_t pointIndex = 0;
             pointIndex < mergedPointCount;
             ++pointIndex) {
            const int32_t nextPointIndex =
                (pointIndex + 1) % mergedPointCount;
            vec3_t edge;
            for (int32_t axis = 0;
                 axis < CM_PATCH_VECTOR_AXIS_COUNT;
                 ++axis) {
                edge[axis] = (float)(
                    (long double)mergedPoints[nextPointIndex][axis] -
                    (long double)mergedPoints[pointIndex][axis]);
            }
            (void)VectorNormalize2(edge, edge);
            edgeNormals[pointIndex][0] = (float)(
                (long double)edge[2] * (long double)normal[1] -
                (long double)edge[1] * (long double)normal[2]);
            edgeNormals[pointIndex][1] = (float)(
                (long double)edge[0] * (long double)normal[2] -
                (long double)edge[2] * (long double)normal[0]);
            edgeNormals[pointIndex][2] = (float)(
                (long double)edge[1] * (long double)normal[0] -
                (long double)edge[0] * (long double)normal[1]);
        }

        qboolean pointOutside = qfalse;
        for (int32_t pointIndex = 0;
             pointIndex < mergedPointCount;
             ++pointIndex) {
            vec3_t delta;
            for (int32_t axis = 0;
                 axis < CM_PATCH_VECTOR_AXIS_COUNT;
                 ++axis) {
                delta[axis] = (float)(
                    (long double)sourcePoint[axis] -
                    (long double)mergedPoints[pointIndex][axis]);
            }

            const float edgeDistance = (float)(
                ((long double)delta[2] *
                     (long double)edgeNormals[pointIndex][2] +
                 (long double)delta[1] *
                     (long double)edgeNormals[pointIndex][1]) +
                (long double)delta[0] *
                    (long double)edgeNormals[pointIndex][0]);
            if (edgeDistance >= CM_WINDING_CLIP_EPSILON) {
                pointOutside = qtrue;
            }
            keepEdge[pointIndex] =
                edgeDistance >= -CM_WINDING_CLIP_EPSILON
                    ? qtrue : qfalse;
        }

        if (pointOutside == qfalse) {
            continue;
        }

        int32_t startEdge = 0;
        while (startEdge < mergedPointCount &&
               (keepEdge[startEdge % mergedPointCount] != qfalse ||
                keepEdge[(startEdge + 1) % mergedPointCount] == qfalse)) {
            startEdge++;
        }
        if (startEdge == mergedPointCount) {
            continue;
        }

        newPoints[0][0] = sourcePoint[0];
        newPoints[0][1] = sourcePoint[1];
        newPoints[0][2] = sourcePoint[2];
        int32_t newPointCount = 1;
        startEdge = (startEdge + 1) % mergedPointCount;

        for (int32_t pointOffset = 0;
             pointOffset < mergedPointCount;
             ++pointOffset) {
            const int32_t edgeIndex =
                (startEdge + pointOffset) % mergedPointCount;
            const int32_t nextPointIndex =
                (startEdge + pointOffset + 1) % mergedPointCount;
            if (keepEdge[edgeIndex] == qfalse ||
                keepEdge[nextPointIndex] == qfalse) {
                newPoints[newPointCount][0] =
                    mergedPoints[nextPointIndex][0];
                newPoints[newPointCount][1] =
                    mergedPoints[nextPointIndex][1];
                newPoints[newPointCount][2] =
                    mergedPoints[nextPointIndex][2];
                newPointCount++;
            }
        }

        mergedPointCount = newPointCount;
        Com_Memcpy(mergedPoints, newPoints,
                   (size_t)newPointCount * sizeof(newPoints[0]));
    }

    FreeWinding(*winding);
    winding_t *const merged = AllocWinding(mergedPointCount);
    merged->numpoints = mergedPointCount;
    *winding = merged;
    Com_Memcpy(merged->p, mergedPoints,
               (size_t)mergedPointCount * sizeof(mergedPoints[0]));
}
#else
void AddWindingToConvexHull(const winding_t *source,
                            winding_t **winding,
                            const vec3_t normal)
{
    if (*winding == NULL) {
        *winding = CopyWinding(source);
        return;
    }

    int32_t mergedPointCount = (*winding)->numpoints;
    vec3_t mergedPoints[CM_WINDING_MERGE_POINT_LIMIT];
    vec3_t newPoints[CM_WINDING_MERGE_POINT_LIMIT];
    vec3_t edgeNormals[CM_WINDING_MERGE_POINT_LIMIT];
    qboolean keepEdge[CM_WINDING_MERGE_POINT_LIMIT];

    uint32_t mergedBytes =
        (uint32_t)mergedPointCount * (uint32_t)sizeof(mergedPoints[0]);
    memcpy(mergedPoints, (*winding)->p, (size_t)mergedBytes);

    for (int32_t sourcePointIndex = 0;
         sourcePointIndex < source->numpoints;
         ++sourcePointIndex) {
        const float *const sourcePoint = source->p[sourcePointIndex];

        for (int32_t pointIndex = 0;
             pointIndex < mergedPointCount;
             ++pointIndex) {
            const int32_t nextPointIndex =
                (pointIndex + 1) % mergedPointCount;
            vec3_t edge = {
                mergedPoints[nextPointIndex][0] -
                    mergedPoints[pointIndex][0],
                mergedPoints[nextPointIndex][1] -
                    mergedPoints[pointIndex][1],
                mergedPoints[nextPointIndex][2] -
                    mergedPoints[pointIndex][2]
            };
            (void)VectorNormalize2(edge, edge);
            CrossProduct(normal, edge, edgeNormals[pointIndex]);
        }

        qboolean pointOutside = qfalse;
        for (int32_t pointIndex = 0;
             pointIndex < mergedPointCount;
             ++pointIndex) {
            const vec3_t delta = {
                sourcePoint[0] - mergedPoints[pointIndex][0],
                sourcePoint[1] - mergedPoints[pointIndex][1],
                sourcePoint[2] - mergedPoints[pointIndex][2]
            };
            const float edgeDistance =
                delta[0] * edgeNormals[pointIndex][0] +
                delta[1] * edgeNormals[pointIndex][1] +
                delta[2] * edgeNormals[pointIndex][2];

            if (CM_WINDING_CLIP_EPSILON <= edgeDistance) {
                pointOutside = qtrue;
            }
            keepEdge[pointIndex] =
                -CM_WINDING_CLIP_EPSILON <= edgeDistance
                    ? qtrue : qfalse;
        }

        if (pointOutside == qfalse) {
            continue;
        }

        int32_t startEdge = 0;
        while (startEdge < mergedPointCount &&
               (keepEdge[startEdge % mergedPointCount] != qfalse ||
                keepEdge[(startEdge + 1) % mergedPointCount] == qfalse)) {
            startEdge++;
        }
        if (startEdge == mergedPointCount) {
            continue;
        }

        newPoints[0][0] = sourcePoint[0];
        newPoints[0][1] = sourcePoint[1];
        newPoints[0][2] = sourcePoint[2];
        int32_t newPointCount = 1;
        startEdge = (startEdge + 1) % mergedPointCount;

        for (int32_t pointOffset = 0;
             pointOffset < mergedPointCount;
             ++pointOffset) {
            const int32_t edgeIndex =
                (startEdge + pointOffset) % mergedPointCount;
            const int32_t nextPointIndex =
                (startEdge + pointOffset + 1) % mergedPointCount;
            if (keepEdge[edgeIndex] == qfalse ||
                keepEdge[nextPointIndex] == qfalse) {
                newPoints[newPointCount][0] =
                    mergedPoints[nextPointIndex][0];
                newPoints[newPointCount][1] =
                    mergedPoints[nextPointIndex][1];
                newPoints[newPointCount][2] =
                    mergedPoints[nextPointIndex][2];
                newPointCount++;
            }
        }

        mergedPointCount = newPointCount;
        const uint32_t newBytes =
            (uint32_t)newPointCount * (uint32_t)sizeof(newPoints[0]);
        memcpy(mergedPoints, newPoints, (size_t)newBytes);
    }

    FreeWinding(*winding);
    winding_t *const merged = AllocWinding(mergedPointCount);
    merged->numpoints = mergedPointCount;
    *winding = merged;
    mergedBytes =
        (uint32_t)mergedPointCount * (uint32_t)sizeof(mergedPoints[0]);
    memcpy(merged->p, mergedPoints, (size_t)mergedBytes);
}
#endif
