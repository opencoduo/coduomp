#include "backend.h"

#include "../math/vector_math.h"
#include "gl_api.h"

#include <math.h>
#include <stddef.h>
#include <string.h>

enum {
    R_STATIC_MODEL_CACHE_VERTEX_CAPACITY = 65536,
    R_STATIC_MODEL_CACHE_VERTEX_BYTES = 24,
    R_STATIC_MODEL_CACHE_STORAGE_BYTES =
        R_STATIC_MODEL_CACHE_VERTEX_CAPACITY *
        R_STATIC_MODEL_CACHE_VERTEX_BYTES,
    SMC_PAGE_COUNT = 128,
    SMC_PAGE_VERTEX_COUNT = 512,
    SMC_SURFACES_PER_PAGE = 16,
    SMC_TREE_NODE_COUNT = 31,
    SMC_MIN_BLOCK_VERTEX_COUNT = 32,
    SMC_SIZE_CLASS_COUNT = 5,
    SMC_MIN_SIZE_SHIFT = 5,
    SMC_MAX_SIZE_SHIFT = 9
};

typedef struct smc_tree_node_s {
    uint16_t usedVertexCount;                 /* original +0x00 */
    uint8_t allocated;                        /* original +0x02 */
    uint8_t padding03; /* original +0x03; unused by CoDUOMP.exe. */
} smc_tree_node_t;

typedef struct smc_page_s {
    renderer_static_model_cache_link_t lruLink; /* original +0x000 */
    int32_t lastUsedFrame;                       /* original +0x008 */
    smc_tree_node_t tree[SMC_TREE_NODE_COUNT];   /* original +0x00c */
    renderer_cached_static_model_surface_t
        surfaces[SMC_SURFACES_PER_PAGE];         /* original +0x088 */
} smc_page_t;

typedef struct smc_cache_s {
    smc_page_t pages[SMC_PAGE_COUNT];
    renderer_static_model_cache_link_t
        freeLists[SMC_SIZE_CLASS_COUNT];          /* original +0xe400 */
    renderer_static_model_cache_link_t lruList;  /* original +0xe428 */
    int32_t allocatedVertexCapacity;              /* original +0xe430 */
    int32_t usedVertexCount;                      /* original +0xe434 */
} smc_cache_t;

#if UINTPTR_MAX == UINT32_MAX
_Static_assert(_Alignof(smc_tree_node_t) == 0x2,
               "smc_tree_node_t original alignment");
_Static_assert(offsetof(smc_tree_node_t, usedVertexCount) == 0x00,
               "smc_tree_node_t usedVertexCount offset");
_Static_assert(sizeof(((smc_tree_node_t *)0)->usedVertexCount) == 0x02,
               "smc_tree_node_t usedVertexCount extent");
_Static_assert(offsetof(smc_tree_node_t, allocated) == 0x02,
               "smc_tree_node_t allocated offset");
_Static_assert(sizeof(((smc_tree_node_t *)0)->allocated) == 0x01,
               "smc_tree_node_t allocated extent");
_Static_assert(offsetof(smc_tree_node_t, padding03) == 0x03,
               "smc_tree_node_t padding03 offset");
_Static_assert(sizeof(((smc_tree_node_t *)0)->padding03) == 0x01,
               "smc_tree_node_t padding03 extent");
_Static_assert(sizeof(smc_tree_node_t) == 0x04,
               "smc_tree_node_t original size");

_Static_assert(_Alignof(smc_page_t) == 0x4,
               "smc_page_t original alignment");
_Static_assert(offsetof(smc_page_t, lruLink) == 0x000,
               "smc_page_t lruLink offset");
_Static_assert(sizeof(((smc_page_t *)0)->lruLink) == 0x008,
               "smc_page_t lruLink extent");
_Static_assert(offsetof(smc_page_t, lastUsedFrame) == 0x008,
               "smc_page_t lastUsedFrame offset");
_Static_assert(sizeof(((smc_page_t *)0)->lastUsedFrame) == 0x004,
               "smc_page_t lastUsedFrame extent");
_Static_assert(offsetof(smc_page_t, tree) == 0x00c,
               "smc_page_t tree offset");
_Static_assert(sizeof(((smc_page_t *)0)->tree) == 0x07c,
               "smc_page_t tree extent");
_Static_assert(offsetof(smc_page_t, surfaces) == 0x088,
               "smc_page_t surfaces offset");
