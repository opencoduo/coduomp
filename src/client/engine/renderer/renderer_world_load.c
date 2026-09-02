#include "backend.h"

#include "gl_api.h"
#include "gl_state.h"
#include "../platform/crt_boundary.h"
#include "../math/vector_math.h"
#include "../system_event.h"
#include "qcommon/bsp_validation.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

enum {
    R_WORLD_SURFACE_SHADER_USAGE = 8,
    R_TRIANGLE_SOUP_INDEX_COUNT = 3,
    R_MAX_INDEXED_LIGHTS_PER_LEAF = 15,
    R_MAX_MAP_CORONAS = 512,
    R_MAX_ENTITY_PAIRS = 64,
    R_CORONA_SHADER_USAGE = 4,
    R_ENTITY_KEY_CHARS = 2048,
    R_ENTITY_VALUE_CHARS = 2048
};

/* NOT_FROM_ORIGINAL_SOURCE: validate the complete renderer-owned triangle-
 * soup record graph before R_BuildLightmapMergability or R_LoadSurfaces turns
 * any disk field into an array index or pointer. */
static qboolean coduomp_renderer_validate_triangle_soup_records(const lump_t *surfaceLump, const lump_t *vertexLump,
                                                                const lump_t *indexLump)
{
    if ((uint32_t)surfaceLump->filelen % sizeof(dsurface_t) != 0U || (uint32_t)vertexLump->filelen % sizeof(drawVert_t) != 0U ||
        (uint32_t)indexLump->filelen % sizeof(int16_t) != 0U) {
        ri.Error(ERR_DROP,
                 "\x15"
                 "LoadMap: funny triangle soup lump size in %s",
                 rendererWorldData.name);
        return qfalse;
    }

    const dsurface_t *const surfaces = (const dsurface_t *)(rendererWorldFileBase + surfaceLump->fileofs);
    const int16_t *const indices = (const int16_t *)(rendererWorldFileBase + indexLump->fileofs);
    const uint32_t surfaceCount = (uint32_t)surfaceLump->filelen / sizeof(dsurface_t);
    const uint32_t vertexCount = (uint32_t)vertexLump->filelen / sizeof(drawVert_t);
    const uint32_t indexCount = (uint32_t)indexLump->filelen / sizeof(int16_t);

    for (uint32_t surfaceIndex = 0; surfaceIndex < surfaceCount; ++surfaceIndex) {
        const dsurface_t *const surface = &surfaces[surfaceIndex];
        if (surface->shaderNum < 0 || surface->shaderNum >= rendererWorldData.numShaders || surface->lightmapNum < -1 ||
            surface->lightmapNum >= R_MAX_LIGHTMAPS || surface->firstVert < 0 || (uint32_t)surface->firstVert > vertexCount ||
            (uint32_t)surface->numVerts > vertexCount - (uint32_t)surface->firstVert || surface->firstIndex > indexCount ||
            (uint32_t)surface->numIndexes > indexCount - surface->firstIndex || surface->numVerts == 0 || surface->numIndexes == 0 ||
            surface->numIndexes % R_TRIANGLE_SOUP_INDEX_COUNT != 0) {
            ri.Error(ERR_DROP,
                     "\x15"
                     "LoadMap: invalid triangle soup surface %u",
                     surfaceIndex);
            return qfalse;
        }

        const int16_t *const surfaceIndices = &indices[surface->firstIndex];
        if (surfaceIndices[0] != 0) {
            ri.Error(ERR_DROP, "\x15"
                               "First index is not 0 in triangle soup surface");
            return qfalse;
        }
        for (uint32_t index = 0; index < surface->numIndexes; ++index) {
            if (surfaceIndices[index] < 0 || (uint32_t)surfaceIndices[index] >= (uint32_t)surface->numVerts) {
                ri.Error(ERR_DROP, "\x15"
                                   "Bad index in triangle soup surface");
                return qfalse;
            }
        }
    }
    return qtrue;
}

#if UINTPTR_MAX == UINT32_MAX
_Static_assert(_Alignof(dheader_t) == 4, "renderer BSP header alignment changed");
_Static_assert(offsetof(dheader_t, ident) == 0, "renderer BSP header ident moved");
_Static_assert(offsetof(dheader_t, version) == 4, "renderer BSP header version moved");
_Static_assert(offsetof(dheader_t, lumps) == 8, "renderer BSP header lump table moved");
_Static_assert(sizeof(dheader_t) == 272, "renderer BSP header layout changed");
#endif

/* R_LoadEntities stores each parsed entity as fixed key/value records. Record
 * zero is always the classname pair; the lookup helpers deliberately begin at
 * record one because callers already dispatch on that classname. */
typedef struct renderer_entity_pair_s {
    char key[R_ENTITY_KEY_CHARS];
    char value[R_ENTITY_VALUE_CHARS];
} renderer_entity_pair_t;

#if UINTPTR_MAX == UINT32_MAX
_Static_assert(_Alignof(renderer_entity_pair_t) == 1, "renderer entity-pair alignment changed");
_Static_assert(offsetof(renderer_entity_pair_t, key) == 0, "renderer entity-pair key moved");
_Static_assert(offsetof(renderer_entity_pair_t, value) == 2048, "renderer entity-pair value moved");
_Static_assert(sizeof(renderer_entity_pair_t) == 4096, "renderer entity-pair layout changed");
#endif

#if UINTPTR_MAX == UINT32_MAX
_Static_assert(_Alignof(dmodel_t) == 4, "renderer disk-submodel alignment changed");
_Static_assert(sizeof(dmodel_t) == 0x30, "renderer disk-submodel layout changed");
_Static_assert(offsetof(dmodel_t, mins) == 0x00, "renderer disk-submodel minimum bounds moved");
_Static_assert(offsetof(dmodel_t, maxs) == 0x0c, "renderer disk-submodel maximum bounds moved");
_Static_assert(offsetof(dmodel_t, firstSurface) == 0x18, "renderer disk-submodel first surface moved");
_Static_assert(offsetof(dmodel_t, numSurfaces) == 0x1c, "renderer disk-submodel surface count moved");
_Static_assert(offsetof(dmodel_t, firstLeafSurface) == 0x20, "renderer disk-submodel first leaf surface moved");
_Static_assert(offsetof(dmodel_t, numLeafSurfaces) == 0x24, "renderer disk-submodel leaf-surface count moved");
_Static_assert(offsetof(dmodel_t, firstLeafBrush) == 0x28, "renderer disk-submodel first leaf brush moved");
_Static_assert(offsetof(dmodel_t, numLeafBrushes) == 0x2c, "renderer disk-submodel leaf-brush count moved");
#endif

/* The node and leaf lumps both use 36-byte records. R_LoadNodesAndLeafs
 * consumes only the node prefix and the marked leaf fields. The sibling
 * collision-format declaration identifies the node tail as serialized bounds,
 * but CoDUOMP.exe and coduo_lnxded both advance over it without reading it.
 * The collision loader consumes the other five leaf fields from this same disk
 * record. */
#if UINTPTR_MAX == UINT32_MAX
_Static_assert(_Alignof(dnode_t) == 4, "renderer disk-node alignment changed");
_Static_assert(offsetof(dnode_t, planeNum) == 0x00, "renderer disk-node plane index moved");
_Static_assert(offsetof(dnode_t, children) == 0x04, "renderer disk-node children moved");
_Static_assert(offsetof(dnode_t, mins) == 0x0c, "renderer disk-node minimum bounds moved");
_Static_assert(offsetof(dnode_t, maxs) == 0x18, "renderer disk-node maximum bounds moved");
_Static_assert(sizeof(dnode_t) == 0x24, "renderer disk-node layout changed");
_Static_assert(_Alignof(dleaf_t) == 4, "renderer disk-leaf alignment changed");
_Static_assert(sizeof(dleaf_t) == 0x24, "renderer disk-leaf layout changed");
_Static_assert(offsetof(dleaf_t, cluster) == 0x00, "renderer disk-leaf cluster moved");
_Static_assert(offsetof(dleaf_t, area) == 0x04, "renderer disk-leaf area moved");
_Static_assert(offsetof(dleaf_t, firstLeafTerrainPatch) == 0x08, "renderer disk-leaf first terrain patch moved");
_Static_assert(offsetof(dleaf_t, numLeafTerrainPatches) == 0x0c, "renderer disk-leaf terrain-patch count moved");
_Static_assert(offsetof(dleaf_t, firstLeafBrush) == 0x10, "renderer disk-leaf first brush moved");
_Static_assert(offsetof(dleaf_t, numLeafBrushes) == 0x14, "renderer disk-leaf brush count moved");
_Static_assert(offsetof(dleaf_t, cellNum) == 0x18, "renderer disk-leaf cell offset moved");
_Static_assert(offsetof(dleaf_t, firstLightIndex) == 0x1c, "renderer disk-leaf first light moved");
_Static_assert(offsetof(dleaf_t, lightCount) == 0x20, "renderer disk-leaf light count moved");
#endif

/* Serialized type-selected light payload. The shorter variants deliberately
 * remain short: customSpot proves the complete 16-byte union allocation, so
 * the bytes after another active member are alternate storage, not padding in
 * a padded substructure. */
typedef union renderer_disk_light_parameters_u {
    struct {
        float linearAttenuation;            /* payload +0x00 */
    } linearPoint;
    struct {
        float quadraticAttenuation;         /* payload +0x00 */
        float constantAttenuation;          /* payload +0x04 */
    } customPoint;
    struct {
        float cutoffCos;                    /* payload +0x00 */
        int32_t exponent;                   /* payload +0x04 */
    } spot;
    struct {
        float quadraticAttenuation;         /* payload +0x00 */
        float constantAttenuation;          /* payload +0x04 */
        float cutoffCos;                    /* payload +0x08 */
        int32_t exponent;                   /* payload +0x0c */
    } customSpot;
} renderer_disk_light_parameters_t;

