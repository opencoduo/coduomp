#include "../animation/dobj.h"
#include "gl_api.h"
#include "gl_state.h"
#include "renderer_vbo.h"

#include <stdint.h>
#include <string.h>

enum {
    XMODEL_NO_SINGLE_BONE = -1,
    XMODEL_GPU_BUFFER_ALIGNMENT = 32,
    XSURFACE_REFRESH_VERTICES = 1,
    XSURFACE_REFRESH_INDICES = 2
};

/* Source: CoDUOMP.exe 0x0049f800..0x0049f8fa.
 * Name: same-module Mac symbol R_OptimizeRigidXSurfaceARB.  The 2 MiB local
 * vertex staging area, 32-byte interleaved stride, aligned index byte count,
 * GL targets/usages, failure cleanup, and two-handle result layout are all
 * direct machine-code facts. */
void R_OptimizeRigidXSurfaceARB(XSurface *surface,
                                void *(*alloc)(size_t size))
{
    XSurfaceARBVert vertices[XMODEL_MAX_VERTICES];

    if (surface->optimizedDataARB != NULL) {
        return;
    }

    for (int32_t vertexIndex = 0;
         vertexIndex < surface->vertexCount;
         ++vertexIndex) {
        memcpy(vertices[vertexIndex].texCoord,
               surface->texCoords[vertexIndex],
               sizeof(vertices[vertexIndex].texCoord));
        memcpy(&vertices[vertexIndex].rigidVertex,
               &surface->vertexData.rigidVertices[vertexIndex],
               sizeof(vertices[vertexIndex].rigidVertex));
    }

    const uint32_t indexBytes =
        ((uint32_t)(int32_t)surface->triangleCount *
             (uint32_t)sizeof(surface->triangles[0]) +
         (XMODEL_GPU_BUFFER_ALIGNMENT - 1u)) &
        ~(uint32_t)(XMODEL_GPU_BUFFER_ALIGNMENT - 1u);
    uint32_t indexBuffer = R_CreateBufferARB(
        GL_ELEMENT_ARRAY_BUFFER_ARB, indexBytes, surface->triangles,
        GL_STATIC_DRAW_ARB);
    if (indexBuffer == 0) {
        return;
    }

    uint32_t vertexBuffer = R_CreateBufferARB(
        GL_ARRAY_BUFFER_ARB,
        (size_t)((uint32_t)(int32_t)surface->vertexCount *
                 (uint32_t)sizeof(vertices[0])),
        vertices,
        GL_STATIC_DRAW_ARB);
    if (vertexBuffer == 0) {
        qglDeleteBuffersARB(1, &indexBuffer);
        return;
    }

    XSurfaceOptimizedDataARB *optimized = alloc(sizeof(*optimized));
    optimized->vertexBuffer = vertexBuffer;
    optimized->indexBuffer = indexBuffer;
    surface->optimizedDataARB = optimized;
}

/* Source: CoDUOMP.exe 0x0049f900..0x0049fa0d.
 * Name: same-module Mac symbol R_OptimizeRigidXSurfaceNV.  The source first
 * reserves an aligned interleaved-vertex range from either renderer static
 * pool, falling back to Hunk_AllocAlignInternal, then records the selected source and
 * copies each 8-byte texcoord plus 24-byte rigid vertex into a 32-byte slot. */
void R_OptimizeRigidXSurfaceNV(XSurface *surface,
                               void *(*alloc)(size_t size))
{
    if (surface->optimizedDataNV != NULL) {
        return;
    }

    XSurfaceOptimizedDataNV *optimized = alloc(sizeof(*optimized));
    const uint32_t vertexBytes =
        ((uint32_t)(int32_t)surface->vertexCount *
             (uint32_t)sizeof(XSurfaceARBVert) +
         (XMODEL_GPU_BUFFER_ALIGNMENT - 1u)) &
        ~(uint32_t)(XMODEL_GPU_BUFFER_ALIGNMENT - 1u);

    optimized->memorySource = R_AllocMemoryNV(
        R_STATIC_VERTEX_MEMORY_PRIMARY, vertexBytes,
        &optimized->interleavedVertices);

    XSurfaceARBVert *vertices =
        (XSurfaceARBVert *)optimized->interleavedVertices;
    for (int32_t vertexIndex = 0;
         vertexIndex < surface->vertexCount;
         ++vertexIndex) {
        memcpy(vertices[vertexIndex].texCoord,
               surface->texCoords[vertexIndex],
               sizeof(vertices[vertexIndex].texCoord));
        memcpy(&vertices[vertexIndex].rigidVertex,
               &surface->vertexData.rigidVertices[vertexIndex],
               sizeof(vertices[vertexIndex].rigidVertex));
    }

    surface->optimizedDataNV = optimized;
}

