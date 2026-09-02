#include "backend.h"

#include "gl_api.h"
#include "gl_state.h"

#include <string.h>

enum renderer_static_model_surface_flag_mask_e {
    R_SMODEL_T2V3_SURFACE_FLAGS_MASK = 0x3ff5fd00,
    R_SMODEL_T2N3V3_SURFACE_FLAGS_MASK = 0x3ff7fc00
};

enum {
    R_STATIC_MODEL_VERTEX_STAGING_COUNT = UINT16_MAX + 1U
};

/* NOT_FROM_ORIGINAL_SOURCE: readable carrier for the mutually exclusive
 * 20-byte and 32-byte interleaved scratch forms in the optimized paths. */
typedef union renderer_static_model_vertex_staging_u {
    renderer_static_model_t2v3_vertex_t
        t2v3[R_STATIC_MODEL_VERTEX_STAGING_COUNT];
    renderer_static_model_t2n3v3_vertex_t
        t2n3v3[R_STATIC_MODEL_VERTEX_STAGING_COUNT];
} renderer_static_model_vertex_staging_t;

/* NOT_FROM_ORIGINAL_SOURCE: readable expression for the repeated
 * (size + 31) & ~31 instruction sequence in both original functions. */
static size_t coduomp_align_static_model_bytes(size_t size)
{
    return (size + (R_STATIC_VERTEX_MEMORY_ALIGNMENT - 1U)) &
           ~(size_t)(R_STATIC_VERTEX_MEMORY_ALIGNMENT - 1U);
}

/* Source: CoDUOMP.exe 0x00518520..0x0051867d.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00518520_0051867e.mcode.
 * Name and Hunk_AllocInternal source boundary: exact same-module Mac symbol
 * R_OptimizeSModelSurfGeneric. Both binaries prove the two shader-surface
 * masks, surface type selectors, interleaved strides, and component order. */
void R_OptimizeSModelSurfGeneric(renderer_static_model_surface_t *surface)
{
    const uint32_t surfaceFlags = surface->shader->surfaceFlags;

    if ((surfaceFlags & R_SMODEL_T2V3_SURFACE_FLAGS_MASK) == 0) {
        surface->storageSource = R_STATIC_VERTEX_MEMORY_HUNK;
        surface->surfaceType = R_SURFACE_STATIC_MODEL_T2V3_GENERIC;

        renderer_static_model_t2v3_vertex_t *optimized =
            ri.Hunk_Alloc((size_t)surface->vertexCount *
                          sizeof(*optimized));
        surface->optimized.vertices = (uint8_t *)optimized;

        for (uint16_t vertexIndex = 0;
             vertexIndex < surface->vertexCount; ++vertexIndex) {
            memcpy(optimized[vertexIndex].texCoord,
                   surface->texCoords[vertexIndex],
                   sizeof(optimized[vertexIndex].texCoord));
            memcpy(optimized[vertexIndex].position,
                   surface->vertices[vertexIndex],
                   sizeof(optimized[vertexIndex].position));
        }
        return;
    }

    if ((surfaceFlags & R_SMODEL_T2N3V3_SURFACE_FLAGS_MASK) == 0) {
        surface->storageSource = R_STATIC_VERTEX_MEMORY_HUNK;
        surface->surfaceType = R_SURFACE_STATIC_MODEL_T2N3V3_GENERIC;

        renderer_static_model_t2n3v3_vertex_t *optimized =
            ri.Hunk_Alloc((size_t)surface->vertexCount *
                          sizeof(*optimized));
        surface->optimized.vertices = (uint8_t *)optimized;

        for (uint16_t vertexIndex = 0;
             vertexIndex < surface->vertexCount; ++vertexIndex) {
            memcpy(optimized[vertexIndex].texCoord,
                   surface->texCoords[vertexIndex],
                   sizeof(optimized[vertexIndex].texCoord));
            memcpy(optimized[vertexIndex].normal,
                   surface->normals[vertexIndex],
                   sizeof(optimized[vertexIndex].normal));
            memcpy(optimized[vertexIndex].position,
                   surface->vertices[vertexIndex],
                   sizeof(optimized[vertexIndex].position));
        }
    }
}

/* Source: CoDUOMP.exe 0x00518680..0x0051888d.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00518680_0051888e.mcode.
 * Name and R_AllocMemoryNV source boundary: exact same-module Mac symbol
 * R_OptimizeSModelSurfNV. Windows LTCG inlines the allocator but preserves
 * its primary/secondary/hunk selection and strict pool-limit comparisons. */