#if UINTPTR_MAX == UINT32_MAX
_Static_assert(_Alignof(renderer_disk_light_parameters_t) == 4, "renderer disk-light parameter alignment changed");
_Static_assert(sizeof(renderer_disk_light_parameters_t) == 0x10, "renderer disk-light parameter allocation changed");
_Static_assert(offsetof(renderer_disk_light_parameters_t, linearPoint.linearAttenuation) == 0x00, "renderer disk linear attenuation moved");
_Static_assert(offsetof(renderer_disk_light_parameters_t, customPoint.quadraticAttenuation) == 0x00,
               "renderer disk custom-point quadratic attenuation moved");
_Static_assert(offsetof(renderer_disk_light_parameters_t, customPoint.constantAttenuation) == 0x04,
               "renderer disk custom-point constant attenuation moved");
_Static_assert(offsetof(renderer_disk_light_parameters_t, spot.cutoffCos) == 0x00, "renderer disk spot cutoff moved");
_Static_assert(offsetof(renderer_disk_light_parameters_t, spot.exponent) == 0x04, "renderer disk spot exponent moved");
_Static_assert(offsetof(renderer_disk_light_parameters_t, customSpot.quadraticAttenuation) == 0x00,
               "renderer disk custom-spot quadratic attenuation moved");
_Static_assert(offsetof(renderer_disk_light_parameters_t, customSpot.constantAttenuation) == 0x04,
               "renderer disk custom-spot constant attenuation moved");
_Static_assert(offsetof(renderer_disk_light_parameters_t, customSpot.cutoffCos) == 0x08, "renderer disk custom-spot cutoff moved");
_Static_assert(offsetof(renderer_disk_light_parameters_t, customSpot.exponent) == 0x0c, "renderer disk custom-spot exponent moved");
#endif

/* Serialized 72-byte static-light record. Windows and both shipped PPC
 * clients consume only +0x00..+0x37. No CoDUOMP renderer path reads the final
 * 16 bytes, and their source meaning remains unproved. */
typedef struct renderer_disk_light_s {
    renderer_light_type_t type;            /* disk +0x00 */
    vec3_t color;                          /* disk +0x04 */
    vec3_t position;                       /* disk +0x10 */
    vec3_t direction;                      /* disk +0x1c */
    renderer_disk_light_parameters_t parameters; /* disk +0x28 */
    uint8_t unconsumed038[16];             /* disk +0x38; renderer-unused */
} renderer_disk_light_t;

#if UINTPTR_MAX == UINT32_MAX
_Static_assert(_Alignof(renderer_disk_light_t) == 4, "renderer disk-light alignment changed");
_Static_assert(sizeof(renderer_disk_light_t) == 0x48, "renderer disk-light layout changed");
_Static_assert(offsetof(renderer_disk_light_t, type) == 0x00, "renderer disk-light type moved");
_Static_assert(offsetof(renderer_disk_light_t, color) == 0x04, "renderer disk-light color moved");
_Static_assert(offsetof(renderer_disk_light_t, position) == 0x10, "renderer disk-light position moved");
_Static_assert(offsetof(renderer_disk_light_t, direction) == 0x1c, "renderer disk-light direction moved");
_Static_assert(offsetof(renderer_disk_light_t, parameters) == 0x28, "renderer disk-light parameters moved");
_Static_assert(offsetof(renderer_disk_light_t, parameters.customSpot.cutoffCos) == 0x30, "renderer disk-light custom-spot cutoff moved");
_Static_assert(offsetof(renderer_disk_light_t, parameters.customSpot.exponent) == 0x34, "renderer disk-light custom-spot exponent moved");
_Static_assert(offsetof(renderer_disk_light_t, unconsumed038) == 0x38, "renderer disk-light unconsumed tail moved");
#endif

/* Source: CoDUOMP.exe 0x0050bdf0..0x0050be5e and same-module Mac symbol
 * ParseTriangleSoup at source address 0x10055c00..0x10055cf7.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0050bdf0_0050be5f.mcode.
 * Windows LTCG retains this out-of-line body and also inlines it into
 * R_LoadSurfaces at 0x0050c0e6..0x0050c151. Both builds prove the six source
 * arguments and the same disk-record field accesses. */
void ParseTriangleSoup(const dsurface_t *diskSurface, renderer_shader_surface_build_t *build,
                       const renderer_lightmap_placement_t *lightmapPlacements, const drawVert_t *vertices, msurface_t *worldSurface,
                       const int16_t *indices)
{
    worldSurface->shader = build->shader;
    if (r_singleShader->integer != 0 && (worldSurface->shader->flags & SHADER_FLAG_SKY) == 0) {
        worldSurface->shader = tr.defaultShader;
    }

    const renderer_lightmap_placement_t *lightmapPlacement = NULL;
    if (diskSurface->lightmapNum >= 0) {
        lightmapPlacement = &lightmapPlacements[diskSurface->lightmapNum];
    }

    (void)BuildOptimizedSurface(worldSurface, build, lightmapPlacement, diskSurface->numVerts, &vertices[diskSurface->firstVert],
                                diskSurface->numIndexes, &indices[diskSurface->firstIndex]);
}

/* Source: CoDUOMP.exe 0x0050be60..0x0050c240.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0050be60_0050c240.mcode.
 * Name, four-lump/placement signature, ParseTriangleSoup boundary, and
 * Sys_PumpEvents boundary: exact same-module Mac symbol R_LoadSurfaces.
 * Windows LTCG inlines ShaderForShaderNum, ParseTriangleSoup, and the Win32
 * body of Sys_PumpEvents; this source restores those original calls. */
void R_LoadSurfaces(const lump_t *surfaceLump, const lump_t *vertexLump, const lump_t *indexLump,
                    const renderer_lightmap_placement_t *lightmapPlacements)
{
    const dsurface_t *diskSurfaces = (const dsurface_t *)(rendererWorldFileBase + surfaceLump->fileofs);
    if (((uint32_t)surfaceLump->filelen % sizeof(*diskSurfaces)) != 0) {
        ri.Error(ERR_DROP, "\x15LoadMap: funny lump size in %s", rendererWorldData.name);
    }

    const drawVert_t *vertices = (const drawVert_t *)(rendererWorldFileBase + vertexLump->fileofs);
    if (((uint32_t)vertexLump->filelen % sizeof(*vertices)) != 0) {
        ri.Error(ERR_DROP, "\x15LoadMap: funny lump size in %s", rendererWorldData.name);
    }

    const int16_t *indices = (const int16_t *)(rendererWorldFileBase + indexLump->fileofs);
    if (((uint32_t)indexLump->filelen % sizeof(*indices)) != 0) {
        ri.Error(ERR_DROP, "\x15LoadMap: funny lump size in %s", rendererWorldData.name);
    }

    const int32_t surfaceCount = (int32_t)((uint32_t)surfaceLump->filelen / sizeof(*diskSurfaces));
    rendererWorldData.numsurfaces = surfaceCount;
    if (surfaceCount == 0) {
        ri.Error(ERR_DROP, "\x15LoadMap: no surfaces in %s", rendererWorldData.name);
    }

    rendererWorldData.surfaces = ri.Hunk_Alloc((size_t)surfaceCount * sizeof(*rendererWorldData.surfaces));

    int32_t *surfaceGroupMarkers = ri.Hunk_AllocateTempMemory((size_t)surfaceCount * sizeof(*surfaceGroupMarkers));
    memset(surfaceGroupMarkers, 0, (size_t)surfaceCount * sizeof(*surfaceGroupMarkers));

    int32_t groupMarker = 0;
    shader_t *skyShader = NULL;
    for (int32_t shaderIndex = 0; shaderIndex < rendererWorldData.numShaders; ++shaderIndex) {
        for (int32_t lightmapIndex = -1; lightmapIndex < tr.lightmapCount; ++lightmapIndex) {
            ++groupMarker;
            int32_t selectedSurfaceCount = 0;
            int32_t totalVertexCount = 0;

            for (int32_t surfaceIndex = 0; surfaceIndex < surfaceCount; ++surfaceIndex) {
                const dsurface_t *diskSurface = &diskSurfaces[surfaceIndex];
                if (diskSurface->shaderNum != shaderIndex)
                    continue;

                int32_t mappedLightmapIndex = diskSurface->lightmapNum;
                if (mappedLightmapIndex >= 0) {
                    mappedLightmapIndex = lightmapPlacements[mappedLightmapIndex].atlasIndex;
                }
                if (mappedLightmapIndex != lightmapIndex)
                    continue;

                if ((uint32_t)totalVertexCount > UINT16_MAX - (uint32_t)diskSurface->numVerts) {
                    ri.Error(ERR_DROP, "\x15"
                                       "surface group has more than 65,535 vertices");
                    ri.Hunk_FreeTempMemory(surfaceGroupMarkers);
                    return;
                }
                totalVertexCount += diskSurface->numVerts;
                ++selectedSurfaceCount;
                surfaceGroupMarkers[surfaceIndex] = groupMarker;
            }

            if (totalVertexCount == 0)
                continue;

            shader_t *shader = ShaderForShaderNum(shaderIndex, lightmapIndex, R_WORLD_SURFACE_SHADER_USAGE);
            renderer_shader_surface_build_t build;
            BeginShaderSurfaces(shader, totalVertexCount, &build);

            const qboolean isSky = shader->optimalStageIteratorFunc == RB_StageIteratorSky;
            if (isSky != qfalse) {
                if (lightmapIndex != -1) {
                    ri.Error(ERR_DROP, "\x15sky shader '%s' has a lightmap\n", shader->name);
                }
                if (skyShader != NULL) {
                    ri.Error(ERR_DROP, "\x15more than one sky shader: at least '%s' and '%s'\n", shader->name, skyShader->name);
                }
                skyShader = shader;
                rendererWorldData.skySurfaces = ri.Hunk_Alloc((size_t)selectedSurfaceCount * sizeof(*rendererWorldData.skySurfaces));
            }

            for (int32_t surfaceIndex = 0; surfaceIndex < surfaceCount; ++surfaceIndex) {
                if (surfaceGroupMarkers[surfaceIndex] != groupMarker)
                    continue;

                msurface_t *worldSurface = &rendererWorldData.surfaces[surfaceIndex];
                ParseTriangleSoup(&diskSurfaces[surfaceIndex], &build, lightmapPlacements, vertices, worldSurface, indices);

                if (isSky != qfalse) {
                    rendererWorldData.skySurfaces[rendererWorldData.skySurfaceCount++] = worldSurface;
                }
            }
        }

        Sys_PumpEvents();
    }

    ri.Hunk_FreeTempMemory(surfaceGroupMarkers);
}

