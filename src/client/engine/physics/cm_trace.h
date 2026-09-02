#ifndef CODUOMP_CM_TRACE_H
#define CODUOMP_CM_TRACE_H

#include "../q_shared.h"
#include "../surface_types.h"
#include "collision/collision_map_load.h"
#include "qcommon/collision_trace_work_types.h"
#include "collision/collision_leaf_queries.h"
#include "collision/collision_geometry.h"
#include "collision/collision_area.h"
#include "collision/collision_point_contents.h"
#include "collision/collision_queries.h"
#include "collision/collision_box_trace.h"
#include "collision/collision_brush_traces.h"
#include "collision/collision_capsule_traces.h"
#include "collision/collision_entity_traversal.h"
#include "collision/collision_trace_bounds.h"
#include "collision/collision_tree_traces.h"
#include "collision/collision_trace_entry.h"
#include "collision/collision_patch_dispatch.h"
#include "collision/collision_patch_trace.h"
#include "collision/collision_terrain_dispatch.h"
#include "collision/collision_terrain_trace.h"
#include "collision/collision_leaf_traces.h"
#include "collision/collision_world_sector.h"
#include "collision/collision_static_model_trace.h"
#include "collision/collision_server_trace.h"

struct worldSector_s;
struct worldSectorAreaLink_s;
struct svEntity_s;

#ifdef __cplusplus
extern "C" {
#endif

void CM_TransformedBoxTraceExternal(trace_t *trace, const vec3_t start, const vec3_t end, const vec3_t mins, const vec3_t maxs,
                                    int32_t modelHandle, int32_t contentMask, const vec3_t origin, const vec3_t angles, qboolean capsule);
void CM_TransformedBoxTrace(trace_t *trace, const vec3_t start, const vec3_t end, const vec3_t mins, const vec3_t maxs, int32_t modelHandle,
                            int32_t contentMask, const vec3_t origin, const vec3_t angles, qboolean capsule);

#ifdef __cplusplus
}
#endif

