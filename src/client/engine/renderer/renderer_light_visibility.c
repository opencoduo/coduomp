#include "backend.h"

#include "../math/vector_math.h"
#include "../physics/cm_trace.h"
#include "../system_fatal.h"
#include "gl_state.h"

#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

enum {
    REVERSE_BITS_TABLE_SIZE = UINT8_MAX + 1,
    LIGHT_CACHE_TRACE_CONTENTS = 0x2001,
    LIGHT_CACHE_SKY_VISIBLE = 2,
    LIGHT_VIS_DIFFUSE_SUN_BIT = 0x8000,
    DIFFUSE_SUN_WIDE_SAMPLE_MAX_STEPS = 3,
    LIGHT_VIS_GRID_X_MASK = 0x3ff,
    LIGHT_VIS_CLUSTER_MASK = 0xfff,
    LIGHT_VIS_BUCKET_MASK = 0x1fff,
    LIGHT_VIS_GRID_Y_HASH_FACTOR = 0x0c41,
    LIGHT_VIS_GRID_X_HASH_FACTOR = 0x0c3d,
    LIGHT_VIS_GRID_XY_BIAS = 4096,
    LIGHT_VIS_GRID_Z_BIAS = 2048,
    LIGHT_VIS_GRID_XY_SCALE = 32,
    LIGHT_VIS_GRID_Z_SCALE = 64,
    LIGHT_VIS_GRID_XY_MAX_INDEX = 16384,
    LIGHT_VIS_GRID_Z_MAX_INDEX = 8192,
    LIGHT_VIS_HISTORY_FIND = 0,
    LIGHT_VIS_HISTORY_INSERT = 1,
    LIGHT_VIS_HISTORY_INSERT_DUPLICATE = 2,
    LIGHT_VIS_CACHE_DISK_TYPE = 32
};

#define DIFFUSE_SUN_WIDE_SAMPLE_SPACING 32768.0f /* 0x47000000 */
#define DIFFUSE_SUN_NARROW_SAMPLE_SPACING 16384.0f /* 0x46800000 */
#define DIFFUSE_SUN_TRACE_START_SCALE \
    6.103515488575795e-07f /* 0x3523d70a (~1/1638400). FMUL ds:0x5b9f04 at
                            * 0x4c5958/0x4c5996 (R_SampleDiffuseSunVisibility) and
                            * 0x4c6308/0x4c6348 (R_ShowLeafLights). A prior pass used
                            * the ADJACENT .rdata slot 0x5b9f00 = 38.4 (0x4219999a),
                            * which is referenced only by an unrelated function
                            * (0x4e9d7c) -- displacing the sun sample-ray start by
                            * ~offset*38.4 instead of ~0. */
#define DIFFUSE_SUN_TRACE_START_HEIGHT \
    0.100000001490116119384765625f /* 0x3dcccccd */
#define DIFFUSE_SUN_TRACE_END_HEIGHT 32768.0f /* 0x47000000 */

#define LIGHT_CACHE_TRACE_START_OFFSET \
    0.00999999977648258203125f /* 0x3c23d70a */
#define LIGHT_CACHE_DIRECTIONAL_START_SCALE \
    0.100000001490116119384765625f /* 0x3dcccccd */
#define LIGHT_CACHE_DIRECTIONAL_END_SCALE 32768.0f /* 0x47000000 */

#define SHOW_LEAF_LIGHT_VIEW_DOT_MINIMUM \
    0.949999988079071044921875f /* 0x3f733333 */
#define SHOW_LEAF_LIGHT_MAX_DISTANCE_SQUARED 9216.0f /* 0x46100000 */
#define SHOW_LEAF_LIGHT_DIRECTION_LENGTH 1024.0f /* 0x44800000 */

/* Exact original 0x005b9bc0 float. Semantically this is one cache entry as a
 * percentage of the 8192 * 32 entry light-visibility cache. */
#define LIGHT_VIS_CACHE_PERCENT_PER_ENTRY \
    0.0003814697265625f /* 0x39c80000 = 100 / 262144 */

/* Exact Windows constants used to map the current view origin onto the
 * light-vis grid. The two double adjustments make x87 round-to-nearest act as
 * round-half-up for X/Y and floor for Z over the biased nonnegative range. */
#define LIGHT_VIS_GRID_WORLD_MIN -131072.0f /* 0xc8000000 */
#define LIGHT_VIS_GRID_ROUND_EPSILON \
    0.000000000931322574615478515625 /* 0x3e10000000000000 */
#define LIGHT_VIS_GRID_FLOOR_ADJUSTMENT \
    0.499999999068677425384521484375 /* 0x3fdfffffff000000 */

/* Shared renderer debug colors pooled by the original compiler at the noted
 * .rdata addresses. */
static const vec4_t leafLightColorRed =       /* 0x0058fba8 */
    {1.0f, 0.0f, 0.0f, 1.0f};
static const vec4_t leafLightColorGreen =     /* 0x0058fbb8 */
    {0.0f, 1.0f, 0.0f, 1.0f};
static const vec4_t leafLightColorBlue =      /* 0x0058fbd8 */
    {0.0f, 0.0f, 1.0f, 1.0f};
static const vec4_t leafLightColorYellow =    /* 0x0058fbe8 */
    {1.0f, 1.0f, 0.0f, 1.0f};
static const vec4_t leafLightColorGray =      /* 0x0058fc78 */
    {0.75f, 0.75f, 0.75f, 1.0f};
static const vec4_t leafLightColorOrange =    /* 0x0058fca8 */
    {1.0f, 0.699999988079071044921875f, 0.0f, 1.0f};

/* Original storage: lazy-initialization guard at 0x005ce914 (.data, initially
 * one) and 256-byte table at 0x00d92ea8 (.bss). */
static qboolean reverseBitsTableNeedsInitialization = qtrue;
static uint8_t reverseBitsTable[REVERSE_BITS_TABLE_SIZE];

/* Source: CoDUOMP.exe 0x004c56b0..0x004c5774.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004c56b0_004c5775.mcode.
 * Name and four-byte result ordering: same-module Mac symbol reverse_bits.
 * Each output byte reverses the bits of the corresponding input byte; the
 * bytes themselves remain in their original positions. */
