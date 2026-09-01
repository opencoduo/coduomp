#include "collision_map_load.h"

#include "collision_area.h"
#include "collision_map_load_services.h"
#include "collision_patch_build.h"
#include "collision_queries.h"
#include "collision_static_models.h"
#include "collision_triangle_soup.h"
#include "collision_world_sector.h"
#include "filesystem/filesystem.h"
#include "qcommon/hunk.h"
#include "qcommon/bsp_validation.h"
#include "qcommon/q_checksum.h"
#include "qcommon/q_cvar.h"
#include "qcommon/q_memory.h"
#include "qcommon/q_string.h"
#include "q_temp_error_binding.h"

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

enum {
    CM_LOAD_HUNK_ALIGNMENT = 32,
    CM_MAP_NAME_SIZE = 64,
    CM_BSP_LUMP_ALIGNMENT = 4,
    CM_WORLD_MODEL_INDEX = 0,
    CM_BRUSH_AXIAL_SIDE_COUNT = 6,
    CM_MAX_TERRAIN_VERTICES = 65536,
    CM_MAX_TERRAIN_INDICES = 393216,
    CM_LOADING_KEEPALIVE_MASK = 3,
    CM_VISIBILITY_ALIGNMENT = 32
};

_Static_assert((int32_t)CM_MAP_NAME_SIZE == (int32_t)MAX_QPATH,
               "collision map-name capacity diverged from MAX_QPATH");
_Static_assert(_Alignof(dheader_t) == 4,
               "collision BSP header alignment changed");
_Static_assert(offsetof(dheader_t, ident) == 0x00,
               "collision BSP header ident moved");
_Static_assert(offsetof(dheader_t, version) == 0x04,
               "collision BSP header version moved");
_Static_assert(offsetof(dheader_t, lumps) == 0x08,
               "collision BSP header lump table moved");
_Static_assert(offsetof(dheader_t,
                        lumps[HEADER_LUMPS - 1]) == 0x108,
               "collision BSP header last lump moved");
_Static_assert(sizeof(dheader_t) == 0x110,
               "collision BSP header size changed");

#define CM_BRUSH_CONTENTS_MASK UINT32_C(0xdfff7ffb)

void Com_DPrintf(const char *format, ...);
void Sys_OutOfMemory(void);

/* Temporary map-lump owner shared by CM_LoadMapLump and CM_FreeMapLump.
 * The original retained this otherwise-unreferenced pair in the Windows
 * executable at 0x0041d6b0 and 0x0041d780. */
static void *cm_loadedMapLump; /* original 0x008d09a4 */

/* NOT_FROM_ORIGINAL_SOURCE: security validation for signed first/count BSP
 * spans before the original loaders narrow or publish them. */
static qboolean coduo_compat_collision_span_is_valid(
    int32_t first, int32_t count, uint32_t limit)
{
    if (first < 0 || count < 0)
        return qfalse;

    const uint32_t unsignedFirst = (uint32_t)first;
    const uint32_t unsignedCount = (uint32_t)count;
    return unsignedFirst <= limit && unsignedCount <= limit - unsignedFirst
               ? qtrue
               : qfalse;
}

/* NOT_FROM_ORIGINAL_SOURCE: validate every collision-owned BSP reference
 * before the original loader graph converts one into a pointer, allocation
 * cursor, or narrowed runtime span. Reject partial records here as a necessary
 * precondition for inspecting the remaining collision relationships. */
