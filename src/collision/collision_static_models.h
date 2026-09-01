#ifndef CODUO_COLLISION_STATIC_MODELS_H
#define CODUO_COLLISION_STATIC_MODELS_H

#include "qcommon/server_runtime_types.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

extern int32_t cm_staticModelCount;
extern worldSectorAreaLink_t *cm_staticModels;

void *CM_Hunk_AllocXModelMesh(size_t size);
void *CM_Hunk_AllocXModel(size_t size);
void CM_CreateStaticModel(worldSectorAreaLink_t *areaLink,
                          const char *modelName,
                          const vec3_t origin,
                          const vec3_t angles,
                          const vec3_t scale);
void CM_LoadStaticModels(void);

#ifdef __cplusplus
}
#endif

#endif