_Static_assert(sizeof(((smc_page_t *)0)->surfaces) == 0x140,
               "smc_page_t surfaces extent");
_Static_assert(sizeof(smc_page_t) == 0x1c8,
               "smc_page_t original size");

_Static_assert(_Alignof(smc_cache_t) == 0x4,
               "smc_cache_t original alignment");
_Static_assert(offsetof(smc_cache_t, pages) == 0x0000,
               "smc_cache_t pages offset");
_Static_assert(sizeof(((smc_cache_t *)0)->pages) == 0xe400,
               "smc_cache_t pages extent");
_Static_assert(offsetof(smc_cache_t, freeLists) == 0xe400,
               "smc_cache_t freeLists offset");
_Static_assert(sizeof(((smc_cache_t *)0)->freeLists) == 0x0028,
               "smc_cache_t freeLists extent");
_Static_assert(offsetof(smc_cache_t, lruList) == 0xe428,
               "smc_cache_t lruList offset");
_Static_assert(sizeof(((smc_cache_t *)0)->lruList) == 0x0008,
               "smc_cache_t lruList extent");
_Static_assert(offsetof(smc_cache_t, allocatedVertexCapacity) == 0xe430,
               "smc_cache_t allocatedVertexCapacity offset");
_Static_assert(sizeof(((smc_cache_t *)0)->allocatedVertexCapacity) == 0x0004,
               "smc_cache_t allocatedVertexCapacity extent");
_Static_assert(offsetof(smc_cache_t, usedVertexCount) == 0xe434,
               "smc_cache_t usedVertexCount offset");
_Static_assert(sizeof(((smc_cache_t *)0)->usedVertexCount) == 0x0004,
               "smc_cache_t usedVertexCount extent");
_Static_assert(sizeof(smc_cache_t) == 0xe438,
               "smc_cache_t original size");
#endif

/* Original 0x0388c968..0x0389ad9f. */
static smc_cache_t rendererStaticModelCache;

/* NOT_FROM_ORIGINAL_SOURCE: source-level factoring of the repeated two-link
 * sentinel initialization emitted inline by R_InitStaticModelCache. */
static void SMC_InitList(renderer_static_model_cache_link_t *sentinel)
{
    sentinel->next = sentinel;
    sentinel->previous = sentinel;
}

/* NOT_FROM_ORIGINAL_SOURCE: source-level factoring of the repeated intrusive
 * tail insertion sequence visible throughout the original cache functions. */
static void SMC_AppendToList(renderer_static_model_cache_link_t *sentinel,
                             renderer_static_model_cache_link_t *link)
{
    link->next = sentinel;
    link->previous = sentinel->previous;
    sentinel->previous = link;
    link->previous->next = link;
}

/* NOT_FROM_ORIGINAL_SOURCE: source-level factoring of the repeated intrusive
 * unlink sequence visible throughout the original cache functions. */
static void SMC_RemoveFromList(renderer_static_model_cache_link_t *link)
{
    link->previous->next = link->next;
    link->next->previous = link->previous;
}

/* NOT_FROM_ORIGINAL_SOURCE: typed replacement for the original compiler's
 * division of a cache-relative byte offset by the 0x1c8 page stride. */
static smc_page_t *SMC_PageForSurface(
    renderer_cached_static_model_surface_t *surface)
{
    const ptrdiff_t cacheOffset =
        (uint8_t *)surface - (uint8_t *)&rendererStaticModelCache.pages[0];
    const size_t pageIndex = (size_t)cacheOffset / sizeof(smc_page_t);

    return &rendererStaticModelCache.pages[pageIndex];
}

/* Source: CoDUOMP.exe 0x00512ab0..0x00512b6f.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00512ab0_00512b70.mcode.
 * Name and source recursion boundary: same-module Mac symbol
 * SMC_FreeCachedSurface_r. */