static qboolean coduo_compat_collision_bsp_references_are_valid(
    const uint8_t *fileBase, const dheader_t *header)
{
    const lump_t *const shaderLump = &header->lumps[BSP_LUMP_SHADERS];
    const lump_t *const planeLump = &header->lumps[BSP_LUMP_PLANES];
    const lump_t *const brushSideLump =
        &header->lumps[BSP_LUMP_BRUSH_SIDES];
    const lump_t *const brushLump = &header->lumps[BSP_LUMP_BRUSHES];
    const lump_t *const nodeLump = &header->lumps[BSP_LUMP_NODES];
    const lump_t *const leafLump = &header->lumps[BSP_LUMP_LEAFS];
    const lump_t *const leafBrushLump =
        &header->lumps[BSP_LUMP_LEAF_BRUSHES];
    const lump_t *const leafSurfaceLump =
        &header->lumps[BSP_LUMP_LEAF_SURFACES];
    const lump_t *const patchLump =
        &header->lumps[BSP_LUMP_TERRAIN_PATCHES];
    const lump_t *const vertexLump =
        &header->lumps[BSP_LUMP_TERRAIN_VERTICES];
    const lump_t *const indexLump =
        &header->lumps[BSP_LUMP_TERRAIN_INDICES];
    const lump_t *const modelLump = &header->lumps[BSP_LUMP_MODELS];

    if ((uint32_t)shaderLump->filelen % sizeof(dshader_t) != 0 ||
        (uint32_t)planeLump->filelen % sizeof(dplane_t) != 0 ||
        (uint32_t)brushSideLump->filelen % sizeof(dbrushside_t) != 0 ||
        (uint32_t)brushLump->filelen % sizeof(dbrush_t) != 0 ||
        (uint32_t)nodeLump->filelen % sizeof(dnode_t) != 0 ||
        (uint32_t)leafLump->filelen % sizeof(dleaf_t) != 0 ||
        (uint32_t)leafBrushLump->filelen % sizeof(int32_t) != 0 ||
        (uint32_t)leafSurfaceLump->filelen % sizeof(int32_t) != 0 ||
        (uint32_t)patchLump->filelen % sizeof(dterrainPatch_t) != 0 ||
        (uint32_t)vertexLump->filelen % sizeof(vec3_t) != 0 ||
        (uint32_t)indexLump->filelen % sizeof(int16_t) != 0 ||
        (uint32_t)modelLump->filelen % sizeof(dmodel_t) != 0) {
        return qfalse;
    }

    const uint32_t materialCount =
        (uint32_t)shaderLump->filelen / sizeof(dshader_t);
    const uint32_t planeCount =
        (uint32_t)planeLump->filelen / sizeof(dplane_t);
    const uint32_t brushSideCount =
        (uint32_t)brushSideLump->filelen / sizeof(dbrushside_t);
    const uint32_t brushCount =
        (uint32_t)brushLump->filelen / sizeof(dbrush_t);
    const uint32_t nodeCount =
        (uint32_t)nodeLump->filelen / sizeof(dnode_t);
    const uint32_t leafCount =
        (uint32_t)leafLump->filelen / sizeof(dleaf_t);
    const uint32_t leafBrushCount =
        (uint32_t)leafBrushLump->filelen / sizeof(int32_t);
    const uint32_t leafSurfaceCount =
        (uint32_t)leafSurfaceLump->filelen / sizeof(int32_t);
    const uint32_t patchCount =
        (uint32_t)patchLump->filelen / sizeof(dterrainPatch_t);
    const uint32_t vertexCount =
        (uint32_t)vertexLump->filelen / sizeof(vec3_t);
    const uint32_t indexCount =
        (uint32_t)indexLump->filelen / sizeof(int16_t);
    const uint32_t modelCount =
        (uint32_t)modelLump->filelen / sizeof(dmodel_t);

    const dbrush_t *const brushes =
        (const dbrush_t *)(fileBase + brushLump->fileofs);
    const dbrushside_t *const brushSides =
        (const dbrushside_t *)(fileBase + brushSideLump->fileofs);
    uint32_t sideCursor = 0;
    for (uint32_t brushIndex = 0; brushIndex < brushCount; ++brushIndex) {
        const int32_t sideCount = brushes[brushIndex].numSides;
        const int32_t materialIndex = brushes[brushIndex].shaderNum;
        if (sideCount < CM_BRUSH_AXIAL_SIDE_COUNT ||
            materialIndex < 0 || (uint32_t)materialIndex >= materialCount ||
            sideCursor > brushSideCount ||
            (uint32_t)sideCount > brushSideCount - sideCursor) {
            return qfalse;
        }

        for (int32_t sideIndex = 0; sideIndex < sideCount; ++sideIndex) {
            const dbrushside_t *const side =
                &brushSides[sideCursor + (uint32_t)sideIndex];
            if (side->shaderNum < 0 ||
                (uint32_t)side->shaderNum >= materialCount) {
                return qfalse;
            }
            if (sideIndex >= CM_BRUSH_AXIAL_SIDE_COUNT &&
                (side->plane.planeNum < 0 ||
                 (uint32_t)side->plane.planeNum >= planeCount)) {
                return qfalse;
            }
        }
        sideCursor += (uint32_t)sideCount;
    }

    const dnode_t *const nodes =
        (const dnode_t *)(fileBase + nodeLump->fileofs);
    for (uint32_t nodeIndex = 0; nodeIndex < nodeCount; ++nodeIndex) {
        if (nodes[nodeIndex].planeNum < 0 ||
            (uint32_t)nodes[nodeIndex].planeNum >= planeCount) {
            return qfalse;
        }
        for (int32_t childIndex = 0; childIndex < 2; ++childIndex) {
            const int32_t child = nodes[nodeIndex].children[childIndex];
            if ((child >= 0 && (uint32_t)child >= nodeCount) ||
                (child < 0 && child < -(int32_t)leafCount)) {
                return qfalse;
            }
        }
    }

    const dleaf_t *const leafs =
        (const dleaf_t *)(fileBase + leafLump->fileofs);
    const int32_t *const leafBrushes =
        (const int32_t *)(fileBase + leafBrushLump->fileofs);
    const int32_t *const leafSurfaces =
        (const int32_t *)(fileBase + leafSurfaceLump->fileofs);
    for (uint32_t index = 0; index < leafBrushCount; ++index) {
        if (leafBrushes[index] < 0 ||
            (uint32_t)leafBrushes[index] >= brushCount) {
            return qfalse;
        }
    }
    for (uint32_t index = 0; index < leafSurfaceCount; ++index) {
        if (leafSurfaces[index] < 0 ||
            (uint32_t)leafSurfaces[index] >= patchCount) {
            return qfalse;
        }
    }

    for (uint32_t leafIndex = 0; leafIndex < leafCount; ++leafIndex) {
        const dleaf_t *const leaf = &leafs[leafIndex];
        if (coduo_compat_collision_span_is_valid(
                leaf->firstLeafBrush, leaf->numLeafBrushes,
                leafBrushCount) == qfalse ||
            coduo_compat_collision_span_is_valid(
                leaf->firstLeafTerrainPatch,
                leaf->numLeafTerrainPatches,
                leafSurfaceCount) == qfalse) {
            return qfalse;
        }
    }

    const dterrainPatch_t *const patches =
        (const dterrainPatch_t *)(fileBase + patchLump->fileofs);
    const int16_t *const terrainIndices =
        (const int16_t *)(fileBase + indexLump->fileofs);
    for (uint32_t patchIndex = 0; patchIndex < patchCount; ++patchIndex) {
        const dterrainPatch_t *const patch = &patches[patchIndex];
        if (patch->shaderNum < 0 ||
            (uint32_t)patch->shaderNum >= materialCount) {
            return qfalse;
        }

        if (patch->collisionMode == 0) {
            const int32_t width = patch->data.curve.width;
            const int32_t height = patch->data.curve.height;
            if (width <= 2 || height <= 2 ||
                width > CM_PATCH_POINT_GRID_SIZE ||
                height > CM_PATCH_POINT_GRID_SIZE ||
                (width & 1) == 0 || (height & 1) == 0) {
                return qfalse;
            }
            const uint32_t pointCount = (uint32_t)width * (uint32_t)height;
            const uint32_t firstVertex = patch->data.curve.firstVert;
            if (firstVertex > vertexCount ||
                pointCount > vertexCount - firstVertex) {
                return qfalse;
            }
        } else {
            const int32_t patchVertexCount = patch->data.terrain.numVerts;
            const int32_t patchIndexCount = patch->data.terrain.numIndexes;
            const uint32_t firstVertex = patch->data.terrain.firstVert;
            const uint32_t firstIndex = patch->data.terrain.firstIndex;
            if (patchVertexCount < 0 || patchIndexCount < 0 ||
                patchIndexCount % CM_TRIANGLE_VERTEX_COUNT != 0 ||
                firstVertex > vertexCount ||
                (uint32_t)patchVertexCount > vertexCount - firstVertex ||
                firstIndex > indexCount ||
                (uint32_t)patchIndexCount > indexCount - firstIndex) {
                return qfalse;
            }
            for (int32_t index = 0; index < patchIndexCount; ++index) {
                const int16_t vertexIndex =
                    terrainIndices[firstIndex + (uint32_t)index];
                if (vertexIndex < 0 ||
                    (uint32_t)vertexIndex >= (uint32_t)patchVertexCount) {
                    return qfalse;
                }
            }
        }
    }

    const dmodel_t *const models =
        (const dmodel_t *)(fileBase + modelLump->fileofs);
    for (uint32_t modelIndex = 1; modelIndex < modelCount; ++modelIndex) {
        if (coduo_compat_collision_span_is_valid(
                models[modelIndex].firstLeafBrush,
                models[modelIndex].numLeafBrushes,
                brushCount) == qfalse ||
            coduo_compat_collision_span_is_valid(
                models[modelIndex].firstLeafSurface,
                models[modelIndex].numLeafSurfaces,
                leafSurfaceCount) == qfalse) {
            return qfalse;
        }
    }

    return qtrue;
}

/*
 * Complete collision-map/BSP loader shared by the Windows client/listen
 * server and Linux dedicated engine. Direct comparison of CoDUOMP.exe
 * 0x0041c400..0x0041d7d1 with coduo_lnxded
 * 0x0804a06c..0x0804bc57 proves the same fifteen-function loader graph.
 * Linux's intervening CM_Little* calls are identity operations on its
 * authoritative i386 target, and its Hunk_AllocInternal resolves to the same
 * 32-byte aligned allocation used explicitly by Windows. The platform-owned
 * loading keepalive and optional diagnostics are bound at the source edge.
 */

/* Source: CoDUOMP.exe 0x0041d180..0x0041d3f2.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0041d180_0041d3f3.mcode.
 * Name and signature: exact same-module Mac symbol CM_LoadMap. The Windows
 * body proves the cvar registration and cache path, complete collision-state
 * reset, 33-lump/version-59 BSP header, loader order, entity-string copy, and
 * final collision/world initialization order. The original clears one
 * contiguous i386 global block; maintained source clears its typed,
 * pointer-width-independent globals individually. */
