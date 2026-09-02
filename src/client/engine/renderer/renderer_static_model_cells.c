#include "backend.h"

#include "math/q_math.h"

enum renderer_model_box_side_e {
    R_MODEL_BOX_SIDE_FRONT = 1,
    R_MODEL_BOX_SIDE_BACK = 2,
    R_MODEL_BOX_SIDE_CROSS = 3,
    R_MODEL_AXIAL_PLANE_TYPE_COUNT = 3
};

/* Source: CoDUOMP.exe 0x005183a0..0x005183d0, exporter-gap recovery.
 * Name: exact same-module Mac static symbol R_AddModelToCell. The maintained
 * name distinguishes this load-time static-model linker from the different
 * per-view helper with the same original static name. The Windows compiler
 * also inlines it into R_FilterModelIntoCells_r at
 * 0x0051844a..0x00518481. Both binaries prove that only the current list head
 * is checked for duplicate model identity before a new eight-byte link is
 * prepended. */
void R_AddStaticModelToCell(world_t *world, renderer_static_model_t *model, int32_t cellIndex)
{
    renderer_world_cell_t *cell = &world->cells[cellIndex];
    renderer_cell_model_link_t *link = cell->modelLinks;

    if (link != NULL && link->model == model)
        return;

    link = ri.Hunk_Alloc(sizeof(*link));
    link->model = model;
    link->next = cell->modelLinks;
    cell->modelLinks = link;
}

/* Source: CoDUOMP.exe 0x005183e0..0x00518519.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_005183e0_0051851a.mcode.
 * Name and source-level R_AddModelToCell boundary: exact same-module Mac
 * static symbol R_FilterModelIntoCells_r. The maintained name distinguishes
 * this load-time static-model filter from the per-view entity filter with the
 * same original static name. The Windows optimizer inlines the leaf insertion
 * and turns several recursive calls into tail traversal. Its instructions
 * remain authoritative for the -2/-1 cell sentinels, axial bounds splitting,
 * side-to-child mapping, and allocation behavior. */
void R_FilterStaticModelIntoCells_r(world_t *world, const mnode_t *node, renderer_static_model_t *model, const vec3_t mins,
                                    const vec3_t maxs)
{
    while (node->cellIndex == R_WORLD_NODE_INTERNAL) {
        const int32_t side = BoxOnPlaneSide(mins, maxs, node->data.node.plane);

        if (side != R_MODEL_BOX_SIDE_CROSS) {
            node = node->data.node.children[side - R_MODEL_BOX_SIDE_FRONT];
            continue;
        }

        if (node->data.node.plane->type >= R_MODEL_AXIAL_PLANE_TYPE_COUNT) {
            R_FilterStaticModelIntoCells_r(world, node->data.node.children[0], model, mins, maxs);
            R_FilterStaticModelIntoCells_r(world, node->data.node.children[1], model, mins, maxs);
            return;
        }

        vec3_t frontMins = {mins[0], mins[1], mins[2]};
        vec3_t backMaxs = {maxs[0], maxs[1], maxs[2]};
        const int32_t axis = node->data.node.plane->type;

        frontMins[axis] = node->data.node.plane->dist;
        backMaxs[axis] = node->data.node.plane->dist;

        if (frontMins[axis] < maxs[axis]) {
            R_FilterStaticModelIntoCells_r(world, node->data.node.children[0], model, frontMins, maxs);
        }
        R_FilterStaticModelIntoCells_r(world, node->data.node.children[1], model, mins, backMaxs);
        return;
    }

    if (node->cellIndex >= 0)
        R_AddStaticModelToCell(world, model, node->cellIndex);
}