static void SMC_FreeCachedSurface_r(smc_cache_t *cache, smc_page_t *page,
                                    int32_t nodeIndex, int32_t depth)
{
    smc_tree_node_t *node = &page->tree[nodeIndex];

    if (node->usedVertexCount == 0) {
        const int32_t surfaceIndex =
            ((nodeIndex + 1) << depth) - SMC_SURFACES_PER_PAGE;
        SMC_RemoveFromList(&page->surfaces[surfaceIndex].freeLink);
        return;
    }

    node->usedVertexCount = 0;
    if (node->allocated == 0) {
        SMC_FreeCachedSurface_r(cache, page, nodeIndex * 2 + 1,
                                depth - 1);
        SMC_FreeCachedSurface_r(cache, page, nodeIndex * 2 + 2,
                                depth - 1);
        return;
    }

    const int32_t surfaceIndex =
        ((nodeIndex + 1) << depth) - SMC_SURFACES_PER_PAGE;
    renderer_cached_static_model_surface_t *surface =
        &page->surfaces[surfaceIndex];

    surface->cached.owner->surfaceLightingCache[surface->cached.surfaceIndex] =
        NULL;
    cache->allocatedVertexCapacity -= 1 << (depth + SMC_MIN_SIZE_SHIFT);
    cache->usedVertexCount -= surface->cached.source->vertexCount;
    node->allocated = 0;
}

/* Source: CoDUOMP.exe 0x00512b70..0x00512bd0.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00512b70_00512bd1.mcode.
 * Name: same-module Mac symbol SMC_ForceFreeBlock. */
static qboolean SMC_ForceFreeBlock(smc_cache_t *cache)
{
    renderer_static_model_cache_link_t *pageLink = cache->lruList.next;
    smc_page_t *page = (smc_page_t *)pageLink;

    if (page->lastUsedFrame == tr.frameCount)
        return qfalse;

    SMC_FreeCachedSurface_r(cache, page, 0, SMC_SIZE_CLASS_COUNT - 1);
    SMC_RemoveFromList(&page->lruLink);
    SMC_AppendToList(&cache->freeLists[0],
                     &page->surfaces[0].freeLink);
    return qtrue;
}

/* Source: CoDUOMP.exe 0x00512be0..0x00512ce8.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00512be0_00512ce9.mcode.
 * Name: same-module Mac symbol SMC_GetFreeBlockOfSize. */
static qboolean SMC_GetFreeBlockOfSize(smc_cache_t *cache,
                                       int32_t listIndex)
{
    if (listIndex == 0)
        return SMC_ForceFreeBlock(cache);

    renderer_static_model_cache_link_t *parentList =
        &cache->freeLists[listIndex - 1];
    if (parentList->previous == parentList &&
        SMC_GetFreeBlockOfSize(cache, listIndex - 1) == qfalse) {
        return qfalse;
    }

    renderer_cached_static_model_surface_t *block =
        (renderer_cached_static_model_surface_t *)parentList->previous;
    SMC_RemoveFromList(&block->freeLink);

    smc_page_t *page = SMC_PageForSurface(block);
    if (listIndex == 1)
        SMC_AppendToList(&cache->lruList, &page->lruLink);

    renderer_static_model_cache_link_t *targetList =
        &cache->freeLists[listIndex];
    const int32_t surfaceIndex = (int32_t)(block - page->surfaces);
    const int32_t buddySurfaceIndex =
        surfaceIndex + (1 << ((SMC_SIZE_CLASS_COUNT - 1) - listIndex));

    SMC_AppendToList(targetList, &block->freeLink);
    SMC_AppendToList(targetList,
                     &page->surfaces[buddySurfaceIndex].freeLink);
    return qtrue;
}

/* Source: CoDUOMP.exe 0x00512cf0..0x00512dd9.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00512cf0_00512dda.mcode.
 * Name and source parameter order: same-module Mac symbol SMC_Allocate. */
static renderer_cached_static_model_surface_t *SMC_Allocate(
    smc_cache_t *cache, int32_t sizeShift)
{
    const int32_t listIndex = SMC_MAX_SIZE_SHIFT - sizeShift;
    renderer_static_model_cache_link_t *freeList =
        &cache->freeLists[listIndex];

    if (freeList->previous == freeList &&
        SMC_GetFreeBlockOfSize(cache, listIndex) == qfalse) {
        return NULL;
    }

    renderer_cached_static_model_surface_t *surface =
        (renderer_cached_static_model_surface_t *)freeList->previous;
    SMC_RemoveFromList(&surface->freeLink);

    smc_page_t *page = SMC_PageForSurface(surface);
    if (listIndex == 0)
        SMC_AppendToList(&cache->lruList, &page->lruLink);

    const int32_t pageIndex =
        (int32_t)(page - rendererStaticModelCache.pages);
    const int32_t surfaceIndex =
        (int32_t)(surface - page->surfaces);
    int32_t nodeIndex =
        ((surfaceIndex + SMC_SURFACES_PER_PAGE) >>
         ((SMC_SIZE_CLASS_COUNT - 1) - listIndex)) - 1;

    page->tree[nodeIndex].allocated = 1;
    for (; nodeIndex >= 0; nodeIndex = (nodeIndex - 1) >> 1)
        page->tree[nodeIndex].usedVertexCount += (uint16_t)(1 << sizeShift);

    surface->cached.vertexOffset =
        pageIndex * SMC_PAGE_VERTEX_COUNT +
        surfaceIndex * SMC_MIN_BLOCK_VERTEX_COUNT;
    return surface;
}