void CM_LoadMap(const char *mapName, qboolean clientLoad,
                int32_t *checksum)
{
    char localMapName[CM_MAP_NAME_SIZE];
    void *fileBuffer = NULL;
    int32_t fileLength;
    dheader_t header;

    if (mapName == NULL || mapName[0] == '\0') {
        Com_Error(
            ERR_DROP,
            "\x15" "CM_LoadMap: NULL name");
    }

    strncpy(localMapName, mapName,
            sizeof(localMapName) - 1U);
    localMapName[sizeof(localMapName) - 1U] = '\0';

    cm_noCurves = Cvar_Get(
        "cm_noCurves", "0", CVAR_CHEAT);
    cm_playerCurveClip = Cvar_Get(
        "cm_playerCurveClip", "1",
        CVAR_ARCHIVE | CVAR_CHEAT);

    Com_DPrintf(
        "CM_LoadMap( %s, %i )\n",
        localMapName, clientLoad);

    if (clientLoad != qfalse &&
        sv_running->integer != 0) {
        *checksum = cm_checksum;
        return;
    }

    COLLISION_MAP_LOAD_DIAGNOSTICS_BEGIN(localMapName);

    Com_Memset(cm_mapName, 0, sizeof(cm_mapName));
    cm_staticModelCount = 0;
    cm_staticModels = NULL;
    cm_numMaterials = 0;
    cm_materials = NULL;
    cm_numBrushSides = 0;
    cm_brushSides = NULL;
    cm_numPlanes = 0;
    cm_planes = NULL;
    cm_numNodes = 0;
    cm_nodes = NULL;
    cm_numLeafs = 0;
    cm_leafs = NULL;
    cm_numLeafBrushes = 0;
    cm_leafbrushes = NULL;
    cm_numLeafSurfaces = 0;
    cm_leafsurfaces = NULL;
    cm_numSubModels = 0;
    cm_models = NULL;
    cm_numBrushes = 0;
    cm_brushes = NULL;
    cm_numClusters = 0;
    cm_clusterBytes = 0;
    cm_visibility = NULL;
    cm_visibilityLoaded = qfalse;
    cm_entityStringLength = 0;
    cm_entityString = NULL;
    cm_numAreas = 0;
    cm_areas = NULL;
    cm_areaPortals = NULL;
    cm_numTerrainPatches = 0;
    cm_terrainPatches = NULL;
    cm_floodValid = 0;
    cm_checkcount = 0;
    Com_Memset(cm_worldMins, 0, sizeof(cm_worldMins));
    Com_Memset(cm_worldMaxs, 0, sizeof(cm_worldMaxs));
    cm_boxBrush = NULL;
    Com_Memset(&cm_boxModel, 0, sizeof(cm_boxModel));
    Com_Memset(
        &cm_worldSectorRoot, 0,
        sizeof(cm_worldSectorRoot));
    cm_freeWorldSectors = NULL;
    Com_Memset(
        &cm_nullWorldSector, 0,
        sizeof(cm_nullWorldSector));
    Com_Memset(
        cm_worldSectorPool, 0,
        sizeof(cm_worldSectorPool));

    fileLength = FS_ReadFile(
        localMapName, &fileBuffer);
    if (fileBuffer == NULL) {
        /* NOT_FROM_ORIGINAL_SOURCE: keep the completed map path as data through
         * the single variadic formatting pass. */
        Com_Error(ERR_DROP, "EXE_ERR_COULDNT_LOAD\x15%s", localMapName);
    }

    /* NOT_FROM_ORIGINAL_SOURCE: validate the header and every nonempty lump
     * range against the loaded file before dereferencing or dispatching it. */
    const int32_t invalidLump =
        coduo_compat_bsp_invalid_lump_index(fileBuffer, fileLength);
    if (invalidLump == CODUO_BSP_VALIDATION_SHORT_HEADER) {
        Com_Error(ERR_DROP, "CM_LoadMap: %s has a truncated BSP header",
                  localMapName);
    }
    if (invalidLump >= 0) {
        Com_Error(ERR_DROP, "CM_LoadMap: %s has invalid BSP lump %i",
                  localMapName, invalidLump);
    }

    cm_checksum = (int32_t)Com_BlockChecksum(fileBuffer, fileLength);
    *checksum = cm_checksum;

    Com_Memcpy(&header, fileBuffer, sizeof(header));
    /* The original little-endian build retains an empty counted loop here;
     * no byte swapping is performed on Windows. */
    if (header.version != BSP_VERSION) {
        /* NOT_FROM_ORIGINAL_SOURCE: keep the completed map diagnostic as data
         * through the single variadic formatting pass. */
        Com_Error(ERR_DROP,
                  "EXE_ERR_WRONG_MAP_VERSION_NUM\x15%s\x15(%i "
                  "\x14" "EXE_ERR_SHOULD_BE\x15 %i)",
                  localMapName, header.version, BSP_VERSION);
    }

    cm_fileBase = fileBuffer;
    /* NOT_FROM_ORIGINAL_SOURCE: validate collision cross-references and spans
     * before the first pointer conversion or collision allocation. */
    if (coduo_compat_collision_bsp_references_are_valid(
            cm_fileBase, &header) == qfalse) {
        Com_Error(ERR_DROP, "CM_LoadMap: %s has invalid collision references",
                  localMapName);
    }
    CMod_LoadShaders(
        &header.lumps[BSP_LUMP_SHADERS]);
    CMod_LoadPlanes(
        &header.lumps[BSP_LUMP_PLANES]);
    CMod_LoadBrushes(
        &header.lumps[BSP_LUMP_BRUSHES],
        &header.lumps[BSP_LUMP_BRUSH_SIDES]);
    CMod_LoadNodes(
        &header.lumps[BSP_LUMP_NODES]);
    CMod_LoadLeafs(
        &header.lumps[BSP_LUMP_LEAFS]);
    CMod_LoadLeafBrushes(
        &header.lumps[BSP_LUMP_LEAF_BRUSHES]);
    CMod_LoadLeafSurfaces(
        &header.lumps[BSP_LUMP_LEAF_SURFACES]);
    CMod_LoadLeafCurvesAndTerrain(
        &header.lumps[BSP_LUMP_TERRAIN_PATCHES],
        &header.lumps[BSP_LUMP_TERRAIN_VERTICES],
        &header.lumps[BSP_LUMP_TERRAIN_INDICES]);
    CMod_LoadSubmodels(
        &header.lumps[BSP_LUMP_MODELS]);
    CMod_LoadVisibility(
        &header.lumps[BSP_LUMP_VISIBILITY]);

    CMod_LoadEntityString(
        &header.lumps[BSP_LUMP_ENTITIES]);

    FS_FreeFile(fileBuffer);
    CM_InitBoxHull();
    CM_FloodAreaConnections();
    CM_InitWorldSector();
    CM_LoadStaticModels();

    if (clientLoad == qfalse) {
        strncpy(cm_mapName, localMapName,
                sizeof(cm_mapName) - 1U);
        cm_mapName[sizeof(cm_mapName) - 1U] = '\0';
    }

    COLLISION_MAP_LOAD_DIAGNOSTICS_END(localMapName);
}

/* Source: CoDUOMP.exe 0x0041d400..0x0041d6a5.
 * Evidence: repaired executable-gap record
 * coduomp/mcode/CoDUOMP/FUN_0041d400_0041d6a6.mcode.
 * Name: exact same-module Mac symbol CM_SaveLump. The renderer import-table
 * call at 0x004c8224 proves the four arguments. The function rebuilds all 33
 * BSP lumps with four-byte alignment, temporarily clears a loose map file's
 * read-only bit, writes the replacement image, and optionally returns its
 * checksum. */
