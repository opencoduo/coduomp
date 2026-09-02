#include "backend.h"

#include "gl_api.h"
#include "gl_state.h"
#include "../math/vector_math.h"

#include <string.h>

/* Original 0x04887b14: round-robin position used by the incremental world-VBO
 * refresh command. */
enum renderer_world_optimization_mask_e {
    R_WORLD_OPTIMIZATION_BLOCK_MASK = 0x3ff5f900
};

/* Source: CoDUOMP.exe 0x0050b8c0..0x0050b974. The nested body beginning at
 * 0x0050b90d is a continuation, not a separate source function.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0050b8c0_0050b975.mcode.
 * Name and one-shader signature: exact same-module Mac symbol
 * CanOptimizeShader. */
qboolean CanOptimizeShader(const shader_t *shader)
{
    if (r_optimizeWorld->integer == 0 || r_optimize->integer == 0)
        return qfalse;
    if ((shader->flags & SHADER_FLAG_DEFAULTED) != 0)
        return qfalse;
    if ((shader->surfaceFlags & R_WORLD_OPTIMIZATION_BLOCK_MASK) != 0)
        return qfalse;
    if (shader->numUnfoggedPasses == 0 || shader->sort == SHADER_SORT_PORTAL)
        return qfalse;

    const shaderStage_t *firstStage = shader->stages[0];
    for (int32_t stageIndex = 0; stageIndex < shader->numUnfoggedPasses; ++stageIndex) {
        const shaderStage_t *stage = shader->stages[stageIndex];
        if ((stage->flags & SHADER_STAGE_PER_LIGHT) != 0)
            continue;
        if (stage->bundle[0].textureEnvMode == 0 || stage->bundle[1].textureEnvMode == 0 || stage->rgbGen != firstStage->rgbGen ||
            stage->alphaGen != firstStage->alphaGen) {
            return qfalse;
        }
    }

    return qtrue;
}

/* Source: CoDUOMP.exe 0x0050b980..0x0050bb69.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0050b980_0050bb6a.mcode.
 * Name and ordinary shader/count/build-record signature: exact same-module
 * Mac symbol BeginShaderSurfaces. */
void BeginShaderSurfaces(shader_t *shader, int32_t vertexCount, renderer_shader_surface_build_t *build)
{
    if (vertexCount >= UINT16_MAX + 1) {
        ri.Error(ERR_DROP, "\x15surface with shader %s has more than 65,536 vertices", shader->name);
    }

    build->optimized = CanOptimizeShader(shader);
    build->vertexBytes = (size_t)((uint32_t)vertexCount * (uint32_t)sizeof(renderer_world_interleaved_vertex_t));

    if (build->optimized != qfalse) {
        shader->optimizedBackend = SHADER_BACKEND_OPTIMIZED_GENERIC;

        if (glConfig.vertexBufferObjectAvailable != qfalse) {
            shader->optimizedVertexStorage.glBuffer = R_CreateBufferARB(GL_ARRAY_BUFFER_ARB, build->vertexBytes, NULL, GL_STATIC_DRAW_ARB);
            if (shader->optimizedVertexStorage.glBuffer != 0) {
                shader->optimizedBackend = SHADER_BACKEND_OPTIMIZED_ARB;
                build->storageMode = tr.defaultStorageMode;
            }
        } else if (glConfig.vertexArrayRangeMode != R_VERTEX_ARRAY_RANGE_NONE) {
            shader->optimizedBackend = SHADER_BACKEND_OPTIMIZED_NV;
            build->storageMode =
                R_AllocMemoryNV(R_STATIC_VERTEX_MEMORY_PRIMARY, build->vertexBytes, &shader->optimizedVertexStorage.address);
        } else if (glConfig.vertexArrayObjectATIAvailable != qfalse) {
            build->storageMode =
                R_AllocMemoryATI(R_STATIC_VERTEX_MEMORY_SECONDARY, build->vertexBytes, &shader->optimizedVertexStorageOffset);
            if (build->storageMode == R_STATIC_VERTEX_MEMORY_PRIMARY || build->storageMode == R_STATIC_VERTEX_MEMORY_SECONDARY) {
                shader->optimizedBackend = SHADER_BACKEND_OPTIMIZED_ATI;
                shader->optimizedVertexStorage =
                    build->storageMode == R_STATIC_VERTEX_MEMORY_PRIMARY ? tr.staticVertexMemoryPrimary : tr.staticVertexMemorySecondary;
                build->storageMode = R_STATIC_VERTEX_MEMORY_HUNK;
            }
        }

        if (shader->optimizedBackend == SHADER_BACKEND_OPTIMIZED_GENERIC) {
            build->storageMode = R_STATIC_VERTEX_MEMORY_HUNK;
            shader->optimizedVertexStorage.address = ri.Hunk_Alloc(build->vertexBytes);
        }
    } else {
        shader->optimizedBackend = SHADER_BACKEND_UNOPTIMIZED;
        build->storageMode = tr.defaultStorageMode;
    }

    build->shader = shader;
    build->firstVertex = 0;
}

