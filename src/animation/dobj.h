#ifndef SHARED_ANIMATION_DOBJ_H
#define SHARED_ANIMATION_DOBJ_H

#include "qcommon/dobj_types.h"
#include "qcommon/q_shared_types.h"
#include "qcommon/xanim_types.h"
#include "qcommon/xmodel_types.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

qboolean DObjSkelAreBonesUpToDate(const DObj *obj,
                                  const uint32_t *partBits);
qboolean DObjSkelIsBoneUpToDate(const DObj *obj, int32_t boneIndex);
int32_t DObjGetAllocSkelSize(const DObj *obj);
qboolean DObjRefreshEvalStorageKey(DObj *obj, int32_t key);
qboolean DObjSkelExists(const DObj *obj, int32_t key);
void DObjCreateSkel(DObj *obj, dobj_eval_storage_t *storage);
uint8_t DObjGetNumModels(const DObj *obj);
XModel *DObjGetModel(const DObj *obj, int32_t modelIndex);
int32_t DObjGetLodForDist(const DObj *obj, int32_t modelIndex,
                          float distance);
XAnimTree *DObjGetTree(const DObj *obj);
int32_t DObjNumBones(const DObj *obj);
DObjAnimMat *DObjGetRotTransArray(const DObj *obj);
void DObjGetBounds(const DObj *obj, vec3_t mins, vec3_t maxs);
qboolean DObjSetRotTransIndex(DObj *obj, const uint8_t *partBits,
                              int32_t boneIndex);
qboolean DObjSetControlRotTransIndex(DObj *obj,
                                     const uint8_t *partBits,
                                     int32_t boneIndex);
int32_t DObjFindPartIndex(const DObj *obj, uint16_t partName);
int32_t DObjGetBoneIndex(const DObj *obj, const char *tagName);
const char *DObjGetBoneName(const DObj *obj, int32_t boneIndex);
qboolean DObjHasContents(const DObj *obj, int32_t contentsMask);
int32_t DObjGetNumSurfaces(const DObj *obj,
                           const int32_t *lodIndices);
XSurface *DObjGetSurface(const DObj *obj, int32_t modelIndex,
                         int32_t surfaceIndex,
                         const int32_t *lodIndices);
const char *DObjGetSurfaceName(const DObj *obj, int32_t modelIndex,
                               int32_t surfaceIndex,
                               const int32_t *lodIndices);
void DObjGetSurfaces(const DObj *obj,
                     dobj_surface_ref_t *surfaceRefs,
                     uint32_t *partBits,
                     const int32_t *lodIndices);
DObjSkelMat *DObjGetMatrixArray(const DObj *obj, int32_t modelIndex);
void DObjGetBoneInfo(const DObj *obj, XModelPartColl **partCollisions);
qboolean DObjBad(const DObj *obj);
void DObjDumpInfo(const DObj *obj);
void DObjDisplayAnim(const DObj *obj);
qboolean DObjChildSkipped(const DObj *obj, uint8_t modelIndex);
void DObjInit(void);
void DObjShutdown(void);
void DObjCreate(const DObjModel *models, uint16_t modelCount,
                XAnimTree *runtimeTree, DObj *obj, uint16_t gameId);
void DObjFree(DObj *obj, qboolean releaseRuntimeTree);
void DObjBuildTracePartRemap(DObj *obj);
void DObjGetHierarchyBits(DObj *obj, int32_t boneIndex,
                          uint32_t *partBits);
void DObjCompleteHierarchyBits(DObj *obj, uint32_t *partBits);
void DObjCalcSkel(DObj *obj, const uint32_t *partBits);
void DObjTraceParts(const DObj *obj, const vec3_t start,
                    const vec3_t end, const uint8_t *partState,
                    dobj_trace_result_t *trace);
void DObjTraceModelParts(const DObj *obj, const vec3_t start,
                         const vec3_t end, int32_t contentsMask,
                         dobj_trace_result_t *trace);

extern int32_t dobj_skelCacheKey;
extern int32_t dobjLastAllocatedIndex;
extern uint16_t serverDObjHandleByEntity[DOBJ_SERVER_HANDLE_COUNT];
extern uint8_t dobjAllocState[DOBJ_ALLOC_STATE_BYTES];
extern DObj dobjPool[DOBJ_POOL_COUNT];
extern uint16_t clientDObjHandleByEntity[DOBJ_CLIENT_HANDLE_COUNT];
extern qboolean comDObjInitialized;

void Com_InitDObj(void);
void Com_ShutdownDObj(void);
int32_t Com_GetFreeDObjIndex(void);
void Com_ClientDObjCreate(const DObjModel *models, uint16_t modelCount,
                          XAnimTree *runtimeTree,
                          int32_t entityNum, uint16_t gameId);
void Com_ServerDObjCreate(const DObjModel *models, uint16_t modelCount,
                          XAnimTree *runtimeTree,
                          int32_t entityNum, uint16_t gameId);
void Com_SafeClientDObjFree(int32_t entityNum,
                            qboolean releaseRuntimeTree);
void Com_SafeServerDObjFree(int32_t entityNum,
                            qboolean releaseRuntimeTree);
void Com_MirrorServerDObjsToClient(void);
DObj *Com_GetClientDObj(int32_t entityNum);
DObj *Com_GetServerDObj(int32_t entityNum);

extern uint16_t xanimDefaultPartRemapHandle;

#ifdef __cplusplus
}
#endif

#endif
