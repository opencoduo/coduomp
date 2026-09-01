#ifndef CODUO_COLLISION_MAP_LOAD_H
#define CODUO_COLLISION_MAP_LOAD_H

#include "qcommon/bsp_types.h"
#include "qcommon/collision_map_types.h"
#include "qcommon/qcommon_runtime_types.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Complete collision-map loader state shared by the Windows client/listen
 * server and Linux dedicated engine.  The Windows addresses document the
 * original storage identity retained by the recovered client. */
extern uint8_t *cm_fileBase;                       /* CoDUOMP 0x04957f1c */
extern char cm_mapName[MAX_QPATH];                  /* CoDUOMP 0x0494ddc0 */
extern int32_t cm_checksum;                         /* CoDUOMP 0x008d09a8 */
extern int32_t cm_entityStringLength;               /* CoDUOMP 0x0494de60 */
extern int32_t cm_numMaterials;                     /* CoDUOMP 0x0494de08 */
extern dshader_t *cm_materials;                     /* CoDUOMP 0x0494de0c */
extern int32_t cm_numPlanes;                        /* CoDUOMP 0x0494de18 */
extern cplane_t *cm_planes;                         /* CoDUOMP 0x0494de1c */
extern int32_t cm_numNodes;                         /* CoDUOMP 0x0494de20 */
extern collisionNode_t *cm_nodes;                   /* CoDUOMP 0x0494de24 */
extern int32_t cm_numLeafs;                         /* CoDUOMP 0x0494de28 */
extern collisionLeaf_t *cm_leafs;                   /* CoDUOMP 0x0494de2c */
extern int32_t cm_numLeafBrushes;                   /* CoDUOMP 0x0494de30 */
extern int32_t *cm_leafbrushes;                     /* CoDUOMP 0x0494de34 */
extern int32_t cm_numLeafSurfaces;                  /* CoDUOMP 0x0494de38 */
extern int32_t *cm_leafsurfaces;                    /* CoDUOMP 0x0494de3c */
extern int32_t cm_numSubModels;                     /* CoDUOMP 0x0494de40 */
extern collisionModel_t *cm_models;                 /* CoDUOMP 0x0494de44 */
extern int32_t cm_numBrushes;                       /* CoDUOMP 0x0494de48 */
extern collisionBrush_t *cm_brushes;                /* CoDUOMP 0x0494de4c */
extern int32_t cm_numClusters;                      /* CoDUOMP 0x0494de50 */
extern int32_t cm_clusterBytes;                     /* CoDUOMP 0x0494de54 */
extern uint8_t *cm_visibility;                      /* CoDUOMP 0x0494de58 */
extern qboolean cm_visibilityLoaded;                /* CoDUOMP 0x0494de5c */
extern char *cm_entityString;                       /* CoDUOMP 0x0494de64 */
extern int32_t cm_numAreas;                         /* CoDUOMP 0x0494de68 */
extern collisionArea_t *cm_areas;                   /* CoDUOMP 0x0494de6c */
extern int32_t *cm_areaPortals;                     /* CoDUOMP 0x0494de70 */
extern int32_t cm_numTerrainPatches;                /* CoDUOMP 0x0494de74 */
extern collisionTerrainPatch_t *cm_terrainPatches;  /* CoDUOMP 0x0494de78 */
extern int32_t cm_floodValid;                       /* CoDUOMP 0x0494de7c */
/* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
extern int32_t cm_checkcount;                       /* CoDUOMP 0x0494de80 */
extern collisionBrush_t *cm_boxBrush;               /* CoDUOMP 0x0494de9c */
extern collisionModel_t cm_boxModel;                /* CoDUOMP 0x0494dea0 */
extern int32_t cm_numBrushSides;                    /* CoDUOMP 0x0494de10 */
extern collisionBrushSide_t *cm_brushSides;         /* CoDUOMP 0x0494de14 */
extern cvar_t *cm_noCurves;                         /* CoDUOMP 0x0494ddbc */
extern cvar_t *cm_playerCurveClip;                  /* CoDUOMP 0x04957f20 */

void CMod_LoadShaders(const lump_t *lump);
void CMod_LoadNodes(const lump_t *lump);
void CMod_LoadSubmodels(const lump_t *lump);
void CMod_LoadBrushes(const lump_t *brushLump,
                      const lump_t *brushSideLump);
void CMod_LoadLeafs(const lump_t *lump);
void CMod_LoadPlanes(const lump_t *lump);
void CMod_LoadLeafBrushes(const lump_t *lump);
void CMod_LoadLeafSurfaces(const lump_t *lump);
void CMod_LoadLeafCurvesAndTerrain(const lump_t *patchLump,
                                   const lump_t *vertexLump,
                                   const lump_t *indexLump);
void CMod_LoadEntityString(const lump_t *lump);
void CMod_LoadVisibility(const lump_t *lump);
void CM_LoadMap(const char *mapName, qboolean clientLoad,
                int32_t *checksum);
void CM_SaveLump(int32_t lumpIndex, const void *replacementData,
                 int32_t replacementLength, int32_t *checksum);
int32_t CM_LoadMapLump(int32_t lumpIndex, void **buffer);
void CM_FreeMapLump(void *buffer);

#ifdef __cplusplus
}
#endif

#endif
