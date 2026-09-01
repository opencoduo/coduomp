#ifndef SHARED_ANIMATION_XMODEL_H
#define SHARED_ANIMATION_XMODEL_H

#include "qcommon/dobj_types.h"
#include "qcommon/q_shared_types.h"
#include "qcommon/xmodel_types.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void *(*xmodel_asset_alloc_fn)(size_t size);

void ReadBlend(XSurface *surface, XSimpleBlendInfo *blend,
               const uint8_t **position);
void XSurfaceUnstrip(const XStripInfo *strips,
                     XSurfaceTriangle *triangles);
void XModelReadSurface(XSurface *surface, const uint8_t **position,
                       xmodel_asset_alloc_fn alloc);
void XModelEnforceExist(qboolean enforce);
qboolean XModelBad(const XModel *model);
void XModelSetOptimize(qboolean enabled);
void XModelCreateDefaultSurface(XSurface *surface);
fileData_t *XModelCreateDefaultParts(void);
XModelSurfs *XModelCreateDefaultSurfs(void);
void XModelCreateDefault(XModel *model);
XModel *XModelCreateDefault_(void);
XModelSurfs *XModelSurfsCloneSurfs(const XModelSurfs *modelSurfs,
                                   xmodel_asset_alloc_fn alloc);
XModelSurfs *XModelSurfsPrecache(const char *name,
                                 xmodel_asset_alloc_fn alloc);
fileData_t *XModelPartsPrecache(const char *name,
                                xmodel_asset_alloc_fn alloc);
qboolean XModelExists(const char *name);
const uint8_t *XModelLoadConfigFile(const char *name,
                                    const uint8_t *cursor,
                                    XModelConfig *header);
const uint8_t *XModelLoadCollData(
    const uint8_t *cursor, XModelInfo *collision,
    xmodel_asset_alloc_fn alloc);
XModel *XModelPrecache(const char *name, xmodel_load_mode_t loadMode,
                       xmodel_asset_alloc_fn alloc,
                       xmodel_asset_alloc_fn optionalAlloc);
qboolean XModelGetStaticBounds(const XModel *model,
                               axis_t transform,
                               vec3_t mins, vec3_t maxs);

int16_t XModelLittleInt16(int16_t value);
uint32_t XModelLittleUInt32(uint32_t value);
float XModelLittleFloat(float value);
void XModelExpandQuatToAxis(float *quat);
const char *XModelGetName(const XModel *model);
int32_t XModelNumBones(const XModel *model);
const uint16_t *XModelBoneNames(const XModel *model);
int32_t XModelGetBoneIndex(const XModel *model, uint16_t partName);
void XModelGetBounds(const XModel *model, vec3_t mins, vec3_t maxs);
int32_t XModelGetSurfaces(const XModel *model, XSurface ***surfacesOut,
                          int32_t lodIndex);
const char *XModelGetSurfaceName(const XModel *model,
                                 int32_t surfaceIndex, int32_t lodIndex);
int32_t XModelGetContents(const XModel *model);
int32_t XModelGetNumLods(const XModel *model);
int16_t XModelGetModelFileCount(const XModel *model);
void XModelGetBasePose(const XModel *model, DObjSkelMat *basePose);
int32_t XModelTraceLine(const XModel *model, trace_t *trace,
                        const DObjSkelMat *basePose,
                        const vec3_t start, const vec3_t end,
                        int32_t contentsMask);
void XModelSetTestLods(int32_t lodIndex, float distance);
void XModelSetTestLodDist(float distance);
int32_t XModelGetLodForDist(const XModel *model, float distance);
void XModelSurfsFree(fileData_t *fileData);
void XModelPartsFree(fileData_t *fileData);
void XModelFree(fileData_t *fileData);
void XModelClearData(void *rangeStart, void *rangeEnd);

XSurface *XSurfaceCloneSurface(const XSurface *surface,
                               xmodel_asset_alloc_fn alloc);
int32_t XSurfaceGetNumVerts(const XSurface *surface);
int32_t XSurfaceGetNumTris(const XSurface *surface);
int32_t XSurfaceTileMode(const XSurface *surface);
void XSurfaceGetTris(const XSurface *surface,
                     XSurfaceTriangle *triangles, int16_t baseIndex);
uint8_t *XSurfaceGetBlendInfoArray(const XSurface *surface);
vec2_t *XSurfaceGetTexCoordArray(const XSurface *surface);
XSurfaceWeightedPoint *XSurfaceGetVertexInfoArray(
    const XSurface *surface);
int32_t XSurfaceGetBoneIndex(const XSurface *surface);
void XSurfaceRemapTextureCoordinates(XSurface *surface,
                                     const vec2_t scale,
                                     const vec2_t offset,
                                     int32_t sourceUIndex,
                                     int32_t sourceVIndex);
void XSurfaceGetVerts(const XSurface *surface,
                      const DObjSkelMat *basePose,
                      vec3_t *outVerts, vec2_t *outTexCoords,
                      vec3_t *outNormals);

/* The default assets and the surface-name fallback share this exact string
 * object in both retained engines. */
extern const char xmodel_defaultName[];
extern XModelInfo xmodel_defaultCollision;

#ifdef __cplusplus
}
#endif

#endif