/* NOT_FROM_ORIGINAL_SOURCE: source-level expression of the repeated x87
 * scale, nearest-integer conversion, and [0,255] clamp emitted four times by
 * R_EvaluateLightingAtPoint. The tiny double bias is the exact constant at
 * 0x005b9d00 added before each FISTP. */
static uint8_t SMC_LightingByte(float component)
{
    const double roundingBias = 0.000000000931322574615478515625;
    long value = lrint((double)(component * 255.0f) + roundingBias);

    if (value < 0)
        value = 0;
    else if (value > 255)
        value = 255;
    return (uint8_t)value;
}

/* Source: CoDUOMP.exe 0x00512de0..0x0051333c.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00512de0_0051333d.mcode.
 * Name and source parameter roles: same-module Mac symbol
 * R_EvaluateLightingAtPoint. Windows is authoritative for the little-endian
 * color order and has no Mac byte-swapping branch. */
static void R_EvaluateLightingAtPoint(trRefEntity_t *entity,
                                      const vec3_t point,
                                      const vec3_t normal,
                                      uint8_t color[4])
{
    vec4_t accumulated;
    int32_t lightIndex;

    if (tr.world->lightIndexCount == 0) {
        color[0] = (uint8_t)tr.identityLightByte;
        color[1] = (uint8_t)tr.identityLightByte;
        color[2] = (uint8_t)tr.identityLightByte;
        color[3] = 255;
        return;
    }

    accumulated[0] = tr.world->entityAmbientBase[0] +
        entity->diffuseSunContribution * tr.world->entityAmbientScale[0];
    accumulated[1] = tr.world->entityAmbientBase[1] +
        entity->diffuseSunContribution * tr.world->entityAmbientScale[1];
    accumulated[2] = tr.world->entityAmbientBase[2] +
        entity->diffuseSunContribution * tr.world->entityAmbientScale[2];
    accumulated[3] = tr.world->entityAmbientBase[3] +
        entity->diffuseSunContribution * tr.world->entityAmbientScale[3];

    for (lightIndex = 0; lightIndex < entity->lightCount; ++lightIndex) {
        const renderer_light_t *light = entity->lights[lightIndex].light;
        float attenuation = entity->lights[lightIndex].scale;
        long double attenuationRaw = (long double)attenuation;
        float diffuseFactor;

        if (light->position[3] == 0.0f) {
            const long double diffuseFactorRaw =
                ((long double)normal[2] * light->position[2] +
                 (long double)normal[0] * light->position[0]) +
                (long double)normal[1] * light->position[1];

            diffuseFactor = (float)diffuseFactorRaw;
            if (diffuseFactorRaw < 0.0L)
                diffuseFactor = 0.0f;
        } else {
            vec3_t direction = {
                light->position[0] - point[0],
                light->position[1] - point[1],
                light->position[2] - point[2]
            };
            const long double distanceRaw = VectorNormalize(direction);

            const long double diffuseFactorRaw =
                ((long double)normal[2] * direction[2] +
                 (long double)normal[1] * direction[1]) +
                (long double)normal[0] * direction[0];

            diffuseFactor = (float)diffuseFactorRaw;
            if (diffuseFactorRaw < 0.0L)
                diffuseFactor = 0.0f;

            attenuationRaw /=
                (long double)light->constantAttenuation +
                distanceRaw *
                    ((long double)light->linearAttenuation +
                     distanceRaw * light->quadraticAttenuation);
            attenuation = (float)attenuationRaw;

            if (light->spotCutoff != 180.0f) {
                const double roundingBias =
                    0.000000000931322574615478515625;
                const float spotFactor = (float)(
                    ((long double)direction[2] * light->spotDirection[2] +
                     (long double)direction[1] * light->spotDirection[1]) +
                    (long double)direction[0] * light->spotDirection[0]);
                const float tableCoordinate =
                    (light->spotCutoff + 90.0f) *
                    2.8444445133209228515625f;
                const int32_t tableIndex =
                    (int32_t)lrint((double)tableCoordinate + roundingBias) &
                    1023;

                if (tr.sinTable[tableIndex] < spotFactor) {
                    int32_t exponent;

                    /* The point-light denominator is rounded to the stack
                     * float before this path; the repeated spot products then
                     * remain live in x87 until the color accumulation. */
                    attenuationRaw = (long double)attenuation;
                    for (exponent = 0;
                         (float)exponent < light->spotExponent;
                         ++exponent) {
                        attenuationRaw *= (long double)spotFactor;
                    }
                } else {
                    attenuationRaw = 0.0L;
                }
            }
        }

        accumulated[0] = (float)(
            attenuationRaw *
                ((long double)diffuseFactor * light->diffuse[0] +
                 (long double)light->ambient[0]) +
            (long double)accumulated[0]);
        accumulated[1] = (float)(
            attenuationRaw *
                ((long double)diffuseFactor * light->diffuse[1] +
                 (long double)light->ambient[1]) +
            (long double)accumulated[1]);
        accumulated[2] = (float)(
            attenuationRaw *
                ((long double)diffuseFactor * light->diffuse[2] +
                 (long double)light->ambient[2]) +
            (long double)accumulated[2]);
        accumulated[3] = (float)(
            attenuationRaw *
                ((long double)diffuseFactor * light->diffuse[3] +
                 (long double)light->ambient[3]) +
            (long double)accumulated[3]);
    }

    color[0] = SMC_LightingByte(accumulated[0]);
    color[1] = SMC_LightingByte(accumulated[1]);
    color[2] = SMC_LightingByte(accumulated[2]);
    color[3] = SMC_LightingByte(accumulated[3]);
}

