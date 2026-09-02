#ifndef CODUO_COLLISION_QUERIES_H
#define CODUO_COLLISION_QUERIES_H

#include "qcommon/collision_map_types.h"

#include <stdint.h>

enum {
    CM_WORLD_MODEL = 0,
    CM_TEMP_CAPSULE_MODEL_HANDLE = 510,
    CM_TEMP_BOX_MODEL_HANDLE = 511,
    CM_MAX_CLIP_HANDLE = 512
};

#ifdef __cplusplus
extern "C" {
#endif

const collisionModel_t *CM_ClipHandleToModel(int32_t handle);
int32_t CM_InlineModel(int32_t modelNum);
int32_t CM_NumClusters(void);
int32_t CM_NumInlineModels(void);
char *CM_EntityString(void);
int32_t CM_LeafCluster(int32_t leafNum);
int32_t CM_LeafArea(int32_t leafNum);

void CM_InitBoxHull(void);
int32_t CM_TempBoxModel(const vec3_t mins, const vec3_t maxs, int32_t contents, qboolean capsule);
int32_t CM_TempBoxModelContents(void);
void CM_ModelBounds(int32_t modelHandle, vec3_t mins, vec3_t maxs);

uint32_t CM_SignbitsForNormal(const vec3_t normal);
int32_t CM_PointLeafnum_r(const vec3_t point, int32_t nodeNum);
int32_t CM_PointLeafnum(const vec3_t point);
const uint8_t *CM_ClusterPVS(int32_t cluster);
cplane_t *CM_PlaneForIndex(int32_t planeIndex);

#ifdef __cplusplus
}
#endif

#endif
