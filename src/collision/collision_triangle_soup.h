#ifndef SHARED_COLLISION_TRIANGLE_SOUP_H
#define SHARED_COLLISION_TRIANGLE_SOUP_H

#include "qcommon/collision_map_types.h"
#include "qcommon/q_vector_types.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

collisionTriangleSoup_t *CM_GenerateTerrainCollide(int32_t indexCount, const int16_t *indices, uint32_t vertexCount, const vec3_t *vertices,
                                                   vec3_t bounds[2]);

#ifdef __cplusplus
}
#endif

#endif
