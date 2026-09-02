#include "collision_queries.h"

#include "compat/coduo_x87emu.h"
#include "qcommon/q_shared_types.h"

#include <stddef.h>
#include <stdint.h>

extern int32_t cm_numSubModels;
extern collisionModel_t *cm_models;
extern int32_t cm_numClusters;
extern char *cm_entityString;
extern collisionLeaf_t *cm_leafs;
extern collisionNode_t *cm_nodes;
extern int32_t cm_numBrushes;
extern collisionBrush_t *cm_brushes;
extern collisionBrush_t *cm_boxBrush;
extern collisionModel_t cm_boxModel;
extern int32_t cm_numLeafBrushes;
extern int32_t *cm_leafbrushes;
extern int32_t cm_clusterBytes;
extern uint8_t *cm_visibility;
extern qboolean cm_visibilityLoaded;
extern cplane_t *cm_planes;

void Com_Error(errorParm_t code, const char *format, ...);

/*
 * Collision-model and public query core shared by the Windows client/listen
 * server and Linux dedicated server.  The retained model-handle blocks are
 * CoDUOMP.exe 0x0041d7e0..0x0041da7e and coduo_lnxded
 * 0x0804bc58..0x0804c018.  The short metadata accessors agree directly after
 * relocation and ABI scheduling are removed.  Supporting Mac symbols provide
 * the canonical CM_ClipHandleToModel, CM_InlineModel, CM_NumInlineModels,
 * CM_EntityString, CM_LeafCluster, CM_LeafArea, CM_InitBoxHull,
 * CM_TempBoxModel, CM_TempBoxModelContents, CM_ModelBounds, and
 * CM_SignbitsForNormal names.
 */

const collisionModel_t *CM_ClipHandleToModel(int32_t handle)
{
    if (handle < 0) {
        Com_Error(ERR_DROP,
                  "\x15" "CM_ClipHandleToModel: bad handle %i", handle);
    }

    if (handle < cm_numSubModels) {
        return &cm_models[handle];
    }

    if (handle == CM_TEMP_BOX_MODEL_HANDLE ||
        handle == CM_TEMP_CAPSULE_MODEL_HANDLE) {
        return &cm_boxModel;
    }

    if (handle < CM_MAX_CLIP_HANDLE) {
        Com_Error(ERR_DROP,
                  "\x15"
                  "CM_ClipHandleToModel: bad handle %i < %i < %i",
                  cm_numSubModels, handle, CM_MAX_CLIP_HANDLE);
    }

    Com_Error(ERR_DROP,
              "\x15" "CM_ClipHandleToModel: bad handle %i",
              handle + CM_MAX_CLIP_HANDLE);
    return NULL;
}

int32_t CM_InlineModel(int32_t modelNum)
{
    if (modelNum < 0 || modelNum >= cm_numSubModels) {
        Com_Error(ERR_DROP, "\x15" "CM_InlineModel: bad number");
    }
    return modelNum;
}

int32_t CM_NumClusters(void)
{
    return cm_numClusters;
}

int32_t CM_NumInlineModels(void)
{
    return cm_numSubModels;
}

char *CM_EntityString(void)
{
    return cm_entityString;
}

int32_t CM_LeafCluster(int32_t leafNum)
{
    return cm_leafs[leafNum].cluster;
}

int32_t CM_LeafArea(int32_t leafNum)
{
    return cm_leafs[leafNum].area;
}

void CM_InitBoxHull(void)
{
    cm_boxBrush = &cm_brushes[cm_numBrushes];
    cm_boxBrush->nonAxialSideCount = 0;
    cm_boxBrush->nonAxialSides = NULL;
    cm_boxBrush->contents = -1;

    for (int32_t side = 0; side < 6; ++side) {
        cm_boxBrush->axialMaterialIndices[side] = -1;
    }

    cm_boxModel.leaf.numLeafBrushes = 1;
    cm_boxModel.leaf.firstLeafBrush = cm_numLeafBrushes;
    cm_leafbrushes[cm_numLeafBrushes] = cm_numBrushes;
}

int32_t CM_TempBoxModel(const vec3_t mins, const vec3_t maxs,
                        int32_t contents, qboolean capsule)
{
    for (int32_t axis = 0; axis < 3; ++axis) {
        cm_boxModel.mins[axis] = mins[axis];
        cm_boxModel.maxs[axis] = maxs[axis];
        cm_boxBrush->mins[axis] = mins[axis];
        cm_boxBrush->maxs[axis] = maxs[axis];
    }

    cm_boxBrush->contents = contents;
    return capsule != qfalse
               ? CM_TEMP_CAPSULE_MODEL_HANDLE
               : CM_TEMP_BOX_MODEL_HANDLE;
}

int32_t CM_TempBoxModelContents(void)
{
    return cm_boxBrush->contents;
}