/* Source: CoDUOMP.exe 0x0050b2d0..0x0050b4e4.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0050b2d0_0050b4e5.mcode.
 * Name, five-argument source signature, and big-endian LittleFloat calls:
 * exact same-module Mac symbol LittleVertices_T2T2C4V3. Windows proves the
 * direct little-endian copies used by all maintained modern targets. */
void LittleVertices_T2T2C4V3(const drawVert_t *vertices, int32_t vertexCount, renderer_world_mesh_surface_t *surface,
                             const renderer_lightmap_placement_t *lightmapPlacement, int32_t firstVertex)
{
    for (int32_t vertexIndex = 0; vertexIndex < vertexCount; ++vertexIndex) {
        const drawVert_t *source = &vertices[vertexIndex];

        if (surface->normals != NULL) {
            memcpy(surface->normals[vertexIndex], source->normal, sizeof(surface->normals[vertexIndex]));
        }
        memcpy(surface->texCoords[vertexIndex], source->st, sizeof(surface->texCoords[vertexIndex]));
        memcpy(surface->lightmapCoords[vertexIndex], source->lightmap, sizeof(surface->lightmapCoords[vertexIndex]));

        if (lightmapPlacement != NULL) {
            surface->lightmapCoords[vertexIndex][0] =
                (float)((long double)lightmapPlacement->sScale * (long double)surface->lightmapCoords[vertexIndex][0] +
                        (long double)lightmapPlacement->sOffset);
            surface->lightmapCoords[vertexIndex][1] =
                (float)((long double)lightmapPlacement->tScale * (long double)surface->lightmapCoords[vertexIndex][1] +
                        (long double)lightmapPlacement->tOffset);
        }

        memcpy(surface->colors[vertexIndex], source->color, sizeof(surface->colors[vertexIndex]));
        memcpy(surface->positions[vertexIndex], source->xyz, sizeof(surface->positions[vertexIndex]));
        AddPointToBounds(surface->positions[vertexIndex], surface->boundsMin, surface->boundsMax);
    }

    if (surface->tangents == NULL || surface->bitangents == NULL || surface->normals == NULL) {
        return;
    }

    tess.vertexCount = surface->vertexCount;
    tess.stageTangentsValid = qfalse;
    tess.stageBitangentsValid = qfalse;
    tess.indexCount = surface->indexCount;
    tess.requiresVertexBasis = qtrue;

    const uint32_t vertexVec3Bytes = (uint32_t)surface->vertexCount * (uint32_t)sizeof(*surface->normals);
    const uint32_t vertexVec2Bytes = (uint32_t)surface->vertexCount * (uint32_t)sizeof(*surface->texCoords);
    memcpy(tess.stageNormals, surface->normals, (size_t)vertexVec3Bytes);
    memcpy(tess.texCoords[0], surface->texCoords, (size_t)vertexVec2Bytes);
    memcpy(tess.xyz, surface->positions, (size_t)vertexVec3Bytes);
    for (int32_t index = 0; index < surface->indexCount; ++index) {
        tess.indexes[index] = (uint16_t)(surface->indices[index] - firstVertex);
    }

    RB_CalcTangentSpace();

    memcpy(surface->tangents, tess.stageTangents, (size_t)vertexVec3Bytes);
    memcpy(surface->bitangents, tess.stageBitangents, (size_t)vertexVec3Bytes);

    tess.vertexCount = 0;
    tess.indexCount = 0;
    tess.stageTangentsValid = qfalse;
    tess.stageBitangentsValid = qfalse;
    tess.requiresVertexBasis = qfalse;
}

/* Source: CoDUOMP.exe 0x0050bb70..0x0050bddb.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0050bb70_0050bddc.mcode.
 * Name and seven-argument source signature: exact same-module Mac symbol
 * BuildOptimizedSurface. The check of each still-zero destination index
 * before it is overwritten is present in both shipped binaries; Hunk_AllocInternal's
 * zero-fill contract makes it ineffective for ordinary positive vertex
 * counts, but it is retained as original behavior. */