void CM_SaveLump(int32_t lumpIndex, const void *replacementData,
                 int32_t replacementLength, int32_t *checksum)
{
    void *fileBuffer = NULL;
    dheader_t header;
    size_t newFileLength = sizeof(header);

    const int32_t fileLength = FS_ReadFile(cm_mapName, &fileBuffer);
    if (fileBuffer == NULL) {
        /* NOT_FROM_ORIGINAL_SOURCE: keep the completed map path as data. */
        Com_Error(ERR_DROP, "EXE_ERR_COULDNT_LOAD\x15%s", cm_mapName);
    }

    /* NOT_FROM_ORIGINAL_SOURCE: validate the source header, retained ranges,
     * and replacement extent before indexing, summing, allocating, or copying. */
    const int32_t invalidLump =
        coduo_compat_bsp_invalid_lump_index(fileBuffer, fileLength);
    if (invalidLump == CODUO_BSP_VALIDATION_SHORT_HEADER) {
        Com_Error(ERR_DROP, "CM_SaveLump: %s has a truncated BSP header",
                  cm_mapName);
    }
    if (invalidLump >= 0) {
        Com_Error(ERR_DROP, "CM_SaveLump: %s has invalid BSP lump %i",
                  cm_mapName, invalidLump);
    }
    if (lumpIndex < 0 || lumpIndex >= HEADER_LUMPS ||
        replacementLength < 0 ||
        (replacementLength != 0 && replacementData == NULL)) {
        Com_Error(ERR_DROP, "CM_SaveLump: invalid replacement lump");
    }

    Com_Memcpy(&header, fileBuffer, sizeof(header));
    for (int32_t index = 0;
         index < HEADER_LUMPS;
         ++index) {
        if (index == lumpIndex)
            header.lumps[index].filelen = replacementLength;

        const size_t lumpLength =
            (size_t)(uint32_t)header.lumps[index].filelen;
        const size_t alignedLength =
            (lumpLength + (CM_BSP_LUMP_ALIGNMENT - 1U)) &
            ~(size_t)(CM_BSP_LUMP_ALIGNMENT - 1U);
        if (alignedLength > (size_t)INT32_MAX - newFileLength) {
            Com_Error(ERR_DROP, "CM_SaveLump: rebuilt BSP is too large");
        }
        newFileLength += alignedLength;
    }

    uint8_t *const newFile = malloc(newFileLength);
    if (newFile == NULL)
        Sys_OutOfMemory();
    Com_Memset(newFile, 0, newFileLength);

    size_t cursor = sizeof(header);
    for (int32_t index = 0;
         index < HEADER_LUMPS;
         ++index) {
        const int32_t lumpLength =
            header.lumps[index].filelen;
        if (lumpLength != 0) {
            const void *const source =
                index == lumpIndex
                    ? replacementData
                    : (const uint8_t *)fileBuffer +
                          header.lumps[index].fileofs;
            Com_Memcpy(
                newFile + cursor, source,
                (size_t)(uint32_t)lumpLength);
        }

        header.lumps[index].fileofs = (int32_t)cursor;
        cursor +=
            ((size_t)(uint32_t)lumpLength +
             (CM_BSP_LUMP_ALIGNMENT - 1U)) &
            ~(size_t)(CM_BSP_LUMP_ALIGNMENT - 1U);
    }

    Com_Memcpy(newFile, &header, sizeof(header));

    const qboolean restoreReadOnly =
        FS_MakeReadOnly(cm_mapName, qfalse);
    FS_WriteFile(cm_mapName, newFile, (int32_t)cursor);
    if (restoreReadOnly != qfalse)
        (void)FS_MakeReadOnly(cm_mapName, qtrue);

    if (checksum != NULL)
        *checksum = (int32_t)Com_BlockChecksum(newFile, (int32_t)cursor);

    free(newFile);
    FS_FreeFile(fileBuffer);
}

/* Source: CoDUOMP.exe 0x0041d6b0..0x0041d77a.
 * Evidence: repaired executable-gap record
 * coduomp/mcode/CoDUOMP/FUN_0041d6b0_0041d77b.mcode.
 * Name and conventional signature: matching recovered Linux engine
 * CM_LoadMapLump. The retained Windows body has no live xrefs and uses an
 * LTCG register assignment for lumpIndex, but proves the header read, relative
 * seek, temporary-hunk allocation, published buffer, and returned byte count. */
int32_t CM_LoadMapLump(int32_t lumpIndex, void **buffer)
{
    int32_t fileHandle;
    dheader_t header;

    const int32_t fileLength = FS_FOpenFileRead(
        cm_mapName, &fileHandle, qfalse);
    if (fileHandle == 0) {
        /* NOT_FROM_ORIGINAL_SOURCE: keep the completed map path as data. */
        Com_Error(ERR_DROP, "EXE_ERR_COULDNT_LOAD\x15%s", cm_mapName);
    }

    /* NOT_FROM_ORIGINAL_SOURCE: validate the selected lump, header, seek, and
     * data-read results before allocation or publication. */
    if (lumpIndex < 0 || lumpIndex >= HEADER_LUMPS ||
        fileLength < (int32_t)sizeof(header) ||
        FS_Read(&header, (int32_t)sizeof(header), fileHandle) !=
            (int32_t)sizeof(header)) {
        FS_FCloseFile(fileHandle);
        Com_Error(ERR_DROP, "CM_LoadMapLump: invalid BSP header or lump index");
    }

    const int32_t invalidLump =
        coduo_compat_bsp_invalid_lump_index(&header, fileLength);
    if (invalidLump != CODUO_BSP_VALIDATION_VALID) {
        FS_FCloseFile(fileHandle);
        Com_Error(ERR_DROP, "CM_LoadMapLump: invalid BSP lump %i", invalidLump);
    }

    const int32_t lumpLength =
        header.lumps[lumpIndex].filelen;
    if (lumpLength == 0) {
        FS_FCloseFile(fileHandle);
        return 0;
    }

    const int32_t relativeOffset =
        header.lumps[lumpIndex].fileofs -
        (int32_t)sizeof(header);
    if (FS_Seek(fileHandle, relativeOffset,
                FS_SEEK_ORIGIN_CURRENT) != 0) {
        FS_FCloseFile(fileHandle);
        Com_Error(ERR_DROP, "CM_LoadMapLump: could not seek to BSP lump %i",
                  lumpIndex);
    }

    cm_loadedMapLump = Hunk_AllocateTempMemoryInternal(
        (size_t)(uint32_t)lumpLength);
    if (FS_Read(cm_loadedMapLump, lumpLength, fileHandle) != lumpLength) {
        Hunk_FreeTempMemory(cm_loadedMapLump);
        cm_loadedMapLump = NULL;
        FS_FCloseFile(fileHandle);
        Com_Error(ERR_DROP, "CM_LoadMapLump: short read for BSP lump %i",
                  lumpIndex);
    }
    FS_FCloseFile(fileHandle);
    *buffer = cm_loadedMapLump;
    return lumpLength;
}

/* Source: CoDUOMP.exe 0x0041d780..0x0041d7d1.
 * Evidence: repaired executable-gap record
 * coduomp/mcode/CoDUOMP/FUN_0041d780_0041d7d2.mcode.
 * Name and ignored public argument: matching recovered Linux engine
 * CM_FreeMapLump. Windows proves that the saved allocation, rather than the
 * caller's argument, is passed to Hunk_FreeTempMemory. */
void CM_FreeMapLump(void *buffer)
{
    (void)buffer;
    Hunk_FreeTempMemory(cm_loadedMapLump);
}

/* Source: CoDUOMP.exe 0x0041c400..0x0041c47d.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0041c400_0041c47e.mcode.
 * Name and lump argument: exact same-module Mac symbol CMod_LoadShaders.
 * Windows proves the 72-byte disk/runtime record identity, one-record
 * allocation prefix, 32-byte hunk alignment, and direct byte copy. */
void CMod_LoadShaders(const lump_t *lump)
{
    const uint32_t fileLength =
        (uint32_t)lump->filelen;
    const uint32_t materialSize =
        (uint32_t)sizeof(dshader_t);
    const uint32_t materialCount =
        fileLength / materialSize;
    const size_t materialBytes =
        (size_t)materialCount * sizeof(dshader_t);
    dshader_t *allocation;

    if (fileLength % materialSize != 0) {
        Com_Error(
            ERR_DROP,
            "\x15"
            "CMod_LoadShaders: funny lump size");
    }
    if (materialCount < 1) {
        Com_Error(
            ERR_DROP,
            "\x15"
            "Map with no shaders");
    }

    allocation = Hunk_AllocAlignInternal(
        materialBytes + sizeof(dshader_t),
        CM_LOAD_HUNK_ALIGNMENT);
    cm_materials = allocation + 1;
    cm_numMaterials = (int32_t)materialCount;
    Com_Memcpy(
        cm_materials,
        cm_fileBase + (uint32_t)lump->fileofs,
        materialBytes);
}

/* Source: CoDUOMP.exe 0x0041c620..0x0041c704.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0041c620_0041c705.mcode.
 * Name and lump argument: exact same-module Mac symbol CMod_LoadNodes.
 * Windows proves the 36-byte disk stride, plane-index scaling, two signed
 * 16-bit runtime children, overflow diagnostics, and 32-byte allocation
 * alignment. */