void R_OptimizeSModelSurfNV(renderer_static_model_surface_t *surface)
{
    const uint32_t surfaceFlags = surface->shader->surfaceFlags;

    if ((surfaceFlags & R_SMODEL_T2V3_SURFACE_FLAGS_MASK) == 0) {
        surface->surfaceType = R_SURFACE_STATIC_MODEL_T2V3_NV;
        surface->storageSource = R_AllocMemoryNV(
            R_STATIC_VERTEX_MEMORY_PRIMARY,
            (size_t)surface->vertexCount *
                sizeof(renderer_static_model_t2v3_vertex_t),
            &surface->optimized.vertices);

        renderer_static_model_t2v3_vertex_t *optimized =
            (renderer_static_model_t2v3_vertex_t *)
                surface->optimized.vertices;
        for (uint16_t vertexIndex = 0;
             vertexIndex < surface->vertexCount; ++vertexIndex) {
            memcpy(optimized[vertexIndex].texCoord,
                   surface->texCoords[vertexIndex],
                   sizeof(optimized[vertexIndex].texCoord));
            memcpy(optimized[vertexIndex].position,
                   surface->vertices[vertexIndex],
                   sizeof(optimized[vertexIndex].position));
        }
        return;
    }

    if ((surfaceFlags & R_SMODEL_T2N3V3_SURFACE_FLAGS_MASK) == 0) {
        surface->surfaceType = R_SURFACE_STATIC_MODEL_T2N3V3_NV;
        surface->storageSource = R_AllocMemoryNV(
            R_STATIC_VERTEX_MEMORY_PRIMARY,
            (size_t)surface->vertexCount *
                sizeof(renderer_static_model_t2n3v3_vertex_t),
            &surface->optimized.vertices);

        renderer_static_model_t2n3v3_vertex_t *optimized =
            (renderer_static_model_t2n3v3_vertex_t *)
                surface->optimized.vertices;
        for (uint16_t vertexIndex = 0;
             vertexIndex < surface->vertexCount; ++vertexIndex) {
            memcpy(optimized[vertexIndex].texCoord,
                   surface->texCoords[vertexIndex],
                   sizeof(optimized[vertexIndex].texCoord));
            memcpy(optimized[vertexIndex].normal,
                   surface->normals[vertexIndex],
                   sizeof(optimized[vertexIndex].normal));
            memcpy(optimized[vertexIndex].position,
                   surface->vertices[vertexIndex],
                   sizeof(optimized[vertexIndex].position));
        }
    }
}

/* Source: CoDUOMP.exe 0x00518890..0x00518a89.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00518890_00518a8a.mcode.
 * Exact same-module Mac symbol R_OptimizeSModelSurfATI. Windows proves that
 * the optional ATI element-array payload precedes the interleaved vertices in
 * the selected shared object buffer and that successful ATI allocations are
 * rewritten to storage mode 3 after the object-buffer name is selected. */
qboolean R_OptimizeSModelSurfATI(renderer_static_model_surface_t *surface)
{
    renderer_static_model_vertex_staging_t staging;
    const uint32_t surfaceFlags = surface->shader->surfaceFlags;
    const size_t alignedIndexBytes = coduomp_align_static_model_bytes(
        (size_t)surface->indexCount * sizeof(*surface->indices));
    size_t vertexBytes;

    if ((surfaceFlags & R_SMODEL_T2V3_SURFACE_FLAGS_MASK) == 0) {
        surface->surfaceType = R_SURFACE_STATIC_MODEL_T2V3_ATI;
        vertexBytes = (size_t)surface->vertexCount *
                      sizeof(staging.t2v3[0]);
    } else if ((surfaceFlags & R_SMODEL_T2N3V3_SURFACE_FLAGS_MASK) == 0) {
        surface->surfaceType = R_SURFACE_STATIC_MODEL_T2N3V3_ATI;
        vertexBytes = (size_t)surface->vertexCount *
                      sizeof(staging.t2n3v3[0]);
    } else {
        return qfalse;
    }

    const size_t allocationBytes =
        vertexBytes + (glConfig.elementArrayATIAvailable
                           ? alignedIndexBytes
                           : 0U);
    size_t allocationOffset = 0;
    surface->storageSource = R_AllocMemoryATI(
        R_STATIC_VERTEX_MEMORY_PRIMARY, allocationBytes,
        &allocationOffset);
    if (surface->storageSource == R_STATIC_VERTEX_MEMORY_NONE)
        return qfalse;

    const renderer_static_vertex_memory_base_t bufferBase =
        surface->storageSource == R_STATIC_VERTEX_MEMORY_PRIMARY
            ? tr.staticVertexMemoryPrimary
            : tr.staticVertexMemorySecondary;
    surface->optimized.atiObjectBuffer = bufferBase.atiObjectBuffer;
    surface->storageSource = R_STATIC_VERTEX_MEMORY_HUNK;

    if (glConfig.elementArrayATIAvailable) {
        surface->backend.ati.vertexOffset =
            (uint32_t)(allocationOffset + alignedIndexBytes);
        surface->backend.ati.indexOffset = (uint32_t)allocationOffset;
        qglUpdateObjectBufferATI(
            surface->optimized.atiObjectBuffer,
            surface->backend.ati.indexOffset,
            (int32_t)((size_t)surface->indexCount *
                      sizeof(*surface->indices)),
            surface->indices, GL_PRESERVE_ATI);
    } else {
        surface->backend.ati.indexOffset = 0;
        surface->backend.ati.vertexOffset = (uint32_t)allocationOffset;
    }

    if (surface->surfaceType == R_SURFACE_STATIC_MODEL_T2V3_ATI) {
        for (uint16_t vertexIndex = 0;
             vertexIndex < surface->vertexCount; ++vertexIndex) {
            memcpy(staging.t2v3[vertexIndex].texCoord,
                   surface->texCoords[vertexIndex],
                   sizeof(staging.t2v3[vertexIndex].texCoord));
            memcpy(staging.t2v3[vertexIndex].position,
                   surface->vertices[vertexIndex],
                   sizeof(staging.t2v3[vertexIndex].position));
        }
    } else {
        for (uint16_t vertexIndex = 0;
             vertexIndex < surface->vertexCount; ++vertexIndex) {
            memcpy(staging.t2n3v3[vertexIndex].texCoord,
                   surface->texCoords[vertexIndex],
                   sizeof(staging.t2n3v3[vertexIndex].texCoord));
            memcpy(staging.t2n3v3[vertexIndex].normal,
                   surface->normals[vertexIndex],
                   sizeof(staging.t2n3v3[vertexIndex].normal));
            memcpy(staging.t2n3v3[vertexIndex].position,
                   surface->vertices[vertexIndex],
                   sizeof(staging.t2n3v3[vertexIndex].position));
        }
    }

    qglUpdateObjectBufferATI(
        surface->optimized.atiObjectBuffer,
        surface->backend.ati.vertexOffset, (int32_t)vertexBytes,
        &staging, GL_PRESERVE_ATI);
    return qtrue;
}