qboolean BuildOptimizedSurface(msurface_t *worldSurface, renderer_shader_surface_build_t *build,
                               const renderer_lightmap_placement_t *lightmapPlacement, int32_t vertexCount, const drawVert_t *vertices,
                               int32_t indexCount, const int16_t *indices)
{
    /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered engine boundary input and state before use. */
    if (vertexCount <= 0 || indexCount <= 0) {
        ri.Error(ERR_DROP, "\x15"
                           "Invalid empty triangle soup surface");
        return qfalse;
    }
    if (indices[0] != 0) {
        ri.Error(ERR_DROP, "\x15"
                           "First index is not 0 in triangle soup surface");
        return qfalse;
    }

    shader_t *shader = worldSurface->shader;
    const qboolean canOptimize = CanOptimizeShader(shader);
    const uint32_t indexBytes = (uint32_t)indexCount * (uint32_t)sizeof(uint16_t);
    const uint32_t vertexPayloadBytes = (uint32_t)vertexCount * (uint32_t)(4U * sizeof(vec3_t) + 2U * sizeof(vec2_t) + sizeof(uint8_t[4]));
    const uint32_t payloadBytes = indexBytes + vertexPayloadBytes;
    const size_t allocationBytes = offsetof(renderer_world_mesh_surface_t, indices) + (size_t)payloadBytes;
    renderer_world_mesh_surface_t *surface = ri.Hunk_Alloc(allocationBytes);

    uint8_t *vertexStorage = (uint8_t *)surface->indices + indexBytes;
    const uint32_t vertexVec3Bytes = (uint32_t)vertexCount * (uint32_t)sizeof(vec3_t);
    const uint32_t vertexVec2Bytes = (uint32_t)vertexCount * (uint32_t)sizeof(vec2_t);
    surface->tangents = (vec3_t *)vertexStorage;
    vertexStorage += vertexVec3Bytes;
    surface->bitangents = (vec3_t *)vertexStorage;
    vertexStorage += vertexVec3Bytes;
    surface->normals = (vec3_t *)vertexStorage;
    vertexStorage += vertexVec3Bytes;
    surface->positions = (vec3_t *)vertexStorage;
    vertexStorage += vertexVec3Bytes;
    surface->texCoords = (vec2_t *)vertexStorage;
    vertexStorage += vertexVec2Bytes;
    surface->lightmapCoords = (vec2_t *)vertexStorage;
    vertexStorage += vertexVec2Bytes;
    surface->colors = (uint8_t(*)[4])vertexStorage;

    surface->surfaceType = (renderer_surface_type_t)shader->optimizedBackend;
    surface->storageMode = build->storageMode;
    surface->vertexCount = vertexCount;
    surface->indexCount = indexCount;
    for (int32_t component = 0; component < 3; ++component) {
        /* 0x0050bc0e MOV EAX,0x48800000 (+262144.0f) / 0x0050bc1c 0xc8800000
         * (-262144.0f). A prior pass used +/-65536.0f (0x47800000), too small for a
         * world reaching +/-131072. Same sentinel class as R_BModelWorldBounds. */
        surface->boundsMin[component] = 262144.0f;
        surface->boundsMax[component] = -262144.0f;
    }

    for (int32_t index = 0; index < indexCount; ++index) {
        const int32_t sourceIndex = indices[index];
        if (sourceIndex < 0 || sourceIndex >= vertexCount) {
            ri.Error(ERR_DROP, "\x15"
                               "Bad index in triangle soup surface");
            return qfalse;
        }
        surface->indices[index] = (uint16_t)(sourceIndex + build->firstVertex);
    }

    LittleVertices_T2T2C4V3(vertices, vertexCount, surface, lightmapPlacement, build->firstVertex);

    if (canOptimize != qfalse && shader->numUnfoggedPasses != 0)
        AdjustColors(surface, worldSurface);

    const uint32_t vertexOffset = (uint32_t)build->firstVertex * (uint32_t)sizeof(renderer_world_interleaved_vertex_t);
    switch (surface->surfaceType) {
    case R_SURFACE_OPTIMIZED_GENERIC:
    case R_SURFACE_OPTIMIZED_NV:
        OptimizeVertices_T2T2C4V3_GenericOrNV(
            surface, (renderer_world_interleaved_vertex_t *)(void *)(shader->optimizedVertexStorage.address + vertexOffset));
        break;

    case R_SURFACE_OPTIMIZED_ARB:
        OptimizeVertices_T2T2C4V3_ARB(surface, shader->optimizedVertexStorage.glBuffer, vertexOffset);
        break;

    case R_SURFACE_OPTIMIZED_ATI:
        OptimizeVertices_T2T2C4V3_ATI(surface, shader->optimizedVertexStorage.atiObjectBuffer,
                                      shader->optimizedVertexStorageOffset + vertexOffset);
        break;

    default:
        break;
    }

    for (int32_t component = 0; component < 3; ++component) {
        surface->boundsMin[component] -= shader->boundsExpansion;
        surface->boundsMax[component] += shader->boundsExpansion;
    }

    if (build->optimized != qfalse)
        build->firstVertex = (int32_t)((uint32_t)build->firstVertex + (uint32_t)vertexCount);

    worldSurface->data = (renderer_surface_t *)surface;
    return qtrue;
}