/* Source: CoDUOMP.exe 0x0050c240..0x0050c36f.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0050c240_0050c36f.mcode.
 * Name, lump signature, and source-level R_AllocModel call: exact same-module
 * Mac symbol R_LoadSubmodels. Windows LTCG inlines R_AllocModel; the original
 * source creates one renderer model named "*n" for every BSP brush model. */
void R_LoadSubmodels(const lump_t *submodelLump)
{
    const dmodel_t *diskSubmodels = (const dmodel_t *)(rendererWorldFileBase + submodelLump->fileofs);
    if (((uint32_t)submodelLump->filelen % sizeof(*diskSubmodels)) != 0) {
        ri.Error(ERR_DROP, "\x15LoadMap: funny lump size in %s", rendererWorldData.name);
    }

    const int32_t submodelCount = (int32_t)((uint32_t)submodelLump->filelen / sizeof(*diskSubmodels));

    /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered engine boundary input and state before use. */
    if (submodelCount > R_MAX_MODELS - tr.modelCount) {
        ri.Error(ERR_DROP,
                 "\x15"
                 "LoadMap: too many submodels in %s",
                 rendererWorldData.name);
        return;
    }

    rendererWorldData.bmodels = ri.Hunk_Alloc((size_t)submodelCount * sizeof(*rendererWorldData.bmodels));

    for (int32_t submodelIndex = 0; submodelIndex < submodelCount; ++submodelIndex) {
        bmodel_t *bmodel = &rendererWorldData.bmodels[submodelIndex];
        model_t *model = R_AllocModel();
        model->type = MODEL_BMODEL;
        model->bmodel = bmodel;
        Com_sprintf(model->name, sizeof(model->name), "*%d", submodelIndex);

        memcpy(bmodel->bounds[0], diskSubmodels[submodelIndex].mins, sizeof(bmodel->bounds[0]));
        memcpy(bmodel->bounds[1], diskSubmodels[submodelIndex].maxs, sizeof(bmodel->bounds[1]));
        bmodel->firstSurface = &rendererWorldData.surfaces[diskSubmodels[submodelIndex].firstSurface];
        bmodel->numSurfaces = diskSubmodels[submodelIndex].numSurfaces;
    }
}

/* Source: CoDUOMP.exe 0x0050c370..0x0050c3b4.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0050c370_0050c3b4.mcode.
 * Name and ordinary node/parent signature: exact same-module Mac symbol
 * R_SetParentAndCell. An internal node inherits a concrete cell only when
 * both child subtrees resolve to the same cell. */
void R_SetParentAndCell(mnode_t *node, mnode_t *parent)
{
    node->parent = parent;
    if (node->contents != R_WORLD_NODE_NO_CELL)
        return;

    R_SetParentAndCell(node->data.node.children[0], node);
    R_SetParentAndCell(node->data.node.children[1], node);
    node->cellIndex = R_WORLD_NODE_INTERNAL;
    if (node->data.node.children[0]->cellIndex == node->data.node.children[1]->cellIndex) {
        node->cellIndex = node->data.node.children[0]->cellIndex;
    }
}

/* Source: CoDUOMP.exe 0x0050c3c0..0x0050c594.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0050c3c0_0050c594.mcode.
 * Name, two-lump signature, disk strides, and R_SetParentAndCell boundary:
 * exact same-module Mac symbol R_LoadNodesAndLeafs. */
void R_LoadNodesAndLeafs(const lump_t *nodeLump, const lump_t *leafLump)
{
    const dnode_t *diskNodes = (const dnode_t *)(rendererWorldFileBase + nodeLump->fileofs);
    const dleaf_t *diskLeafs = (const dleaf_t *)(rendererWorldFileBase + leafLump->fileofs);
    if (((uint32_t)nodeLump->filelen % sizeof(*diskNodes)) != 0 || ((uint32_t)leafLump->filelen % sizeof(*diskLeafs)) != 0) {
        ri.Error(ERR_DROP, "\x15LoadMap: funny lump size in %s", rendererWorldData.name);
    }

    const int32_t nodeCount = (int32_t)((uint32_t)nodeLump->filelen / sizeof(*diskNodes));
    const int32_t leafCount = (int32_t)((uint32_t)leafLump->filelen / sizeof(*diskLeafs));
    rendererWorldData.numDecisionNodes = nodeCount;
    rendererWorldData.numnodes = nodeCount + leafCount;
    rendererWorldData.nodes = ri.Hunk_Alloc((size_t)rendererWorldData.numnodes * sizeof(*rendererWorldData.nodes));

    for (int32_t nodeIndex = 0; nodeIndex < nodeCount; ++nodeIndex) {
        mnode_t *node = &rendererWorldData.nodes[nodeIndex];
        node->contents = R_WORLD_NODE_NO_CELL;
        node->data.node.plane = ri.CM_PlaneForIndex(diskNodes[nodeIndex].planeNum);

        for (int32_t childIndex = 0; childIndex < 2; ++childIndex) {
            const int32_t diskChild = diskNodes[nodeIndex].children[childIndex];
            if (diskChild >= 0) {
                node->data.node.children[childIndex] = &rendererWorldData.nodes[diskChild];
            } else {
                node->data.node.children[childIndex] = &rendererWorldData.nodes[nodeCount - diskChild - 1];
            }
        }
    }

    mnode_t *leafs = &rendererWorldData.nodes[nodeCount];
    for (int32_t leafIndex = 0; leafIndex < leafCount; ++leafIndex) {
        mnode_t *leaf = &leafs[leafIndex];
        leaf->cellIndex = diskLeafs[leafIndex].cellNum;
        leaf->data.leaf.cluster = diskLeafs[leafIndex].cluster;
        if (leaf->data.leaf.cluster >= rendererWorldData.numClusters) {
            rendererWorldData.numClusters = leaf->data.leaf.cluster + 1;
        }

        leaf->data.leaf.hasSunLight = qfalse;
        leaf->data.leaf.firstLightIndex = diskLeafs[leafIndex].firstLightIndex;
        leaf->data.leaf.lightCount = diskLeafs[leafIndex].lightCount;
        if (leaf->data.leaf.lightCount != 0 && rendererWorldData.lightIndexes[leaf->data.leaf.firstLightIndex] < 0) {
            leaf->data.leaf.hasSunLight = qtrue;
            ++leaf->data.leaf.firstLightIndex;
            --leaf->data.leaf.lightCount;
        }

        if (leaf->data.leaf.lightCount > R_MAX_INDEXED_LIGHTS_PER_LEAF) {
            ri.Error(ERR_DROP, "\x15R_LoadNodesAndLeafs: too many lights in leaf. The map needs to be recompiled.");
        }
    }

    R_SetParentAndCell(rendererWorldData.nodes, NULL);
}

/* Source: CoDUOMP.exe 0x0050c5a0..0x0050c61e.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0050c5a0_0050c61e.mcode.
 * Name and one-lump signature: exact same-module Mac symbol R_LoadShaders.
 * Mac byte-swaps surfaceFlags/contentFlags after the copy; the original
 * Windows body proves that this normalization is a no-op on every maintained
 * little-endian target. */
void R_LoadShaders(const lump_t *shaderLump)
{
    const dshader_t *diskShaders = (const dshader_t *)(rendererWorldFileBase + shaderLump->fileofs);
    if (((uint32_t)shaderLump->filelen % sizeof(*diskShaders)) != 0) {
        ri.Error(ERR_DROP, "\x15LoadMap: funny lump size in %s", rendererWorldData.name);
    }

    rendererWorldData.numShaders = (int32_t)((uint32_t)shaderLump->filelen / sizeof(*diskShaders));
    const size_t shaderBytes = (size_t)rendererWorldData.numShaders * sizeof(*rendererWorldData.shaders);
    rendererWorldData.shaders = ri.Hunk_Alloc(shaderBytes);
    memcpy(rendererWorldData.shaders, diskShaders, shaderBytes);
}

/* Source: CoDUOMP.exe 0x0050c620..0x0050c95e.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0050c620_0050c95e.mcode.
 * Name and one-lump signature: exact same-module Mac symbol R_LoadLights.
 * Windows proves the native little-endian field reads; Mac independently
 * proves the same disk layout through LittleLong/LittleFloat conversions. */
