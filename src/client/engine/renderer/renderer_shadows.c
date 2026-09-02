#include "backend.h"

#include "gl_api.h"
#include "gl_state.h"
#include "renderer_cvars.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

enum {
    R_MAX_SHADOW_VERTICES = R_MAX_TESS_VERTICES / 2,
    R_MAX_SHADOW_EDGES_PER_VERTEX = 32,
    R_MAX_SHADOW_TRIANGLES = R_MAX_TESS_INDEXES / 3
};

typedef struct renderer_shadow_edge_s {
    int32_t secondVertex;
    qboolean facing;
} renderer_shadow_edge_t;

#if UINTPTR_MAX == UINT32_MAX
_Static_assert(_Alignof(renderer_shadow_edge_t) == 4,
               "i386 shadow-edge alignment changed");
_Static_assert(offsetof(renderer_shadow_edge_t, secondVertex) == 0x00,
               "i386 shadow-edge second vertex moved");
_Static_assert(offsetof(renderer_shadow_edge_t, facing) == 0x04,
               "i386 shadow-edge facing flag moved");
_Static_assert(sizeof(renderer_shadow_edge_t) == 0x08,
               "i386 shadow-edge size changed");
#endif

/* Original Windows storage: triangle-facing flags at 0x027937d8, the
 * 65,536-by-32 edge table at 0x028137d8, and per-vertex edge counts at
 * 0x038137d8. Only the first half of the tessellation vertices may be used by
 * a shadow volume because its extruded copy occupies the second half. */
static qboolean
    rendererShadowTriangleFacing[R_MAX_SHADOW_TRIANGLES]; /* 0x027937d8 */
static renderer_shadow_edge_t
    rendererShadowEdges[R_MAX_TESS_VERTICES]
                       [R_MAX_SHADOW_EDGES_PER_VERTEX]; /* 0x028137d8 */
static int32_t
    rendererShadowEdgeCounts[R_MAX_TESS_VERTICES]; /* 0x038137d8 */

/* Source: CoDUOMP.exe 0x004e8710..0x004e8842.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004e8710_004e8843.mcode.
 * Name: exact same-module Mac symbol RB_ProjectionShadowDeform. The three
 * orientation-axis Z components form world-up in the entity's local frame;
 * every tessellation vertex is projected along the adjusted entity light
 * direction onto the entity's world-Z shadow plane. */
void RB_ProjectionShadowDeform(void)
{
    const float groundDistance =
        backEnd.orientation.origin[2] -
        backEnd.currentEntity->e.shadowPlane;
    const vec3_t groundNormal = {
        backEnd.orientation.axis[0][2],
        backEnd.orientation.axis[1][2],
        backEnd.orientation.axis[2][2]
    };
    long double lightDirectionRaw[3] = {
        (long double)backEnd.currentEntity->lightDir[0],
        (long double)backEnd.currentEntity->lightDir[1],
        (long double)backEnd.currentEntity->lightDir[2]
    };
    vec3_t projectedDirection;
    long double lightDotRaw =
        (lightDirectionRaw[2] * (long double)groundNormal[2] +
         lightDirectionRaw[1] * (long double)groundNormal[1]) +
        lightDirectionRaw[0] * (long double)groundNormal[0];
    float roundedLightDirectionZ =
        backEnd.currentEntity->lightDir[2];

    if (lightDotRaw < 0.5L) {
        const long double adjustment = 0.5L - lightDotRaw;

        /* 0x004e8781..0x004e87b5 keeps the three adjusted directions in x87
         * registers. Only Z is also stored as float, and the recomputed dot
         * consumes its retained value. */
        lightDirectionRaw[0] +=
            adjustment * (long double)groundNormal[0];
        lightDirectionRaw[1] +=
            adjustment * (long double)groundNormal[1];
        lightDirectionRaw[2] +=
            adjustment * (long double)groundNormal[2];
        roundedLightDirectionZ = (float)lightDirectionRaw[2];
        lightDotRaw =
            (lightDirectionRaw[2] * (long double)groundNormal[2] +
             lightDirectionRaw[1] * (long double)groundNormal[1]) +
            lightDirectionRaw[0] * (long double)groundNormal[0];
    }

    const float inverseLightDot = (float)(1.0L / lightDotRaw);
    /* 0x004e87ce..0x004e87e7 multiplies retained X/Y by the rounded inverse,
     * then reloads the stored float Z for the third component. */
    projectedDirection[0] = (float)(
        lightDirectionRaw[0] * (long double)inverseLightDot);
    projectedDirection[1] = (float)(
        lightDirectionRaw[1] * (long double)inverseLightDot);
    projectedDirection[2] =
        roundedLightDirectionZ * inverseLightDot;

    for (int32_t vertexIndex = 0;
         vertexIndex < tess.vertexCount;
         ++vertexIndex) {
        float *vertex =
            &tess.xyz[(size_t)vertexIndex *
                      (size_t)tess.vertexComponentCount];
        const float height =
            vertex[2] * groundNormal[2] +
            vertex[1] * groundNormal[1] +
            vertex[0] * groundNormal[0] +
            groundDistance;

        vertex[0] -= projectedDirection[0] * height;
        vertex[1] -= projectedDirection[1] * height;
        vertex[2] -= projectedDirection[2] * height;
    }
}