uint32_t reverse_bits(uint32_t value)
{
    uint32_t result = 0;

    if (reverseBitsTableNeedsInitialization != qfalse) {
        reverseBitsTableNeedsInitialization = qfalse;

        for (uint32_t byteValue = 0;
             byteValue < REVERSE_BITS_TABLE_SIZE;
             ++byteValue) {
            uint8_t reversed = 0;

            for (uint32_t sourceBit = 0;
                 sourceBit < CHAR_BIT;
                 ++sourceBit) {
                if ((byteValue & (1U << sourceBit)) != 0U) {
                    reversed |= (uint8_t)(
                        1U << ((CHAR_BIT - 1U) - sourceBit));
                }
            }
            reverseBitsTable[byteValue] = reversed;
        }
    }

    for (uint32_t byteIndex = 0;
         byteIndex < sizeof(value);
         ++byteIndex) {
        const uint32_t shift = byteIndex * CHAR_BIT;
        const uint32_t byteValue = (value >> shift) & UINT8_MAX;

        result |= (uint32_t)reverseBitsTable[byteValue] << shift;
    }

    return result;
}

/* Source: CoDUOMP.exe 0x004c5780..0x004c57a7, exporter-gap recovery.
 * Name: same-module Mac symbol R_SkyTracePassed. Windows proves the contents,
 * fraction, surfaceFlags, and startsolid offsets against trace_t directly. */
qboolean R_SkyTracePassed(const trace_t *trace)
{
    if (trace->startsolid != 0) {
        return ((uint32_t)trace->contents &
                CONTENTS_SKY) != 0U
                   ? qtrue
                   : qfalse;
    }

    if (trace->fraction == 1.0f ||
        ((uint32_t)trace->surfaceFlags & SURF_SKY) != 0U) {
        return qtrue;
    }
    return qfalse;
}

/* Source: CoDUOMP.exe 0x004c57f0..0x004c5856.
 * Name and source-level R_SkyTracePassed call: same-module Mac symbol
 * R_LightCacheSkyTrace. Windows inlines that predicate after the exact
 * CM_BoxTrace call and returns the byte-valued visibility weight 0 or 2. */
uint8_t R_LightCacheSkyTrace(const vec3_t start, const vec3_t end)
{
    trace_t trace;

    CM_BoxTrace(&trace, start, end, vec3_origin, vec3_origin,
                CM_WORLD_MODEL, LIGHT_CACHE_TRACE_CONTENTS, qfalse);
    return R_SkyTracePassed(&trace) != qfalse
               ? LIGHT_CACHE_SKY_VISIBLE
               : 0;
}

/* Source: CoDUOMP.exe 0x004c5860..0x004c58b2.
 * Name: same-module Mac symbol R_LightCacheTrace. A light-cache ray passes
 * only when the world trace neither starts solid nor stops before fraction
 * one. Writing the comparison as a less-than rejection matches the Windows
 * x87 unordered path as well. */
qboolean R_LightCacheTrace(const vec3_t start, const vec3_t end)
{
    trace_t trace;

    CM_BoxTrace(&trace, start, end, vec3_origin, vec3_origin,
                CM_WORLD_MODEL, LIGHT_CACHE_TRACE_CONTENTS, qfalse);
    if (trace.startsolid != 0 || trace.fraction < 1.0f)
        return qfalse;
    return qtrue;
}

/* Source: CoDUOMP.exe 0x004c5cb0..0x004c5cff.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004c5cb0_004c5d00.mcode.
 * Name and source parameter roles: same-module Mac symbol R_LightVisHash.
 * Windows proves the packed key, bit-reversed Z contribution, both hash
 * coefficients, and 8192-bucket mask. Unsigned arithmetic states the original
 * 32-bit shift/wrap behavior for negative grid coordinates explicitly. */
void R_LightVisHash(int32_t gridX, int32_t gridY, int32_t gridZ,
                    int32_t cluster,
                    uint32_t *cacheKey, uint32_t *bucketIndex)
{
    *cacheKey = ((uint32_t)gridY << 22) |
                (((uint32_t)gridX & LIGHT_VIS_GRID_X_MASK) << 12) |
                ((uint32_t)cluster & LIGHT_VIS_CLUSTER_MASK);
    *bucketIndex =
        (reverse_bits((uint32_t)gridZ) +
         (uint32_t)gridY * LIGHT_VIS_GRID_Y_HASH_FACTOR -
         (uint32_t)gridX * LIGHT_VIS_GRID_X_HASH_FACTOR) &
        LIGHT_VIS_BUCKET_MASK;
}

/* Source: CoDUOMP.exe 0x004c5d00..0x004c5f16.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004c5d00_004c5f17.mcode.
 * Name and source parameter roles: same-module Mac symbol
 * R_GetCachedVisibility. Windows proves the 32-entry bucket walk, history
 * record layout and limit, miss statistics, full-bucket move, grid-to-world
 * conversion, sampling call, output byte, and UINT32_MAX blocked sentinel. */