/* Source: CoDUOMP.exe 0x00513340..0x00513784.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00513340_00513785.mcode.
 * Name and source parameter roles: same-module Mac symbol
 * R_CacheStaticModelSurface. Windows proves the five-argument source boundary
 * despite LTCG carrying the first two arguments in EAX and ECX. */
renderer_cached_static_model_surface_t *R_CacheStaticModelSurface(
    renderer_static_model_surface_t *source,
    renderer_static_model_t *owner,
    int32_t surfaceIndex,
    trRefEntity_t *entity,
    const axis_t inverseAxis)
{
    renderer_cached_static_model_surface_t *cached;
    shaderStage_t *stage;
    renderer_cached_static_model_vertex_t *vertices;
    size_t vertexBytes;
    int32_t sizeShift;
    int32_t blockVertexCount;
    uint16_t vertexIndex;

    if (r_smc_enable->integer == 0 ||
        source->vertexCount > SMC_PAGE_VERTEX_COUNT) {
        return NULL;
    }

    sizeShift = SMC_MIN_SIZE_SHIFT;
    blockVertexCount = 1 << sizeShift;
    while (blockVertexCount < source->vertexCount) {
        ++sizeShift;
        blockVertexCount = 1 << sizeShift;
    }

    cached = SMC_Allocate(&rendererStaticModelCache, sizeShift);
    if (cached == NULL)
        return NULL;

    rendererStaticModelCache.allocatedVertexCapacity += blockVertexCount;
    rendererStaticModelCache.usedVertexCount += source->vertexCount;
    cached->cached.surfaceType = tr.cachedStaticModelSurfaceType;
    cached->cached.owner = owner;
    cached->cached.source = source;
    cached->cached.surfaceIndex = surfaceIndex;

    vertexBytes = (size_t)source->vertexCount * sizeof(*vertices);
    vertices = ri.Hunk_AllocateTempMemory(vertexBytes);

    for (vertexIndex = 0; vertexIndex < source->vertexCount; ++vertexIndex) {
        const vec3_t *sourcePosition = &source->vertices[vertexIndex];

        memcpy(vertices[vertexIndex].texCoord,
               source->texCoords[vertexIndex],
               sizeof(vertices[vertexIndex].texCoord));
        vertices[vertexIndex].position[0] =
            owner->entity.axis[0][0] * (*sourcePosition)[0] +
            owner->entity.axis[1][0] * (*sourcePosition)[1] +
            owner->entity.axis[2][0] * (*sourcePosition)[2] +
            owner->entity.origin[0];
        vertices[vertexIndex].position[1] =
            owner->entity.axis[0][1] * (*sourcePosition)[0] +
            owner->entity.axis[1][1] * (*sourcePosition)[1] +
            owner->entity.axis[2][1] * (*sourcePosition)[2] +
            owner->entity.origin[1];
        vertices[vertexIndex].position[2] =
            owner->entity.axis[0][2] * (*sourcePosition)[0] +
            owner->entity.axis[1][2] * (*sourcePosition)[1] +
            owner->entity.axis[2][2] * (*sourcePosition)[2] +
            owner->entity.origin[2];
    }

    stage = source->cachedShader->stages[0];
    switch (stage->rgbGen) {
    case CGEN_LIGHTING_DIFFUSE:
        for (vertexIndex = 0;
             vertexIndex < source->vertexCount;
             ++vertexIndex) {
            const vec3_t *sourceNormal = &source->normals[vertexIndex];
            vec3_t normal = {
                inverseAxis[0][0] * (*sourceNormal)[0] +
                    inverseAxis[0][1] * (*sourceNormal)[1] +
                    inverseAxis[0][2] * (*sourceNormal)[2],
                inverseAxis[1][0] * (*sourceNormal)[0] +
                    inverseAxis[1][1] * (*sourceNormal)[1] +
                    inverseAxis[1][2] * (*sourceNormal)[2],
                inverseAxis[2][0] * (*sourceNormal)[0] +
                    inverseAxis[2][1] * (*sourceNormal)[1] +
                    inverseAxis[2][2] * (*sourceNormal)[2]
            };

            (void)VectorNormalize(normal);
            R_EvaluateLightingAtPoint(entity,
                                      vertices[vertexIndex].position,
                                      normal,
                                      vertices[vertexIndex].color);
        }
        break;

    case CGEN_IDENTITY:
        for (vertexIndex = 0;
             vertexIndex < source->vertexCount;
             ++vertexIndex) {
            memset(vertices[vertexIndex].color, 255,
                   sizeof(vertices[vertexIndex].color));
        }
        break;

    case CGEN_IDENTITY_LIGHTING:
        for (vertexIndex = 0;
             vertexIndex < source->vertexCount;
             ++vertexIndex) {
            memset(vertices[vertexIndex].color,
                   (uint8_t)tr.identityLightByte,
                   sizeof(vertices[vertexIndex].color));
        }
        break;

    case CGEN_LIGHTING_PRECALC:
        for (vertexIndex = 0;
             vertexIndex < source->vertexCount;
             ++vertexIndex) {
            memcpy(vertices[vertexIndex].color,
                   entity->e.shaderRGBA,
                   sizeof(vertices[vertexIndex].color));
        }
        break;

    case CGEN_CONSTANT:
        for (vertexIndex = 0;
             vertexIndex < source->vertexCount;
             ++vertexIndex) {
            memcpy(vertices[vertexIndex].color, stage->constantColor,
                   sizeof(vertices[vertexIndex].color));
        }
        break;

    default:
        break;
    }

    if (stage->alphaGen == AGEN_UNSPECIFIED &&
        stage->rgbGen != CGEN_IDENTITY) {
        for (vertexIndex = 0;
             vertexIndex < source->vertexCount;
             ++vertexIndex) {
            vertices[vertexIndex].color[3] = 255;
        }
    } else if (stage->alphaGen == AGEN_CONSTANT &&
               stage->rgbGen != CGEN_CONSTANT) {
        for (vertexIndex = 0;
             vertexIndex < source->vertexCount;
             ++vertexIndex) {
            vertices[vertexIndex].color[3] = stage->constantColor[3];
        }
    }

    switch (tr.cachedStaticModelSurfaceType) {
    case R_SURFACE_CACHED_STATIC_MODEL_GENERIC:
    case R_SURFACE_CACHED_STATIC_MODEL_NV:
        memcpy(tr.cachedStaticModelStorage.address +
                   (size_t)cached->cached.vertexOffset * sizeof(*vertices),
               vertices, vertexBytes);
        break;

    case R_SURFACE_CACHED_STATIC_MODEL_ARB:
        qglBindBufferARB(GL_ARRAY_BUFFER_ARB,
                         tr.cachedStaticModelStorage.glBuffer);
        qglBufferSubDataARB(
            GL_ARRAY_BUFFER_ARB,
            (size_t)cached->cached.vertexOffset * sizeof(*vertices),
            vertexBytes, vertices);
        qglBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
        break;

    case R_SURFACE_CACHED_STATIC_MODEL_ATI:
        qglUpdateObjectBufferATI(
            tr.cachedStaticModelStorage.atiObjectBuffer,
            (uint32_t)(
                tr.cachedStaticModelStorageOffset +
                (size_t)cached->cached.vertexOffset * sizeof(*vertices)),
            (int32_t)vertexBytes, vertices, GL_PRESERVE_ATI);
        break;

    default:
        break;
    }

    ri.Hunk_FreeTempMemory(vertices);
    return cached;
}