/* Source: CoDUOMP.exe 0x0049fa10..0x0049fb76.
 * Name: same-module Mac symbol R_OptimizeRigidXSurfaceATI.  This backend
 * reserves a combined aligned range in one of the renderer's two ATI object
 * buffers, optionally uploads the aligned triangle block, then uploads the
 * same 32-byte interleaved rigid vertices used by the ARB/NV paths. */
void R_OptimizeRigidXSurfaceATI(XSurface *surface,
                                void *(*alloc)(size_t size))
{
    XSurfaceARBVert vertices[XMODEL_MAX_VERTICES];

    if (surface->optimizedDataATI != NULL) {
        return;
    }

    size_t indexBytes = glConfig.elementArrayATIAvailable != qfalse
        ? (((size_t)surface->triangleCount * sizeof(surface->triangles[0]) +
            (XMODEL_GPU_BUFFER_ALIGNMENT - 1U)) &
           ~(size_t)(XMODEL_GPU_BUFFER_ALIGNMENT - 1U))
        : 0U;
    size_t vertexBytes =
        (size_t)surface->vertexCount * sizeof(vertices[0]);
    size_t totalBytes =
        (vertexBytes + indexBytes + (XMODEL_GPU_BUFFER_ALIGNMENT - 1U)) &
        ~(size_t)(XMODEL_GPU_BUFFER_ALIGNMENT - 1U);

    size_t indexOffset;
    const renderer_static_vertex_memory_source_t memorySource =
        R_AllocMemoryATI(R_STATIC_VERTEX_MEMORY_PRIMARY, totalBytes,
                         &indexOffset);
    if (memorySource == R_STATIC_VERTEX_MEMORY_NONE) {
        return;
    }
    const renderer_static_vertex_memory_base_t bufferBase =
        memorySource == R_STATIC_VERTEX_MEMORY_PRIMARY
            ? tr.staticVertexMemoryPrimary
            : tr.staticVertexMemorySecondary;

    XSurfaceOptimizedDataATI *optimized = alloc(sizeof(*optimized));
    optimized->memorySource = R_STATIC_VERTEX_MEMORY_HUNK;
    optimized->objectBuffer = bufferBase.atiObjectBuffer;
    optimized->indexOffset = (uint32_t)indexOffset;
    optimized->vertexOffset = (uint32_t)(indexOffset + indexBytes);

    for (int32_t vertexIndex = 0;
         vertexIndex < surface->vertexCount;
         ++vertexIndex) {
        memcpy(vertices[vertexIndex].texCoord,
               surface->texCoords[vertexIndex],
               sizeof(vertices[vertexIndex].texCoord));
        memcpy(&vertices[vertexIndex].rigidVertex,
               &surface->vertexData.rigidVertices[vertexIndex],
               sizeof(vertices[vertexIndex].rigidVertex));
    }

    if (indexBytes != 0U) {
        qglUpdateObjectBufferATI(
            optimized->objectBuffer, optimized->indexOffset,
            (uint32_t)indexBytes,
            surface->triangles, GL_PRESERVE_ATI);
    }
    qglUpdateObjectBufferATI(
        optimized->objectBuffer, optimized->vertexOffset,
        (uint32_t)vertexBytes,
        vertices, GL_PRESERVE_ATI);
    surface->optimizedDataATI = optimized;
}

/* Source: CoDUOMP.exe 0x0049f570..0x0049f7c6.
 * Name: same-module Mac symbol XSurfaceRefresh_ARB.  The caller-supplied low
 * two flag bits independently select vertex and index refresh.  Each selected
 * buffer is rebound, orphaned with a null data upload, filled with current
 * surface data, and finally unbound. */
