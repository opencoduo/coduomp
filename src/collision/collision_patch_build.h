#ifndef SHARED_COLLISION_PATCH_BUILD_H
#define SHARED_COLLISION_PATCH_BUILD_H

#include "qcommon/collision_map_types.h"
#include "qcommon/q_shared_types.h"
#include "qcommon/q_vector_types.h"
#include "winding.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

qboolean CM_PlaneEqual(const vec4_t left, const vec4_t right,
                       qboolean *flipped);
qboolean CM_PlaneFromPoints(vec4_t plane, const vec3_t point0,
                            const vec3_t point1, const vec3_t point2);
qboolean CM_NeedsSubdivision(const vec3_t point0,
                             const vec3_t point1,
                             const vec3_t point2,
                             int32_t maxError);
void CM_Subdivide(const vec3_t point0, const vec3_t point1,
                  const vec3_t point2, vec3_t midpoint01,
                  vec3_t center, vec3_t midpoint12);
void CM_SnapVector(vec3_t normal);
int32_t CM_FindPlane(const vec4_t plane, qboolean *flipped);
int32_t CM_FindPlane2(const vec3_t point0, const vec3_t point1,
                      const vec3_t point2);
int32_t CM_PointOnPlaneSide(const vec3_t point, int32_t planeIndex);
int32_t CM_GridPlane(const patchPlaneGrid_t planeGrid,
                     int32_t x, int32_t y, int32_t triangle);
int32_t CM_EdgePlaneNum(const cGrid_t *grid,
                        const patchPlaneGrid_t planeGrid,
                        int32_t x, int32_t y, int32_t edge);
void CM_SetBorderInward(facet_t *facet,
                        const cGrid_t *grid,
                        const patchPlaneGrid_t planeGrid,
                        int32_t x, int32_t y, int32_t mode);
qboolean CM_ValidateFacet(const facet_t *facet);
void CM_TransposeGrid(cGrid_t *grid);
void CM_SetGridWrapWidth(cGrid_t *grid);
void CM_SubdivideGridColumns(cGrid_t *grid, int32_t maxError);
qboolean CM_ComparePoints(const vec3_t left, const vec3_t right);
void CM_RemoveDegenerateColumns(cGrid_t *grid);
void CM_AddFacetBevels(facet_t *facet);
void CM_PatchCollideFromGrid(const cGrid_t *grid,
                             patchCollide_t *patchCollide);
patchCollide_t *CM_GeneratePatchCollide(uint32_t width, uint32_t height,
                                        int32_t maxError,
                                        const vec3_t *points,
                                        vec3_t bounds[2]);

#ifdef __cplusplus
}
#endif

#endif