void R_LoadLights(const lump_t *lightLump)
{
    const float luminanceRed = 0.29899999499320984f;
    const float luminanceGreen = 0.5870000123977661f;
    const float luminanceBlue = 0.11400000005960464f;
    const float localAmbientScale = 0.10000000149011612f;
    const float localDiffuseScale = 0.800000011920929f;
    const double radiansToDegrees = 57.29577791868205; /* 180 / pi */
    const renderer_disk_light_t *diskLights = (const renderer_disk_light_t *)(rendererWorldFileBase + lightLump->fileofs);

    if (((uint32_t)lightLump->filelen % sizeof(*diskLights)) != 0) {
        ri.Error(ERR_DROP, "\x15R_LoadLights: funny lump size in %s", rendererWorldData.name);
    }

    rendererWorldData.lightCount = (int32_t)((uint32_t)lightLump->filelen / sizeof(*diskLights));
    rendererWorldData.lights = ri.Hunk_Alloc((size_t)rendererWorldData.lightCount * sizeof(*rendererWorldData.lights));
    rendererWorldData.sunLight = NULL;

    for (int32_t lightIndex = 0; lightIndex < rendererWorldData.lightCount; ++lightIndex) {
        const renderer_disk_light_t *diskLight = &diskLights[lightIndex];
        renderer_light_t *light = &rendererWorldData.lights[lightIndex];
        vec3_t scaledColor;

        light->type = diskLight->type;
        const long double scaledColor0Raw = (long double)tr.identityLight * (long double)diskLight->color[0];
        const long double scaledColor2Raw = (long double)tr.identityLight * (long double)diskLight->color[2];
        scaledColor[0] = (float)scaledColor0Raw;
        scaledColor[1] = tr.identityLight * diskLight->color[1];
        scaledColor[2] = (float)scaledColor2Raw;

        /* 0x0050c6a4..0x0050c6e3 keeps scaled color 0 live, stores color 1,
         * and stores color 2 without popping before the luminance dot. */
        const long double intensityRaw =
            (scaledColor2Raw * (long double)luminanceBlue + (long double)scaledColor[1] * (long double)luminanceGreen) +
            scaledColor0Raw * (long double)luminanceRed;
        light->intensity = (float)intensityRaw;

        const long double inverseIntensityRaw = intensityRaw != 0.0L ? 1.0L / intensityRaw : intensityRaw;
        light->color[0] = (float)(scaledColor0Raw * inverseIntensityRaw);
        light->color[1] = (float)((long double)scaledColor[1] * inverseIntensityRaw);
        light->color[2] = (float)(inverseIntensityRaw * (long double)scaledColor[2]);

        if (light->type == R_LIGHT_TYPE_SUN) {
            memset(light->ambient, 0, sizeof(light->ambient));
            memcpy(light->diffuse, rendererWorldData.sunDiffuseColor, sizeof(light->diffuse));
            light->intensity = light->diffuse[0] * luminanceRed + light->diffuse[1] * luminanceGreen + light->diffuse[2] * luminanceBlue;
            rendererWorldData.sunLight = light;
            rendererWorldData.entitySunLightIntensity = rendererWorldData.entityAmbientScale[0] * luminanceRed +
                                                        rendererWorldData.entityAmbientScale[1] * luminanceGreen +
                                                        rendererWorldData.entityAmbientScale[2] * luminanceBlue;
        } else {
            /* The surviving scaled-color-0 register feeds both 0x0050c7a8
             * and 0x0050c7d3; components 1/2 reload their float spills. */
            light->ambient[0] = (float)(scaledColor0Raw * (long double)localAmbientScale);
            light->ambient[1] = scaledColor[1] * localAmbientScale;
            light->ambient[2] = scaledColor[2] * localAmbientScale;
            light->diffuse[0] = (float)(scaledColor0Raw * (long double)localDiffuseScale);
            light->diffuse[1] = scaledColor[1] * localDiffuseScale;
            light->diffuse[2] = scaledColor[2] * localDiffuseScale;
            light->ambient[3] = 1.0f;
            light->diffuse[3] = 1.0f;
        }

        light->specular[0] = 0.0f;
        light->specular[1] = 0.0f;
        light->specular[2] = 0.0f;
        light->specular[3] = 1.0f;
        light->constantAttenuation = 0.0f;
        light->linearAttenuation = 0.0f;
        light->quadraticAttenuation = 0.0f;
        light->spotExponent = 0.0f;
        light->spotCutoff = 180.0f;

        switch (light->type) {
        case R_LIGHT_TYPE_SUN:
            memcpy(light->position, diskLight->direction, sizeof(diskLight->direction));
            light->position[3] = 0.0f;
            light->constantAttenuation = 1.0f;
            break;

        case R_LIGHT_TYPE_POINT:
            memcpy(light->position, diskLight->position, sizeof(diskLight->position));
            light->position[3] = 1.0f;
            light->quadraticAttenuation = 1.0f;
            break;

        case R_LIGHT_TYPE_LINEAR_POINT:
            memcpy(light->position, diskLight->position, sizeof(diskLight->position));
            light->position[3] = 1.0f;
            light->linearAttenuation = diskLight->parameters.linearPoint.linearAttenuation;
            break;

        case R_LIGHT_TYPE_CUSTOM_POINT:
            memcpy(light->position, diskLight->position, sizeof(diskLight->position));
            light->position[3] = 1.0f;
            light->quadraticAttenuation = diskLight->parameters.customPoint.quadraticAttenuation;
            light->constantAttenuation = diskLight->parameters.customPoint.constantAttenuation;
            break;

        case R_LIGHT_TYPE_SPOT:
            memcpy(light->position, diskLight->position, sizeof(diskLight->position));
            light->position[3] = 1.0f;
            light->quadraticAttenuation = 1.0f;
            for (int32_t component = 0; component < 3; ++component) {
                light->spotDirection[component] = -diskLight->direction[component];
            }
            light->spotCutoff = (float)(acos((double)diskLight->parameters.spot.cutoffCos) * radiansToDegrees);
            light->spotExponent = (float)diskLight->parameters.spot.exponent;
            break;

        case R_LIGHT_TYPE_CUSTOM_SPOT:
            memcpy(light->position, diskLight->position, sizeof(diskLight->position));
            light->position[3] = 1.0f;
            light->quadraticAttenuation = diskLight->parameters.customSpot.quadraticAttenuation;
            light->constantAttenuation = diskLight->parameters.customSpot.constantAttenuation;
            for (int32_t component = 0; component < 3; ++component) {
                light->spotDirection[component] = -diskLight->direction[component];
            }
            light->spotCutoff = (float)(acos((double)diskLight->parameters.customSpot.cutoffCos) * radiansToDegrees);
            light->spotExponent = (float)diskLight->parameters.customSpot.exponent;
            break;

        case R_LIGHT_TYPE_COLOR_ONLY:
        case R_LIGHT_TYPE_DIFFUSE_SUN:
        default:
            break;
        }
    }
}

/* Source: CoDUOMP.exe 0x0050c980..0x0050c9e0.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0050c980_0050c9e0.mcode.
 * Name and one-lump signature: exact same-module Mac symbol
 * R_LoadLightIndexes. Windows proves direct little-endian 16-bit copies; Mac
 * performs the corresponding LittleShort conversion. */
void R_LoadLightIndexes(const lump_t *lightIndexLump)
{
    const int16_t *diskLightIndexes = (const int16_t *)(rendererWorldFileBase + lightIndexLump->fileofs);
    if (((uint32_t)lightIndexLump->filelen % sizeof(*diskLightIndexes)) != 0) {
        ri.Error(ERR_DROP, "\x15R_LoadLightIndexes: funny lump size in %s", rendererWorldData.name);
    }

    rendererWorldData.lightIndexCount = (int32_t)((uint32_t)lightIndexLump->filelen / sizeof(*diskLightIndexes));
    const size_t lightIndexBytes = (size_t)rendererWorldData.lightIndexCount * sizeof(*rendererWorldData.lightIndexes);
    rendererWorldData.lightIndexes = ri.Hunk_Alloc(lightIndexBytes);
    memcpy(rendererWorldData.lightIndexes, diskLightIndexes, lightIndexBytes);
}

/* Source: CoDUOMP.exe 0x0050c9e0..0x0050ca14.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0050c9e0_0050ca14.mcode.
 * Name and one-lump signature: exact same-module Mac symbol
 * R_LoadLightVisCache. This function was absent from Ghidra's function list;
 * its complete Windows body was recovered from the executable-gap bytes. */
void R_LoadLightVisCache(const lump_t *lightVisLump)
{
    const renderer_light_vis_disk_entry_t *diskCache =
        (const renderer_light_vis_disk_entry_t *)(rendererWorldFileBase + lightVisLump->fileofs);

    /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if (lightVisLump->filelen > 0 && R_InitLightVisCacheFromBuffer(diskCache, lightVisLump->filelen) == qfalse) {
        ri.Error(ERR_DROP, "\x15R_LoadLightVisCache: funny lump size in %s", rendererWorldData.name);
    }
}

/* Source: CoDUOMP.exe 0x0050ca20..0x0050ca71.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0050ca20_0050ca71.mcode.
 * Name and lookup direction: exact same-module Mac symbol R_ValueForKey.
 * Windows proves the 4096-byte record stride and value offset 2048. */
const char *R_ValueForKey(int32_t pairCount, const renderer_entity_pair_t *pairs, const char *key)
{
    for (int32_t pairIndex = 1; pairIndex < pairCount; ++pairIndex) {
        if (coduo_crt_stricmp(pairs[pairIndex].key, key) == 0)
            return pairs[pairIndex].value;
    }

    return NULL;
}

/* Source: CoDUOMP.exe 0x0050ca80..0x0050ca9c.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0050ca80_0050ca9c.mcode.
 * Name and default-value behavior: exact same-module Mac symbol
 * R_FloatForKey. This was another missing Ghidra function boundary. */
float R_FloatForKey(int32_t pairCount, const renderer_entity_pair_t *pairs, const char *key, float defaultValue)
{
    const char *value = R_ValueForKey(pairCount, pairs, key);
    return value != NULL ? (float)atof(value) : defaultValue;
}

/* Source: CoDUOMP.exe 0x0050caa0..0x0050cad5.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0050caa0_0050cad5.mcode.
 * Name, boolean result, and default-text behavior: exact same-module Mac
 * symbol R_VectorForKey. This was another missing Ghidra boundary. */
qboolean R_VectorForKey(int32_t pairCount, const renderer_entity_pair_t *pairs, const char *key, const char *defaultValue, vec3_t vector)
{
    const char *value = R_ValueForKey(pairCount, pairs, key);
    const qboolean found = value != NULL ? qtrue : qfalse;
    if (value == NULL)
        value = defaultValue;

    (void)sscanf(value, "%f %f %f", &vector[0], &vector[1], &vector[2]);
    return found;
}

/* Source: CoDUOMP.exe 0x0050cae0..0x0050ccc9.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0050cae0_0050ccc9.mcode.
 * Name and source-level helper boundaries: exact same-module Mac symbol
 * R_LoadMiscModel. Windows inlines portions of R_VectorForKey but proves the
 * same keys, defaults, nonzero scalar-angle gate, shadow-model exclusion,
 * and R_CreateStaticModel call. */
