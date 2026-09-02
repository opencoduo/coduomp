#ifndef QCOMMON_COLLISION_TRACE_WORK_TYPES_H
#define QCOMMON_COLLISION_TRACE_WORK_TYPES_H

#include <stddef.h>
#include <stdint.h>

#include "q_collision_types.h"
#include "q_shared_types.h"
#include "q_vector_types.h"

/*
 * Complete collision-trace work records shared by the Windows client engine
 * and Linux dedicated server.  The original i386 bodies agree on every field,
 * access width, and offset.  sphere_t and traceWork_t retain their inherited
 * Quake III names; the remaining names are exposed by the CoD client symbols.
 */
typedef struct sphere_s {
    qboolean use;
    float radius;
    float halfheight;
    vec3_t offset;
} sphere_t;

typedef struct cmTraceSphereRecord_s {
    sphere_t sphere;
    vec3_t extents;
} cmTraceSphereRecord_t;

typedef struct traceWork_s {
    vec3_t start;
    vec3_t end;
    vec3_t delta;
    float deltaLengthSquared;
    vec3_t mins;
    vec3_t maxs;
    vec3_t offsets[8];
    /* Written by both original engines but not read by either one. */
    float maxsSum;
    vec3_t bounds[2];
    int32_t contents;
    qboolean isPoint;
    trace_t trace;
    sphere_t sphere;
    vec3_t sphereExtents;
} traceWork_t;

typedef struct cmPointTraceStaticModelsWork_s {
    trace_t trace;
    int32_t contentsMask;
    vec3_t start;
    vec3_t end;
} cmPointTraceStaticModelsWork_t;

typedef struct cmClipMoveWork_s {
    vec3_t mins;
    vec3_t maxs;
    vec3_t expandedHalfSize;
    vec3_t start;
    vec3_t end;
    trace_t bestTrace;
    int32_t passEntityNum;
    int32_t passOwnerNum;
    int32_t contentsMask;
    qboolean capsule;
} cmClipMoveWork_t;

typedef struct cmClipSightTraceWork_s {
    vec3_t mins;
    vec3_t maxs;
    vec3_t expandedHalfSize;
    vec3_t start;
    vec3_t end;
    int32_t passEntityNum;
    int32_t passOwnerNum;
    int32_t passEntityOwnerNum;
    int32_t passOwnerOwnerNum;
    int32_t contentsMask;
    qboolean capsule;
} cmClipSightTraceWork_t;

typedef struct cmPointTraceWork_s {
    vec3_t start;
    vec3_t end;
    trace_t bestTrace;
    int32_t passEntityNum;
    int32_t passOwnerNum;
    int32_t contentsMask;
    qboolean useDObj;
    const uint8_t *dobjTracePartState;
} cmPointTraceWork_t;

typedef struct cmPointSightTraceWork_s {
    vec3_t start;
    vec3_t end;
    int32_t passEntityNum;
    int32_t passOwnerNum;
    int32_t passEntityOwnerNum;
    int32_t passOwnerOwnerNum;
    int32_t contentsMask;
} cmPointSightTraceWork_t;

typedef struct cmSightTraceStaticModelsWork_s {
    int32_t contentsMask;
    vec3_t start;
    vec3_t end;
} cmSightTraceStaticModelsWork_t;

#if defined(__cplusplus)
#define COLLISION_TRACE_WORK_STATIC_ASSERT(expression, message) static_assert((expression), message)
#define COLLISION_TRACE_WORK_ALIGNOF(type) alignof(type)
#else
#define COLLISION_TRACE_WORK_STATIC_ASSERT(expression, message) _Static_assert((expression), message)
#define COLLISION_TRACE_WORK_ALIGNOF(type) _Alignof(type)
#endif