/* Source: CoDUOMP.exe 0x0050b4f0..0x0050b58a.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0050b4f0_0050b58a.mcode.
 * Name and source-level signature: exact same-module Mac symbol
 * OptimizeVertices_T2T2C4V3_GenericOrNV. */
void OptimizeVertices_T2T2C4V3_GenericOrNV(const renderer_world_mesh_surface_t *surface, renderer_world_interleaved_vertex_t *vertices)
{
    for (int32_t vertexIndex = 0; vertexIndex < surface->vertexCount; ++vertexIndex) {
        memcpy(vertices[vertexIndex].texCoord, surface->texCoords[vertexIndex], sizeof(vertices[vertexIndex].texCoord));
        memcpy(vertices[vertexIndex].lightmapCoord, surface->lightmapCoords[vertexIndex], sizeof(vertices[vertexIndex].lightmapCoord));
        memcpy(vertices[vertexIndex].color, surface->colors[vertexIndex], sizeof(vertices[vertexIndex].color));
        memcpy(vertices[vertexIndex].position, surface->positions[vertexIndex], sizeof(vertices[vertexIndex].position));
    }
}

/* Source: CoDUOMP.exe 0x0050b590..0x0050b64f.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0050b590_0050b64f.mcode.
 * Name and source-level signature: exact same-module Mac symbol
 * OptimizeVertices_T2T2C4V3_ATI. */
void OptimizeVertices_T2T2C4V3_ATI(const renderer_world_mesh_surface_t *surface, uint32_t objectBuffer, size_t vertexOffset)
{
    const uint32_t vertexBytes = (uint32_t)surface->vertexCount * (uint32_t)sizeof(renderer_world_interleaved_vertex_t);
    renderer_world_interleaved_vertex_t *vertices = ri.Hunk_AllocateTempMemory((size_t)vertexBytes);

    for (int32_t vertexIndex = 0; vertexIndex < surface->vertexCount; ++vertexIndex) {
        memcpy(vertices[vertexIndex].texCoord, surface->texCoords[vertexIndex], sizeof(vertices[vertexIndex].texCoord));
        memcpy(vertices[vertexIndex].lightmapCoord, surface->lightmapCoords[vertexIndex], sizeof(vertices[vertexIndex].lightmapCoord));
        memcpy(vertices[vertexIndex].color, surface->colors[vertexIndex], sizeof(vertices[vertexIndex].color));
        memcpy(vertices[vertexIndex].position, surface->positions[vertexIndex], sizeof(vertices[vertexIndex].position));
    }

    qglUpdateObjectBufferATI(objectBuffer, (uint32_t)vertexOffset, (int32_t)vertexBytes, vertices, GL_PRESERVE_ATI);
    ri.Hunk_FreeTempMemory(vertices);
}

/* Source: CoDUOMP.exe 0x0050b650..0x0050b727.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0050b650_0050b727.mcode.
 * Name and source-level signature: exact same-module Mac symbol
 * OptimizeVertices_T2T2C4V3_ARB. */
void OptimizeVertices_T2T2C4V3_ARB(const renderer_world_mesh_surface_t *surface, uint32_t vertexBuffer, size_t vertexOffset)
{
    const uint32_t vertexBytes = (uint32_t)surface->vertexCount * (uint32_t)sizeof(renderer_world_interleaved_vertex_t);
    renderer_world_interleaved_vertex_t *vertices = ri.Hunk_AllocateTempMemory((size_t)vertexBytes);

    for (int32_t vertexIndex = 0; vertexIndex < surface->vertexCount; ++vertexIndex) {
        memcpy(vertices[vertexIndex].texCoord, surface->texCoords[vertexIndex], sizeof(vertices[vertexIndex].texCoord));
        memcpy(vertices[vertexIndex].lightmapCoord, surface->lightmapCoords[vertexIndex], sizeof(vertices[vertexIndex].lightmapCoord));
        memcpy(vertices[vertexIndex].color, surface->colors[vertexIndex], sizeof(vertices[vertexIndex].color));
        memcpy(vertices[vertexIndex].position, surface->positions[vertexIndex], sizeof(vertices[vertexIndex].position));
    }

    qglBindBufferARB(GL_ARRAY_BUFFER_ARB, vertexBuffer);
    qglBufferSubDataARB(GL_ARRAY_BUFFER_ARB, (intptr_t)vertexOffset, (intptr_t)vertexBytes, vertices);
    qglBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
    ri.Hunk_FreeTempMemory(vertices);
}

