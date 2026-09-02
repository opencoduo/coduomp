#ifndef CODUO_COLLISION_SERVER_ENTITY_H
#define CODUO_COLLISION_SERVER_ENTITY_H

#include "qcommon/server_runtime_types.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

extern serverHeader_t sv;
extern vec3_t sv_defaultEntityClipMins;
extern vec3_t sv_defaultEntityClipMaxs;

int32_t SV_ClipHandleForEntity(const sharedEntity_t *entity);
void SV_SetBrushModel(sharedEntity_t *entity);
qboolean SV_EntityContact(const vec3_t mins, const vec3_t maxs, const sharedEntity_t *entity, qboolean capsule);
void SV_UnlinkEntity(sharedEntity_t *entity);
void SV_SnapVector(vec3_t vector);
void SV_LinkEntity(sharedEntity_t *entity);

#ifdef __cplusplus
}
#endif

#endif