/* Source: CoDUOMP.exe 0x004e7e10..0x004e7e45.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004e7e10_004e7e45.mcode.
 * Name: exact same-module Mac symbol R_AddEdgeDef. The Windows LTCG body of
 * RB_ShadowTessEnd at 0x004e83df..0x004e8471 additionally contains three
 * inlined copies of this append operation. */
void R_AddEdgeDef(int32_t firstVertex, int32_t secondVertex,
                  qboolean facing)
{
    const int32_t edgeCount = rendererShadowEdgeCounts[firstVertex];
    renderer_shadow_edge_t *edge;

    if (edgeCount == R_MAX_SHADOW_EDGES_PER_VERTEX)
        return;

    edge = &rendererShadowEdges[firstVertex][edgeCount];
    edge->secondVertex = secondVertex;
    edge->facing = facing;
    rendererShadowEdgeCounts[firstVertex] = edgeCount + 1;
}

/* Source: CoDUOMP.exe 0x004e7e50..0x004e81d1.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004e7e50_004e81d2.mcode.
 * Name and source-level immediate-mode calls: exact same-module Mac symbol
 * R_RenderShadowEdges. The Windows LTCG body inlines the four vertex helpers,
 * RB_glEnd, and RB_EndImmediateMode. */
void R_RenderShadowEdges(void)
{
    RB_BeginImmediateMode();

    for (int32_t firstVertex = 0;
         firstVertex < tess.vertexCount;
         ++firstVertex) {
        const int32_t edgeCount =
            rendererShadowEdgeCounts[firstVertex];

        for (int32_t edgeIndex = 0;
             edgeIndex < edgeCount;
             ++edgeIndex) {
            const renderer_shadow_edge_t *edge =
                &rendererShadowEdges[firstVertex][edgeIndex];
            int32_t reverseFacingCounts[2] = {0, 0};

            if (edge->facing == qfalse)
                continue;

            for (int32_t reverseIndex = 0;
                 reverseIndex <
                     rendererShadowEdgeCounts[edge->secondVertex];
                 ++reverseIndex) {
                const renderer_shadow_edge_t *reverseEdge =
                    &rendererShadowEdges[edge->secondVertex][reverseIndex];

                if (reverseEdge->secondVertex == firstVertex)
                    ++reverseFacingCounts[reverseEdge->facing];
            }

            if (reverseFacingCounts[1] != 0)
                continue;

            const float *firstFront =
                &tess.xyz[(size_t)firstVertex *
                          (size_t)tess.vertexComponentCount];
            const float *firstBack =
                &tess.xyz[(size_t)(firstVertex + tess.vertexCount) *
                          (size_t)tess.vertexComponentCount];
            const float *secondFront =
                &tess.xyz[(size_t)edge->secondVertex *
                          (size_t)tess.vertexComponentCount];
            const float *secondBack =
                &tess.xyz[(size_t)(edge->secondVertex + tess.vertexCount) *
                          (size_t)tess.vertexComponentCount];

            RB_glBegin(GL_TRIANGLE_STRIP);
            RB_glVertex3fv(firstFront);
            RB_glVertex3fv(firstBack);
            RB_glVertex3fv(secondFront);
            RB_glVertex3fv(secondBack);
            RB_glEnd();
        }
    }

    RB_EndImmediateMode();
}

/* Source: CoDUOMP.exe 0x004e81e0..0x004e85a5.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004e81e0_004e85a6.mcode.
 * Name: exact same-module Mac symbol RB_ShadowTessEnd. The Windows build
 * inlines R_AddEdgeDef, GL_Bind, GL_State, GL_Cull, and both calls to
 * R_RenderShadowEdges; the maintained source restores those boundaries. */