void CMod_LoadNodes(const lump_t *lump)
{
    const uint32_t fileLength =
        (uint32_t)lump->filelen;
    const uint32_t diskNodeSize =
        (uint32_t)sizeof(dnode_t);
    const uint32_t nodeCount =
        fileLength / diskNodeSize;
    const dnode_t *diskNode =
        (const dnode_t *)(
            cm_fileBase + (uint32_t)lump->fileofs);

    if (fileLength % diskNodeSize != 0) {
        Com_Error(
            ERR_DROP,
            "\x15"
            "MOD_LoadBmodel: funny lump size");
    }
    if (nodeCount < 1) {
        Com_Error(
            ERR_DROP,
            "\x15"
            "Map has no nodes");
    }

    cm_nodes = Hunk_AllocAlignInternal(
        (size_t)nodeCount * sizeof(cm_nodes[0]),
        CM_LOAD_HUNK_ALIGNMENT);
    cm_numNodes = (int32_t)nodeCount;

    for (uint32_t nodeIndex = 0;
         nodeIndex < nodeCount;
         ++nodeIndex, ++diskNode) {
        collisionNode_t *const node =
            &cm_nodes[nodeIndex];

        /* NOT_FROM_ORIGINAL_SOURCE: the map prepass validates the plane and
         * both child domains before publishing pointers or narrowed fields. */
        node->plane = &cm_planes[diskNode->planeNum];
        for (int32_t childIndex = 0;
             childIndex < 2;
             ++childIndex) {
            const int32_t child =
                diskNode->children[childIndex];
            node->children[childIndex] = (int16_t)child;
            if ((int32_t)node->children[childIndex] !=
                child) {
                Com_Error(
                    ERR_DROP,
                    "\x15"
                    "CMod_LoadNodes: children exceeded");
            }
        }
    }
}

/* Source: CoDUOMP.exe 0x0041c480..0x0041c611.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0041c480_0041c612.mcode.
 * Name and lump argument: exact same-module Mac symbol CMod_LoadSubmodels.
 * Windows proves the 48-byte disk stride, 40-byte i386 runtime stride,
 * expanded bounds, world-model exception, inline brush-index allocation,
 * and all three 16-bit range checks. */
void CMod_LoadSubmodels(const lump_t *lump)
{
    const uint32_t fileLength =
        (uint32_t)lump->filelen;
    const uint32_t diskModelSize =
        (uint32_t)sizeof(dmodel_t);
    const uint32_t modelCount =
        fileLength / diskModelSize;
    const dmodel_t *diskModel =
        (const dmodel_t *)(
            cm_fileBase + (uint32_t)lump->fileofs);

    if (fileLength % diskModelSize != 0) {
        Com_Error(
            ERR_DROP,
            "\x15"
            "CMod_LoadSubmodels: funny lump size");
    }
    if (modelCount < 1) {
        Com_Error(
            ERR_DROP,
            "\x15"
            "Map with no models");
    }

    cm_models = Hunk_AllocAlignInternal(
        (size_t)modelCount * sizeof(cm_models[0]),
        CM_LOAD_HUNK_ALIGNMENT);
    cm_numSubModels = (int32_t)modelCount;
    if (modelCount > MAX_SUBMODELS) {
        Com_Error(
            ERR_DROP,
            "\x15"
            "MAX_SUBMODELS exceeded");
    }

    for (uint32_t modelIndex = 0;
         modelIndex < modelCount;
         ++modelIndex, ++diskModel) {
        collisionModel_t *const model =
            &cm_models[modelIndex];

        for (int32_t axis = 0; axis < 3; ++axis) {
            model->mins[axis] =
                diskModel->mins[axis] - 1.0f;
            model->maxs[axis] =
                diskModel->maxs[axis] + 1.0f;
        }

        if (modelIndex != CM_WORLD_MODEL_INDEX) {
            const uint32_t leafBrushCount =
                (uint32_t)diskModel->numLeafBrushes;
            const uint32_t leafSurfaceCount =
                (uint32_t)diskModel->numLeafSurfaces;
            const uint32_t firstLeafSurface =
                (uint32_t)diskModel->firstLeafSurface;
            int32_t *inlineLeafBrushes;

            /* NOT_FROM_ORIGINAL_SOURCE: the map prepass validates authored
             * brush and leaf-surface spans before allocation or narrowing. */
            model->leaf.numLeafBrushes =
                (uint16_t)leafBrushCount;
            if ((uint32_t)model->leaf.numLeafBrushes !=
                leafBrushCount) {
                Com_Error(
                    ERR_DROP,
                    "\x15"
                    "CMod_LoadSubmodels: numLeafBrushes exceeded");
            }

            inlineLeafBrushes = Hunk_AllocAlignInternal(
                (size_t)leafBrushCount *
                    sizeof(inlineLeafBrushes[0]),
                CM_LOAD_HUNK_ALIGNMENT);
            /* Both pointers are cursors into the same hunk arena. The
             * original stores their signed element-index difference. */
            model->leaf.firstLeafBrush = (int32_t)(
                ((uintptr_t)inlineLeafBrushes -
                 (uintptr_t)cm_leafbrushes) /
                sizeof(inlineLeafBrushes[0]));
            for (uint32_t brushIndex = 0;
                 brushIndex < leafBrushCount;
                 ++brushIndex) {
                inlineLeafBrushes[brushIndex] =
                    diskModel->firstLeafBrush +
                    (int32_t)brushIndex;
            }

            model->leaf.numLeafTerrainPatches =
                (uint16_t)leafSurfaceCount;
            if ((uint32_t)
                    model->leaf.numLeafTerrainPatches !=
                leafSurfaceCount) {
                Com_Error(
                    ERR_DROP,
                    "\x15"
                    "CMod_LoadSubmodels: numLeafSurfaces exceeded");
            }

            model->leaf.firstLeafTerrainPatch =
                (uint16_t)firstLeafSurface;
            if ((uint32_t)
                    model->leaf.firstLeafTerrainPatch !=
                firstLeafSurface) {
                Com_Error(
                    ERR_DROP,
                    "\x15"
                    "CMod_LoadSubmodels: firstLeafSurface exceeded");
            }
        }
    }
}

/* Source: CoDUOMP.exe 0x0041c710..0x0041c959.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0041c710_0041c95a.mcode.
 * Name and two-lump interface: exact same-module Mac symbol
 * CMod_LoadBrushes. Windows proves the compact 4-byte brush and 8-byte
 * brush-side disk records, six axial sides per brush, 52-byte i386 runtime
 * brush stride, side-plane pointer conversion, material checks, and contents
 * mask. */