/* Source: CoDUOMP.exe 0x005137c0..0x00513816.
 * Evidence:
 * coduomp/mcode/CoDUOMP/FUN_005137c0_00513817.mcode.
 * Name and source structure: exact same-module Mac symbol
 * R_UsedCachedStaticModelSurface. MSVC LTCG also inlines this exact
 * page-touch/move-to-tail sequence in the Windows R_AddStaticModelSurfaces
 * body at 0x00519d60..0x0051a069. */
void R_UsedCachedStaticModelSurface(
    renderer_cached_static_model_surface_t *surface)
{
    smc_page_t *page = SMC_PageForSurface(surface);

    page->lastUsedFrame = tr.frameCount;
    SMC_RemoveFromList(&page->lruLink);
    SMC_AppendToList(&rendererStaticModelCache.lruList, &page->lruLink);
}

/* Source: CoDUOMP.exe 0x00513820..0x0051383d.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00513820_0051383e.mcode.
 * Name and signature: exact same-module Mac symbol
 * R_AllocStaticModelCacheGeneric. MSVC LTCG also inlines this source helper
 * in the Windows allocation functions below. */
static void R_AllocStaticModelCacheGeneric(void)
{
    tr.cachedStaticModelStorage.address =
        ri.Hunk_Alloc(R_STATIC_MODEL_CACHE_STORAGE_BYTES);
    tr.cachedStaticModelSurfaceType =
        R_SURFACE_CACHED_STATIC_MODEL_GENERIC;
}