void XSurfaceRefresh_ARB(XSurface *surface, uint32_t refreshFlags)
{
    XSurfaceARBVert vertices[XMODEL_MAX_VERTICES];
    XSurfaceOptimizedDataARB *optimized = surface->optimizedDataARB;
    if (optimized == NULL) {
        return;
    }

    for (int32_t vertexIndex = 0;
         vertexIndex < surface->vertexCount;
         ++vertexIndex) {
        memcpy(vertices[vertexIndex].texCoord,
               surface->texCoords[vertexIndex],
               sizeof(vertices[vertexIndex].texCoord));
        memcpy(&vertices[vertexIndex].rigidVertex,
               &surface->vertexData.rigidVertices[vertexIndex],
               sizeof(vertices[vertexIndex].rigidVertex));
    }

    const uint32_t indexBytes =
        ((uint32_t)(int32_t)surface->triangleCount *
             (uint32_t)sizeof(surface->triangles[0]) +
         (XMODEL_GPU_BUFFER_ALIGNMENT - 1u)) &
        ~(uint32_t)(XMODEL_GPU_BUFFER_ALIGNMENT - 1u);
    const uint32_t vertexBytes =
        (uint32_t)(int32_t)surface->vertexCount *
        (uint32_t)sizeof(vertices[0]);

    if ((refreshFlags & XSURFACE_REFRESH_INDICES) != 0U) {
        qglBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB,
                         optimized->indexBuffer);
        qglBufferDataARB(GL_ELEMENT_ARRAY_BUFFER_ARB, indexBytes, NULL,
                         GL_STATIC_DRAW_ARB);
        qglBufferDataARB(GL_ELEMENT_ARRAY_BUFFER_ARB, indexBytes,
                         surface->triangles, GL_STATIC_DRAW_ARB);
        qglBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, 0);
    }

    if ((refreshFlags & XSURFACE_REFRESH_VERTICES) != 0U) {
        qglBindBufferARB(GL_ARRAY_BUFFER_ARB, optimized->vertexBuffer);
        qglBufferDataARB(GL_ARRAY_BUFFER_ARB, vertexBytes, NULL,
                         GL_STATIC_DRAW_ARB);
        qglBufferDataARB(GL_ARRAY_BUFFER_ARB, vertexBytes, vertices,
                         GL_STATIC_DRAW_ARB);
        qglBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
    }
}

/* Source: CoDUOMP.exe 0x0049f7d0..0x0049f7fb, exporter-gap recovery.
 * Role-derived name: the Windows compiler emits this unreferenced wrapper
 * immediately after XSurfaceRefresh_ARB.  Its two live renderer callers inline
 * the same xmodel-surfaces traversal, while the Mac build does not retain a
 * corresponding standalone symbol. */
void XModelSurfsRefresh_ARB(XModelSurfsData *surfs,
                            uint32_t refreshFlags)
{
    for (int32_t surfaceIndex = 0;
         surfaceIndex < surfs->surfaceCount;
         ++surfaceIndex) {
        XSurfaceRefresh_ARB(surfs->surfaces[surfaceIndex], refreshFlags);
    }
}

/* Source: CoDUOMP.exe 0x0049fb80..0x0049fbf0.
 * Name: same-module Mac symbol XModelOptimize. */
void XModelOptimize(XModelSurfsData *surfs,
                    void *(*alloc)(size_t size))
{
    for (int32_t surfaceIndex = 0;
         surfaceIndex < surfs->surfaceCount;
         ++surfaceIndex) {
        XSurface *surface = surfs->surfaces[surfaceIndex];
        if (surface->boneIndex == XMODEL_NO_SINGLE_BONE) {
            continue;
        }
        if (glConfig.vertexBufferObjectAvailable != qfalse) {
            R_OptimizeRigidXSurfaceARB(surface, alloc);
        } else if (glConfig.vertexArrayRangeMode !=
                   R_VERTEX_ARRAY_RANGE_NONE) {
            R_OptimizeRigidXSurfaceNV(surface, alloc);
        } else if (glConfig.vertexArrayObjectATIAvailable != qfalse) {
            R_OptimizeRigidXSurfaceATI(surface, alloc);
        }
    }
}

/* Source: CoDUOMP.exe 0x0049fbf0..0x0049fc3a, exporter-gap recovery.
 * Name: same-module Mac symbol XSurfaceOptimize. */
void XSurfaceOptimize(XSurface *surface,
                      void *(*alloc)(size_t size))
{
    if (surface->boneIndex == XMODEL_NO_SINGLE_BONE) {
        return;
    }
    if (glConfig.vertexBufferObjectAvailable != qfalse) {
        R_OptimizeRigidXSurfaceARB(surface, alloc);
    } else if (glConfig.vertexArrayRangeMode !=
               R_VERTEX_ARRAY_RANGE_NONE) {
        R_OptimizeRigidXSurfaceNV(surface, alloc);
    } else if (glConfig.vertexArrayObjectATIAvailable != qfalse) {
        R_OptimizeRigidXSurfaceATI(surface, alloc);
    }
}

/* NOT_FROM_ORIGINAL_SOURCE: target-local edge used by the common XModel
 * loader.  The Windows client owns the renderer preprocessing subsystem. */
void xmodel_compat_optimize_loaded_surfs(
    XModelSurfsData *surfs, xmodel_asset_alloc_fn alloc)
{
    XModelOptimize(surfs, alloc);
}