void CMod_LoadBrushes(const lump_t *brushLump,
                      const lump_t *brushSideLump)
{
    const uint32_t brushLength =
        (uint32_t)brushLump->filelen;
    const uint32_t brushSize =
        (uint32_t)sizeof(dbrush_t);
    const uint32_t brushCount =
        brushLength / brushSize;
    const uint32_t brushSideLength =
        (uint32_t)brushSideLump->filelen;
    const uint32_t brushSideSize =
        (uint32_t)sizeof(dbrushside_t);
    const dbrush_t *diskBrush =
        (const dbrush_t *)(
            cm_fileBase + (uint32_t)brushLump->fileofs);
    const dbrushside_t *diskSide =
        (const dbrushside_t *)(
            cm_fileBase + (uint32_t)brushSideLump->fileofs);
    const uint32_t totalBrushSideCount =
        brushSideLength / brushSideSize;
    const int32_t nonAxialSideCount =
        (int32_t)(totalBrushSideCount -
                  brushCount * CM_BRUSH_AXIAL_SIDE_COUNT);
    collisionBrushSide_t *runtimeSide;

    if (brushLength % brushSize != 0) {
        Com_Error(
            ERR_DROP,
            "\x15"
            "CMod_LoadBrushes: funny lump size");
    }
    if (brushSideLength % brushSideSize != 0) {
        Com_Error(
            ERR_DROP,
            "\x15"
            "CMod_LoadBrushes: funny lump size");
    }
    if (nonAxialSideCount < 0) {
        Com_Error(
            ERR_DROP,
            "\x15"
            "CMod_LoadBrushes: bad side count");
    }

    if (nonAxialSideCount != 0) {
        cm_brushSides = Hunk_AllocAlignInternal(
            (size_t)(uint32_t)nonAxialSideCount *
                sizeof(cm_brushSides[0]),
            CM_LOAD_HUNK_ALIGNMENT);
    } else {
        cm_brushSides = NULL;
    }
    cm_numBrushSides = nonAxialSideCount;
    runtimeSide = cm_brushSides;

    cm_brushes = Hunk_AllocAlignInternal(
        ((size_t)brushCount + 1) *
            sizeof(cm_brushes[0]),
        CM_LOAD_HUNK_ALIGNMENT);
    cm_numBrushes = (int32_t)brushCount;

    for (uint32_t brushIndex = 0;
         brushIndex < brushCount;
         ++brushIndex, ++diskBrush) {
        collisionBrush_t *const brush =
            &cm_brushes[brushIndex];
        const int32_t sideCount =
            (int32_t)diskBrush->numSides -
            CM_BRUSH_AXIAL_SIDE_COUNT;

        brush->nonAxialSideCount = sideCount;
        if (sideCount < 0) {
            Com_Error(
                ERR_DROP,
                "\x15"
                "CMod_LoadBrushes: brush has less than 6 sides");
        }
        brush->nonAxialSides =
            sideCount != 0 ? runtimeSide : NULL;

        /* NOT_FROM_ORIGINAL_SOURCE: the map prepass validates the complete side
         * span, material domain, and nonaxial plane references. */
        for (int32_t axis = 0; axis < 3; ++axis) {
            for (int32_t side = 0; side < 2; ++side) {
                const int32_t materialIndex =
                    diskSide->shaderNum;

                if (side == 0) {
                    brush->mins[axis] =
                        diskSide->plane.dist;
                } else {
                    brush->maxs[axis] =
                        diskSide->plane.dist;
                }
                if (materialIndex < 0 ||
                    materialIndex >= cm_numMaterials) {
                    Com_Error(
                        ERR_DROP,
                        "\x15"
                        "CMod_LoadBrushes: bad shaderNum: %i",
                        materialIndex);
                }
                brush->axialMaterialIndices[
                    side * 3 + axis] =
                    (int16_t)materialIndex;
                if ((int32_t)brush->axialMaterialIndices[
                        side * 3 + axis] != materialIndex) {
                    Com_Error(
                        ERR_DROP,
                        "\x15"
                        "CMod_LoadBrushes: axialShaderNum exceeded");
                }
                ++diskSide;
            }
        }

        for (int32_t sideIndex = 0;
             sideIndex < sideCount;
             ++sideIndex, ++diskSide, ++runtimeSide) {
            const int32_t materialIndex =
                diskSide->shaderNum;

            runtimeSide->plane =
                &cm_planes[diskSide->plane.planeNum];
            runtimeSide->materialIndex =
                materialIndex;
            if (materialIndex < 0 ||
                materialIndex >= cm_numMaterials) {
                Com_Error(
                    ERR_DROP,
                    "\x15"
                    "CMod_LoadBrushes: bad shaderNum: %i",
                    materialIndex);
            }
        }

        {
            const int32_t materialIndex =
                (int32_t)diskBrush->shaderNum;

            if (materialIndex < 0 ||
                materialIndex >= cm_numMaterials) {
                Com_Error(
                    ERR_DROP,
                    "\x15"
                    "CMod_LoadBrushes: bad shaderNum: %i",
                    materialIndex);
            }
            brush->contents = (int32_t)(
                (uint32_t)cm_materials[materialIndex].contentFlags &
                CM_BRUSH_CONTENTS_MASK);
        }
    }
}

/* Source: CoDUOMP.exe 0x0041c960..0x0041caf5.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0041c960_0041caf6.mcode.
 * Name and lump argument: exact same-module Mac symbol CMod_LoadLeafs.
 * Windows proves the 36-byte disk record, 16-byte runtime record, signed
 * cluster/area/cell narrowing, unsigned brush/terrain range checks, maximum
 * cluster/area tracking, and area/portal allocation sizes. */
void CMod_LoadLeafs(const lump_t *lump)
{
    const uint32_t fileLength =
        (uint32_t)lump->filelen;
    const uint32_t diskLeafSize =
        (uint32_t)sizeof(dleaf_t);
    const uint32_t leafCount =
        fileLength / diskLeafSize;
    const dleaf_t *diskLeaf =
        (const dleaf_t *)(
            cm_fileBase + (uint32_t)lump->fileofs);

    if (fileLength % diskLeafSize != 0) {
        Com_Error(
            ERR_DROP,
            "\x15"
            "CMod_LoadLeafs: funny lump size");
    }
    if (leafCount < 1) {
        Com_Error(
            ERR_DROP,
            "\x15"
            "Map with no leafs");
    }

    cm_leafs = Hunk_AllocAlignInternal(
        (size_t)leafCount * sizeof(cm_leafs[0]),
        CM_LOAD_HUNK_ALIGNMENT);
    cm_numLeafs = (int32_t)leafCount;

    for (uint32_t leafIndex = 0;
         leafIndex < leafCount;
         ++leafIndex, ++diskLeaf) {
        collisionLeaf_t *const leaf =
            &cm_leafs[leafIndex];
        const int32_t cluster =
            diskLeaf->cluster;
        const int32_t area =
            diskLeaf->area;
        const uint32_t leafBrushCount =
            (uint32_t)diskLeaf->numLeafBrushes;
        const uint32_t firstLeafTerrainPatch =
            (uint32_t)diskLeaf->firstLeafTerrainPatch;
        const uint32_t leafTerrainPatchCount =
            (uint32_t)diskLeaf->numLeafTerrainPatches;

        leaf->cellNum =
            (int16_t)diskLeaf->cellNum;
        if ((int32_t)leaf->cellNum !=
            diskLeaf->cellNum) {
            Com_Error(
                ERR_DROP,
                "\x15"
                "CMod_LoadLeafs: cellnum exceeded");
        }

        leaf->cluster = (int16_t)cluster;
        if ((int32_t)leaf->cluster != cluster) {
            Com_Error(
                ERR_DROP,
                "\x15"
                "CMod_LoadLeafs: cluster exceeded");
        }

        leaf->area = (int16_t)area;
        if ((int32_t)leaf->area != area) {
            Com_Error(
                ERR_DROP,
                "\x15"
                "CMod_LoadLeafs: area exceeded");
        }

        leaf->firstLeafBrush =
            diskLeaf->firstLeafBrush;
        /* NOT_FROM_ORIGINAL_SOURCE: the map prepass validates both authored
         * spans and every referenced brush or terrain entry before narrowing. */
        leaf->numLeafBrushes =
            (uint16_t)leafBrushCount;
        if ((uint32_t)leaf->numLeafBrushes !=
            leafBrushCount) {
            Com_Error(
                ERR_DROP,
                "\x15"
                "CMod_LoadLeafs: numLeafBrushes exceeded");
        }

        leaf->firstLeafTerrainPatch =
            (uint16_t)firstLeafTerrainPatch;
        if ((uint32_t)
                leaf->firstLeafTerrainPatch !=
            firstLeafTerrainPatch) {
            Com_Error(
                ERR_DROP,
                "\x15"
                "CMod_LoadLeafs: firstLeafSurface exceeded");
        }

        leaf->numLeafTerrainPatches =
            (uint16_t)leafTerrainPatchCount;
        if ((uint32_t)
                leaf->numLeafTerrainPatches !=
            leafTerrainPatchCount) {
            Com_Error(
                ERR_DROP,
                "\x15"
                "CMod_LoadLeafs: numLeafSurfaces exceeded");
        }

        if (cm_numClusters <= cluster) {
            cm_numClusters = cluster + 1;
        }
        if (cm_numAreas <= area) {
            cm_numAreas = area + 1;
        }
    }

    const size_t areaCount = (size_t)(uint32_t)cm_numAreas;
    /* NOT_FROM_ORIGINAL_SOURCE: require the square portal-matrix extent to be
     * representable by this build's size_t; hunk capacity remains a separate
     * allocation limit. */
    if (areaCount != 0 &&
        areaCount > SIZE_MAX / sizeof(cm_areaPortals[0]) / areaCount) {
        Com_Error(ERR_DROP, "\x15" "CMod_LoadLeafs: area portal matrix is too large");
    }
    const size_t areaPortalBytes =
        areaCount * areaCount * sizeof(cm_areaPortals[0]);

    cm_areas = Hunk_AllocAlignInternal(
        areaCount * sizeof(cm_areas[0]),
        CM_LOAD_HUNK_ALIGNMENT);
    cm_areaPortals = Hunk_AllocAlignInternal(
        areaPortalBytes,
        CM_LOAD_HUNK_ALIGNMENT);
}