uint32_t R_GetCachedVisibility(
    int32_t gridX, int32_t gridY, int32_t gridZ, int32_t cluster,
    int32_t lightCount, renderer_light_t *const *lights,
    const vec3_t traceTarget, uint8_t *diffuseSunVisibility)
{
    uint32_t cacheKey;
    uint32_t bucketIndex;
    renderer_light_vis_cache_entry_t *bucket;
    renderer_light_vis_cache_entry_t *cacheEntry;
    int32_t remainingEntries = R_LIGHT_VIS_ENTRIES_PER_BUCKET;

    R_LightVisHash(gridX, gridY, gridZ, cluster,
                   &cacheKey, &bucketIndex);
    bucket = rendererLightVisCache[bucketIndex];
    cacheEntry = bucket;

    while (cacheEntry->sampleState != R_LIGHT_VIS_SAMPLE_EMPTY) {
        if (cacheEntry->key == cacheKey) {
            *diffuseSunVisibility = cacheEntry->diffuseSunVisibility;
            return cacheEntry->sampleState == R_LIGHT_VIS_SAMPLE_VALID
                       ? cacheEntry->visibleLightBits
                       : UINT32_MAX;
        }
        ++cacheEntry;
        --remainingEntries;
        if (remainingEntries == 0)
            break;
    }

    if (rendererLightVisHistory != NULL &&
        rendererLightVisHistoryCount < R_LIGHT_VIS_HISTORY_MAX_ENTRIES) {
        renderer_light_vis_history_entry_t *historyEntry =
            &rendererLightVisHistory[rendererLightVisHistoryCount];

        historyEntry->gridX = gridX;
        historyEntry->gridY = gridY;
        historyEntry->gridZ = gridZ;
        historyEntry->traceTarget[0] = traceTarget[0];
        historyEntry->traceTarget[1] = traceTarget[1];
        historyEntry->traceTarget[2] = traceTarget[2];
        (void)R_SortedHistoryEntry(gridX, gridY, gridZ,
                                   LIGHT_VIS_HISTORY_INSERT);
        ++rendererLightVisHistoryCount;
    }

    rendererLightVisRuntimeFillCount = (int32_t)(
        (uint32_t)rendererLightVisRuntimeFillCount + 1u);
    if (remainingEntries == 0) {
        memmove(&bucket[1], &bucket[0],
                (R_LIGHT_VIS_ENTRIES_PER_BUCKET - 1) * sizeof(bucket[0]));
        cacheEntry = &bucket[0];
        rendererLightVisFlushedEntryCount = (int32_t)(
            (uint32_t)rendererLightVisFlushedEntryCount + 1u);
    } else {
        const int32_t probeDepth =
            (R_LIGHT_VIS_ENTRIES_PER_BUCKET + 1) - remainingEntries;

        ++rendererLightVisUsedEntryCount;
        if (rendererLightVisMaxAssociativity < probeDepth)
            rendererLightVisMaxAssociativity = probeDepth;
    }

    {
        vec3_t origin;

        const int32_t originX = (int32_t)(
            ((uint32_t)gridX - LIGHT_VIS_GRID_XY_BIAS) *
            LIGHT_VIS_GRID_XY_SCALE);
        const int32_t originY = (int32_t)(
            ((uint32_t)gridY - LIGHT_VIS_GRID_XY_BIAS) *
            LIGHT_VIS_GRID_XY_SCALE);
        const int32_t originZ = (int32_t)(
            ((uint32_t)gridZ - LIGHT_VIS_GRID_Z_BIAS) *
            LIGHT_VIS_GRID_Z_SCALE);

        origin[0] = (float)originX;
        origin[1] = (float)originY;
        origin[2] = (float)originZ;
        cacheEntry->key = cacheKey;
        (void)R_SampleLightVisibility(cacheEntry, origin,
                                      lightCount, lights, traceTarget);
    }

    *diffuseSunVisibility = cacheEntry->diffuseSunVisibility;
    return cacheEntry->sampleState == R_LIGHT_VIS_SAMPLE_VALID
               ? cacheEntry->visibleLightBits
               : UINT32_MAX;
}

/* Source: CoDUOMP.exe 0x004c58c0..0x004c5a52.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004c58c0_004c5a53.mcode.
 * Name and source-level trace helper: same-module Mac symbol
 * R_SampleDiffuseSunVisibility. R_LoadNodesAndLeafs proves that the tested
 * leaf field is a sun-light marker: a negative first light-list entry is
 * skipped and represented by hasSunLight. The original LTCG register ABI is
 * cacheEntry=ESI, origin=EDI, and leaf=EAX. Trace coordinates preserve each
 * retained multiply/add until its binary32 destination store. */
void R_SampleDiffuseSunVisibility(
    renderer_light_vis_cache_entry_t *cacheEntry,
    const vec3_t origin,
    const mnode_t *leaf)
{
    float sampleSpacing;
    float sampleRadius;
    vec3_t traceStart;
    vec3_t traceEnd;

    if (leaf->data.leaf.hasSunLight == qfalse)
        return;

    sampleSpacing = tr.diffuseSunSteps <= DIFFUSE_SUN_WIDE_SAMPLE_MAX_STEPS
                        ? DIFFUSE_SUN_WIDE_SAMPLE_SPACING
                        : DIFFUSE_SUN_NARROW_SAMPLE_SPACING;
    const int32_t sampleStepCount = (int32_t)(
        (uint32_t)tr.diffuseSunSteps - 1u);
    sampleRadius = (float)(
        (long double)sampleStepCount *
        ((long double)sampleSpacing * 0.5L));

    traceStart[2] = (float)(
        (long double)origin[2] + DIFFUSE_SUN_TRACE_START_HEIGHT);
    traceEnd[2] = (float)(
        (long double)origin[2] + DIFFUSE_SUN_TRACE_END_HEIGHT);

    float yOffset = -sampleRadius;
    while (yOffset <= sampleRadius) {
        traceStart[1] = (float)(
            (long double)yOffset * DIFFUSE_SUN_TRACE_START_SCALE +
            origin[1]);
        traceEnd[1] =
            (float)((long double)yOffset + origin[1]);

        float xOffset = -sampleRadius;
        while (xOffset <= sampleRadius) {
            uint8_t visibility;

            traceStart[0] = (float)(
                (long double)xOffset * DIFFUSE_SUN_TRACE_START_SCALE +
                origin[0]);
            traceEnd[0] =
                (float)((long double)xOffset + origin[0]);
            visibility = R_LightCacheSkyTrace(traceStart, traceEnd);
            if (visibility != 0) {
                cacheEntry->visibleLightBits |= LIGHT_VIS_DIFFUSE_SUN_BIT;
                cacheEntry->diffuseSunVisibility =
                    (uint8_t)(cacheEntry->diffuseSunVisibility + visibility);
            }

            /* 0x004c5a05 stores the next offset to the float loop local but
             * compares the still-retained x87 sum against sampleRadius. */
            const long double nextXOffset =
                (long double)xOffset + (long double)sampleSpacing;
            xOffset = (float)nextXOffset;
            if (nextXOffset > (long double)sampleRadius)
                break;
        }

        /* 0x004c5a20 uses the same store-without-pop loop test for Y. */
        const long double nextYOffset =
            (long double)yOffset + (long double)sampleSpacing;
        yOffset = (float)nextYOffset;
        if (nextYOffset > (long double)sampleRadius)
            break;
    }
}

/* Source: CoDUOMP.exe 0x004c5a60..0x004c5ca1.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004c5a60_004c5ca2.mcode.
 * Name, parameter roles, and source-level helper calls: same-module Mac symbol
 * R_SampleLightVisibility. Windows instructions prove the original LTCG ABI
 * (cacheEntry=EBX, origin=ECX, traceTarget=EAX), every leaf rejection, both
 * light trace constructions, the indexed-light bit writes, and cache state.
 * R_LoadNodesAndLeafs separately proves that a leaf has at most 15 indexed
 * lights, so each shift below is within the 16-bit result. */
