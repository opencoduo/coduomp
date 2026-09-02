#ifndef SHARED_COLLISION_WINDING_H
#define SHARED_COLLISION_WINDING_H

#include "qcommon/collision_map_types.h"
#include "qcommon/q_shared_types.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
    CM_WINDING_SIDE_FRONT = 0,
    CM_WINDING_SIDE_BACK = 1,
    CM_WINDING_SIDE_ON = 2,
    CM_WINDING_SIDE_CROSS = -2,
    CM_WINDING_SIDE_COUNT = 3,
    CM_WINDING_MIN_POINT_COUNT = 3,
    CM_WINDING_MAX_POINTS = 64,
    CM_WINDING_TEMP_POINT_LIMIT = 65,
    CM_WINDING_SPLIT_EXTRA_POINTS = 4,
    CM_WINDING_MERGE_POINT_LIMIT = 128,
    CM_WINDING_WORLD_COORD_LIMIT = 131072,
    CM_WINDING_FREED_SENTINEL = (int32_t)0xdeaddeadU
};

#define CM_WINDING_CLIP_EPSILON 0.10000000149011612f
#define CM_WINDING_MIN_AREA 1.0f
#define CM_WINDING_AXIS_NORMAL 1.0f
#define CM_WINDING_COLINEAR_DOT_EPSILON 0.999

/* The shared patch-builder translation unit owns these collision-work values;
 * winding operations expose them because the original builders and winding
 * code use the same counters and temporary distance slots. */
extern int32_t cm_windingActiveCount;
extern int32_t cm_windingPeakActiveCount;
extern float cm_windingSplitDist;
extern float cm_windingChopDist;

void PrintWinding(const winding_t *winding);
winding_t *AllocWinding(int32_t pointCapacity);
void FreeWinding(winding_t *winding);
void RemoveColinearPoints(winding_t *winding);
void WindingPlane(const winding_t *winding, vec3_t normal,
                  float *distance);
float WindingArea(const winding_t *winding);
void WindingBounds(const winding_t *winding, vec3_t mins, vec3_t maxs);
void WindingCenter(const winding_t *winding, vec3_t center);
winding_t *BaseWindingForPlane(const vec3_t normal, float distance);
winding_t *CopyWinding(const winding_t *winding);
winding_t *ReverseWinding(const winding_t *winding);
void ClipWindingEpsilon(const winding_t *winding,
                        const vec3_t normal, float distance,
                        float epsilon, winding_t **front,
                        winding_t **back);
winding_t *ChopWinding(winding_t *winding,
                       const vec3_t normal, float distance);
void ChopWindingInPlace(winding_t **winding, const vec3_t normal,
                        float distance, float epsilon);
void CheckWinding(const winding_t *winding);
int32_t WindingOnPlaneSide(const winding_t *winding,
                           const vec3_t normal, float distance);
void AddWindingToConvexHull(const winding_t *source,
                            winding_t **winding,
                            const vec3_t normal);

#ifdef __cplusplus
}
#endif

#endif