/* Source: CoDUOMP.exe 0x00518a90..0x00518c16.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00518a90_00518c17.mcode.
 * Exact same-module Mac symbol R_OptimizeSModelSurfARB. The original creates
 * the aligned index buffer first, deletes it if vertex-buffer creation fails,
 * and leaves storageSource unchanged on both success and failure. */
qboolean R_OptimizeSModelSurfARB(renderer_static_model_surface_t *surface)
{
    renderer_static_model_vertex_staging_t staging;
    const uint32_t surfaceFlags = surface->shader->surfaceFlags;
    const size_t alignedIndexBytes = coduomp_align_static_model_bytes(
        (size_t)surface->indexCount * sizeof(*surface->indices));
    size_t vertexBytes;

    if ((surfaceFlags & R_SMODEL_T2V3_SURFACE_FLAGS_MASK) == 0) {
        surface->surfaceType = R_SURFACE_STATIC_MODEL_T2V3_ARB;
        vertexBytes = (size_t)surface->vertexCount *
                      sizeof(staging.t2v3[0]);
    } else if ((surfaceFlags & R_SMODEL_T2N3V3_SURFACE_FLAGS_MASK) == 0) {
        surface->surfaceType = R_SURFACE_STATIC_MODEL_T2N3V3_ARB;
        vertexBytes = (size_t)surface->vertexCount *
                      sizeof(staging.t2n3v3[0]);
    } else {
        return qfalse;
    }

    surface->backend.arb.indexBuffer = R_CreateBufferARB(
        GL_ELEMENT_ARRAY_BUFFER_ARB, alignedIndexBytes,
        surface->indices, GL_STATIC_DRAW_ARB);
    if (surface->backend.arb.indexBuffer == 0)
        return qfalse;

    if (surface->surfaceType == R_SURFACE_STATIC_MODEL_T2V3_ARB) {
        for (uint16_t vertexIndex = 0;
             vertexIndex < surface->vertexCount; ++vertexIndex) {
            memcpy(staging.t2v3[vertexIndex].texCoord,
                   surface->texCoords[vertexIndex],
                   sizeof(staging.t2v3[vertexIndex].texCoord));
            memcpy(staging.t2v3[vertexIndex].position,
                   surface->vertices[vertexIndex],
                   sizeof(staging.t2v3[vertexIndex].position));
        }
    } else {
        for (uint16_t vertexIndex = 0;
             vertexIndex < surface->vertexCount; ++vertexIndex) {
            memcpy(staging.t2n3v3[vertexIndex].texCoord,
                   surface->texCoords[vertexIndex],
                   sizeof(staging.t2n3v3[vertexIndex].texCoord));
            memcpy(staging.t2n3v3[vertexIndex].normal,
                   surface->normals[vertexIndex],
                   sizeof(staging.t2n3v3[vertexIndex].normal));
            memcpy(staging.t2n3v3[vertexIndex].position,
                   surface->vertices[vertexIndex],
                   sizeof(staging.t2n3v3[vertexIndex].position));
        }
    }

    surface->optimized.vertexBuffer = R_CreateBufferARB(
        GL_ARRAY_BUFFER_ARB, vertexBytes, &staging,
        GL_STATIC_DRAW_ARB);
    if (surface->optimized.vertexBuffer == 0) {
        qglDeleteBuffersARB(1, &surface->backend.arb.indexBuffer);
        return qfalse;
    }

    return qtrue;
}