mnode_t *R_SampleLightVisibility(
    renderer_light_vis_cache_entry_t *cacheEntry,
    const vec3_t origin,
    int32_t lightCount,
    renderer_light_t *const *lights,
    const vec3_t traceTarget)
{
    mnode_t *leaf;
    qboolean blocked = qfalse;

    cacheEntry->visibleLightBits = 0;
    cacheEntry->diffuseSunVisibility = 0;
    leaf = R_PointInLeaf(origin);

    if (((uint32_t)leaf->contents & CONTENTS_SOLID) != 0U ||
        leaf->data.leaf.cluster < 0) {
        blocked = qtrue;
    } else {
        vec3_t traceStart;

        traceStart[0] = traceTarget[0] - origin[0];
        traceStart[1] = traceTarget[1] - origin[1];
        traceStart[2] = traceTarget[2] - origin[2];
        VectorNormalizeFast(traceStart);
        traceStart[0] = origin[0] +
                        traceStart[0] * LIGHT_CACHE_TRACE_START_OFFSET;
        traceStart[1] = origin[1] +
                        traceStart[1] * LIGHT_CACHE_TRACE_START_OFFSET;
        traceStart[2] = origin[2] +
                        traceStart[2] * LIGHT_CACHE_TRACE_START_OFFSET;

        if (CM_BoxSightTrace(0, traceTarget, traceStart,
                             vec3_origin, vec3_origin,
                             CM_WORLD_MODEL, LIGHT_CACHE_TRACE_CONTENTS,
                             qfalse) != 0) {
            blocked = qtrue;
        }
    }

    if (blocked == qfalse) {
        for (int32_t lightIndex = 0;
             lightIndex < lightCount;
             ++lightIndex) {
            const renderer_light_t *light = lights[lightIndex];
            vec3_t traceStart;

            if (light->position[3] == 0.0f) {
                vec3_t traceEnd;

                traceStart[0] = origin[0] +
                    light->position[0] * LIGHT_CACHE_DIRECTIONAL_START_SCALE;
                traceStart[1] = origin[1] +
                    light->position[1] * LIGHT_CACHE_DIRECTIONAL_START_SCALE;
                traceStart[2] = origin[2] +
                    light->position[2] * LIGHT_CACHE_DIRECTIONAL_START_SCALE;
                traceEnd[0] = origin[0] +
                    light->position[0] * LIGHT_CACHE_DIRECTIONAL_END_SCALE;
                traceEnd[1] = origin[1] +
                    light->position[1] * LIGHT_CACHE_DIRECTIONAL_END_SCALE;
                traceEnd[2] = origin[2] +
                    light->position[2] * LIGHT_CACHE_DIRECTIONAL_END_SCALE;

                if (R_LightCacheSkyTrace(traceStart, traceEnd) !=
                    LIGHT_CACHE_SKY_VISIBLE) {
                    continue;
                }
            } else {
                traceStart[0] = light->position[0] - origin[0];
                traceStart[1] = light->position[1] - origin[1];
                traceStart[2] = light->position[2] - origin[2];
                VectorNormalizeFast(traceStart);
                traceStart[0] = origin[0] +
                    traceStart[0] * LIGHT_CACHE_DIRECTIONAL_START_SCALE;
                traceStart[1] = origin[1] +
                    traceStart[1] * LIGHT_CACHE_DIRECTIONAL_START_SCALE;
                traceStart[2] = origin[2] +
                    traceStart[2] * LIGHT_CACHE_DIRECTIONAL_START_SCALE;

                if (R_LightCacheTrace(traceStart, light->position) == qfalse)
                    continue;
            }

            cacheEntry->visibleLightBits |=
                (uint16_t)(1U << (uint32_t)lightIndex);
        }

        R_SampleDiffuseSunVisibility(cacheEntry, origin, leaf);
    }

    cacheEntry->sampleState = blocked != qfalse
                                  ? R_LIGHT_VIS_SAMPLE_BLOCKED
                                  : R_LIGHT_VIS_SAMPLE_VALID;
    return leaf;
}

/* Source: CoDUOMP.exe 0x004c60c0..0x004c63c3.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004c60c0_004c63c4.mcode.
 * Name, parameter roles, and debug-draw purpose: same-module Mac symbol
 * R_ShowLeafLights. Windows proves both optional view gates, the leaf light
 * table walk, entity-light membership test, positional/directional colors,
 * and the diffuse-sun debug sample grid. */