void R_LoadMiscModel(int32_t pairCount, const renderer_entity_pair_t *pairs)
{
    static const char defaultOrigin[] = "0 0 0";
    static const char defaultScale[] = "1 1 1";
    static const char shadowModelPrefix[] = "xmodel/shadow_";
    enum {
        R_SHADOW_MODEL_PREFIX_LENGTH = 14
    };

    vec3_t origin;
    if (R_VectorForKey(pairCount, pairs, "origin", defaultOrigin, origin) == qfalse) {
        ri.Error(ERR_DROP, "\x15R_LoadMiscModel: no origin specified\n");
    }

    const char *model = R_ValueForKey(pairCount, pairs, "model");
    if (model == NULL) {
        ri.Error(ERR_DROP, "\x15R_LoadMiscModel: no model specified in misc_model at (%.0f %.0f %.0f)\n", origin[0], origin[1], origin[2]);
    }

    if (coduo_crt_strnicmp(model, shadowModelPrefix, R_SHADOW_MODEL_PREFIX_LENGTH) == 0) {
        return;
    }

    vec3_t angles;
    const char *angle = R_ValueForKey(pairCount, pairs, "angle");
    const double scalarAngle = angle != NULL ? atof(angle) : 0.0;
    if (scalarAngle != 0.0) {
        angles[0] = 0.0f;
        angles[1] = (float)scalarAngle;
        angles[2] = 0.0f;
    } else {
        (void)R_VectorForKey(pairCount, pairs, "angles", defaultOrigin, angles);
    }

    vec3_t scale;
    if (R_VectorForKey(pairCount, pairs, "modelscale_vec", defaultScale, scale) == qfalse) {
        const float uniformScale = R_FloatForKey(pairCount, pairs, "modelscale", 1.0f);
        scale[0] = uniformScale;
        scale[1] = uniformScale;
        scale[2] = uniformScale;
    }

    vec3_t lightingPrecalc;
    (void)R_VectorForKey(pairCount, pairs, "lightingPrecalc", defaultScale, lightingPrecalc);
    R_CreateStaticModel(model, origin, angles, scale, lightingPrecalc);
}

/* Source: CoDUOMP.exe 0x0050ccd0..0x0050cf4f.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0050ccd0_0050cf4f.mcode.
 * Name, entity keys, defaults, 512-record limit, and complete 32-byte output
 * layout: exact same-module Mac symbol R_LoadCorona plus Windows operand
 * widths and world-relative addresses. */
void R_LoadCorona(int32_t pairCount, const renderer_entity_pair_t *pairs)
{
    const float coronaScale = 25.5f;
    const float defaultZCutoff = -0.15000000596046448f;
    const float defaultZFadeOut = -0.25f;

    if (rendererWorldData.coronaCount >= R_MAX_MAP_CORONAS) {
        ri.Error(ERR_DROP, va("\x15MAX_MAP_CORONAS(%i) exceeded", R_MAX_MAP_CORONAS));
    }

    renderer_world_corona_t *corona = &rendererWorldData.coronas[rendererWorldData.coronaCount++];
    corona->shader = R_FindShader("flareShader", -1, qtrue, R_CORONA_SHADER_USAGE);

    (void)R_VectorForKey(pairCount, pairs, "origin", "0 0 0", corona->origin);

    const char *scaleText = R_ValueForKey(pairCount, pairs, "scale");
    const long double scaleRaw = scaleText != NULL ? (long double)atof(scaleText) : 1.0L;
    const float scale = (float)scaleRaw;
    /* R_FloatForKey is inlined at 0x0050cd5f..0x0050cd93: atof's value is
     * stored as float but the positivity check consumes retained ST0.
     * TEST AH,0x41 followed by JP accepts the unordered NaN case. */
    if (scaleRaw <= 0.0L) {
        ri.Error(ERR_DROP, "\x15"
                           "corona scale must be > 0");
    }
    corona->scale = scale * coronaScale;
    corona->zCutoff = R_FloatForKey(pairCount, pairs, "zcutoff", defaultZCutoff);
    corona->zFadeOut = R_FloatForKey(pairCount, pairs, "zfadeout", defaultZFadeOut);

    vec3_t color;
    (void)R_VectorForKey(pairCount, pairs, "dl_color", "1 1 1", color);
    for (int32_t component = 0; component < 3; ++component) {
        long double scaled = (long double)color[component] * (long double)255.0f;
        if (scaled < 0.0L)
            scaled = 0.0L;
        else if (scaled > 255.0L)
            scaled = 255.0L;
        corona->color[component] = (uint8_t)coduo_fp_to_i32_extended(scaled);
    }
    corona->color[3] = 255;
}

/* Source: CoDUOMP.exe 0x0050cf50..0x0050d6cb.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0050cf50_0050d6cb.mcode.
 * Name and one-lump signature: exact same-module Mac symbol R_LoadEntities.
 * Windows proves the 64 fixed key/value records, worldspawn lighting formulas,
 * classname dispatch, temporary 512-corona table, and final hunk copy. */
void R_LoadEntities(const lump_t *entityLump)
{
    const float legacyAmbientScale = 0.01568627543747425f; /* 4 / 255 */
    const char *diskEntityString = (const char *)(rendererWorldFileBase + entityLump->fileofs);

    rendererWorldData.entityString = ri.Hunk_Alloc((size_t)entityLump->filelen + 1u);
    /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered engine boundary input and state before use. */
    memcpy(rendererWorldData.entityString, diskEntityString, (size_t)entityLump->filelen);
    rendererWorldData.entityString[entityLump->filelen] = '\0';
    rendererWorldData.entityParsePoint = rendererWorldData.entityString;

    char *parse = rendererWorldData.entityParsePoint;
    char *token = Com_Parse(&parse);
    if (token[0] == '\0' || token[0] != '{')
        return;

    float ambient = 0.0f;
    vec3_t entityAmbientColor = {0.0f, 0.0f, 0.0f};
    float diffuseFraction = 0.5f;
    vec3_t sunColor = {0.0f, 0.0f, 0.0f};
    vec3_t sunDiffuseColor = {0.0f, 0.0f, 0.0f};
    qboolean hasSunDiffuseColor = qfalse;
    float sunlight = 1.0f;
    char key[R_ENTITY_KEY_CHARS];
    char value[R_ENTITY_VALUE_CHARS];

    for (;;) {
        token = Com_Parse(&parse);
        if (token[0] == '\0' || token[0] == '}')
            break;
        Q_strncpyz(key, token, (int32_t)sizeof(key));

        token = Com_Parse(&parse);
        if (token[0] == '\0' || token[0] == '}')
            break;
        Q_strncpyz(value, token, (int32_t)sizeof(value));

        if (Q_stricmp(key, "ambient") == 0) {
            const long double ambientRaw = (long double)atof(value);
            ambient = (float)ambientRaw;
            /* 0x0050d133 stores ambient as float while comparing retained
             * atof precision with the legacy-scale threshold. */
            if (ambientRaw > 2.0L) {
                ri.Printf(R_PRINT_ALL,
                          "^3WARNING: ambient too big, assuming it uses the old 0-255 scale instead of the proper 0-1 "
                          "scale (value = '%s')\n",
                          value);
                ambient *= legacyAmbientScale;
            }
        } else if (Q_stricmp(key, "_color") == 0) {
            (void)sscanf(value, "%f %f %f", &entityAmbientColor[0], &entityAmbientColor[1], &entityAmbientColor[2]);
        } else if (Q_stricmp(key, "diffuseFraction") == 0) {
            diffuseFraction = (float)atof(value);
        } else if (Q_stricmp(key, "suncolor") == 0) {
            (void)sscanf(value, "%f %f %f", &sunColor[0], &sunColor[1], &sunColor[2]);
            (void)ColorNormalize(sunColor, sunColor);
        } else if (Q_stricmp(key, "sundiffusecolor") == 0) {
            (void)sscanf(value, "%f %f %f", &sunDiffuseColor[0], &sunDiffuseColor[1], &sunDiffuseColor[2]);
            (void)ColorNormalize(sunDiffuseColor, sunDiffuseColor);
            hasSunDiffuseColor = qtrue;
        } else if (Q_stricmp(key, "sunlight") == 0) {
            sunlight = (float)atof(value);
        } else if (Q_stricmp(key, "sundirection") == 0) {
            vec3_t sunDirectionAngles;
            (void)sscanf(value, "%f %f %f", &sunDirectionAngles[0], &sunDirectionAngles[1], &sunDirectionAngles[2]);
            AngleVectors(sunDirectionAngles, tr.sunDirection, NULL, NULL);
        }
    }

    rendererWorldData.entityAmbientBase[3] = 1.0f;
    if (ambient != 0.0f && ColorNormalize(entityAmbientColor, entityAmbientColor) != 0.0f) {
        const long double ambientScale = (long double)tr.identityLight * ambient;
        for (int32_t component = 0; component < 3; ++component) {
            rendererWorldData.entityAmbientBase[component] = (float)((long double)entityAmbientColor[component] * ambientScale);
        }
        qglLightModelfv(GL_LIGHT_MODEL_AMBIENT, rendererWorldData.entityAmbientBase);
    }

    if (hasSunDiffuseColor == qfalse) {
        memcpy(sunDiffuseColor, sunColor, sizeof(sunDiffuseColor));
    }

    const long double sunScale = ((long double)sunlight - ambient) * tr.identityLight;
    const long double directScale = (1.0L - (long double)diffuseFraction) * sunScale;
    /* The third direct-color FSTP at 0x0050d3f5 consumes directScale and
     * exposes the still-live sunScale. 0x0050d3fb then multiplies that value
     * by diffuseFraction for the authored sundiffusecolor path. */
    const long double diffuseScale = sunScale * (long double)diffuseFraction;
    for (int32_t component = 0; component < 3; ++component) {
        rendererWorldData.sunDiffuseColor[component] = (float)((long double)sunColor[component] * directScale);
        rendererWorldData.entityAmbientScale[component] = (float)((long double)sunDiffuseColor[component] * diffuseScale);
    }
    rendererWorldData.sunDiffuseColor[3] = 1.0f;
    rendererWorldData.entityAmbientScale[3] = 1.0f;

    renderer_world_corona_t temporaryCoronas[R_MAX_MAP_CORONAS];
    rendererWorldData.coronas = temporaryCoronas;
    rendererWorldData.coronaCount = 0;

    renderer_entity_pair_t pairs[R_MAX_ENTITY_PAIRS];
    for (;;) {
        token = Com_Parse(&parse);
        if (token[0] == '\0' || token[0] != '{')
            break;

        pairs[0].key[0] = '\0';
        int32_t pairCount = 1;
        for (;;) {
            token = Com_Parse(&parse);
            if (token[0] == '\0' || token[0] == '}')
                break;

            if (strcmp(token, "classname") == 0) {
                strcpy(pairs[0].key, token);
                token = Com_Parse(&parse);
                strcpy(pairs[0].value, token);
                continue;
            }

            if (pairCount == R_MAX_ENTITY_PAIRS) {
                ri.Error(ERR_DROP, "\x15R_LoadEntities: MAX_SPAWN_VARS (%i) reached\n", R_MAX_ENTITY_PAIRS);
            }

            strcpy(pairs[pairCount].key, token);
            token = Com_Parse(&parse);
            strcpy(pairs[pairCount].value, token);
            ++pairCount;
        }

        if (pairs[0].key[0] == '\0') {
            ri.Error(ERR_DROP, "\x15R_LoadEntities: entity without a classname\n");
        }

        if (coduo_crt_stricmp(pairs[0].value, "misc_model") == 0) {
            R_LoadMiscModel(pairCount, pairs);
        } else if (coduo_crt_stricmp(pairs[0].value, "corona") == 0) {
            R_LoadCorona(pairCount, pairs);
        }
    }

    if (rendererWorldData.coronaCount != 0) {
        const size_t coronaBytes = (size_t)rendererWorldData.coronaCount * sizeof(*rendererWorldData.coronas);
        rendererWorldData.coronas = ri.Hunk_Alloc(coronaBytes);
        Com_Memcpy(rendererWorldData.coronas, temporaryCoronas, coronaBytes);
    } else {
        rendererWorldData.coronas = NULL;
    }
}

