#ifndef CODUOMP_DOBJ_H
#define CODUOMP_DOBJ_H

#include <stddef.h>
#include <stdint.h>

#include "animation/dobj.h"
#include "animation/xmodel.h"
#include "qcommon/dobj_types.h"
#include "qcommon/file_data.h"
#include "qcommon/xanim_types.h"
#include "qcommon/xmodel_types.h"
#include "../q_shared.h"
#include "../renderer/gl_api.h"
#include "../renderer/renderer_vbo.h"
#include "../scripting/script_runtime.h"

typedef struct material_s material_t;

enum {
    DOBJ_PART_REMAP_STRING_TYPE = 12,
    XMODEL_MAX_VERTICES = 65536
};

/* Interleaved vertex uploaded by all three optimized rigid-surface backends. */
typedef struct XSurfaceARBVert_s {
    vec2_t texCoord;
    XSurfaceRigidVert rigidVertex;
} XSurfaceARBVert;

typedef struct XSurfaceOptimizedDataATI_s {
    renderer_static_vertex_memory_source_t memorySource;
    uint32_t objectBuffer;
    uint32_t vertexOffset;
    uint32_t indexOffset;
} XSurfaceOptimizedDataATI;
typedef struct XSurfaceOptimizedDataARB_s {
    uint32_t vertexBuffer;
    uint32_t indexBuffer;
} XSurfaceOptimizedDataARB;
typedef struct XSurfaceOptimizedDataNV_s {
    renderer_static_vertex_memory_source_t memorySource;
    uint8_t *interleavedVertices;
} XSurfaceOptimizedDataNV;

#if UINTPTR_MAX == UINT32_MAX
_Static_assert(_Alignof(XSurfaceARBVert) == 4,
               "i386 optimized rigid-vertex alignment changed");
_Static_assert(offsetof(XSurfaceARBVert, texCoord) == 0x00,
               "i386 optimized rigid-vertex texcoord moved");
_Static_assert(offsetof(XSurfaceARBVert, rigidVertex) == 0x08,
               "i386 optimized rigid-vertex payload moved");
_Static_assert(offsetof(XSurfaceARBVert, rigidVertex.normal) == 0x08,
               "i386 optimized rigid-vertex normal moved");
_Static_assert(offsetof(XSurfaceARBVert, rigidVertex.position) == 0x14,
               "i386 optimized rigid-vertex position moved");
_Static_assert(sizeof(XSurfaceARBVert) == 0x20,
               "i386 optimized rigid-vertex size changed");
_Static_assert(_Alignof(XSurfaceOptimizedDataATI) == 4,
               "i386 ATI optimized-data alignment changed");
_Static_assert(offsetof(XSurfaceOptimizedDataATI, memorySource) == 0x00,
               "i386 ATI optimized-data memory source moved");
_Static_assert(offsetof(XSurfaceOptimizedDataATI, objectBuffer) == 0x04,
               "i386 ATI optimized-data object buffer moved");
_Static_assert(offsetof(XSurfaceOptimizedDataATI, vertexOffset) == 0x08,
               "i386 ATI optimized-data vertex offset moved");
_Static_assert(offsetof(XSurfaceOptimizedDataATI, indexOffset) == 0x0c,
               "i386 ATI optimized-data index offset moved");
_Static_assert(sizeof(XSurfaceOptimizedDataATI) == 0x10,
               "i386 ATI optimized-data size changed");
_Static_assert(_Alignof(XSurfaceOptimizedDataARB) == 4,
               "i386 ARB optimized-data alignment changed");
_Static_assert(offsetof(XSurfaceOptimizedDataARB, vertexBuffer) == 0x00,
               "i386 ARB optimized-data vertex buffer moved");
_Static_assert(offsetof(XSurfaceOptimizedDataARB, indexBuffer) == 0x04,
               "i386 ARB optimized-data index buffer moved");
_Static_assert(sizeof(XSurfaceOptimizedDataARB) == 0x08,
               "i386 ARB optimized-data size changed");
_Static_assert(_Alignof(XSurfaceOptimizedDataNV) == 4,
               "i386 NV optimized-data alignment changed");
_Static_assert(offsetof(XSurfaceOptimizedDataNV, memorySource) == 0x00,
               "i386 NV optimized-data memory source moved");
_Static_assert(offsetof(XSurfaceOptimizedDataNV,
                        interleavedVertices) == 0x04,
               "i386 NV optimized-data vertices moved");
_Static_assert(sizeof(XSurfaceOptimizedDataNV) == 0x08,
               "i386 NV optimized-data size changed");
_Static_assert(offsetof(XAnimTree, poolNodeHandles) == 12,
               "i386 XAnim runtime-tree prefix changed");
#endif

void XModelOptimize(XModelSurfsData *surfs,
                    void *(*alloc)(size_t size));
void XSurfaceOptimize(XSurface *surface,
                      void *(*alloc)(size_t size));
void R_OptimizeRigidXSurfaceARB(XSurface *surface,
                                void *(*alloc)(size_t size));
void R_OptimizeRigidXSurfaceNV(XSurface *surface,
                               void *(*alloc)(size_t size));
void R_OptimizeRigidXSurfaceATI(XSurface *surface,
                                void *(*alloc)(size_t size));
void XSurfaceRefresh_ARB(XSurface *surface, uint32_t refreshFlags);
void XModelSurfsRefresh_ARB(XModelSurfsData *surfs,
                            uint32_t refreshFlags);
dobj_eval_storage_t *DObjGetEvaluationStorage(DObj *obj);
void DObjCalcAnim(DObj *obj, const uint32_t *partBits);

#endif