/* Source: CoDUOMP.exe 0x00513840..0x00513899.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00513840_0051389a.mcode.
 * Name: same-module Mac symbol R_AllocStaticModelCacheARB. */
static void R_AllocStaticModelCacheARB(void)
{
    tr.cachedStaticModelStorage.glBuffer =
        R_CreateBufferARB(GL_ARRAY_BUFFER_ARB,
                          R_STATIC_MODEL_CACHE_STORAGE_BYTES,
                          NULL, tr.vboUsage);

    if (tr.cachedStaticModelStorage.glBuffer == 0) {
        R_AllocStaticModelCacheGeneric();
        return;
    }

    tr.cachedStaticModelStorageSource = tr.defaultStorageMode;
    tr.cachedStaticModelSurfaceType =
        R_SURFACE_CACHED_STATIC_MODEL_ARB;
}

/* Source: CoDUOMP.exe 0x005138a0..0x0051395d.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_005138a0_0051395e.mcode.
 * Name: same-module Mac symbol R_AllocStaticModelCacheATI. */
static void R_AllocStaticModelCacheATI(void)
{
    tr.cachedStaticModelStorage.address = NULL;
    tr.cachedStaticModelStorageSource = R_AllocMemoryATI(
        R_STATIC_VERTEX_MEMORY_PRIMARY,
        R_STATIC_MODEL_CACHE_STORAGE_BYTES,
        &tr.cachedStaticModelStorageOffset);

    if (tr.cachedStaticModelStorageSource ==
        R_STATIC_VERTEX_MEMORY_PRIMARY) {
        tr.cachedStaticModelStorageSource =
            R_STATIC_VERTEX_MEMORY_HUNK;
        tr.cachedStaticModelStorage.atiObjectBuffer =
            tr.staticVertexMemoryPrimary.atiObjectBuffer;
        tr.cachedStaticModelSurfaceType =
            R_SURFACE_CACHED_STATIC_MODEL_ATI;
        return;
    }

    if (tr.cachedStaticModelStorageSource ==
        R_STATIC_VERTEX_MEMORY_SECONDARY) {
        tr.cachedStaticModelStorageSource =
            R_STATIC_VERTEX_MEMORY_HUNK;
        tr.cachedStaticModelStorage.atiObjectBuffer =
            tr.staticVertexMemorySecondary.atiObjectBuffer;
        tr.cachedStaticModelSurfaceType =
            R_SURFACE_CACHED_STATIC_MODEL_ATI;
        return;
    }

    ri.Printf(R_PRINT_ALL,
              "^3Couldn't allocate memory for the static model surface "
              "cache using ATI optimizations");
    tr.cachedStaticModelStorageSource = R_STATIC_VERTEX_MEMORY_NONE;
    R_AllocStaticModelCacheGeneric();
}