/* Source: CoDUOMP.exe 0x0050d6d0..0x0050d756.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0050d6d0_0050d756.mcode.
 * Name and public renderer-export signature: exact same-module Mac symbol
 * R_GetEntityToken and renderer export-table slot 45. The exhausted path
 * rewinds the parse cursor so the next caller starts at the worldspawn. */
qboolean R_GetEntityToken(char *buffer, int32_t bufferSize)
{
    char *token = Com_Parse(&rendererWorldData.entityParsePoint);
    Q_strncpyz(buffer, token, bufferSize);

    if (rendererWorldData.entityParsePoint != NULL && token[0] != '\0')
        return qtrue;

    rendererWorldData.entityParsePoint = rendererWorldData.entityString;
    return qfalse;
}

/* Source: CoDUOMP.exe 0x0050d760..0x0050d843.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0050d760_0050d843.mcode.
 * Name and recursive two-argument signature: exact same-module Mac symbol
 * R_FinishLoadingAABBTrees_r. Windows inlines ClearBounds, then expands a
 * terminal tree from surface bounds or an internal tree from its children. */
int32_t R_FinishLoadingAABBTrees_r(renderer_aabb_tree_t *tree, int32_t nextTreeIndex)
{
    const float clearBound = 262144.0f; /* exact bits 0x48800000 */
    for (int32_t component = 0; component < 3; ++component) {
        tree->mins[component] = clearBound;
        tree->maxs[component] = -clearBound;
    }

    if (tree->childCount == 0) {
        for (int32_t surfaceIndex = 0; surfaceIndex < tree->surfaceCount; ++surfaceIndex) {
            const renderer_lit_surface_t *surface = (const renderer_lit_surface_t *)tree->surfaces[surfaceIndex].data;
            ExpandBounds(surface->boundsMin, surface->boundsMax, tree->mins, tree->maxs);
        }
        return nextTreeIndex;
    }

    tree->children = &rendererWorldData.aabbTrees[nextTreeIndex];
    nextTreeIndex = (int32_t)((uint32_t)nextTreeIndex + (uint32_t)tree->childCount);
    for (int32_t childIndex = 0; childIndex < tree->childCount; ++childIndex) {
        renderer_aabb_tree_t *child = &tree->children[childIndex];
        nextTreeIndex = R_FinishLoadingAABBTrees_r(child, nextTreeIndex);
        ExpandBounds(child->mins, child->maxs, tree->mins, tree->maxs);
    }
    return nextTreeIndex;
}

/* Source: CoDUOMP.exe 0x0050d850..0x0050d90f.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0050d850_0050d90f.mcode.
 * Name and one-lump signature: exact same-module Mac symbol R_LoadAABBTrees.
 * The disk records encode child ownership by depth-first ordering; the final
 * pass resolves those spans and derives every tree's bounds. */
void R_LoadAABBTrees(const lump_t *aabbTreeLump)
{
    const renderer_disk_aabb_tree_t *diskTrees = (const renderer_disk_aabb_tree_t *)(rendererWorldFileBase + aabbTreeLump->fileofs);
    if (((uint32_t)aabbTreeLump->filelen % sizeof(*diskTrees)) != 0) {
        ri.Error(ERR_DROP, "\x15LoadMap: funny lump size in %s", rendererWorldData.name);
    }

    rendererWorldData.aabbTreeCount = (int32_t)((uint32_t)aabbTreeLump->filelen / sizeof(*diskTrees));
    rendererWorldData.aabbTrees = ri.Hunk_Alloc((size_t)rendererWorldData.aabbTreeCount * sizeof(*rendererWorldData.aabbTrees));

    for (int32_t treeIndex = 0; treeIndex < rendererWorldData.aabbTreeCount; ++treeIndex) {
        renderer_aabb_tree_t *tree = &rendererWorldData.aabbTrees[treeIndex];
        tree->surfaces = &rendererWorldData.surfaces[diskTrees[treeIndex].firstSurface];
        tree->surfaceCount = diskTrees[treeIndex].surfaceCount;
        tree->childCount = diskTrees[treeIndex].childCount;
    }

    int32_t treeIndex = 0;
    while (treeIndex < rendererWorldData.aabbTreeCount) {
        treeIndex = R_FinishLoadingAABBTrees_r(&rendererWorldData.aabbTrees[treeIndex], treeIndex + 1);
    }
}

/* Source: CoDUOMP.exe 0x0050d910..0x0050dcb6.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0050d910_0050dcb7.mcode.
 * Name and one-lump signature: exact same-module Mac symbol R_LoadCells. The
 * large Windows body is one four-record unroll of this ordinary conversion
 * loop. Portal indexes remain scalar until R_LoadPortals allocates that table;
 * every other authored index is resolved immediately to native storage. */
void R_LoadCells(const lump_t *cellLump)
{
    const renderer_disk_cell_t *diskCells = (const renderer_disk_cell_t *)(rendererWorldFileBase + cellLump->fileofs);
    if (((uint32_t)cellLump->filelen % sizeof(*diskCells)) != 0) {
        ri.Error(ERR_DROP, "\x15LoadMap: funny lump size in %s", rendererWorldData.name);
    }

    rendererWorldData.cellCount = (int32_t)((uint32_t)cellLump->filelen / sizeof(*diskCells));
    rendererWorldData.cells = ri.Hunk_Alloc((size_t)rendererWorldData.cellCount * sizeof(*rendererWorldData.cells));

    for (int32_t cellIndex = 0; cellIndex < rendererWorldData.cellCount; ++cellIndex) {
        const renderer_disk_cell_t *diskCell = &diskCells[cellIndex];
        renderer_world_cell_t *cell = &rendererWorldData.cells[cellIndex];

        memcpy(cell->mins, diskCell->mins, sizeof(cell->mins));
        memcpy(cell->maxs, diskCell->maxs, sizeof(cell->maxs));
        cell->aabbTree = &rendererWorldData.aabbTrees[diskCell->aabbTreeIndex];
        cell->portalReference.firstPortal = diskCell->firstPortal;
        cell->portalCount = diskCell->portalCount;
        cell->cullGroups = &rendererWorldData.cullGroupIndexes[diskCell->firstCullGroup];
        cell->cullGroupCount = diskCell->cullGroupCount;
        cell->occluders = &rendererWorldData.occluderIndexes[diskCell->firstOccluder];
        cell->occluderCount = diskCell->occluderCount;
    }
}

/* Source: CoDUOMP.exe 0x0050dcc0..0x0050ddcf.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0050dcc0_0050ddd0.mcode.
 * Name and one-lump signature: exact same-module Mac symbol R_LoadPortalVerts.
 * The Windows four-record unroll copies the three float words verbatim. */
void R_LoadPortalVerts(const lump_t *portalVertexLump)
{
    const vec3_t *diskVertices = (const vec3_t *)(rendererWorldFileBase + portalVertexLump->fileofs);
    if (((uint32_t)portalVertexLump->filelen % sizeof(*diskVertices)) != 0) {
        ri.Error(ERR_DROP, "\x15LoadMap: funny lump size in %s", rendererWorldData.name);
    }

    const int32_t vertexCount = (int32_t)((uint32_t)portalVertexLump->filelen / sizeof(*diskVertices));
    const uint32_t portalVertexBytes = (uint32_t)vertexCount * (uint32_t)sizeof(*rendererWorldData.portalVerts);
    rendererWorldData.portalVerts = ri.Hunk_Alloc((size_t)portalVertexBytes);

    for (int32_t vertexIndex = 0; vertexIndex < vertexCount; ++vertexIndex) {
        memcpy(rendererWorldData.portalVerts[vertexIndex], diskVertices[vertexIndex], sizeof(vec3_t));
    }
}

/* Source: CoDUOMP.exe 0x0050ddd0..0x0050df19.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0050ddd0_0050df1a.mcode.
 * Name and one-lump signature: exact same-module Mac symbol R_LoadPortals.
 * The original loader subtracts the exact float32 0.001 plane epsilon, builds
 * the three DPVS bounds selectors, and then resolves the temporary per-cell
 * first-portal values after the portal table exists. */