void R_ShowLeafLights(const vec3_t point, const trRefEntity_t *entity)
{
    mnode_t *leaf;
    int32_t showMode;

    if (tr.world == NULL || tr.world->nodes == NULL)
        return;

    showMode = r_showLeafLights->integer;
    if (showMode == R_SHOW_LEAF_LIGHTS_VIEW_CONE) {
        vec3_t viewDirection;
        float viewDot;

        viewDirection[0] = point[0] - tr.refdef.vieworg[0];
        viewDirection[1] = point[1] - tr.refdef.vieworg[1];
        viewDirection[2] = point[2] - tr.refdef.vieworg[2];
        VectorNormalizeFast(viewDirection);
        viewDot = tr.refdef.viewaxis[0][2] * viewDirection[2] +
                  tr.refdef.viewaxis[0][1] * viewDirection[1] +
                  tr.refdef.viewaxis[0][0] * viewDirection[0];
        if (viewDot < SHOW_LEAF_LIGHT_VIEW_DOT_MINIMUM)
            return;
    }

    if (showMode == R_SHOW_LEAF_LIGHTS_NEARBY &&
        VectorDistanceSquared(point, tr.refdef.vieworg) >
            SHOW_LEAF_LIGHT_MAX_DISTANCE_SQUARED) {
        return;
    }

    leaf = R_PointInLeaf(point);
    if (leaf == NULL)
        return;

    for (int32_t leafLightIndex = 0;
         leafLightIndex < leaf->data.leaf.lightCount;
         ++leafLightIndex) {
        const int32_t worldLightIndex = tr.world->lightIndexes[
            leaf->data.leaf.firstLightIndex + leafLightIndex];
        const renderer_light_t *light =
            &tr.world->lights[worldLightIndex];
        qboolean attachedToEntity = qfalse;
        const vec4_t *color;
        vec3_t directionalEnd;
        const float *lineEnd;

        for (int32_t entityLightIndex = 0;
             entityLightIndex < entity->lightCount;
             ++entityLightIndex) {
            if (entity->lights[entityLightIndex].light == light) {
                attachedToEntity = qtrue;
                break;
            }
        }

        color = attachedToEntity != qfalse
                    ? &leafLightColorGreen
                    : &leafLightColorRed;
        if (light->position[3] == 0.0f) {
            color = attachedToEntity != qfalse
                        ? &leafLightColorYellow
                        : &leafLightColorBlue;
            directionalEnd[0] = point[0] +
                light->position[0] * SHOW_LEAF_LIGHT_DIRECTION_LENGTH;
            directionalEnd[1] = point[1] +
                light->position[1] * SHOW_LEAF_LIGHT_DIRECTION_LENGTH;
            directionalEnd[2] = point[2] +
                light->position[2] * SHOW_LEAF_LIGHT_DIRECTION_LENGTH;
            lineEnd = directionalEnd;
        } else {
            lineEnd = light->position;
        }
        R_AddDebugLine(point, lineEnd, *color);
    }

    if (r_showLeafLights->integer >= R_SHOW_LEAF_LIGHTS_NEARBY &&
        leaf->data.leaf.hasSunLight != qfalse) {
        float sampleSpacing =
            tr.diffuseSunSteps <= DIFFUSE_SUN_WIDE_SAMPLE_MAX_STEPS
                ? DIFFUSE_SUN_WIDE_SAMPLE_SPACING
                : DIFFUSE_SUN_NARROW_SAMPLE_SPACING;
        float sampleRadius = (float)(tr.diffuseSunSteps - 1) *
                             (sampleSpacing * 0.5f);
        vec3_t traceStart;
        vec3_t traceEnd;
        vec3_t blockedTraceEnd;

        traceStart[2] = point[2] + DIFFUSE_SUN_TRACE_START_HEIGHT;
        traceEnd[2] = point[2] + DIFFUSE_SUN_TRACE_END_HEIGHT;

        float yOffset = -sampleRadius;
        while (yOffset <= sampleRadius) {
            traceStart[1] = point[1] +
                yOffset * DIFFUSE_SUN_TRACE_START_SCALE;
            traceEnd[1] = point[1] + yOffset;

            float xOffset = -sampleRadius;
            while (xOffset <= sampleRadius) {
                qboolean skyVisible;

                traceStart[0] = point[0] +
                    xOffset * DIFFUSE_SUN_TRACE_START_SCALE;
                traceEnd[0] = point[0] + xOffset;
                skyVisible = R_LightCacheSkyTrace(traceStart, traceEnd) != 0
                                 ? qtrue
                                 : qfalse;

                if (skyVisible != qfalse) {
                    R_AddDebugLine(traceStart, traceEnd,
                                   leafLightColorOrange);
                } else {
                    /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
                    blockedTraceEnd[0] = traceStart[0];
                    blockedTraceEnd[1] = traceStart[1];
                    blockedTraceEnd[2] = traceStart[2];
                    R_AddDebugLine(traceStart, blockedTraceEnd,
                                   leafLightColorGray);
                }

                /* 0x004c6392 stores the float offset while retaining the x87
                 * increment for the loop-bound comparison. */
                const long double nextXOffset =
                    (long double)xOffset + (long double)sampleSpacing;
                xOffset = (float)nextXOffset;
                if (nextXOffset > (long double)sampleRadius)
                    break;
            }

            /* 0x004c63a9 is the matching retained Y increment. */
            const long double nextYOffset =
                (long double)yOffset + (long double)sampleSpacing;
            yOffset = (float)nextYOffset;
            if (nextYOffset > (long double)sampleRadius)
                break;
        }
    }
}

/* Source: CoDUOMP.exe 0x004c7bf0..0x004c7c5e.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004c7bf0_004c7c5e.mcode.
 * Name: exact same-module Mac symbol R_VC_Stats_f, independently confirmed by
 * the r_vc_stats command registration at 0x004c4d64 and the function's
 * complete light-visibility-cache statistics output. */
void R_VC_Stats_f(void)
{
    ri.Printf(R_PRINT_ALL, "light visibility cache performance:\n");
    ri.Printf(R_PRINT_ALL, "%i entries used (%.1f%%)\n",
              rendererLightVisUsedEntryCount,
              (double)rendererLightVisUsedEntryCount *
                  (double)LIGHT_VIS_CACHE_PERCENT_PER_ENTRY);
    ri.Printf(R_PRINT_ALL, "%i max associativity\n",
              rendererLightVisMaxAssociativity);
    ri.Printf(R_PRINT_ALL, "%i entries flushed\n",
              rendererLightVisFlushedEntryCount);
    ri.Printf(R_PRINT_ALL,
              "%i entries filled in at runtime instead of read from disk\n",
              rendererLightVisRuntimeFillCount);
}

/* Source: CoDUOMP.exe 0x004c7c60..0x004c7cd5.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004c7c60_004c7cd5.mcode.
 * Name: same-module Mac symbol R_LightVisHistoryFilename. The Windows body
 * proves that world.name is a 64-byte path at world +0x000: it copies up to
 * the first period, checks the completed name, and appends ".vclog". */
void R_LightVisHistoryFilename(char *filename)
{
    const char *worldName = tr.world->name;
    char *write = filename;
    static const char suffix[] = ".vclog";

    while (*worldName != '\0' && *worldName != '.')
        *write++ = *worldName++;
    *write = '\0';

    if ((size_t)(write - filename) + sizeof(suffix) >
        R_WORLD_NAME_SIZE) {
        ri.Error(ERR_DROP,
                 "light vis cache log filename '%s.vclog' is too long\n",
                 filename);
    }

    memcpy(write, suffix, sizeof(suffix));
}

/* Source: CoDUOMP.exe 0x004c7ce0..0x004c7ebe.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004c7ce0_004c7ebe.mcode.
 * Name: same-module Mac symbol R_InitLightVisHistory. Windows proves both
 * allocation extents, the r_vc_makelog mode-2 file load, 24-byte record
 * validation/clamping, and replay of each saved point through the cache. */