/* Source: CoDUOMP.exe 0x00513960..0x00513a0e.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00513960_00513a0f.mcode.
 * Name: same-module Mac symbol R_AllocStaticModelCacheNV. The allocator body
 * is source-level R_AllocMemoryNV inlined by Windows LTCG. */
static void R_AllocStaticModelCacheNV(void)
{
    tr.cachedStaticModelStorage.address = NULL;
    tr.cachedStaticModelStorageSource = R_AllocMemoryNV(
        R_STATIC_VERTEX_MEMORY_PRIMARY,
        R_STATIC_MODEL_CACHE_STORAGE_BYTES,
        &tr.cachedStaticModelStorage.address);
    tr.cachedStaticModelSurfaceType =
        R_SURFACE_CACHED_STATIC_MODEL_NV;
}

/* Source: CoDUOMP.exe 0x00513a10..0x00513b78.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00513a10_00513b79.mcode.
 * Name and source initialization shape: same-module Mac symbol
 * R_InitStaticModelCache. */
void R_InitStaticModelCache(void)
{
    int32_t listIndex;
    int32_t pageIndex;

    if (glConfig.vertexBufferObjectAvailable) {
        R_AllocStaticModelCacheARB();
    } else if (glConfig.vertexArrayRangeMode != R_VERTEX_ARRAY_RANGE_NONE) {
        R_AllocStaticModelCacheNV();
    } else if (glConfig.vertexArrayObjectATIAvailable) {
        R_AllocStaticModelCacheATI();
    } else {
        R_AllocStaticModelCacheGeneric();
    }

    memset(&rendererStaticModelCache, 0, sizeof(rendererStaticModelCache));
    SMC_InitList(&rendererStaticModelCache.lruList);
    for (listIndex = 0; listIndex < SMC_SIZE_CLASS_COUNT; ++listIndex)
        SMC_InitList(&rendererStaticModelCache.freeLists[listIndex]);

    for (pageIndex = 0; pageIndex < SMC_PAGE_COUNT; ++pageIndex) {
        SMC_AppendToList(&rendererStaticModelCache.freeLists[0],
                         &rendererStaticModelCache.pages[pageIndex]
                              .surfaces[0].freeLink);
    }
}

/* Source: CoDUOMP.exe 0x00513b80..0x00513bd3. This function was absent from
 * Ghidra's function records but is a complete instruction range between INT3
 * alignment banks. Name: same-module Mac symbol R_StaticModelCacheStats_f. */
void R_StaticModelCacheStats_f(void)
{
    const float cachePercentPerVertex = 0.00152587890625f;

    ri.Printf(R_PRINT_ALL,
              "%.2f%% of cache is currently allocated.\n",
              (double)(rendererStaticModelCache.allocatedVertexCapacity *
                       cachePercentPerVertex));

    if (rendererStaticModelCache.allocatedVertexCapacity != 0) {
        ri.Printf(
            R_PRINT_ALL,
            "%.2f%% allocated cache vertices are used.\n",
            (double)(rendererStaticModelCache.usedVertexCount * 100.0f /
                     rendererStaticModelCache.allocatedVertexCapacity));
    }
}

/* Source: CoDUOMP.exe 0x00513be0..0x00513c4f. This function was absent from
 * Ghidra's function records but is a complete instruction range between INT3
 * alignment banks. Name: same-module Mac symbol R_StaticModelCacheFlush_f. */
void R_StaticModelCacheFlush_f(void)
{
    while (rendererStaticModelCache.lruList.previous !=
           &rendererStaticModelCache.lruList) {
        smc_page_t *page =
            (smc_page_t *)rendererStaticModelCache.lruList.previous;

        SMC_FreeCachedSurface_r(&rendererStaticModelCache, page, 0,
                                SMC_SIZE_CLASS_COUNT - 1);
        SMC_RemoveFromList(&page->lruLink);
        SMC_AppendToList(&rendererStaticModelCache.freeLists[0],
                         &page->surfaces[0].freeLink);
    }
}