/* Source: CoDUOMP.exe 0x0041cb00..0x0041cc08.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0041cb00_0041cc09.mcode.
 * Name and lump argument: exact same-module Mac symbol CMod_LoadPlanes.
 * Windows proves the 16-byte disk plane, 20-byte runtime plane, sign-bit
 * accumulation, and axial type selection. */
void CMod_LoadPlanes(const lump_t *lump)
{
    const uint32_t fileLength =
        (uint32_t)lump->filelen;
    const uint32_t diskPlaneSize =
        (uint32_t)sizeof(dplane_t);
    const uint32_t planeCount =
        fileLength / diskPlaneSize;
    const dplane_t *diskPlane =
        (const dplane_t *)(
            cm_fileBase + (uint32_t)lump->fileofs);

    if (fileLength % diskPlaneSize != 0) {
        Com_Error(
            ERR_DROP,
            "\x15"
            "MOD_LoadBmodel: funny lump size");
    }
    if (planeCount < 1) {
        Com_Error(
            ERR_DROP,
            "\x15"
            "Map with no planes");
    }

    cm_planes = Hunk_AllocAlignInternal(
        (size_t)planeCount * sizeof(cm_planes[0]),
        CM_LOAD_HUNK_ALIGNMENT);
    cm_numPlanes = (int32_t)planeCount;

    for (uint32_t planeIndex = 0;
         planeIndex < planeCount;
         ++planeIndex, ++diskPlane) {
        cplane_t *const plane =
            &cm_planes[planeIndex];
        uint8_t signBits = 0;

        for (int32_t axis = 0; axis < 3; ++axis) {
            plane->normal[axis] =
                diskPlane->normal[axis];
            if (plane->normal[axis] < 0.0f) {
                signBits |= (uint8_t)(1U << axis);
            }
        }
        plane->dist = diskPlane->dist;

        if (plane->normal[0] == 1.0f) {
            plane->type = 0;
        } else if (plane->normal[1] == 1.0f) {
            plane->type = 1;
        } else if (plane->normal[2] == 1.0f) {
            plane->type = 2;
        } else {
            plane->type = 3;
        }
        plane->signbits = signBits;
    }
}

/* Source: CoDUOMP.exe 0x0041cc10..0x0041cc6f.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0041cc10_0041cc70.mcode.
 * Name and lump argument: exact same-module Mac symbol
 * CMod_LoadLeafBrushes. The extra allocated index is the original loader's
 * sentinel/storage reserve; only the count-addressed entries are copied. */
void CMod_LoadLeafBrushes(const lump_t *lump)
{
    const uint32_t fileLength =
        (uint32_t)lump->filelen;
    const uint32_t leafBrushCount =
        fileLength / (uint32_t)sizeof(cm_leafbrushes[0]);
    const int32_t *diskLeafBrush =
        (const int32_t *)(
            cm_fileBase + (uint32_t)lump->fileofs);

    if (fileLength % sizeof(cm_leafbrushes[0]) != 0) {
        Com_Error(
            ERR_DROP,
            "\x15"
            "MOD_LoadBmodel: funny lump size");
    }

    cm_leafbrushes = Hunk_AllocAlignInternal(
        ((size_t)leafBrushCount + 1) *
            sizeof(cm_leafbrushes[0]),
        CM_LOAD_HUNK_ALIGNMENT);
    cm_numLeafBrushes =
        (int32_t)leafBrushCount;
    for (uint32_t index = 0;
         index < leafBrushCount;
         ++index) {
        cm_leafbrushes[index] =
            diskLeafBrush[index];
    }
}

/* Source: CoDUOMP.exe 0x0041cc70..0x0041cccf.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0041cc70_0041ccd0.mcode.
 * Name and lump argument: exact same-module Mac symbol
 * CMod_LoadLeafSurfaces. */
void CMod_LoadLeafSurfaces(const lump_t *lump)
{
    const uint32_t fileLength =
        (uint32_t)lump->filelen;
    const uint32_t leafSurfaceCount =
        fileLength / (uint32_t)sizeof(cm_leafsurfaces[0]);
    const int32_t *diskLeafSurface =
        (const int32_t *)(
            cm_fileBase + (uint32_t)lump->fileofs);

    if (fileLength % sizeof(cm_leafsurfaces[0]) != 0) {
        Com_Error(
            ERR_DROP,
            "\x15"
            "CMod_LoadLeafSurfaces: funny lump size");
    }

    cm_leafsurfaces = Hunk_AllocAlignInternal(
        (size_t)leafSurfaceCount *
            sizeof(cm_leafsurfaces[0]),
        CM_LOAD_HUNK_ALIGNMENT);
    cm_numLeafSurfaces =
        (int32_t)leafSurfaceCount;
    for (uint32_t index = 0;
         index < leafSurfaceCount;
         ++index) {
        cm_leafsurfaces[index] =
            diskLeafSurface[index];
    }
}

/* Source: CoDUOMP.exe 0x0041ccd0..0x0041d0ab.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0041ccd0_0041d0ac.mcode.
 * Name and three-lump interface: exact same-module Mac symbol
 * CMod_LoadLeafCurvesAndTerrain. Windows proves the 16-byte disk descriptor,
 * 44-byte i386 runtime record, curve/terrain branch selector, stack workspace
 * capacities, source offsets, backend calls, and inlined loading keepalive. */