void R_InitLightVisHistory(void)
{
    void *fileBuffer;
    int32_t fileSize;
    uint32_t loadBytes;
    uint32_t loadEntryCount;

    rendererLightVisHistoryCount = 0;
    rendererLightVisSortedHistoryCount = 0;

    if (r_vc_makelog->integer == 0) {
        rendererLightVisHistory = NULL;
        rendererLightVisSortedHistory = NULL;
        return;
    }

    rendererLightVisHistory = malloc(
        R_LIGHT_VIS_HISTORY_MAX_ENTRIES *
        sizeof(*rendererLightVisHistory));
    if (rendererLightVisHistory == NULL)
        Sys_OutOfMemory();
    memset(rendererLightVisHistory, 0,
           R_LIGHT_VIS_HISTORY_MAX_ENTRIES *
               sizeof(*rendererLightVisHistory));

    rendererLightVisSortedHistory = malloc(
        R_LIGHT_VIS_HISTORY_MAX_ENTRIES *
        sizeof(*rendererLightVisSortedHistory));
    if (rendererLightVisSortedHistory == NULL)
        Sys_OutOfMemory();
    memset(rendererLightVisSortedHistory, 0,
           R_LIGHT_VIS_HISTORY_MAX_ENTRIES *
               sizeof(*rendererLightVisSortedHistory));

    if (r_vc_makelog->integer != 2)
        return;

    {
        char filename[R_WORLD_NAME_SIZE];

        R_LightVisHistoryFilename(filename);
        fileSize = ri.FS_ReadFile(filename, &fileBuffer);
    }

    loadBytes = (uint32_t)fileSize;
    if (loadBytes % sizeof(*rendererLightVisHistory) == 0U) {
        if (loadBytes >
            R_LIGHT_VIS_HISTORY_MAX_ENTRIES *
                sizeof(*rendererLightVisHistory)) {
            loadBytes = R_LIGHT_VIS_HISTORY_MAX_ENTRIES *
                        sizeof(*rendererLightVisHistory);
        }

        memcpy(rendererLightVisHistory, fileBuffer, loadBytes);
        loadEntryCount = loadBytes / sizeof(*rendererLightVisHistory);

        for (uint32_t entryIndex = 0;
             entryIndex < loadEntryCount;
             ++entryIndex) {
            renderer_light_vis_history_entry_t *historyEntry =
                &rendererLightVisHistory[entryIndex];
            mnode_t *leaf =
                R_PointInLeaf(historyEntry->traceTarget);

            if (leaf->data.leaf.cluster >= 0) {
                renderer_light_t *lights[R_LIGHT_VIS_MAX_LEAF_LIGHTS];
                uint8_t diffuseSunVisibility;

                for (int32_t lightIndex = 0;
                     lightIndex < leaf->data.leaf.lightCount;
                     ++lightIndex) {
                    const int32_t worldLightIndex =
                        tr.world->lightIndexes[
                            leaf->data.leaf.firstLightIndex + lightIndex];

                    lights[lightIndex] =
                        &tr.world->lights[worldLightIndex];
                }

                (void)R_GetCachedVisibility(
                    historyEntry->gridX, historyEntry->gridY,
                    historyEntry->gridZ, leaf->data.leaf.cluster,
                    leaf->data.leaf.lightCount, lights,
                    historyEntry->traceTarget, &diffuseSunVisibility);
            }
        }
    }

    ri.FS_FreeFile(fileBuffer);
}

/* Source: CoDUOMP.exe 0x004c7ec0..0x004c7f45.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004c7ec0_004c7f45.mcode.
 * Name: same-module Mac symbol R_SaveLightVisHistory. */
void R_SaveLightVisHistory(void)
{
    if (rendererLightVisHistory == NULL || tr.world == NULL)
        return;

    {
        char filename[R_WORLD_NAME_SIZE];
        /* LEA/SHL at 0x004c7eeb..0x004c7ef4 multiply the count by
         * the 24-byte record size with wrapping 32-bit arithmetic. */
        const uint32_t writeBytes =
            (uint32_t)rendererLightVisHistoryCount *
            (uint32_t)sizeof(*rendererLightVisHistory);

        R_LightVisHistoryFilename(filename);
        ri.FS_WriteFile(filename, rendererLightVisHistory,
                        (int32_t)writeBytes);
    }

    free(rendererLightVisHistory);
    free(rendererLightVisSortedHistory);
    rendererLightVisHistory = NULL;
    rendererLightVisSortedHistory = NULL;
    rendererLightVisHistoryCount = 0;
    rendererLightVisSortedHistoryCount = 0;
}

/* Source: CoDUOMP.exe 0x004c8490..0x004c84e3.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004c8490_004c84e3.mcode.
 * Name: same-module Mac symbol R_AddSortedHistoryEntry. */
qboolean R_AddSortedHistoryEntry(
    int32_t insertIndex,
    const renderer_light_vis_sort_entry_t *entry)
{
    uint32_t moveBytes;

    if (rendererLightVisSortedHistoryCount >=
        R_LIGHT_VIS_HISTORY_MAX_ENTRIES) {
        return qfalse;
    }

    /* SUB/SHL at 0x004c84a5..0x004c84a7 form this byte count with
     * wrapping 32-bit arithmetic before the original i386 memmove call. */
    moveBytes =
        ((uint32_t)rendererLightVisSortedHistoryCount -
         (uint32_t)insertIndex) *
        (uint32_t)sizeof(*rendererLightVisSortedHistory);
    memmove(&rendererLightVisSortedHistory[insertIndex + 1],
            &rendererLightVisSortedHistory[insertIndex],
            (size_t)moveBytes);
    memcpy(&rendererLightVisSortedHistory[insertIndex], entry,
           sizeof(*entry));
    ++rendererLightVisSortedHistoryCount;
    return qtrue;
}

/* Source: CoDUOMP.exe 0x004c84f0..0x004c8595.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004c84f0_004c8595.mcode.
 * Name: same-module Mac symbol R_SortedHistoryEntry. Windows compares the
 * packed X/Y shorts first and Z second, then optionally inserts at the binary
 * search position. Mode 2 inserts a duplicate even when the key exists. */
int32_t R_SortedHistoryEntry(int32_t gridX, int32_t gridY,
                             int32_t gridZ, int32_t updateMode)
{
    renderer_light_vis_sort_entry_t sought;
    const uint32_t soughtXY =
        (uint32_t)(uint16_t)gridX |
        ((uint32_t)(uint16_t)gridY << 16);
    int32_t lower = 0;
    int32_t upper = rendererLightVisSortedHistoryCount - 1;

    sought.gridX = (int16_t)gridX;
    sought.gridY = (int16_t)gridY;
    sought.gridZ = (int16_t)gridZ;

    while (lower <= upper) {
        const int32_t middle = (lower + upper) / 2;
        const renderer_light_vis_sort_entry_t *entry =
            &rendererLightVisSortedHistory[middle];
        const uint32_t entryXY =
            (uint32_t)(uint16_t)entry->gridX |
            ((uint32_t)(uint16_t)entry->gridY << 16);
        int32_t comparison = (int32_t)(soughtXY - entryXY);

        if (comparison == 0) {
            comparison = (int32_t)(uint16_t)gridZ -
                         (int32_t)(uint16_t)entry->gridZ;
            if (comparison == 0) {
                if (updateMode == LIGHT_VIS_HISTORY_INSERT_DUPLICATE)
                    (void)R_AddSortedHistoryEntry(middle, &sought);
                return middle;
            }
        }

        if (comparison < 0)
            upper = middle - 1;
        else
            lower = middle + 1;
    }

    if (updateMode != LIGHT_VIS_HISTORY_FIND &&
        R_AddSortedHistoryEntry(lower, &sought) != qfalse) {
        return lower;
    }

    return -1;
}

