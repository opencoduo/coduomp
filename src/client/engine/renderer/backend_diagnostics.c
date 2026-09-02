#include "backend.h"

#include "gl_api.h"
#include "gl_state.h"
#include "renderer_cvars.h"

#include <stdint.h>

enum {
    RB_DIAGNOSTIC_LINE_BATCH_VERTICES = 256
};

#define RB_DIAGNOSTIC_DEPTH_NEAR 0.0
#define RB_DIAGNOSTIC_DEPTH_FAR 1.0
#define RB_DIAGNOSTIC_TRIS_DEPTH_FAR 0.10000000149011612 /* original double 0x3fb99999a0000000 */
#define RB_DIAGNOSTIC_NORMAL_DEPTH_FAR 0.9990000128746033 /* original double 0x3feff7cee0000000 */

/* Source: CoDUOMP.exe 0x004eb2b0..0x004eb36d.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004eb2b0_004eb36e.mcode.
 * Name and source-level argument: exact same-module Mac symbol DrawTris.
 * The function temporarily installs the dedicated show-tris shader state;
 * the Windows body intentionally contains only the state/depth operations
 * below and then restores the caller's tessellation fields. */
void DrawTris(shaderCommands_t *tessellation)
{
    shader_t *savedShader;
    int32_t savedStageCount;
    shaderStage_t **savedStages;

    if (r_showtris->integer <= 2 && (tessellation->entity == &backEnd.entity2D || tessellation->entity == &tr.worldEntity)) {
        return;
    }

    savedShader = tessellation->shader;
    savedStageCount = tessellation->activeStageCount;
    savedStages = tessellation->activeStages;

    tessellation->shader = tr.showTrisShader;
    tessellation->activeStages = tr.showTrisShader->stages;
    tessellation->activeStageCount = tr.showTrisShader->numUnfoggedPasses;

    if ((r_showtris->integer & 1) != 0) {
        qglDepthRange(RB_DIAGNOSTIC_DEPTH_NEAR, RB_DIAGNOSTIC_TRIS_DEPTH_FAR);
    }
    qglDepthMask((uint8_t)qfalse);
    qglDepthRange(RB_DIAGNOSTIC_DEPTH_NEAR, RB_DIAGNOSTIC_DEPTH_FAR);

    tessellation->activeStages = savedStages;
    tessellation->shader = savedShader;
    tessellation->activeStageCount = savedStageCount;
}

/* Source: CoDUOMP.exe 0x004eb370..0x004eb68c.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004eb370_004eb68d.mcode.
 * Name and source-level argument: exact same-module Mac symbol DrawNormals.
 * Both binaries prove the tangent/bitangent/normal colors, two-unit line
 * length, and the 256-vertex flush cadence. */
void DrawNormals(shaderCommands_t *tessellation)
{
    int32_t vertexIndex;

    if (tessellation->requiresVertexBasis == qfalse)
        return;

    RB_BeginImmediateMode();
    qglDepthRange(RB_DIAGNOSTIC_DEPTH_NEAR, RB_DIAGNOSTIC_NORMAL_DEPTH_FAR);
    GL_Bind(tr.defaultImage);
    RB_glColor3f(1.0f, 1.0f, 1.0f);

    if (tessellation->stageTangentsValid == qfalse || tessellation->stageBitangentsValid == qfalse) {
        RB_CalcTangentSpace();
    }

    if (r_shownormals->integer == 1) {
        qglDepthRange(RB_DIAGNOSTIC_DEPTH_NEAR, RB_DIAGNOSTIC_DEPTH_NEAR);
    }

    GL_State(GLS_DEPTHMASK_TRUE | GLS_POLYMODE_LINE);
    RB_glBegin(GL_LINES);

    for (vertexIndex = 0; vertexIndex < tessellation->vertexCount; ++vertexIndex) {
        const float *xyz = &tessellation->xyz[vertexIndex * tessellation->vertexComponentCount];
        vec3_t endpoint;

        if (tessellation->stageTangentsValid != qfalse && tessellation->stageBitangentsValid != qfalse) {
            const vec3_t *tangent = &tessellation->stageTangents[vertexIndex];
            const vec3_t *bitangent = &tessellation->stageBitangents[vertexIndex];

            RB_glColor3f(1.0f, 0.5f, 0.5f);
            RB_glVertex3fv(xyz);
            endpoint[0] = ((*tangent)[0] + (*tangent)[0]) + xyz[0];
            endpoint[1] = ((*tangent)[1] + (*tangent)[1]) + xyz[1];
            endpoint[2] = ((*tangent)[2] + (*tangent)[2]) + xyz[2];
            RB_glVertex3fv(endpoint);

            RB_glColor3f(0.5f, 1.0f, 0.5f);
            RB_glVertex3fv(xyz);
            endpoint[0] = ((*bitangent)[0] + (*bitangent)[0]) + xyz[0];
            endpoint[1] = ((*bitangent)[1] + (*bitangent)[1]) + xyz[1];
            endpoint[2] = ((*bitangent)[2] + (*bitangent)[2]) + xyz[2];
            RB_glVertex3fv(endpoint);

            RB_glColor3f(0.5f, 0.5f, 1.0f);
        }

        RB_glVertex3fv(xyz);
        endpoint[0] = (tessellation->stageNormals[vertexIndex][0] + tessellation->stageNormals[vertexIndex][0]) + xyz[0];
        endpoint[1] = (tessellation->stageNormals[vertexIndex][1] + tessellation->stageNormals[vertexIndex][1]) + xyz[1];
        endpoint[2] = (tessellation->stageNormals[vertexIndex][2] + tessellation->stageNormals[vertexIndex][2]) + xyz[2];
        RB_glVertex3fv(endpoint);

        if ((vertexIndex & (RB_DIAGNOSTIC_LINE_BATCH_VERTICES - 1)) == RB_DIAGNOSTIC_LINE_BATCH_VERTICES - 1) {
            RB_glEnd();
            RB_glBegin(GL_LINES);
        }
    }

    RB_glEnd();
    qglDepthRange(RB_DIAGNOSTIC_DEPTH_NEAR, RB_DIAGNOSTIC_DEPTH_FAR);
    RB_EndImmediateMode();
}