_Static_assert(_Alignof(lump_t) == 4, "original BSP-lump descriptor alignment changed");
_Static_assert(offsetof(lump_t, filelen) == 0x00, "original BSP-lump file length moved");
_Static_assert(offsetof(lump_t, fileofs) == 0x04, "original BSP-lump file offset moved");
_Static_assert(sizeof(lump_t) == 0x08, "original BSP-lump descriptor size changed");
_Static_assert(_Alignof(dnode_t) == 4, "original BSP node-record alignment changed");
_Static_assert(offsetof(dnode_t, planeNum) == 0x00, "original BSP node plane index moved");
_Static_assert(offsetof(dnode_t, children) == 0x04, "original BSP node children moved");
_Static_assert(offsetof(dnode_t, children[1]) == 0x08, "original BSP node second child moved");
_Static_assert(offsetof(dnode_t, mins) == 0x0c, "original BSP node mins moved");
_Static_assert(offsetof(dnode_t, maxs) == 0x18, "original BSP node maxs moved");
_Static_assert(sizeof(dnode_t) == 0x24, "original BSP node-record size changed");
_Static_assert(_Alignof(dplane_t) == 4, "original BSP plane-record alignment changed");
_Static_assert(offsetof(dplane_t, normal) == 0x00, "original BSP plane normal moved");
_Static_assert(offsetof(dplane_t, dist) == 0x0c, "original BSP plane distance moved");
_Static_assert(sizeof(dplane_t) == 0x10, "original BSP plane-record size changed");
_Static_assert(_Alignof(dmodel_t) == 4, "original BSP model-record alignment changed");
_Static_assert(offsetof(dmodel_t, mins) == 0x00, "original BSP model mins moved");
_Static_assert(offsetof(dmodel_t, maxs) == 0x0c, "original BSP model maxs moved");
_Static_assert(offsetof(dmodel_t, firstSurface) == 0x18, "original BSP model first surface moved");
_Static_assert(offsetof(dmodel_t, numSurfaces) == 0x1c, "original BSP model surface count moved");
_Static_assert(offsetof(dmodel_t, firstLeafSurface) == 0x20, "original BSP model first leaf surface moved");
_Static_assert(offsetof(dmodel_t, numLeafSurfaces) == 0x24, "original BSP model leaf surface count moved");
_Static_assert(offsetof(dmodel_t, firstLeafBrush) == 0x28, "original BSP model first leaf brush moved");
_Static_assert(offsetof(dmodel_t, numLeafBrushes) == 0x2c, "original BSP model leaf brush count moved");
_Static_assert(sizeof(dmodel_t) == 0x30, "original BSP model-record size changed");
_Static_assert(_Alignof(dbrush_t) == 2, "original BSP brush-record alignment changed");
_Static_assert(offsetof(dbrush_t, numSides) == 0x00, "original BSP brush side count moved");
_Static_assert(offsetof(dbrush_t, shaderNum) == 0x02, "original BSP brush material index moved");
_Static_assert(sizeof(dbrush_t) == 0x04, "original BSP brush-record size changed");
_Static_assert(_Alignof(dbrushside_t) == 4, "original BSP brush-side record alignment changed");
_Static_assert(offsetof(dbrushside_t, plane) == 0x00, "original BSP brush-side plane lane moved");
_Static_assert(offsetof(dbrushside_t, plane.dist) == 0x00, "original BSP axial distance moved");
_Static_assert(offsetof(dbrushside_t, plane.planeNum) == 0x00, "original BSP non-axial plane index moved");
_Static_assert(sizeof(((dbrushside_t *)0)->plane) == 0x04, "original BSP brush-side plane lane size changed");
_Static_assert(offsetof(dbrushside_t, shaderNum) == 0x04, "original BSP brush-side material index moved");
_Static_assert(sizeof(dbrushside_t) == 0x08, "original BSP brush-side record size changed");
_Static_assert(_Alignof(dleaf_t) == 4, "original BSP leaf-record alignment changed");
_Static_assert(offsetof(dleaf_t, cluster) == 0x00, "original BSP leaf cluster moved");
_Static_assert(offsetof(dleaf_t, area) == 0x04, "original BSP leaf area moved");
_Static_assert(offsetof(dleaf_t, firstLeafTerrainPatch) == 0x08, "original BSP leaf first terrain patch moved");
_Static_assert(offsetof(dleaf_t, numLeafTerrainPatches) == 0x0c, "original BSP leaf terrain-patch count moved");
_Static_assert(offsetof(dleaf_t, firstLeafBrush) == 0x10, "original BSP leaf first brush moved");
_Static_assert(offsetof(dleaf_t, numLeafBrushes) == 0x14, "original BSP leaf brush count moved");
_Static_assert(offsetof(dleaf_t, cellNum) == 0x18, "original BSP leaf cell number moved");
_Static_assert(offsetof(dleaf_t, firstLightIndex) == 0x1c, "original BSP leaf first light index moved");
_Static_assert(offsetof(dleaf_t, lightCount) == 0x20, "original BSP leaf light count moved");
_Static_assert(sizeof(dleaf_t) == 0x24, "original BSP leaf-record size changed");
_Static_assert(_Alignof(dterrainPatch_t) == 4, "original BSP terrain-patch alignment changed");
_Static_assert(offsetof(dterrainPatch_t, shaderNum) == 0x00, "original BSP terrain-patch material index moved");
_Static_assert(offsetof(dterrainPatch_t, collisionMode) == 0x02, "original BSP terrain-patch mode moved");
_Static_assert(offsetof(dterrainPatch_t, padding03) == 0x03, "original BSP terrain-patch padding byte moved");
_Static_assert(offsetof(dterrainPatch_t, data) == 0x04, "original BSP terrain-patch payload moved");
_Static_assert(offsetof(dterrainPatch_t, data.curve.width) == 0x04, "original BSP curve width moved");
_Static_assert(offsetof(dterrainPatch_t, data.curve.height) == 0x06, "original BSP curve height moved");
_Static_assert(offsetof(dterrainPatch_t, data.curve.maxError) == 0x08, "original BSP curve error moved");
_Static_assert(offsetof(dterrainPatch_t, data.curve.firstVert) == 0x0c, "original BSP curve first vertex moved");
_Static_assert(offsetof(dterrainPatch_t, data.terrain.numVerts) == 0x04, "original BSP terrain vertex count moved");
_Static_assert(offsetof(dterrainPatch_t, data.terrain.numIndexes) == 0x06, "original BSP terrain index count moved");
_Static_assert(offsetof(dterrainPatch_t, data.terrain.firstVert) == 0x08, "original BSP terrain first vertex moved");
_Static_assert(offsetof(dterrainPatch_t, data.terrain.firstIndex) == 0x0c, "original BSP terrain first index moved");
_Static_assert(sizeof(((dterrainPatch_t *)0)->data) == 0x0c, "original BSP terrain-patch payload size changed");
_Static_assert(sizeof(dterrainPatch_t) == 0x10, "original BSP terrain-patch record size changed");
_Static_assert(_Alignof(dvis_t) == 4, "original BSP visibility alignment changed");
_Static_assert(offsetof(dvis_t, numClusters) == 0x00, "original BSP visibility cluster count moved");
_Static_assert(offsetof(dvis_t, clusterBytes) == 0x04, "original BSP visibility cluster bytes moved");
_Static_assert(offsetof(dvis_t, data) == 0x08, "original BSP visibility data moved");
_Static_assert(sizeof(dvis_t) == 0x08, "original BSP visibility header size changed");

#endif