/* Source: CoDUOMP.exe 0x004c85a0..0x004c8812.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004c85a0_004c8812.mcode.
 * Name and source-level callees: same-module Mac symbol
 * R_ShowLightVisCachePoints, whose call graph independently identifies
 * R_SortedHistoryEntry, R_CullPointAndRadius, and R_AddDebugString. Windows
 * proves the exact view-grid conversion, three Z layers, bounded X/Y radius,
 * history rejection, frustum gate, layer colors, scale, and "." marker. */
void R_ShowLightVisCachePoints(void)
{
    int32_t showRadius;
    int32_t centerGridX;
    int32_t centerGridY;
    int32_t centerGridZ;
    float biasedCoordinate;

    if (rendererLightVisHistory == NULL)
        return;

    showRadius = r_vc_showlog->integer;
    if (showRadius <= 0)
        return;

    biasedCoordinate =
        tr.viewParms.orientation.origin[0] - LIGHT_VIS_GRID_WORLD_MIN;
    centerGridX =
        (int32_t)lrint((double)biasedCoordinate +
                       LIGHT_VIS_GRID_ROUND_EPSILON) >> 5;

    biasedCoordinate =
        tr.viewParms.orientation.origin[1] - LIGHT_VIS_GRID_WORLD_MIN;
    centerGridY =
        (int32_t)lrint((double)biasedCoordinate +
                       LIGHT_VIS_GRID_ROUND_EPSILON) >> 5;

    biasedCoordinate =
        tr.viewParms.orientation.origin[2] - LIGHT_VIS_GRID_WORLD_MIN;
    centerGridZ =
        (int32_t)lrint((double)biasedCoordinate -
                       LIGHT_VIS_GRID_FLOOR_ADJUSTMENT) >> 6;

    for (int32_t zOffset = -1; zOffset <= 1; ++zOffset) {
        const int32_t gridZ = centerGridZ + zOffset;

        if (gridZ < 0 || gridZ > LIGHT_VIS_GRID_Z_MAX_INDEX)
            continue;

        for (int32_t yOffset = -showRadius;
             yOffset <= showRadius;
             ++yOffset) {
            const int32_t gridY = centerGridY + yOffset;

            if (gridY < 0 || gridY > LIGHT_VIS_GRID_XY_MAX_INDEX)
                continue;

            for (int32_t xOffset = -showRadius;
                 xOffset <= showRadius;
                 ++xOffset) {
                const int32_t gridX = centerGridX + xOffset;
                const vec4_t *color;
                vec3_t point;

                if (gridX < 0 || gridX > LIGHT_VIS_GRID_XY_MAX_INDEX)
                    continue;
                if (R_SortedHistoryEntry(
                        gridX, gridY, gridZ,
                        LIGHT_VIS_HISTORY_FIND) >= 0) {
                    continue;
                }

                point[0] = (float)((gridX - LIGHT_VIS_GRID_XY_BIAS) *
                                   LIGHT_VIS_GRID_XY_SCALE);
                point[1] = (float)((gridY - LIGHT_VIS_GRID_XY_BIAS) *
                                   LIGHT_VIS_GRID_XY_SCALE);
                point[2] = (float)((gridZ - LIGHT_VIS_GRID_Z_BIAS) *
                                   LIGHT_VIS_GRID_Z_SCALE);

                if (R_CullPointAndRadius(point, 0.0f) != CULL_IN)
                    continue;

                color = zOffset == -1
                            ? &leafLightColorYellow
                            : &leafLightColorGreen;
                R_AddDebugString(point, *color, 1.0f, ".");
            }
        }
    }
}

/* Source: CoDUOMP.exe 0x004c7f50..0x004c8121.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004c7f50_004c8121.mcode.
 * Name: same-module Mac symbol R_PrecalcLightVisCachePoint. Windows proves the
 * 8192x32 table of 12-byte disk records and its five diffuse-sun samples,
 * taken with tr.diffuseSunSteps set successively to one through five. Grid
 * conversion uses wrapping 32-bit subtract/shift arithmetic before FILD. */
void R_PrecalcLightVisCachePoint(
    int32_t gridX, int32_t gridY, int32_t gridZ,
    const vec3_t traceTarget,
    renderer_light_vis_disk_entry_t
        cache[R_LIGHT_VIS_BUCKET_COUNT][R_LIGHT_VIS_ENTRIES_PER_BUCKET])
{
    mnode_t *leaf = R_PointInLeaf(traceTarget);
    renderer_light_t *lights[R_LIGHT_VIS_MAX_LEAF_LIGHTS];
    uint32_t cacheKey;
    uint32_t bucketIndex;
    renderer_light_vis_disk_entry_t *diskEntry;
    vec3_t origin;
    renderer_light_vis_cache_entry_t sampled;
    mnode_t *sampledLeaf;

    if (leaf->data.leaf.cluster < 0)
        return;

    for (int32_t lightIndex = 0;
         lightIndex < leaf->data.leaf.lightCount;
         ++lightIndex) {
        const int32_t worldLightIndex = tr.world->lightIndexes[
            leaf->data.leaf.firstLightIndex + lightIndex];

        lights[lightIndex] = &tr.world->lights[worldLightIndex];
    }

    R_LightVisHash(gridX, gridY, gridZ, leaf->data.leaf.cluster,
                   &cacheKey, &bucketIndex);
    diskEntry = cache[bucketIndex];

    for (int32_t entryIndex = 0;
         entryIndex < R_LIGHT_VIS_ENTRIES_PER_BUCKET;
         ++entryIndex, ++diskEntry) {
        if (diskEntry->sampleState == R_LIGHT_VIS_SAMPLE_EMPTY)
            break;
        if (diskEntry->key == cacheKey)
            return;
    }

    if (diskEntry ==
        &cache[bucketIndex][R_LIGHT_VIS_ENTRIES_PER_BUCKET]) {
        return;
    }

    const int32_t originX = (int32_t)(
        ((uint32_t)gridX - LIGHT_VIS_GRID_XY_BIAS) *
        LIGHT_VIS_GRID_XY_SCALE);
    const int32_t originY = (int32_t)(
        ((uint32_t)gridY - LIGHT_VIS_GRID_XY_BIAS) *
        LIGHT_VIS_GRID_XY_SCALE);
    const int32_t originZ = (int32_t)(
        ((uint32_t)gridZ - LIGHT_VIS_GRID_Z_BIAS) *
        LIGHT_VIS_GRID_Z_SCALE);
    origin[0] = (float)originX;
    origin[1] = (float)originY;
    origin[2] = (float)originZ;

    tr.diffuseSunSteps = 1;
    sampledLeaf = R_SampleLightVisibility(
        &sampled, origin, leaf->data.leaf.lightCount, lights, traceTarget);

    diskEntry->key = cacheKey;
    diskEntry->sampleState = sampled.sampleState;
    diskEntry->diffuseSunVisibility[0] = sampled.diffuseSunVisibility;
    diskEntry->visibleLightBits = sampled.visibleLightBits;

    if (sampledLeaf->data.leaf.hasSunLight != qfalse) {
        for (tr.diffuseSunSteps = 2;
             tr.diffuseSunSteps <= 5;
             ++tr.diffuseSunSteps) {
            if (sampled.sampleState != R_LIGHT_VIS_SAMPLE_BLOCKED) {
                sampled.diffuseSunVisibility = 0;
                R_SampleDiffuseSunVisibility(&sampled, origin, sampledLeaf);
            }
            diskEntry->diffuseSunVisibility[tr.diffuseSunSteps - 1] =
                sampled.diffuseSunVisibility;
        }
    }
}