void R_LoadPortals(const lump_t *portalLump)
{
    const renderer_disk_portal_t *diskPortals = (const renderer_disk_portal_t *)(rendererWorldFileBase + portalLump->fileofs);
    if (((uint32_t)portalLump->filelen % sizeof(*diskPortals)) != 0) {
        ri.Error(ERR_DROP, "\x15LoadMap: funny lump size in %s", rendererWorldData.name);
    }

    rendererWorldData.portalCount = (int32_t)((uint32_t)portalLump->filelen / sizeof(*diskPortals));
    rendererWorldData.portals = ri.Hunk_Alloc((size_t)rendererWorldData.portalCount * sizeof(*rendererWorldData.portals));

    for (int32_t portalIndex = 0; portalIndex < rendererWorldData.portalCount; ++portalIndex) {
        const renderer_disk_portal_t *diskPortal = &diskPortals[portalIndex];
        renderer_portal_t *portal = &rendererWorldData.portals[portalIndex];

        /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered engine boundary input and state before use. */
        if (diskPortal->vertexCount < 0 || diskPortal->vertexCount > R_PORTAL_CLIP_VERTEX_CAPACITY) {
            ri.Error(ERR_DROP, "R_LoadPortals: portal %i has invalid vertex count %i", portalIndex, diskPortal->vertexCount);
        }

        const cplane_t *collisionPlane = ri.CM_PlaneForIndex(diskPortal->planeIndex);

        memcpy(&portal->plane, collisionPlane, sizeof(plane_t));
        R_SetPlaneSidesDPVS(&portal->plane);
        portal->cell = &rendererWorldData.cells[diskPortal->cellIndex];
        portal->vertices = &rendererWorldData.portalVerts[diskPortal->firstVertex];
        portal->vertexCount = diskPortal->vertexCount;
        portal->recursionActive = qfalse;
    }

    for (int32_t cellIndex = 0; cellIndex < rendererWorldData.cellCount; ++cellIndex) {
        renderer_world_cell_t *cell = &rendererWorldData.cells[cellIndex];
        const ptrdiff_t firstPortal = cell->portalReference.firstPortal;
        cell->portalReference.portals = &rendererWorldData.portals[firstPortal];
    }
}

/* Source: CoDUOMP.exe 0x0050df20..0x0050dfcc.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0050df20_0050dfcd.mcode.
 * Name and one-lump signature: exact same-module Mac symbol R_LoadCullGroups. */
void R_LoadCullGroups(const lump_t *cullGroupLump)
{
    const renderer_disk_cull_group_t *diskGroups = (const renderer_disk_cull_group_t *)(rendererWorldFileBase + cullGroupLump->fileofs);
    if (((uint32_t)cullGroupLump->filelen % sizeof(*diskGroups)) != 0) {
        ri.Error(ERR_DROP, "\x15LoadMap: funny lump size in %s", rendererWorldData.name);
    }

    rendererWorldData.cullGroupCount = (int32_t)((uint32_t)cullGroupLump->filelen / sizeof(*diskGroups));
    rendererWorldData.cullGroups = ri.Hunk_Alloc((size_t)rendererWorldData.cullGroupCount * sizeof(*rendererWorldData.cullGroups));

    for (int32_t groupIndex = 0; groupIndex < rendererWorldData.cullGroupCount; ++groupIndex) {
        const renderer_disk_cull_group_t *diskGroup = &diskGroups[groupIndex];
        renderer_cull_group_t *group = &rendererWorldData.cullGroups[groupIndex];

        memcpy(group->mins, diskGroup->mins, sizeof(group->mins));
        memcpy(group->maxs, diskGroup->maxs, sizeof(group->maxs));
        group->surfaces = &rendererWorldData.surfaces[diskGroup->firstSurface];
        group->surfaceCount = diskGroup->surfaceCount;
    }
}

/* Source: CoDUOMP.exe 0x0050dfd0..0x0050e03c.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0050dfd0_0050e03d.mcode.
 * Name and one-lump signature: exact same-module Mac symbol
 * R_LoadCullGroupIndexes. Serialized int32 indexes become native pointers. */
void R_LoadCullGroupIndexes(const lump_t *indexLump)
{
    const int32_t *diskIndexes = (const int32_t *)(rendererWorldFileBase + indexLump->fileofs);
    if (((uint32_t)indexLump->filelen % sizeof(*diskIndexes)) != 0) {
        ri.Error(ERR_DROP, "\x15LoadMap: funny lump size in %s", rendererWorldData.name);
    }

    rendererWorldData.cullGroupIndexCount = (int32_t)((uint32_t)indexLump->filelen / sizeof(*diskIndexes));
    rendererWorldData.cullGroupIndexes =
        ri.Hunk_Alloc((size_t)rendererWorldData.cullGroupIndexCount * sizeof(*rendererWorldData.cullGroupIndexes));

    for (int32_t index = 0; index < rendererWorldData.cullGroupIndexCount; ++index) {
        rendererWorldData.cullGroupIndexes[index] = &rendererWorldData.cullGroups[diskIndexes[index]];
    }
}

/* Source: CoDUOMP.exe 0x0050e040..0x0050e2c8.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0050e040_0050e2c9.mcode.
 * Name and three-lump signature: exact same-module Mac symbol R_LoadOccluders.
 * Plane-index words select collision planes; each four-byte edge record then
 * selects two of those resolved planes and two portal vertices. */
void R_LoadOccluders(const lump_t *occluderLump, const lump_t *planeIndexLump, const lump_t *edgeLump)
{
    const renderer_disk_occluder_t *diskOccluders = (const renderer_disk_occluder_t *)(rendererWorldFileBase + occluderLump->fileofs);
    const int32_t *diskPlaneIndexes = (const int32_t *)(rendererWorldFileBase + planeIndexLump->fileofs);
    const renderer_disk_occluder_edge_t *diskEdges = (const renderer_disk_occluder_edge_t *)(rendererWorldFileBase + edgeLump->fileofs);

    if (((uint32_t)occluderLump->filelen % sizeof(*diskOccluders)) != 0) {
        ri.Error(ERR_DROP, "\x15LoadMap: funny lump size in %s", rendererWorldData.name);
    }

    const int32_t occluderCount = (int32_t)((uint32_t)occluderLump->filelen / sizeof(*diskOccluders));
    renderer_occluder_t *occluders = ri.Hunk_Alloc((size_t)occluderCount * sizeof(*occluders));

    if (((uint32_t)planeIndexLump->filelen % sizeof(*diskPlaneIndexes)) != 0) {
        ri.Error(ERR_DROP, "\x15LoadMap: funny lump size in %s", rendererWorldData.name);
    }

    const int32_t planeCount = (int32_t)((uint32_t)planeIndexLump->filelen / sizeof(*diskPlaneIndexes));
    renderer_dpvs_plane_t *planes = ri.Hunk_Alloc((size_t)planeCount * sizeof(*planes));

    if (((uint32_t)edgeLump->filelen % sizeof(*diskEdges)) != 0) {
        ri.Error(ERR_DROP, "\x15LoadMap: funny lump size in %s", rendererWorldData.name);
    }

    const int32_t edgeCount = (int32_t)((uint32_t)edgeLump->filelen / sizeof(*diskEdges));
    renderer_occluder_edge_t *edges = ri.Hunk_Alloc((size_t)edgeCount * sizeof(*edges));

    rendererWorldData.occluderCount = occluderCount;
    rendererWorldData.occluders = occluders;

    for (int32_t occluderIndex = 0; occluderIndex < occluderCount; ++occluderIndex) {
        const renderer_disk_occluder_t *diskOccluder = &diskOccluders[occluderIndex];
        renderer_occluder_t *occluder = &occluders[occluderIndex];

        occluder->planes = &planes[diskOccluder->firstPlane];
        occluder->planeCount = diskOccluder->planeCount;
        for (int32_t planeIndex = 0; planeIndex < occluder->planeCount; ++planeIndex) {
            renderer_dpvs_plane_t *plane = &occluder->planes[planeIndex];
            const cplane_t *collisionPlane = ri.CM_PlaneForIndex(diskPlaneIndexes[diskOccluder->firstPlane + planeIndex]);

            memcpy(plane, collisionPlane, sizeof(plane_t));
            R_SetPlaneSidesDPVS(plane);
        }

        occluder->edgeCount = diskOccluder->edgeCount;
        occluder->edges = &edges[diskOccluder->firstEdge];
        occluder->vertexCount = diskOccluder->vertexCount;
        occluder->vertices = &rendererWorldData.portalVerts[diskOccluder->firstVertex];

        for (int32_t edgeIndex = 0; edgeIndex < occluder->edgeCount; ++edgeIndex) {
            /* 0x0050e237 reloads the saved start of the disk-edge lump and
             * advances that pointer by four bytes per local edge.  The
             * occluder's firstEdge is used only for the runtime destination
             * span at 0x0050e21f..0x0050e22b; adding it to this input lookup
             * resolves the disk-local plane and vertex bytes twice. */
            const renderer_disk_occluder_edge_t *diskEdge = &diskEdges[edgeIndex];
            renderer_occluder_edge_t *edge = &occluder->edges[edgeIndex];

            edge->planes[0] = &occluder->planes[diskEdge->planeIndices[0]];
            edge->planes[1] = &occluder->planes[diskEdge->planeIndices[1]];
            edge->vertices[0] = &occluder->vertices[diskEdge->vertexIndices[0]];
            edge->vertices[1] = &occluder->vertices[diskEdge->vertexIndices[1]];
        }

        occluder->activePlaneCount = 0;
        occluder->activePlanes = NULL;
    }
}

/* Source: CoDUOMP.exe 0x0050e2d0..0x0050e33c.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0050e2d0_0050e33d.mcode.
 * Name and one-lump signature: exact same-module Mac symbol
 * R_LoadOccluderIndexes. Serialized signed 16-bit indexes become native
 * pointers into the runtime occluder table. */