void RB_ShadowTessEnd(void)
{
    vec3_t lightDirection;
    vec3_t extrusion;

    if (tess.vertexCount >= R_MAX_SHADOW_VERTICES ||
        glConfig.stencilBits < 4) {
        return;
    }

    lightDirection[0] = backEnd.currentEntity->lightDir[0];
    lightDirection[1] = backEnd.currentEntity->lightDir[1];
    lightDirection[2] = backEnd.currentEntity->lightDir[2];
    extrusion[0] = lightDirection[0] * 512.0f;
    extrusion[1] = lightDirection[1] * 512.0f;
    extrusion[2] = lightDirection[2] * 512.0f;

    for (int32_t vertexIndex = 0;
         vertexIndex < tess.vertexCount;
         ++vertexIndex) {
        const float *front =
            &tess.xyz[(size_t)vertexIndex *
                      (size_t)tess.vertexComponentCount];
        float *back =
            &tess.xyz[(size_t)(vertexIndex + tess.vertexCount) *
                      (size_t)tess.vertexComponentCount];

        back[0] = front[0] - extrusion[0];
        back[1] = front[1] - extrusion[1];
        back[2] = front[2] - extrusion[2];
    }

    memset(rendererShadowEdgeCounts, 0,
           (size_t)tess.vertexCount * sizeof(rendererShadowEdgeCounts[0]));

    for (int32_t triangleIndex = 0;
         triangleIndex < tess.indexCount / 3;
         ++triangleIndex) {
        const uint16_t *triangle = &tess.indexes[triangleIndex * 3];
        const int32_t firstIndex = triangle[0];
        const int32_t secondIndex = triangle[1];
        const int32_t thirdIndex = triangle[2];
        const float *first =
            &tess.xyz[(size_t)firstIndex *
                      (size_t)tess.vertexComponentCount];
        const float *second =
            &tess.xyz[(size_t)secondIndex *
                      (size_t)tess.vertexComponentCount];
        const float *third =
            &tess.xyz[(size_t)thirdIndex *
                      (size_t)tess.vertexComponentCount];
        vec3_t firstToSecond;
        vec3_t firstToThird;
        vec3_t normal;
        float facingDot;
        qboolean facing;

        firstToSecond[0] = second[0] - first[0];
        firstToSecond[1] = second[1] - first[1];
        firstToSecond[2] = second[2] - first[2];
        firstToThird[0] = third[0] - first[0];
        firstToThird[1] = third[1] - first[1];
        firstToThird[2] = third[2] - first[2];

        normal[0] = firstToSecond[1] * firstToThird[2] -
                    firstToSecond[2] * firstToThird[1];
        normal[1] = firstToSecond[2] * firstToThird[0] -
                    firstToSecond[0] * firstToThird[2];
        normal[2] = firstToSecond[0] * firstToThird[1] -
                    firstToSecond[1] * firstToThird[0];

        facingDot = normal[2] * lightDirection[2] +
                    normal[1] * lightDirection[1] +
                    normal[0] * lightDirection[0];
        facing = facingDot > 0.0f ? qtrue : qfalse;
        rendererShadowTriangleFacing[triangleIndex] = facing;

        R_AddEdgeDef(firstIndex, secondIndex,
                     rendererShadowTriangleFacing[triangleIndex]);
        R_AddEdgeDef(secondIndex, thirdIndex,
                     rendererShadowTriangleFacing[triangleIndex]);
        R_AddEdgeDef(thirdIndex, firstIndex,
                     rendererShadowTriangleFacing[triangleIndex]);
    }

    GL_Bind(tr.defaultImage);
    /* 0x004e8496 MOV EAX,0x12 = GLS_SRCBLEND_ONE(0x02) | GLS_DSTBLEND_ZERO(0x10),
     * i.e. blend func (GL_ONE, GL_ZERO). A prior pass used GLS_DSTBLEND_ONE (0x20 ->
     * 0x22), an additive dest factor that over-brightens the shadow fill. */
    GL_State(GLS_SRCBLEND_ONE | GLS_DSTBLEND_ZERO);
    qglColor3f(0.20000000298023224f,
               0.20000000298023224f,
               0.20000000298023224f);
    qglColorMask(qfalse, qfalse, qfalse, qfalse);
    qglEnable(GL_STENCIL_TEST);
    qglStencilFunc(GL_ALWAYS, 1, UINT8_MAX);

    GL_Cull(CT_BACK_SIDED);
    qglStencilOp(GL_KEEP, GL_KEEP, GL_INCR);
    R_RenderShadowEdges();

    GL_Cull(CT_FRONT_SIDED);
    qglStencilOp(GL_KEEP, GL_KEEP, GL_DECR);
    R_RenderShadowEdges();

    qglColorMask(qtrue, qtrue, qtrue, qtrue);
}

/* Source: CoDUOMP.exe 0x004e85b0..0x004e8702.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004e85b0_004e8703.mcode.
 * Name: exact same-module Mac symbol RB_ShadowFinish. The Windows LTCG body
 * inlines GL_Cull, GL_Bind, GL_State, the immediate vertex calls, RB_glEnd,
 * and RB_EndImmediateMode. */
void RB_ShadowFinish(void)
{
    if (cg_shadows->integer != 2 || glConfig.stencilBits < 4)
        return;

    qglEnable(GL_STENCIL_TEST);
    qglStencilFunc(GL_NOTEQUAL, 0, UINT8_MAX);
    qglDisable(GL_CLIP_PLANE0);
    GL_Cull(CT_TWO_SIDED);
    GL_Bind(tr.defaultImage);
    qglLoadIdentity();

    RB_BeginImmediateMode();
    RB_glColor3f(0.60000002384185791f,
                 0.60000002384185791f,
                 0.60000002384185791f);
    GL_State(GLS_DEPTHMASK_TRUE |
             GLS_SRCBLEND_DST_COLOR |
             GLS_DSTBLEND_ZERO);
    RB_glBegin(GL_QUADS);
    RB_glVertex3f(-100.0f, 100.0f, -10.0f);
    RB_glVertex3f(100.0f, 100.0f, -10.0f);
    RB_glVertex3f(100.0f, -100.0f, -10.0f);
    RB_glVertex3f(-100.0f, -100.0f, -10.0f);
    RB_glEnd();

    qglDisable(GL_STENCIL_TEST);
    RB_EndImmediateMode();
}