/* Source: CoDUOMP.exe 0x004c8130..0x004c8273.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004c8130_004c8273.mcode.
 * Name: same-module Mac symbol R_PrecalcLightVisCache. The 3 MiB cache extent
 * is exactly 8192 buckets * 32 entries * 12 bytes. */
void R_PrecalcLightVisCache(int32_t *checksum)
{
    char filename[R_WORLD_NAME_SIZE];
    void *fileBuffer;
    int32_t fileSize;
    uint32_t historyEntryCount;
    int32_t savedDiffuseSunSteps;
    renderer_light_vis_disk_entry_t (*cache)
        [R_LIGHT_VIS_ENTRIES_PER_BUCKET];

    if (tr.world->lightIndexCount == 0) {
        ri.Error(ERR_DROP,
                 "cannot precalculate light vis cache; no lights compiled "
                 "into map '%s'\n",
                 tr.world->name);
    }

    R_LightVisHistoryFilename(filename);
    fileSize = ri.FS_ReadFile(filename, &fileBuffer);
    if (fileSize <= 0) {
        ri.Error(ERR_DROP,
                 "light vis cache file '%s' is missing or empty\n",
                 filename);
    }

    historyEntryCount =
        (uint32_t)fileSize / sizeof(renderer_light_vis_history_entry_t);
    if ((uint32_t)fileSize %
            sizeof(renderer_light_vis_history_entry_t) != 0U) {
        ri.Error(ERR_DROP, "light vis cache has funny size\n");
    }

    savedDiffuseSunSteps = tr.diffuseSunSteps;
    cache = ri.Hunk_AllocateTempMemory(
        sizeof(renderer_light_vis_disk_entry_t) *
        R_LIGHT_VIS_BUCKET_COUNT * R_LIGHT_VIS_ENTRIES_PER_BUCKET);

    for (uint32_t entryIndex = 0;
         entryIndex < historyEntryCount;
         ++entryIndex) {
        const renderer_light_vis_history_entry_t *historyEntry =
            &((const renderer_light_vis_history_entry_t *)fileBuffer)
                [entryIndex];

        R_PrecalcLightVisCachePoint(
            historyEntry->gridX, historyEntry->gridY,
            historyEntry->gridZ, historyEntry->traceTarget, cache);
    }

    ri.FS_FreeFile(fileBuffer);
    tr.diffuseSunSteps = savedDiffuseSunSteps;
    ri.CM_SaveLump(
        LIGHT_VIS_CACHE_DISK_TYPE, cache,
        (int32_t)(sizeof(renderer_light_vis_disk_entry_t) *
                  R_LIGHT_VIS_BUCKET_COUNT *
                  R_LIGHT_VIS_ENTRIES_PER_BUCKET),
        checksum);
    ri.Hunk_FreeTempMemory(cache);

    if (r_vc_compile->integer == 2)
        ri.Cmd_ExecuteText(EXEC_NOW, "quit");
    ri.Cvar_Set("r_vc_compile", "0");
}

/* Source: CoDUOMP.exe 0x004c8280..0x004c848e.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004c8280_004c848e.mcode.
 * Name: same-module Mac symbol R_InitLightVisCacheFromBuffer. The Windows body
 * selects diskCache[i].diffuseSunVisibility[tr.diffuseSunSteps - 1] and
 * rebuilds the used-entry and maximum-associativity statistics. */
qboolean R_InitLightVisCacheFromBuffer(
    const renderer_light_vis_disk_entry_t *diskCache,
    int32_t diskCacheSize)
{
    const int32_t expectedSize =
        (int32_t)(sizeof(*diskCache) *
                  R_LIGHT_VIS_BUCKET_COUNT *
                  R_LIGHT_VIS_ENTRIES_PER_BUCKET);

    /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if (r_vc_makelog->integer != 0 ||
        r_vc_compile->integer != 0 ||
        diskCache == NULL || diskCacheSize != expectedSize) {
        return qtrue;
    }

    rendererLightVisUsedEntryCount = 0;
    rendererLightVisMaxAssociativity = 0;
    rendererLightVisFlushedEntryCount = 0;
    rendererLightVisRuntimeFillCount = 0;

    for (int32_t bucketIndex = 0;
         bucketIndex < R_LIGHT_VIS_BUCKET_COUNT;
         ++bucketIndex) {
        for (int32_t associativity = 0;
             associativity < R_LIGHT_VIS_ENTRIES_PER_BUCKET;
             ++associativity) {
            const renderer_light_vis_disk_entry_t *source =
                &diskCache[
                    bucketIndex * R_LIGHT_VIS_ENTRIES_PER_BUCKET +
                    associativity];
            renderer_light_vis_cache_entry_t *destination =
                &rendererLightVisCache[bucketIndex][associativity];

            destination->key = source->key;
            destination->sampleState = source->sampleState;
            destination->diffuseSunVisibility =
                source->diffuseSunVisibility[tr.diffuseSunSteps - 1];
            destination->visibleLightBits = source->visibleLightBits;

            if (destination->sampleState != R_LIGHT_VIS_SAMPLE_EMPTY) {
                ++rendererLightVisUsedEntryCount;
                if (rendererLightVisMaxAssociativity < associativity + 1)
                    rendererLightVisMaxAssociativity = associativity + 1;
            }
        }
    }

    return qtrue;
}