void R_LoadOccluderIndexes(const lump_t *indexLump)
{
    const int16_t *diskIndexes = (const int16_t *)(rendererWorldFileBase + indexLump->fileofs);
    if (((uint32_t)indexLump->filelen % sizeof(*diskIndexes)) != 0) {
        ri.Error(ERR_DROP, "\x15LoadMap: funny lump size in %s", rendererWorldData.name);
    }

    rendererWorldData.occluderIndexCount = (int32_t)((uint32_t)indexLump->filelen / sizeof(*diskIndexes));
    rendererWorldData.occluderIndexes =
        ri.Hunk_Alloc((size_t)rendererWorldData.occluderIndexCount * sizeof(*rendererWorldData.occluderIndexes));

    for (int32_t index = 0; index < rendererWorldData.occluderIndexCount; ++index) {
        rendererWorldData.occluderIndexes[index] = &rendererWorldData.occluders[diskIndexes[index]];
    }
}

/* Source: CoDUOMP.exe 0x0050e340..0x0050e774.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0050e340_0050e775.mcode.
 * Name and public two-argument renderer-export signature: exact same-module
 * Mac symbol RE_LoadWorldMap and renderer export-table slot 6. Windows proves
 * the complete 33-lump call order, every loading-screen update, the version
 * and checksum gates, and the final world publication. Its source-level
 * R_FlushSun and R_LoadLightVisCache calls were inlined by LTCG. */
void RE_LoadWorldMap(const char *name, int32_t *checksum)
{
    void *fileBuffer = NULL;
    renderer_lightmap_placement_t lightmapPlacements[R_MAX_LIGHTMAPS];

    rendererFogCount = 0;
    if (tr.worldMapLoaded != qfalse) {
        ri.Error(ERR_DROP, "\x15"
                           "ERROR: attempted to redundantly load world map\n");
    }

    R_InitStaticModelCache();
    tr.sunDirection[0] = 0.44999998807907104f; /* 0x3ee66666 */
    tr.sunDirection[1] = 0.30000001192092896f; /* 0x3e99999a */
    tr.sunDirection[2] = 0.89999997615814209f; /* 0x3f666666 */
    R_FlushSun();
    tr.sunName[0] = '\0';
    rendererFogs[R_FOG_SKYBOX_VIEW].registered = qfalse;
    rendererFogs[R_FOG_PORTAL_VIEW].registered = qfalse;
    rendererFogs[R_FOG_RESET_SLOT].registered = qfalse;
    rendererFogs[R_FOG_WORLD_VIEW].registered = qfalse;
    rendererFogs[R_FOG_TRANSITION_TO].registered = qfalse;
    rendererFogs[R_FOG_CONFIG_VIEW].registered = qfalse;
    (void)VectorNormalize(tr.sunDirection);

    tr.worldMapLoaded = qtrue;
    const int32_t fileLength = ri.FS_ReadFile(name, &fileBuffer);
    if (fileBuffer == NULL) {
        ri.Error(ERR_DROP,
                 "\x15"
                 "RE_LoadWorldMap: %s not found",
                 name);
    }

    if (checksum != NULL) {
        *checksum = (int32_t)Com_BlockChecksum(fileBuffer, fileLength);
    }

    tr.world = NULL;
    memset(&rendererWorldData, 0, sizeof(rendererWorldData));
    strncpy(rendererWorldData.name, name, sizeof(rendererWorldData.name) - 1U);
    rendererWorldData.name[sizeof(rendererWorldData.name) - 1U] = '\0';

    const char *baseName = rendererWorldData.name;
    for (const char *cursor = rendererWorldData.name; *cursor != '\0'; ++cursor) {
        if (*cursor == '/')
            baseName = cursor + 1;
    }
    strncpy(rendererWorldData.baseName, baseName, sizeof(rendererWorldData.baseName) - 1U);
    rendererWorldData.baseName[sizeof(rendererWorldData.baseName) - 1U] = '\0';
    char *extension = strchr(rendererWorldData.baseName, '.');
    if (extension != NULL)
        *extension = '\0';

    R_SetSkyBox(&rendererWorldData.skyVertexStorage);
    uint8_t *const hunkStart = (uint8_t *)ri.Hunk_Alloc(0);
    /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered engine boundary input and state before use. */
    const int32_t invalidLump = coduo_compat_bsp_invalid_lump_index(fileBuffer, fileLength);
    if (invalidLump == CODUO_BSP_VALIDATION_SHORT_HEADER) {
        ri.Error(ERR_DROP, "RE_LoadWorldMap: %s has a truncated BSP header", name);
    }
    if (invalidLump >= 0) {
        ri.Error(ERR_DROP, "RE_LoadWorldMap: %s has invalid BSP lump %i", name, invalidLump);
    }

    const dheader_t *header = (const dheader_t *)fileBuffer;
    rendererWorldFileBase = (uint8_t *)fileBuffer;

    /* All maintained targets are little-endian. The original Windows
     * LittleLong pass over the 68 header words consequently compiled into an
     * empty counted loop; direct typed reads preserve those exact values. */
    if (header->version != BSP_VERSION) {
        ri.Error(ERR_DROP, va("EXE_ERR_WRONG_MAP_VERSION_NUM\x15%s\x15(%i "
                              "\x14"
                              "EXE_ERR_SHOULD_BE\x15 %i)",
                              name, header->version, BSP_VERSION));
    }

    ri.Cmd_ExecuteText(EXEC_NOW, "updatescreen\n");
    R_LoadShaders(&header->lumps[BSP_LUMP_SHADERS]);

    /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered engine boundary input and state before use. */
    if (coduomp_renderer_validate_triangle_soup_records(&header->lumps[BSP_LUMP_SURFACES], &header->lumps[BSP_LUMP_DRAW_VERTICES],
                                                        &header->lumps[BSP_LUMP_DRAW_INDICES]) == qfalse) {
        ri.FS_FreeFile(fileBuffer);
        tr.worldMapLoaded = qfalse;
        return;
    }

    ri.Cmd_ExecuteText(EXEC_NOW, "updatescreen\n");
    R_LoadLightmaps(&header->lumps[BSP_LUMP_LIGHTMAPS], &header->lumps[BSP_LUMP_SURFACES], lightmapPlacements);

    ri.Cmd_ExecuteText(EXEC_NOW, "updatescreen\n");
    ri.Printf(R_PRINT_ALL, "Loading surfaces...\n");
    R_LoadSurfaces(&header->lumps[BSP_LUMP_SURFACES], &header->lumps[BSP_LUMP_DRAW_VERTICES], &header->lumps[BSP_LUMP_DRAW_INDICES],
                   lightmapPlacements);

    ri.Cmd_ExecuteText(EXEC_NOW, "updatescreen\n");
    ri.Printf(R_PRINT_ALL, "Loading cull groups...\n");
    R_LoadCullGroups(&header->lumps[BSP_LUMP_CULL_GROUPS]);

    ri.Cmd_ExecuteText(EXEC_NOW, "updatescreen\n");
    R_LoadCullGroupIndexes(&header->lumps[BSP_LUMP_CULL_GROUP_INDICES]);

    ri.Cmd_ExecuteText(EXEC_NOW, "updatescreen\n");
    ri.Printf(R_PRINT_ALL, "Loading visibility info...\n");
    R_LoadPortalVerts(&header->lumps[BSP_LUMP_PORTAL_VERTICES]);

    ri.Cmd_ExecuteText(EXEC_NOW, "updatescreen\n");
    R_LoadOccluders(&header->lumps[BSP_LUMP_OCCLUDERS], &header->lumps[BSP_LUMP_OCCLUDER_PLANES], &header->lumps[BSP_LUMP_OCCLUDER_EDGES]);

    ri.Cmd_ExecuteText(EXEC_NOW, "updatescreen\n");
    R_LoadOccluderIndexes(&header->lumps[BSP_LUMP_OCCLUDER_INDICES]);

    ri.Cmd_ExecuteText(EXEC_NOW, "updatescreen\n");
    R_LoadAABBTrees(&header->lumps[BSP_LUMP_AABB_TREES]);
    R_LoadCells(&header->lumps[BSP_LUMP_CELLS]);

    ri.Cmd_ExecuteText(EXEC_NOW, "updatescreen\n");
    R_LoadPortals(&header->lumps[BSP_LUMP_PORTALS]);

    ri.Cmd_ExecuteText(EXEC_NOW, "updatescreen\n");
    R_LoadLightIndexes(&header->lumps[BSP_LUMP_LIGHT_INDICES]);

    ri.Cmd_ExecuteText(EXEC_NOW, "updatescreen\n");
    R_LoadNodesAndLeafs(&header->lumps[BSP_LUMP_NODES], &header->lumps[BSP_LUMP_LEAFS]);

    ri.Cmd_ExecuteText(EXEC_NOW, "updatescreen\n");
    ri.Printf(R_PRINT_ALL, "Loading models and entities...\n");
    R_LoadSubmodels(&header->lumps[BSP_LUMP_MODELS]);

    ri.Cmd_ExecuteText(EXEC_NOW, "updatescreen\n");
    R_LoadEntities(&header->lumps[BSP_LUMP_ENTITIES]);

    ri.Cmd_ExecuteText(EXEC_NOW, "updatescreen\n");
    ri.Printf(R_PRINT_ALL, "Loading lights...\n");
    R_LoadLights(&header->lumps[BSP_LUMP_LIGHTS]);

    ri.Cmd_ExecuteText(EXEC_NOW, "updatescreen\n");
    R_LoadLightVisCache(&header->lumps[BSP_LUMP_LIGHT_VIS_CACHE]);

    ri.Cmd_ExecuteText(EXEC_NOW, "updatescreen\n");
    rendererWorldData.dataSize = (int32_t)((uint8_t *)ri.Hunk_Alloc(0) - hunkStart);
    tr.world = &rendererWorldData;

    if (r_vc_compile->integer != 0)
        R_PrecalcLightVisCache(checksum);
    R_InitLightVisHistory();

    if (tr.sunName[0] != '\0')
        R_LoadSunThroughCvars(tr.sunName);

    ri.FS_FreeFile(fileBuffer);
}