void CMod_LoadLeafCurvesAndTerrain(
    const lump_t *patchLump,
    const lump_t *vertexLump,
    const lump_t *indexLump)
{
    const uint32_t patchLength =
        (uint32_t)patchLump->filelen;
    const uint32_t vertexLength =
        (uint32_t)vertexLump->filelen;
    const uint32_t indexLength =
        (uint32_t)indexLump->filelen;
    const uint32_t patchCount =
        patchLength /
        (uint32_t)sizeof(dterrainPatch_t);
    const dterrainPatch_t *diskPatch =
        (const dterrainPatch_t *)(
            cm_fileBase + (uint32_t)patchLump->fileofs);
    const vec3_t *diskVertices =
        (const vec3_t *)(
            cm_fileBase + (uint32_t)vertexLump->fileofs);
    const int16_t *diskIndices =
        (const int16_t *)(
            cm_fileBase + (uint32_t)indexLump->fileofs);
    vec3_t vertices[CM_MAX_TERRAIN_VERTICES];
    int16_t indices[CM_MAX_TERRAIN_INDICES];

    if (patchLength %
            sizeof(dterrainPatch_t) != 0 ||
        vertexLength % sizeof(vec3_t) != 0 ||
        indexLength % sizeof(int16_t) != 0) {
        Com_Error(
            ERR_DROP,
            "\x15"
            "CMod_LoadLeafpatchesAndTerrain: funny lump size");
    }

    cm_numTerrainPatches =
        (int32_t)patchCount;
    cm_terrainPatches = Hunk_AllocAlignInternal(
        (size_t)patchCount *
            sizeof(cm_terrainPatches[0]),
        CM_LOAD_HUNK_ALIGNMENT);

    for (uint32_t patchIndex = 0;
         patchIndex < patchCount;
         ++patchIndex, ++diskPatch) {
        collisionTerrainPatch_t *const patch =
            &cm_terrainPatches[patchIndex];
        const int32_t materialIndex =
            (int32_t)diskPatch->shaderNum;

        patch->checkcount = 0;
        patch->materialIndex = materialIndex;
        /* NOT_FROM_ORIGINAL_SOURCE: the map prepass validates the material,
         * dimensions, and source spans; the terrain builder validates relative
         * indexes before use. */
        patch->contents =
            cm_materials[materialIndex].contentFlags;

        if (diskPatch->collisionMode == 0) {
            const int32_t width =
                (int32_t)diskPatch->data.curve.width;
            const int32_t height =
                (int32_t)diskPatch->data.curve.height;
            const int32_t pointCount =
                width * height;
            const vec3_t *const sourceVertices =
                &diskVertices[
                    diskPatch->data.curve.firstVert];

            if ((uint32_t)pointCount >
                CM_MAX_TERRAIN_VERTICES) {
                Com_Error(
                    ERR_DROP,
                    "\x15"
                    "CMod_LoadLeafpatchesAndTerrain: more than %i verts in curve\n",
                    CM_MAX_TERRAIN_VERTICES);
            }
            for (int32_t pointIndex = 0;
                 pointIndex < pointCount;
                 ++pointIndex) {
                for (int32_t axis = 0; axis < 3; ++axis) {
                    vertices[pointIndex][axis] =
                        sourceVertices[pointIndex][axis];
                }
            }

            patch->curveCollide =
                CM_GeneratePatchCollide(
                    (uint32_t)width,
                    (uint32_t)height,
                    diskPatch->data.curve.maxError,
                    (const vec3_t *)vertices,
                    patch->bounds);
            patch->terrainCollide = NULL;
        } else {
            const int32_t vertexCount =
                (int32_t)diskPatch->data.terrain.numVerts;
            const vec3_t *const sourceVertices =
                &diskVertices[diskPatch->data.terrain.firstVert];
            const int32_t indexCount =
                (int32_t)diskPatch->data.terrain.numIndexes;
            const int16_t *const sourceIndices =
                &diskIndices[
                    diskPatch->data.terrain.firstIndex];

            if ((uint32_t)vertexCount >
                CM_MAX_TERRAIN_VERTICES) {
                Com_Error(
                    ERR_DROP,
                    "\x15"
                    "CMod_LoadLeafpatchesAndTerrain: more than %i verts in terrain\n",
                    CM_MAX_TERRAIN_VERTICES);
            }
            for (int32_t vertexIndex = 0;
                 vertexIndex < vertexCount;
                 ++vertexIndex) {
                for (int32_t axis = 0; axis < 3; ++axis) {
                    vertices[vertexIndex][axis] =
                        sourceVertices[vertexIndex][axis];
                }
            }

            if ((uint32_t)indexCount >
                CM_MAX_TERRAIN_INDICES) {
                Com_Error(
                    ERR_DROP,
                    "\x15"
                    "CMod_LoadLeafpatchesAndTerrain: more than %i indexes in terrain\n",
                    CM_MAX_TERRAIN_INDICES);
            }
            for (int32_t index = 0;
                 index < indexCount;
                 ++index) {
                indices[index] =
                    sourceIndices[index];
            }

            patch->curveCollide = NULL;
            patch->terrainCollide =
                CM_GenerateTerrainCollide(
                    indexCount, indices,
                    (uint32_t)vertexCount,
                    (const vec3_t *)vertices,
                    patch->bounds);
        }

        /* Windows inlines its message-pump keepalive here; the named
         * cross-platform boundary preserves the source-level operation. */
        if ((cm_numTerrainPatches &
             CM_LOADING_KEEPALIVE_MASK) ==
            CM_LOADING_KEEPALIVE_MASK) {
            COLLISION_MAP_LOADING_KEEPALIVE();
        }
    }
}

/* Source: CoDUOMP.exe 0x0041d0b0..0x0041d0e4.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0041d0b0_0041d0e4.mcode.
 * Name and lump argument: exact same-module Mac symbol
 * CMod_LoadEntityString. */
void CMod_LoadEntityString(const lump_t *lump)
{
    const uint32_t fileLength =
        (uint32_t)lump->filelen;

    /* NOT_FROM_ORIGINAL_SOURCE: preserve the complete entity lump and append a
     * private terminator before any string parser consumes it. */
    cm_entityString = Hunk_AllocAlignInternal(
        (size_t)fileLength + 1u,
        CM_LOAD_HUNK_ALIGNMENT);
    cm_entityStringLength = lump->filelen;
    Com_Memcpy(cm_entityString,
               cm_fileBase + (uint32_t)lump->fileofs,
               (size_t)fileLength);
    cm_entityString[fileLength] = '\0';
}

/* Source: CoDUOMP.exe 0x0041d0f0..0x0041d17a.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0041d0f0_0041d17b.mcode.
 * Name and lump argument: exact same-module Mac symbol
 * CMod_LoadVisibility. Windows proves the no-visibility 32-byte cluster
 * rounding and 0xff fill, and the loaded header/data allocation behavior. */
void CMod_LoadVisibility(const lump_t *lump)
{
    const uint32_t fileLength =
        (uint32_t)lump->filelen;

    if (fileLength == 0) {
        cm_clusterBytes =
            (cm_numClusters +
             CM_VISIBILITY_ALIGNMENT - 1) &
            ~(CM_VISIBILITY_ALIGNMENT - 1);
        cm_visibility = Hunk_AllocAlignInternal(
            (size_t)cm_clusterBytes,
            CM_LOAD_HUNK_ALIGNMENT);
        Com_Memset(
            cm_visibility, 255,
            (size_t)cm_clusterBytes);
        return;
    }

    /* NOT_FROM_ORIGINAL_SOURCE: require a complete visibility header and
     * dimensions whose declared rows exist and cover the cluster-bit domain. */
    if (fileLength < (uint32_t)sizeof(dvis_t)) {
        Com_Error(ERR_DROP, "\x15" "CMod_LoadVisibility: truncated visibility header");
    }

    const dvis_t *const diskVisibility =
        (const dvis_t *)(cm_fileBase + (uint32_t)lump->fileofs);
    const int32_t visibilityClusterCount = diskVisibility->numClusters;
    const int32_t visibilityClusterBytes = diskVisibility->clusterBytes;
    const uint32_t visibilityDataLength =
        fileLength - (uint32_t)sizeof(dvis_t);

    if (visibilityClusterCount <= 0 || visibilityClusterBytes <= 0 ||
        visibilityClusterCount != cm_numClusters) {
        Com_Error(ERR_DROP, "\x15" "CMod_LoadVisibility: invalid visibility dimensions");
    }

    const uint32_t minimumClusterBytes =
        ((uint32_t)visibilityClusterCount - 1U) / 8U + 1U;
    const uint64_t requiredDataLength =
        (uint64_t)(uint32_t)visibilityClusterCount *
        (uint64_t)(uint32_t)visibilityClusterBytes;
    if ((uint32_t)visibilityClusterBytes < minimumClusterBytes ||
        requiredDataLength > (uint64_t)visibilityDataLength) {
        Com_Error(ERR_DROP, "\x15" "CMod_LoadVisibility: visibility data is incomplete");
    }

    cm_visibilityLoaded = qtrue;
    cm_visibility = Hunk_AllocAlignInternal(
        (size_t)fileLength, CM_LOAD_HUNK_ALIGNMENT);
    cm_numClusters = visibilityClusterCount;
    cm_clusterBytes = visibilityClusterBytes;
    Com_Memcpy(cm_visibility, diskVisibility->data,
               (size_t)visibilityDataLength);
}