#if UINTPTR_MAX == UINT32_MAX
#define COLLISION_TRACE_WORK_ASSERT_FIELD(type, field, offset) \
    COLLISION_TRACE_WORK_STATIC_ASSERT(offsetof(type, field) == (offset), #type "." #field " offset")

COLLISION_TRACE_WORK_STATIC_ASSERT(COLLISION_TRACE_WORK_ALIGNOF(sphere_t) == 4, "sphere_t alignment");
COLLISION_TRACE_WORK_ASSERT_FIELD(sphere_t, use, 0x00);
COLLISION_TRACE_WORK_ASSERT_FIELD(sphere_t, radius, 0x04);
COLLISION_TRACE_WORK_ASSERT_FIELD(sphere_t, halfheight, 0x08);
COLLISION_TRACE_WORK_ASSERT_FIELD(sphere_t, offset, 0x0c);
COLLISION_TRACE_WORK_STATIC_ASSERT(sizeof(sphere_t) == 0x18, "sphere_t size");

COLLISION_TRACE_WORK_ASSERT_FIELD(cmTraceSphereRecord_t, sphere, 0x00);
COLLISION_TRACE_WORK_ASSERT_FIELD(cmTraceSphereRecord_t, extents, 0x18);
COLLISION_TRACE_WORK_STATIC_ASSERT(sizeof(cmTraceSphereRecord_t) == 0x24, "cmTraceSphereRecord_t size");

COLLISION_TRACE_WORK_ASSERT_FIELD(traceWork_t, start, 0x00);
COLLISION_TRACE_WORK_ASSERT_FIELD(traceWork_t, end, 0x0c);
COLLISION_TRACE_WORK_ASSERT_FIELD(traceWork_t, delta, 0x18);
COLLISION_TRACE_WORK_ASSERT_FIELD(traceWork_t, deltaLengthSquared, 0x24);
COLLISION_TRACE_WORK_ASSERT_FIELD(traceWork_t, mins, 0x28);
COLLISION_TRACE_WORK_ASSERT_FIELD(traceWork_t, maxs, 0x34);
COLLISION_TRACE_WORK_ASSERT_FIELD(traceWork_t, offsets, 0x40);
COLLISION_TRACE_WORK_ASSERT_FIELD(traceWork_t, maxsSum, 0xa0);
COLLISION_TRACE_WORK_ASSERT_FIELD(traceWork_t, bounds, 0xa4);
COLLISION_TRACE_WORK_ASSERT_FIELD(traceWork_t, contents, 0xbc);
COLLISION_TRACE_WORK_ASSERT_FIELD(traceWork_t, isPoint, 0xc0);
COLLISION_TRACE_WORK_ASSERT_FIELD(traceWork_t, trace, 0xc4);
COLLISION_TRACE_WORK_ASSERT_FIELD(traceWork_t, sphere, 0xf4);
COLLISION_TRACE_WORK_ASSERT_FIELD(traceWork_t, sphereExtents, 0x10c);
COLLISION_TRACE_WORK_STATIC_ASSERT(sizeof(traceWork_t) == 0x118, "traceWork_t size");

COLLISION_TRACE_WORK_ASSERT_FIELD(cmPointTraceStaticModelsWork_t, trace, 0x00);
COLLISION_TRACE_WORK_ASSERT_FIELD(cmPointTraceStaticModelsWork_t, contentsMask, 0x30);
COLLISION_TRACE_WORK_ASSERT_FIELD(cmPointTraceStaticModelsWork_t, start, 0x34);
COLLISION_TRACE_WORK_ASSERT_FIELD(cmPointTraceStaticModelsWork_t, end, 0x40);
COLLISION_TRACE_WORK_STATIC_ASSERT(sizeof(cmPointTraceStaticModelsWork_t) == 0x4c, "cmPointTraceStaticModelsWork_t size");

COLLISION_TRACE_WORK_ASSERT_FIELD(cmClipMoveWork_t, mins, 0x00);
COLLISION_TRACE_WORK_ASSERT_FIELD(cmClipMoveWork_t, maxs, 0x0c);
COLLISION_TRACE_WORK_ASSERT_FIELD(cmClipMoveWork_t, expandedHalfSize, 0x18);
COLLISION_TRACE_WORK_ASSERT_FIELD(cmClipMoveWork_t, start, 0x24);
COLLISION_TRACE_WORK_ASSERT_FIELD(cmClipMoveWork_t, end, 0x30);
COLLISION_TRACE_WORK_ASSERT_FIELD(cmClipMoveWork_t, bestTrace, 0x3c);
COLLISION_TRACE_WORK_ASSERT_FIELD(cmClipMoveWork_t, passEntityNum, 0x6c);
COLLISION_TRACE_WORK_ASSERT_FIELD(cmClipMoveWork_t, passOwnerNum, 0x70);
COLLISION_TRACE_WORK_ASSERT_FIELD(cmClipMoveWork_t, contentsMask, 0x74);
COLLISION_TRACE_WORK_ASSERT_FIELD(cmClipMoveWork_t, capsule, 0x78);
COLLISION_TRACE_WORK_STATIC_ASSERT(sizeof(cmClipMoveWork_t) == 0x7c, "cmClipMoveWork_t size");

COLLISION_TRACE_WORK_ASSERT_FIELD(cmClipSightTraceWork_t, mins, 0x00);
COLLISION_TRACE_WORK_ASSERT_FIELD(cmClipSightTraceWork_t, maxs, 0x0c);
COLLISION_TRACE_WORK_ASSERT_FIELD(cmClipSightTraceWork_t, expandedHalfSize, 0x18);
COLLISION_TRACE_WORK_ASSERT_FIELD(cmClipSightTraceWork_t, start, 0x24);
COLLISION_TRACE_WORK_ASSERT_FIELD(cmClipSightTraceWork_t, end, 0x30);
COLLISION_TRACE_WORK_ASSERT_FIELD(cmClipSightTraceWork_t, passEntityNum, 0x3c);
COLLISION_TRACE_WORK_ASSERT_FIELD(cmClipSightTraceWork_t, passOwnerNum, 0x40);
COLLISION_TRACE_WORK_ASSERT_FIELD(cmClipSightTraceWork_t, passEntityOwnerNum, 0x44);
COLLISION_TRACE_WORK_ASSERT_FIELD(cmClipSightTraceWork_t, passOwnerOwnerNum, 0x48);
COLLISION_TRACE_WORK_ASSERT_FIELD(cmClipSightTraceWork_t, contentsMask, 0x4c);
COLLISION_TRACE_WORK_ASSERT_FIELD(cmClipSightTraceWork_t, capsule, 0x50);
COLLISION_TRACE_WORK_STATIC_ASSERT(sizeof(cmClipSightTraceWork_t) == 0x54, "cmClipSightTraceWork_t size");

COLLISION_TRACE_WORK_ASSERT_FIELD(cmPointTraceWork_t, start, 0x00);
COLLISION_TRACE_WORK_ASSERT_FIELD(cmPointTraceWork_t, end, 0x0c);
COLLISION_TRACE_WORK_ASSERT_FIELD(cmPointTraceWork_t, bestTrace, 0x18);
COLLISION_TRACE_WORK_ASSERT_FIELD(cmPointTraceWork_t, passEntityNum, 0x48);
COLLISION_TRACE_WORK_ASSERT_FIELD(cmPointTraceWork_t, passOwnerNum, 0x4c);
COLLISION_TRACE_WORK_ASSERT_FIELD(cmPointTraceWork_t, contentsMask, 0x50);
COLLISION_TRACE_WORK_ASSERT_FIELD(cmPointTraceWork_t, useDObj, 0x54);
COLLISION_TRACE_WORK_ASSERT_FIELD(cmPointTraceWork_t, dobjTracePartState, 0x58);
COLLISION_TRACE_WORK_STATIC_ASSERT(sizeof(cmPointTraceWork_t) == 0x5c, "cmPointTraceWork_t size");

COLLISION_TRACE_WORK_ASSERT_FIELD(cmPointSightTraceWork_t, start, 0x00);
COLLISION_TRACE_WORK_ASSERT_FIELD(cmPointSightTraceWork_t, end, 0x0c);
COLLISION_TRACE_WORK_ASSERT_FIELD(cmPointSightTraceWork_t, passEntityNum, 0x18);
COLLISION_TRACE_WORK_ASSERT_FIELD(cmPointSightTraceWork_t, passOwnerNum, 0x1c);
COLLISION_TRACE_WORK_ASSERT_FIELD(cmPointSightTraceWork_t, passEntityOwnerNum, 0x20);
COLLISION_TRACE_WORK_ASSERT_FIELD(cmPointSightTraceWork_t, passOwnerOwnerNum, 0x24);
COLLISION_TRACE_WORK_ASSERT_FIELD(cmPointSightTraceWork_t, contentsMask, 0x28);
COLLISION_TRACE_WORK_STATIC_ASSERT(sizeof(cmPointSightTraceWork_t) == 0x2c, "cmPointSightTraceWork_t size");

COLLISION_TRACE_WORK_ASSERT_FIELD(cmSightTraceStaticModelsWork_t, contentsMask, 0x00);
COLLISION_TRACE_WORK_ASSERT_FIELD(cmSightTraceStaticModelsWork_t, start, 0x04);
COLLISION_TRACE_WORK_ASSERT_FIELD(cmSightTraceStaticModelsWork_t, end, 0x10);
COLLISION_TRACE_WORK_STATIC_ASSERT(sizeof(cmSightTraceStaticModelsWork_t) == 0x1c, "cmSightTraceStaticModelsWork_t size");

#undef COLLISION_TRACE_WORK_ASSERT_FIELD
#endif

#undef COLLISION_TRACE_WORK_ALIGNOF
#undef COLLISION_TRACE_WORK_STATIC_ASSERT

#endif