/* Source: CoDUOMP.exe 0x0050b730..0x0050b7ba.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0050b730_0050b7ba.mcode.
 * Name and signature: exact same-module Mac symbol
 * R_RefreshOptimizedWorldSurfaces_ARB. */
void R_RefreshOptimizedWorldSurfaces_ARB(void)
{
    /* 0x0050b731 MOV EAX,tr.world; 0x0050b736 MOV ECX,[EAX+0x9c] (surfaceCount) --
     * the DLL dereferences tr.world with NO null test here, unlike the sibling
     * R_IncrementalRefreshOptimizedWorldSurfaces_ARB (0x0050b7c7 TEST EBP,EBP). A
     * prior pass added a defensive `if (tr.world == NULL) return;` this function does
     * not have; removed to match the machine code. */

    for (int32_t surfaceIndex = 0; surfaceIndex < tr.world->numsurfaces; ++surfaceIndex) {
        msurface_t *worldSurface = &tr.world->surfaces[surfaceIndex];
        renderer_world_mesh_surface_t *surface = (renderer_world_mesh_surface_t *)worldSurface->data;

        if (surface->surfaceType != R_SURFACE_OPTIMIZED_ARB)
            continue;

        uint16_t firstVertex = surface->indices[0];
        for (int32_t index = 1; index < surface->indexCount; ++index) {
            if (surface->indices[index] < firstVertex)
                firstVertex = surface->indices[index];
        }

        OptimizeVertices_T2T2C4V3_ARB(surface, worldSurface->shader->optimizedVertexStorage.glBuffer,
                                      (size_t)firstVertex * sizeof(renderer_world_interleaved_vertex_t));
    }
}

/* Source: CoDUOMP.exe 0x0050b7c0..0x0050b846.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0050b7c0_0050b846.mcode.
 * Name and signature: exact same-module Mac symbol
 * R_IncrementalRefreshOptimizedWorldSurfaces_ARB. */
void R_IncrementalRefreshOptimizedWorldSurfaces_ARB(void)
{
    if (tr.world == NULL)
        return;

    const int32_t previousSurfaceIndex = tr.worldRefreshSurfaceIndex;
    for (;;) {
        ++tr.worldRefreshSurfaceIndex;
        if (tr.worldRefreshSurfaceIndex == tr.world->numsurfaces)
            tr.worldRefreshSurfaceIndex = 0;
        if (tr.worldRefreshSurfaceIndex == previousSurfaceIndex)
            return;

        msurface_t *worldSurface = &tr.world->surfaces[tr.worldRefreshSurfaceIndex];
        renderer_world_mesh_surface_t *surface = (renderer_world_mesh_surface_t *)worldSurface->data;
        if (surface->surfaceType != R_SURFACE_OPTIMIZED_ARB)
            continue;

        uint16_t firstVertex = surface->indices[0];
        for (int32_t index = 1; index < surface->indexCount; ++index) {
            if (surface->indices[index] < firstVertex)
                firstVertex = surface->indices[index];
        }

        OptimizeVertices_T2T2C4V3_ARB(surface, worldSurface->shader->optimizedVertexStorage.glBuffer,
                                      (size_t)firstVertex * sizeof(renderer_world_interleaved_vertex_t));
        return;
    }
}

/* Source: CoDUOMP.exe 0x0050b850..0x0050b8b5.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0050b850_0050b8b5.mcode.
 * Name and signature: exact same-module Mac symbol AdjustColors. */
void AdjustColors(renderer_world_mesh_surface_t *surface, const msurface_t *worldSurface)
{
    /* SHL at 0x0050b85f forms the copy size with 32-bit wrap before the
     * original i386 memcpy operations. */
    const uint32_t colorBytes = (uint32_t)surface->vertexCount * (uint32_t)sizeof(*surface->colors);

    tess.vertexCount = surface->vertexCount;
    memcpy(tess.vertexColors, surface->colors, (size_t)colorBytes);
    RB_ComputeColors(worldSurface->shader->stages[0]);
    memcpy(surface->colors, tess.stageVertexColors, (size_t)colorBytes);
    tess.vertexCount = 0;
}
