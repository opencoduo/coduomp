#include "backend.h"

#include "gl_state.h"

/* Source: CoDUOMP.exe 0x0051ce60..0x0051cec6.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0051ce60_0051cec7.mcode.
 * Name and common node/leaf traversal role: same-module Mac symbol
 * R_PointInLeaf. The Windows instructions prove the root at world +0x98,
 * internal-node sentinel -1, plane at +0x0c, and child pointers at +0x10.
 * The distance is accumulated Z, Y, X before subtracting the plane distance;
 * zero, negative, and unordered values all select child one. */
mnode_t *R_PointInLeaf(const vec3_t point)
{
    mnode_t *node;

    if (tr.world == NULL)
        ri.Error(ERR_DROP, "R_PointInLeaf: bad model");

    node = tr.world->nodes;
    while (node->contents == R_WORLD_NODE_NO_CELL) {
        const cplane_t *plane = node->data.node.plane;
        const long double distance =
            (((long double)point[2] * plane->normal[2] +
              (long double)point[1] * plane->normal[1]) +
             (long double)point[0] * plane->normal[0]) -
            (long double)plane->dist;
        node = distance > 0.0f
                   ? node->data.node.children[0]
                   : node->data.node.children[1];
    }
    return node;
}