/* Source: CoDUOMP.exe 0x004eb690..0x004eb85e.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004eb690_004eb85f.mcode.
 * Name and source-level argument: exact same-module Mac symbol
 * DrawColoredNormals. The unusual color lookup is not a transcription fix:
 * both Windows x87 and Mac PPC bodies use the X components of normals i,
 * i+1, and i+2 as RGB, while the line direction uses normal i's XYZ. */
void DrawColoredNormals(shaderCommands_t *tessellation)
{
    int32_t vertexIndex;

    RB_BeginImmediateMode();
    GL_Bind(tr.defaultImage);
    RB_glColor3f(1.0f, 1.0f, 1.0f);
    GL_State(GLS_DEPTHMASK_TRUE | GLS_POLYMODE_LINE);
    RB_glBegin(GL_LINES);

    for (vertexIndex = 0; vertexIndex < tessellation->vertexCount; ++vertexIndex) {
        const float *xyz = &tessellation->xyz[vertexIndex * tessellation->vertexComponentCount];
        const int32_t greenNormalIndex = vertexIndex + 1 < tessellation->vertexCount ? vertexIndex + 1 : tessellation->vertexCount - 1;
        const int32_t blueNormalIndex = vertexIndex + 2 < tessellation->vertexCount ? vertexIndex + 2 : tessellation->vertexCount - 1;
        vec3_t endpoint;

        /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
        RB_glColor3f((tessellation->stageNormals[vertexIndex][0] + 1.0f) * 0.5f,
                     (tessellation->stageNormals[greenNormalIndex][0] + 1.0f) * 0.5f,
                     (tessellation->stageNormals[blueNormalIndex][0] + 1.0f) * 0.5f);
        RB_glVertex3fv(xyz);
        endpoint[0] = (tessellation->stageNormals[vertexIndex][0] + tessellation->stageNormals[vertexIndex][0]) + xyz[0];
        endpoint[1] = (tessellation->stageNormals[vertexIndex][1] + tessellation->stageNormals[vertexIndex][1]) + xyz[1];
        endpoint[2] = (tessellation->stageNormals[vertexIndex][2] + tessellation->stageNormals[vertexIndex][2]) + xyz[2];
        RB_glVertex3fv(endpoint);
    }

    RB_glEnd();
    qglDepthRange(RB_DIAGNOSTIC_DEPTH_NEAR, RB_DIAGNOSTIC_DEPTH_FAR);
    RB_EndImmediateMode();
}

#undef RB_DIAGNOSTIC_DEPTH_NEAR
#undef RB_DIAGNOSTIC_DEPTH_FAR
#undef RB_DIAGNOSTIC_TRIS_DEPTH_FAR
#undef RB_DIAGNOSTIC_NORMAL_DEPTH_FAR