void CM_ModelBounds(int32_t modelHandle, vec3_t mins, vec3_t maxs)
{
    const collisionModel_t *const model =
        CM_ClipHandleToModel(modelHandle);

    for (int32_t axis = 0; axis < 3; ++axis) {
        mins[axis] = model->mins[axis];
        maxs[axis] = model->maxs[axis];
    }
}

uint32_t CM_SignbitsForNormal(const vec3_t normal)
{
    uint32_t signbits = 0;
    for (int32_t axis = 0; axis < 3; ++axis) {
        if (normal[axis] < 0.0f) {
            signbits |= UINT32_C(1) << axis;
        }
    }
    return signbits;
}

/*
 * CoDUOMP.exe 0x004256a0 compares the live x87 subtract/dot result.  The Linux
 * body at 0x08057184 stores that value to binary32 at 0x080571d0/0x08057205
 * and reloads it before the branch.  Keep this genuine result-affecting split
 * at the whole-function boundary.
 */
#if defined(WINDOWS_BEHAVIOR)
int32_t CM_PointLeafnum_r(const vec3_t point, int32_t nodeNum)
{
    while (nodeNum >= 0) {
        const collisionNode_t *const node = &cm_nodes[nodeNum];
        const cplane_t *const plane = node->plane;
#if EMULATE_X87
        x87f distance;
        if (plane->type < 3) {
            distance = x87f_sub(x87f_load_f32(point[plane->type]),
                                x87f_load_f32(plane->dist));
        } else {
            distance = x87f_sub(
                x87f_add(
                    x87f_add(
                        x87f_mul(x87f_load_f32(plane->normal[2]),
                                 x87f_load_f32(point[2])),
                        x87f_mul(x87f_load_f32(plane->normal[1]),
                                 x87f_load_f32(point[1]))),
                    x87f_mul(x87f_load_f32(plane->normal[0]),
                             x87f_load_f32(point[0]))),
                x87f_load_f32(plane->dist));
        }
        nodeNum = x87f_lt(distance, x87f_load_f32(0.0f))
                      ? node->children[1]
                      : node->children[0];
#else
        long double distance;
        if (plane->type < 3) {
            distance = (long double)point[plane->type] -
                       (long double)plane->dist;
        } else {
            distance =
                (((long double)plane->normal[2] *
                      (long double)point[2] +
                  (long double)plane->normal[1] *
                      (long double)point[1]) +
                 (long double)plane->normal[0] *
                     (long double)point[0]) -
                (long double)plane->dist;
        }
        nodeNum = distance < 0.0L
                      ? node->children[1]
                      : node->children[0];
#endif
    }
    return -1 - nodeNum;
}
#else
int32_t CM_PointLeafnum_r(const vec3_t point, int32_t nodeNum)
{
    while (nodeNum >= 0) {
        const collisionNode_t *const node = &cm_nodes[nodeNum];
        const cplane_t *const plane = node->plane;
        float distance;
#if EMULATE_X87
        x87f computed;
        if (plane->type < 3) {
            computed = x87f_sub(x87f_load_f32(point[plane->type]),
                                x87f_load_f32(plane->dist));
        } else {
            computed = x87f_sub(
                x87f_add(
                    x87f_add(
                        x87f_mul(x87f_load_f32(plane->normal[0]),
                                 x87f_load_f32(point[0])),
                        x87f_mul(x87f_load_f32(plane->normal[1]),
                                 x87f_load_f32(point[1]))),
                    x87f_mul(x87f_load_f32(plane->normal[2]),
                             x87f_load_f32(point[2]))),
                x87f_load_f32(plane->dist));
        }
        distance = x87f_store_f32(computed);
#else
        if (plane->type < 3) {
            distance = point[plane->type] - plane->dist;
        } else {
            distance = ((plane->normal[0] * point[0]) +
                        (plane->normal[1] * point[1])) +
                       (plane->normal[2] * point[2]) - plane->dist;
        }
#endif
        nodeNum = distance < 0.0f
                      ? node->children[1]
                      : node->children[0];
    }
    return -1 - nodeNum;
}
#endif

int32_t CM_PointLeafnum(const vec3_t point)
{
    return CM_PointLeafnum_r(point, 0);
}

const uint8_t *CM_ClusterPVS(int32_t cluster)
{
    /* CoDUOMP.exe 0x00425b60; coduo_lnxded 0x0805784e.  Invalid clusters and
     * an absent visibility lump both select row zero's base. */
    if (cluster < 0 || cluster >= cm_numClusters ||
        cm_visibilityLoaded == qfalse) {
        return cm_visibility;
    }
    return cm_visibility + cluster * cm_clusterBytes;
}

cplane_t *CM_PlaneForIndex(int32_t planeIndex)
{
    /* CoDUOMP.exe 0x0042bba0; coduo_lnxded 0x0805f986. */
    return &cm_planes[planeIndex];
}
